// Booster — SpatialIndex3DDynamic: LSM-style runtime add/remove over SpatialIndex3D.
//
// The stock tree is the fastest STATIC 3D index we have measured (4.1x faster bulk build, 1.9x
// faster window queries than Jolt's quadtree at N=100k), but its lazy-rebuild mutation model loses
// ~600x on game-frame churn: any single mutation dirties the whole tree and the next query pays a
// full O(n log n) rebuild. This wrapper closes that gap without touching the packed tree's query
// speed:
//
//   inserts  -> append to a small delta list (O(1); free-list slot reuse)
//   removes  -> id-keyed tombstone (O(1); the tree is never touched; no box required)
//   queries  -> tree results MINUS tombstoned ids PLUS a delta scan under the tree's exact
//               replicated quantization (hit sets stay bit-exact with a rebuilt tree)
//   merge    -> when the delta crosses the threshold, fold everything into the tree with
//               SpatialIndex3D::merge_apply -- an O(n) incremental sorted-merge, NOT a rebuild
//
// Determinism contract: result SETS are always an exact function of the live entry set (same
// quantized-oracle guarantees as the stock tree, gated across merges in tesseral-bench). Result
// ORDER is tree-traversal order followed by delta order, i.e. dependent on operation history; call
// flush() when a canonical state is wanted (frame boundary, lockstep hash point).
//
// Two correctness rules this layer earned the hard way (both gated by harness tests):
//   1. A kill whose entry is still PENDING in the delta (added and removed within one merge
//      window) is local-only: the pending entry is dropped (delta stays duplicate-free under
//      slot reuse) and NO tombstone is recorded -- the tree has nothing to forget.
//   2. From an EMPTY tree the plain build is cheaper than merge_apply (which copies the entry
//      set twice); the result is identical, so the merge path is chosen by tree size.
//
// Measured (N=100k base, tesseral-bench runs 2-4, vs Jolt quadtree):
//   churn (add 20 + remove 20 + 20 queries x 50): 762.8 ms stock -> 3.7 ms here (Jolt: 6.0 ms)
//   bulk build: 17.3 ms stock tree, 11.4 ms radix-sorted, 25.5 ms through this wrapper
//   10k window queries: 6.0 ms stock tree, 6.6 ms here, 13.7 ms Jolt
//
// MIT licensed, same terms as the rest of LocomoCore. Design + numbers:
// tesseral-bench/docs/ADD-REMOVE-DESIGN.md and docs/RESULTS.md.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Structures\TesseralTree\spatial_index3d.h"

namespace booster {

class SpatialIndex3DDynamic {
public:
    explicit SpatialIndex3DDynamic(std::size_t mergeThreshold = 256, std::size_t tombstoneThreshold = 1024)
        : tau_(mergeThreshold), tombstoneTau_(tombstoneThreshold) {
        setDefaultWorld();
    }

    explicit SpatialIndex3DDynamic(const AABB3& world, int bits = 21,
                                   std::size_t mergeThreshold = 256, std::size_t tombstoneThreshold = 1024)
        : tau_(mergeThreshold), tombstoneTau_(tombstoneThreshold) {
        pinWorld(world, bits);
    }

    /// Bulk load: lay down all entries live, pin the world to their extent (matching
    /// SpatialIndex3D::bulk_build), single merge.
    static SpatialIndex3DDynamic bulk_build(const AABB3* boxes, const Id* ids, std::size_t n,
                                            std::size_t mergeThreshold = 256,
                                            std::size_t tombstoneThreshold = 1024) {
        SpatialIndex3DDynamic idx(mergeThreshold, tombstoneThreshold);
        idx.slots_.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            idx.slots_.push_back(Slot{boxes[i], ids[i], true, true});
            idx.byId_[ids[i]] = (std::uint32_t)i;
            idx.delta_.push_back((std::uint32_t)i);
        }
        if (n > 0) idx.pinWorldToExtent(boxes, n);
        idx.merge();
        return idx;
    }

    void insert(const AABB3& box, Id id) {
        // Re-insert of an existing id: tombstone the old slot first.
        const auto prior = byId_.find(id);
        if (prior != byId_.end()) kill(prior->second);

        std::uint32_t slot;
        if (!freeSlots_.empty()) {
            slot = freeSlots_.back();
            freeSlots_.pop_back();
            slots_[slot] = Slot{box, id, true, true};
        } else {
            slot = (std::uint32_t)slots_.size();
            slots_.push_back(Slot{box, id, true, true});
        }
        byId_[id] = slot;
        removedSinceMerge_.erase(id);
        delta_.push_back(slot);
        if (delta_.size() >= tau_) merge();
    }

    /// Id-keyed O(1) removal (no box needed -- the id -> slot map knows).
    bool remove(Id id) {
        const auto it = byId_.find(id);
        if (it == byId_.end()) return false;
        kill(it->second);
        if (tombstones_ >= tombstoneTau_) merge();
        return true;
    }

