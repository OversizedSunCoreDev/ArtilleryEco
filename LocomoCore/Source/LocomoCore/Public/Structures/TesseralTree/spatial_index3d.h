// Booster — SpatialIndex3D facade: a deterministic 3D spatial index behind a Boost-free, template-free
// pimpl (the 3D counterpart to booster::SpatialIndex). A box is stored as two 3D-interleaved Morton
// codes (x/y/z -> bits 3k/3k+1/3k+2 of a 63-bit code, bit 63 spare). The window query is the exact,
// in-algebra CHEBYSHEV overlap: per-lane tesseral differences whose sign lands on the spare bit --
// six masked epi64 subtracts + OR + movemask, no de-lace. See research/tesseral-rtree/ver2 (bricks 1-3;
// same-harness it beats Boost R* 3D ~1.7-2.2x on query, all EXACT). Requires AVX2.
//
// Model: a bulk-packed (Z-order) static tree with LAZY REBUILD. insert/remove mutate an entry list and
// mark dirty; the next query rebuilds. So query results are a deterministic function of the current
// entry SET -- independent of insertion order -- which folds cleanly into a lockstep/rollback hash.
// Results come back in the tree's (deterministic) traversal order, NOT sorted by Id.
//
// Self-contained: include just this header (Public) + link the .cpp (Private), or include the
// "whole enchilada" header spatial_index3d_inline.h to pull the impl into your own TU.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "LCM_Config.h"

namespace booster {

// Same typedefs as <booster/spatial_index.h> (a redeclaration to the same type is legal), so this
// header stands alone AND coexists with the 2D facade if both are included.
using Id    = std::uint64_t;   // caller-owned stable entity id
using Coord = double;

/// Axis-aligned bounding box (3D). min must be <= max on each axis.
struct AABB3 {
    Coord min_x, min_y, min_z, max_x, max_y, max_z;
};

class SpatialIndex3D {
public:
    SpatialIndex3D();
    /// Pin the fixed-point grid to a world extent. Coordinates are quantized to a `bits`-per-axis grid
    /// (clamped to <= 21, the 3D Morton budget: 2097152 positions/axis).
    explicit SpatialIndex3D(const AABB3& world, int bits = 21);
    ~SpatialIndex3D();

    SpatialIndex3D(SpatialIndex3D&&) noexcept;
    SpatialIndex3D& operator=(SpatialIndex3D&&) noexcept;
    SpatialIndex3D(const SpatialIndex3D&) = delete;
    SpatialIndex3D& operator=(const SpatialIndex3D&) = delete;

    /// Build in one shot from parallel arrays (packing load). boxes[i] is stored under ids[i].
    static SpatialIndex3D bulk_build(const AABB3* boxes, const Id* ids, std::size_t n);

    void        insert(const AABB3& box, Id id);
    /// Removes one entry equal to (box, id). Returns true if one was removed.
    bool        remove(const AABB3& box, Id id);
    /// Folds additions into the packed tree and purges removed ids in O(n)
    /// via a linear sorted-merge, instead of dirtying + full rebuild. The
    /// resulting tree is bit-identical to a full rebuild (same total order,
    /// same packing), and the merge chains: repeated calls stay O(n) each.
    void        merge_apply(const AABB3* addBoxes, const Id* addIds, std::size_t addN,
                            const Id* removeIds, std::size_t removeN);
    void        clear();
    std::size_t size() const;
    bool        empty() const;

    // --- queries (out is cleared then filled; results in deterministic traversal order) ---

    /// Ids whose box intersects `region`.
    void query_intersects(const AABB3& region, std::vector<Id>& out) const;
    /// Ids whose box lies entirely within `region`.
    void query_within(const AABB3& region, std::vector<Id>& out) const;
    /// The k ids whose box is nearest to point (x,y,z), ascending distance, ties by ascending Id.
    void nearest(Coord x, Coord y, Coord z, int k, std::vector<Id>& out) const;

    /// AABB enclosing all entries. Returns all-zero if empty.
    AABB3 bounds() const;

private:
    struct Impl;
    Impl* p_;
};

} // namespace booster