    /// Merge now (explicit flush point: frame boundary, lockstep hash point).
    void flush() { merge(); }

    void clear() {
        slots_.clear();
        delta_.clear();
        freeSlots_.clear();
        byId_.clear();
        removedSinceMerge_.clear();
        tombstones_ = 0;
        treeSize_ = 0;
        treeReady_ = false;
    }

    std::size_t size() const { return byId_.size(); }
    bool empty() const { return byId_.empty(); }
    /// Unmerged inserts waiting in the delta.
    std::size_t pending() const { return delta_.size(); }
    /// Merges performed so far (observability for the amortization policy).
    std::size_t merges() const { return merges_; }

    /// Ids whose box intersects `region` (tree order, then delta order).
    void query_intersects(const AABB3& region, std::vector<Id>& out) const {
        queryImpl<0>(region, out);
    }

    /// Ids whose box lies entirely within `region`.
    void query_within(const AABB3& region, std::vector<Id>& out) const {
        queryImpl<1>(region, out);
    }

    /// Nearest-k passthrough. Forces a merge first: best-first traversal has no defined
    /// meaning over a split tree+delta state, so this is the honest contract.
    void nearest(Coord x, Coord y, Coord z, int k, std::vector<Id>& out) {
        flush();
        tree_.nearest(x, y, z, k, out);
    }

private:
    struct Slot {
        AABB3 box;
        Id id;
        bool alive;
        /// True while this slot has a pending (unmerged) delta entry. Kills of such slots
        /// are local-only (rule 1 in the header comment).
        bool inDelta;
    };

    // --- quantization replication: must match SpatialIndex3D::Impl exactly, since the delta
    // scan has to speak the same grid the tree quantizes into.

    void setDefaultWorld() {
        org_[0] = org_[1] = org_[2] = -100000.0;
        scl_[0] = scl_[1] = scl_[2] = 1.0;
        maxq_ = (1 << 21) - 1;
        hasWorld_ = false;
    }

    void pinWorld(const AABB3& w, int bits) {
        const int b = bits < 1 ? 1 : (bits > 21 ? 21 : bits);
        maxq_ = (std::int32_t)((1u << b) - 1u);
        org_[0] = w.min_x; org_[1] = w.min_y; org_[2] = w.min_z;
        const double s0 = w.max_x - w.min_x, s1 = w.max_y - w.min_y, s2 = w.max_z - w.min_z;
        scl_[0] = s0 > 0 ? (double)maxq_ / s0 : 1.0;
        scl_[1] = s1 > 0 ? (double)maxq_ / s1 : 1.0;
        scl_[2] = s2 > 0 ? (double)maxq_ / s2 : 1.0;
        hasWorld_ = true;
    }

    void pinWorldToExtent(const AABB3* boxes, std::size_t n) {
        AABB3 w = boxes[0];
        for (std::size_t i = 1; i < n; ++i) {
            w.min_x = std::min(w.min_x, boxes[i].min_x);
            w.min_y = std::min(w.min_y, boxes[i].min_y);
            w.min_z = std::min(w.min_z, boxes[i].min_z);
            w.max_x = std::max(w.max_x, boxes[i].max_x);
            w.max_y = std::max(w.max_y, boxes[i].max_y);
            w.max_z = std::max(w.max_z, boxes[i].max_z);
        }
        pinWorld(w, 21);
    }

    std::int32_t qc(double v, int a) const {
        const long long t = std::llround((v - org_[a]) * scl_[a]);
        return (std::int32_t)(t < 0 ? 0 : (t > maxq_ ? maxq_ : t));
    }

    void kill(std::uint32_t slot) {
        Slot& s = slots_[slot];
        s.alive = false;
        byId_.erase(s.id);
        if (s.inDelta) {
            // Rule 1: never reached the tree. Drop the pending delta entry (keeps delta_
            // duplicate-free under slot reuse); no tombstone -- the tree has nothing to forget.
            for (std::size_t k = 0; k < delta_.size(); ++k) {
                if (delta_[k] == slot) {
                    delta_[k] = delta_.back();
                    delta_.pop_back();
                    break;
                }
            }
            s.inDelta = false;
        } else {
            removedSinceMerge_.insert(s.id);
            ++tombstones_; // tree-resident bloat drives the tombstone merge
        }
        freeSlots_.push_back(slot);
    }

    void ensureTree() {
        if (treeReady_) return;
        if (hasWorld_) {
            const AABB3 w{org_[0], org_[1], org_[2],
                          org_[0] + maxq_ / scl_[0],
                          org_[1] + maxq_ / scl_[1],
                          org_[2] + maxq_ / scl_[2]};
            tree_ = SpatialIndex3D(w, 21);
        } else {
            tree_ = SpatialIndex3D(); // default world matches default qc
        }
        treeReady_ = true;
    }

    void merge() {
        if (delta_.empty() && tombstones_ == 0) return;
        // Entries leave the pending state; delta_ is cleared below either way.
        for (const std::uint32_t slot : delta_) slots_[slot].inDelta = false;

        if (treeSize_ > 0) {
            // Incremental O(n) merge: additions are the live delta slots, removals the
            // tombstoned ids; the tree folds them into its packed run with merge_apply.
            std::vector<AABB3> addBoxes;
            std::vector<Id> addIds;
            for (const std::uint32_t slot : delta_) {
                const Slot& s = slots_[slot];
                if (s.alive) {
                    addBoxes.push_back(s.box);
                    addIds.push_back(s.id);
                }
            }
            const std::vector<Id> rems(removedSinceMerge_.begin(), removedSinceMerge_.end());
            ensureTree();
            tree_.merge_apply(addBoxes.data(), addIds.data(), addBoxes.size(), rems.data(), rems.size());
        } else {
            // Rule 2: from an EMPTY tree the plain build is cheaper and identical.
            SpatialIndex3D fresh = hasWorld_
                ? SpatialIndex3D(AABB3{org_[0], org_[1], org_[2],
                                       org_[0] + maxq_ / scl_[0],
                                       org_[1] + maxq_ / scl_[1],
                                       org_[2] + maxq_ / scl_[2]},
                                 21)
                : SpatialIndex3D();
            for (const Slot& s : slots_) {
                if (s.alive) fresh.insert(s.box, s.id);
            }
            tree_ = std::move(fresh);
            treeReady_ = true;
            // Force the build now (explicit flush point, not a surprise mid-query).
            std::vector<Id> sink;
            tree_.query_intersects({0, 0, 0, 0, 0, 0}, sink);
        }

        purgeDeadSlots();
        treeSize_ = slots_.size();
        delta_.clear();
        removedSinceMerge_.clear();
        freeSlots_.clear();
        tombstones_ = 0;
        ++merges_;
    }

    /// Drop tombstoned slots from the shadow storage (the tree just purged them from itself);
    /// rebuilds the id -> slot map. Keeps storage proportional to the LIVE set, so long
    /// sessions cannot leak dead slots between free-list generations.
    void purgeDeadSlots() {
        std::size_t w = 0;
        byId_.clear();
        for (std::size_t r = 0; r < slots_.size(); ++r) {
            if (slots_[r].alive) {
                if (w != r) slots_[w] = slots_[r];
                byId_[slots_[w].id] = (std::uint32_t)w;
                ++w;
            }
        }
        slots_.resize(w);
    }

    template <int MODE>
    void queryImpl(const AABB3& region, std::vector<Id>& out) const {
        out.clear();
        // Tree results minus tombstones (exact, tree order).
        std::vector<Id> treeHits;
        if (MODE == 0) const_cast<SpatialIndex3D&>(tree_).query_intersects(region, treeHits);
        else           const_cast<SpatialIndex3D&>(tree_).query_within(region, treeHits);
        out.reserve(treeHits.size());
        for (const Id id : treeHits) {
            if (removedSinceMerge_.find(id) == removedSinceMerge_.end()) out.push_back(id);
        }
        // Delta scan: same quantized-box test the tree would apply (intersect vs within).
        const std::int32_t qlo[3] = {qc(region.min_x, 0), qc(region.min_y, 1), qc(region.min_z, 2)};
        const std::int32_t qhi[3] = {qc(region.max_x, 0), qc(region.max_y, 1), qc(region.max_z, 2)};
        for (const std::uint32_t slot : delta_) {
            const Slot& s = slots_[slot];
            if (!s.alive) continue; // tombstoned in the delta: no ghosts
            const std::int32_t mn[3] = {qc(s.box.min_x, 0), qc(s.box.min_y, 1), qc(s.box.min_z, 2)};
            const std::int32_t mx[3] = {qc(s.box.max_x, 0), qc(s.box.max_y, 1), qc(s.box.max_z, 2)};
            const bool hit = MODE == 0
                ? (mx[0] >= qlo[0] && mn[0] <= qhi[0] &&
                   mx[1] >= qlo[1] && mn[1] <= qhi[1] &&
                   mx[2] >= qlo[2] && mn[2] <= qhi[2])
                : (mn[0] >= qlo[0] && mx[0] <= qhi[0] &&
                   mn[1] >= qlo[1] && mx[1] <= qhi[1] &&
                   mn[2] >= qlo[2] && mx[2] <= qhi[2]);
            if (hit) out.push_back(s.id);
        }
    }

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> delta_;
    std::vector<std::uint32_t> freeSlots_;
    std::unordered_map<Id, std::uint32_t> byId_;
    std::unordered_set<Id> removedSinceMerge_;
    SpatialIndex3D tree_;
    std::size_t tombstones_ = 0;
    std::size_t merges_ = 0;
    const std::size_t tau_;
    const std::size_t tombstoneTau_;
    double org_[3];
    double scl_[3];
    std::int32_t maxq_ = (1 << 21) - 1;
    bool hasWorld_ = false;
    bool treeReady_ = false;
    std::size_t treeSize_ = 0;
};

} // namespace booster
