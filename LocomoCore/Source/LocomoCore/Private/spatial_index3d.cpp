// Booster — SpatialIndex3D implementation. 3D bit-interleaved Morton codes (x/y/z -> bits 3k/3k+1/3k+2,
// 63-bit code, bit 63 spare). Window query = the tesseral algebra's Chebyshev overlap (six masked epi64
// subtracts, sign on the spare bit, no de-lace). Bulk-packed static tree + lazy rebuild for insert/
// remove; the tree (nodes/root/dirty) is `mutable` so a const query can rebuild on demand.
// Determinism: integer only in the query (no FP), stable code+id ordering in the pack.
//
// Requires AVX2 (the 4-wide epi64 kernel).
#include "Structures\TesseralTree\spatial_index3d.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>
#include <vector>

#if !defined(__AVX2__)
#  error "Booster SpatialIndex3D requires AVX2 (compile with /arch:AVX2 or -mavx2)."
#endif
#include <immintrin.h>

#if defined(__GNUC__) || defined(__clang__)
#  define BOOSTER3_PREFETCH(p) __builtin_prefetch((const void*)(p))
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#  include <xmmintrin.h>
#  define BOOSTER3_PREFETCH(p) _mm_prefetch((const char*)(p), _MM_HINT_T0)
#else
#  define BOOSTER3_PREFETCH(p) ((void)0)
#endif

namespace booster {
namespace {

using i32 = std::int32_t; using i64 = std::int64_t; using u32 = std::uint32_t; using u64 = std::uint64_t;
constexpr int MAXK = 16, PAD = 16;   // PAD multiple of 4 for the 4-wide epi64 kernel
constexpr u64 MX = 0x1249249249249249ull, MY = MX << 1, MZ = MX << 2;
constexpr u64 SIGNB = 0x8000000000000000ull;

// 21-bit coord -> every 3rd bit (libmorton magic-bits split-by-3), and the inverse.
inline u64 split3(u32 a) {
    u64 x = a & 0x1FFFFFull;
    x = (x | x << 32) & 0x1F00000000FFFFull;
    x = (x | x << 16) & 0x1F0000FF0000FFull;
    x = (x | x << 8)  & 0x100F00F00F00F00Full;
    x = (x | x << 4)  & 0x10C30C30C30C30C3ull;
    x = (x | x << 2)  & 0x1249249249249249ull;
    return x;
}
inline u32 compact3(u64 x) {
    x &= 0x1249249249249249ull;
    x = (x | (x >> 2))  & 0x10C30C30C30C30C3ull;
    x = (x | (x >> 4))  & 0x100F00F00F00F00Full;
    x = (x | (x >> 8))  & 0x1F0000FF0000FFull;
    x = (x | (x >> 16)) & 0x1F00000000FFFFull;
    x = (x | (x >> 32)) & 0x1FFFFFull;
    return (u32)x;
}
inline u64 mort3(u32 x, u32 y, u32 z) { return split3(x) | (split3(y) << 1) | (split3(z) << 2); }
inline u32 unX(u64 c) { return compact3(c); }
inline u32 unY(u64 c) { return compact3(c >> 1); }
inline u32 unZ(u64 c) { return compact3(c >> 2); }

// Morton -> Hilbert re-lace (21 3-bit levels, A4 walk; verbatim from research ver1/zer/DualZerIndex.h).
// Used ONLY as the bulk-load SORT KEY -- Hilbert runs stay spatially compact where the Z-curve jumps at
// octant boundaries, so leaf MBRs come out tighter => ~20-36% faster query at the same fill (pack bake-off,
// research/tesseral-rtree/ver2/query_pack_bench.cpp). A bijection, so set-determinism is preserved
// (same Morton code -> same key; id still breaks exact-position ties). Leaves still store Morton codes.
static constexpr std::uint8_t M2H[96] = {48,33,27,34,47,78,28,77,66,29,51,52,65,30,72,63,76,95,75,24,53,54,82,81,18,3,17,80,61,4,62,15,0,59,71,60,49,50,86,85,84,83,5,90,79,56,6,89,32,23,1,94,11,12,2,93,42,41,13,14,35,88,36,31,92,37,87,38,91,74,8,73,46,45,9,10,7,20,64,19,70,25,39,16,69,26,44,43,22,55,21,68,57,40,58,67};
inline u64 mortonToHilbert(u64 in)
{
    u32 tr = 0;
    u64 out = 0; 
    for (int i = 60; i >= 0; i -= 3)
    {
        tr = M2H[tr | ((in >> i) & 7u)];
        out = (out << 3) | (tr & 7u); 
        tr &= ~7u; 
    } 
    return out;
}

// per-lane min/max on 3D codes (dilated values keep per-lane order). code -> code (MBRs).
inline u64 lmin(u64 a, u64 b) { u64 ax=a&MX,bx=b&MX,ay=a&MY,by=b&MY,az=a&MZ,bz=b&MZ; return (ax<bx?ax:bx)|(ay<by?ay:by)|(az<bz?az:bz); }
inline u64 lmax(u64 a, u64 b) { u64 ax=a&MX,bx=b&MX,ay=a&MY,by=b&MY,az=a&MZ,bz=b&MZ; return (ax>bx?ax:bx)|(ay>by?ay:by)|(az>bz?az:bz); }

struct IBox3 { i32 mn[3], mx[3]; };
struct Entry { IBox3 b; Id id; };
struct alignas(32) Node { int count; bool leaf; alignas(32) u64 lo[PAD], hi[PAD]; u64 pay[PAD]; };

inline u64 loCode(const IBox3& b) { return mort3((u32)b.mn[0], (u32)b.mn[1], (u32)b.mn[2]); }
inline u64 hiCode(const IBox3& b) { return mort3((u32)b.mx[0], (u32)b.mx[1], (u32)b.mx[2]); }
inline bool ibEq(const IBox3& a, const IBox3& b) {
    return a.mn[0]==b.mn[0]&&a.mn[1]==b.mn[1]&&a.mn[2]==b.mn[2]&&a.mx[0]==b.mx[0]&&a.mx[1]==b.mx[1]&&a.mx[2]==b.mx[2];
}

} // namespace

// LSD radix sort of an index array by a u64 key: 8 stable counting-sort passes of 8 bits, with an
// early-out when the remaining high bytes are all zero (small ids / low hilbert keys skip passes).
// Measured ~1.5x faster than std::sort for the bulk pack at N=100k (tesseral-bench run 3); the tree's
// README already prescribes this ("use the radix sort from gks hull").
inline void radixPass(std::vector<u32>& src, std::vector<u32>& dst, const std::vector<u64>& keyOf, int byteIdx) {
    std::uint32_t count[256] = {};
    const std::size_t n = src.size();
    const int shift = byteIdx * 8;
    for (std::size_t i = 0; i < n; ++i) ++count[(keyOf[src[i]] >> shift) & 0xFF];
    std::uint32_t sum = 0;
    for (std::uint32_t& c : count) { const std::uint32_t t = c; c = sum; sum += t; }
    for (std::size_t i = 0; i < n; ++i) dst[count[(keyOf[src[i]] >> shift) & 0xFF]++] = src[i];
}

// Stable total order by keyOf. Sort the tie-break key FIRST with a separate call (stable passes
// preserve it), then the primary key. Result lands back in `ord` (uses `tmp` as scratch).
inline void radixSortByKey(std::vector<u32>& ord, std::vector<u32>& tmp, const std::vector<u64>& keyOf) {
    tmp.resize(ord.size());
    u64 maxKey = 0;
    for (u64 k : keyOf) maxKey |= k; // OR-fold: highest set bit anywhere
    std::vector<u32>* a = &ord;
    std::vector<u32>* b = &tmp;
    for (int byte = 0; byte < 8; ++byte) {
        if (byte > 0 && (maxKey >> (byte * 8)) == 0) break; // nothing left to sort by
        radixPass(*a, *b, keyOf, byte);
        std::swap(a, b);
    }
    if (a != &ord) ord = *a; // odd number of passes: copy back
}

struct SpatialIndex3D::Impl {
    std::vector<Entry> entries;
    // built tree (lazy): mutable so a const query can rebuild on demand.
    mutable std::vector<Node> nodes;
    mutable int root = 0;
    mutable bool dirty = true;
    // persistent build scratch -- reused across rebuilds so a long-lived index's periodic rebuild
    // does no per-batch allocation (resize/clear keep capacity once warm).
    mutable std::vector<u64> scLo, scHi, scHil;
    mutable std::vector<u32> scOrd;
    mutable std::vector<int> scCur, scNx;
    // quantization
    double org[3] = {-100000.0, -100000.0, -100000.0};
    double scl[3] = {1.0, 1.0, 1.0};
    i32 maxq = (1 << 21) - 1; int bits = 21;

    void setWorld(const AABB3& w, int b) {
        bits = b < 1 ? 1 : (b > 21 ? 21 : b); maxq = (i32)((1u << bits) - 1u);
        org[0]=w.min_x; org[1]=w.min_y; org[2]=w.min_z;
        double s0=w.max_x-w.min_x, s1=w.max_y-w.min_y, s2=w.max_z-w.min_z;
        scl[0]= s0>0 ? (double)maxq/s0 : 1.0; scl[1]= s1>0 ? (double)maxq/s1 : 1.0; scl[2]= s2>0 ? (double)maxq/s2 : 1.0;
    }
    i32 qc(double v, int a) const { i64 t = (i64)std::llround((v - org[a]) * scl[a]); return (i32)(t < 0 ? 0 : (t > maxq ? maxq : t)); }
    double deq(i32 v, int a) const { return org[a] + (double)v / scl[a]; }
    IBox3 toI(const AABB3& b) const { return IBox3{{qc(b.min_x,0),qc(b.min_y,1),qc(b.min_z,2)},{qc(b.max_x,0),qc(b.max_y,1),qc(b.max_z,2)}}; }

    void build() const {
        nodes.clear(); root = 0;
        const std::size_t n = entries.size();
        if (n == 0)
        {
            nodes.emplace_back(); 
            nodes[0].leaf = true;
            nodes[0].count = 0;
            root = 0;
            dirty = false; return;
        }
        nodes.reserve(n / MAXK * 2 + 8);   // enough for all nodes -> no realloc mid-build (refs stay valid)
        scLo.resize(n); 
        scHi.resize(n); 
        scHil.resize(n); 
        scOrd.resize(n);   // warm: resize keeps capacity, no alloc
        for (std::size_t i = 0; i < n; ++i)
        {
            scLo[i]=loCode(entries[i].b); 
            scHi[i]=hiCode(entries[i].b); 
            scHil[i]=mortonToHilbert(scLo[i]);
            scOrd[i]=(u32)i;
        }
        // Hilbert-order bulk load (tighter leaves than Z-order). Stable total order: Hilbert key then id
        // -> deterministic tree independent of insertion order (Hilbert is a bijection). Leaves store Morton.
        // The sort is two stable LSD radix passes (tie-break id key first): identical total order
        // (hil, id) to the old std::sort comparator, ~1.5x faster at N=100k.
        {
            std::vector<u64> idKey(n);
            for (std::size_t i = 0; i < n; ++i) idKey[i] = entries[i].id;
            std::vector<u32> tmp;
            radixSortByKey(scOrd, tmp, idKey); // tie-break key first (stable)
            radixSortByKey(scOrd, tmp, scHil); // then the hilbert key (stable)
        }
        packFromSortedOrder();
    }

    // The leaf+parent packing phase of build(), factored out so mergeApply can repack after a
    // linear sorted-merge. Expects scOrd/scLo/scHi/entries to describe the run to pack (scOrd maps
    // pack-order -> entries index).
    void packFromSortedOrder() const {
        nodes.clear();
        const std::size_t n = scOrd.size();
        if (n == 0) {
            nodes.emplace_back();
            nodes[0].leaf = true;
            nodes[0].count = 0;
            root = 0;
            dirty = false;
            return;
        }
        nodes.reserve(n / MAXK * 2 + 8);   // enough for all nodes -> no realloc mid-build (refs stay valid)
        scCur.clear();
        for (std::size_t i = 0; i < n; ) {
            int ni = (int)nodes.size(); nodes.emplace_back(); Node& L = nodes[ni]; L.leaf = true; int c = 0;
            while (c < MAXK && i < n) { u32 e = scOrd[i]; L.lo[c]=scLo[e]; L.hi[c]=scHi[e]; L.pay[c]=entries[e].id; ++c; ++i; }
            L.count = c; scCur.push_back(ni);
        }
        while (scCur.size() > 1) {
            scNx.clear();
            for (std::size_t j = 0; j < scCur.size(); ) {
                int ni = (int)nodes.size(); nodes.emplace_back(); Node& P = nodes[ni]; P.leaf = false; int c = 0;
                while (c < MAXK && j < scCur.size()) {
                    const Node& C = nodes[scCur[j]]; u64 mn = C.lo[0], mx = C.hi[0];
                    for (int k = 1; k < C.count; ++k) { mn = lmin(mn, C.lo[k]); mx = lmax(mx, C.hi[k]); }
                    P.lo[c]=mn; P.hi[c]=mx; P.pay[c]=(u64)scCur[j]; ++c; ++j;
                }
                P.count = c; scNx.push_back(ni);
            }
            scCur.swap(scNx);
        }
        root = scCur[0]; dirty = false;
    }

    // O(n) incremental merge: folds sorted additions into the packed tree and purges removed ids
    // WITHOUT re-sorting the world. The previous build's scratch (scOrd/scHil) already describes
    // the live run in (hil,id) order, so the merge is a linear two-pointer pass followed by a
    // repack. After every call the invariant is refreshed (entries sorted, scOrd identity), so
    // merges chain. Deterministic: the merged run equals the full-sort run, so the resulting tree
    // is bit-identical to a full rebuild (gated by a churn-equivalence test in tesseral-bench).
    void mergeApply(const AABB3* addBoxes, const Id* addIds, std::size_t addN,
                    const Id* remIds, std::size_t remN) {
        // Invariant check: the fast path needs the clean-tree scratch (scOrd in sync with
        // entries). Anything else (dirtied by direct insert/remove, or first build) takes the
        // storage path: append adds, purge removes by id, full build.
        if (dirty || scOrd.size() != entries.size()) {
            entries.reserve(entries.size() + addN);
            for (std::size_t i = 0; i < addN; ++i) entries.push_back(Entry{toI(addBoxes[i]), addIds[i]});
            if (remN > 0) {
                std::unordered_set<Id> gone(remIds, remIds + remN);
                std::size_t w = 0;
                for (std::size_t r = 0; r < entries.size(); ++r)
                    if (gone.find(entries[r].id) == gone.end()) entries[w++] = entries[r];
                entries.resize(w);
            }
            dirty = true;
            build();
            return;
        }

        // 1) Quantize + key the additions, then sort them by (hil, id) with two stable radix
        //    passes (tie-break first), O(addN).
        std::vector<Entry> adds(addN);
        std::vector<u64> addHil(addN), addIdKey(addN);
        std::vector<u32> ord(addN);
        for (std::size_t i = 0; i < addN; ++i) {
            adds[i] = Entry{toI(addBoxes[i]), addIds[i]};
            addHil[i] = mortonToHilbert(loCode(adds[i].b));
            addIdKey[i] = adds[i].id;
            ord[i] = (u32)i;
        }
        {
            std::vector<u32> tmp;
            radixSortByKey(ord, tmp, addIdKey);
            radixSortByKey(ord, tmp, addHil);
        }

        // 2) Linear merge: old live run (via scOrd, already (hil,id)-sorted, tombstones filtered)
        //    vs sorted adds. Old scratch stays valid read-only until it is overwritten below.
        const std::size_t oldN = scOrd.size();
        const std::unordered_set<Id> gone(remIds, remIds + remN);
        std::vector<Entry> merged;
        std::vector<u64> mergedHil;
        merged.reserve(oldN + addN);
        mergedHil.reserve(oldN + addN);
        std::size_t i = 0, j = 0;
        auto oldKeyLess = [&](std::size_t oi, std::size_t aj) {
            const u32 e = scOrd[oi];
            const u64 h = scHil[e];
            const Id id = entries[e].id;
            const u64 ah = addHil[ord[aj]];
            const Id aid = adds[ord[aj]].id;
            return h != ah ? h < ah : id < aid;
        };
        while (i < oldN || j < addN) {
            // Skip tombstoned old entries.
            while (i < oldN && gone.find(entries[scOrd[i]].id) != gone.end()) ++i;
            if (i >= oldN) break;
            if (j >= addN || oldKeyLess(i, j)) {
                const u32 e = scOrd[i];
                merged.push_back(entries[e]);
                mergedHil.push_back(scHil[e]);
                ++i;
            } else {
                const u32 a = ord[j];
                merged.push_back(adds[a]);
                mergedHil.push_back(addHil[a]);
                ++j;
            }
        }
        while (j < addN) {
            const u32 a = ord[j];
            merged.push_back(adds[a]);
            mergedHil.push_back(addHil[a]);
            ++j;
        }

        // 3) Refresh storage + scratch to the new invariant (entries sorted, scOrd identity),
        //    then repack.
        entries = std::move(merged);
        const std::size_t n2 = entries.size();
        scLo.resize(n2);
        scHi.resize(n2);
        scHil.resize(n2);
        scOrd.resize(n2);
        for (std::size_t k = 0; k < n2; ++k) {
            scLo[k] = loCode(entries[k].b);
            scHi[k] = hiCode(entries[k].b);
            scHil[k] = mergedHil[k];
            scOrd[k] = (u32)k;
        }
        packFromSortedOrder();
    }

    void ensureBuilt() const { if (dirty) build(); }

    // MODE 0 = intersect (overlap), MODE 1 = within (containment). Internal nodes always use intersect.
    template <int MODE, class Sink>
    void query(const IBox3& reg, Sink sink) const {
        ensureBuilt();
        const u64 qlo = loCode(reg), qhi = hiCode(reg);
        const __m256i vMX=_mm256_set1_epi64x((long long)MX), vMY=_mm256_set1_epi64x((long long)MY), vMZ=_mm256_set1_epi64x((long long)MZ);
        const __m256i qXl=_mm256_set1_epi64x((long long)(qlo&MX)), qXh=_mm256_set1_epi64x((long long)(qhi&MX));
        const __m256i qYl=_mm256_set1_epi64x((long long)(qlo&MY)), qYh=_mm256_set1_epi64x((long long)(qhi&MY));
        const __m256i qZl=_mm256_set1_epi64x((long long)(qlo&MZ)), qZh=_mm256_set1_epi64x((long long)(qhi&MZ));
        int st[256], sp = 0; st[sp++] = root;
        while (sp) {
            const Node& n = nodes[st[--sp]]; int cnt = n.count;
            if (n.leaf) {
                for (int b = 0; b < cnt; b += 4) {
                    __m256i blo=_mm256_loadu_si256((const __m256i*)(n.lo+b)), bhi=_mm256_loadu_si256((const __m256i*)(n.hi+b));
                    __m256i xh=_mm256_and_si256(bhi,vMX), xl=_mm256_and_si256(blo,vMX);
                    __m256i yh=_mm256_and_si256(bhi,vMY), yl=_mm256_and_si256(blo,vMY);
                    __m256i zh=_mm256_and_si256(bhi,vMZ), zl=_mm256_and_si256(blo,vMZ);
                    __m256i d;
                    if (MODE == 1)   // within: blo>=qlo & bhi<=qhi  (disjoint-from-containment sign)
                        d = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_sub_epi64(xl,qXl),_mm256_sub_epi64(qXh,xh)),
                                                            _mm256_or_si256(_mm256_sub_epi64(yl,qYl),_mm256_sub_epi64(qYh,yh))),
                                            _mm256_or_si256(_mm256_sub_epi64(zl,qZl),_mm256_sub_epi64(qZh,zh)));
                    else             // intersect: bhi>=qlo & qhi>=blo
                        d = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_sub_epi64(xh,qXl),_mm256_sub_epi64(qXh,xl)),
                                                            _mm256_or_si256(_mm256_sub_epi64(yh,qYl),_mm256_sub_epi64(qYh,yl))),
                                            _mm256_or_si256(_mm256_sub_epi64(zh,qZl),_mm256_sub_epi64(qZh,zl)));
                    unsigned hit = (~(unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(d))) & 0xFu;
                    int lim = cnt - b; if (lim < 4) hit &= (1u << lim) - 1u;
                    while (hit) { int i = b + (int)_tzcnt_u32(hit); sink(n.pay[i]); hit &= hit - 1; }
                }
            } else {
                for (int b = 0; b < cnt; b += 4) {
                    __m256i blo=_mm256_loadu_si256((const __m256i*)(n.lo+b)), bhi=_mm256_loadu_si256((const __m256i*)(n.hi+b));
                    __m256i xh=_mm256_and_si256(bhi,vMX), xl=_mm256_and_si256(blo,vMX);
                    __m256i yh=_mm256_and_si256(bhi,vMY), yl=_mm256_and_si256(blo,vMY);
                    __m256i zh=_mm256_and_si256(bhi,vMZ), zl=_mm256_and_si256(blo,vMZ);
                    __m256i d = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_sub_epi64(xh,qXl),_mm256_sub_epi64(qXh,xl)),
                                                                _mm256_or_si256(_mm256_sub_epi64(yh,qYl),_mm256_sub_epi64(qYh,yl))),
                                                _mm256_or_si256(_mm256_sub_epi64(zh,qZl),_mm256_sub_epi64(qZh,zl)));
                    unsigned hit = (~(unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(d))) & 0xFu;
                    int lim = cnt - b; if (lim < 4) hit &= (1u << lim) - 1u;
                    while (hit) { int i = b + (int)_tzcnt_u32(hit); int c = (int)n.pay[i]; BOOSTER3_PREFETCH(&nodes[c]); if (sp < 256) st[sp++] = c; hit &= hit - 1; }
                }
            }
        }
    }

    static i64 dist2(u64 lo, u64 hi, i32 x, i32 y, i32 z) {
        i32 x0=(i32)unX(lo),y0=(i32)unY(lo),z0=(i32)unZ(lo),x1=(i32)unX(hi),y1=(i32)unY(hi),z1=(i32)unZ(hi);
        i64 dx = x<x0 ? (i64)x0-x : (x>x1 ? (i64)x-x1 : 0);
        i64 dy = y<y0 ? (i64)y0-y : (y>y1 ? (i64)y-y1 : 0);
        i64 dz = z<z0 ? (i64)z0-z : (z>z1 ? (i64)z-z1 : 0);
        return dx*dx + dy*dy + dz*dz;
    }
};

// -------------------------------------------------------------------------- API

SpatialIndex3D::SpatialIndex3D() : p_(new Impl) {}
SpatialIndex3D::SpatialIndex3D(const AABB3& world, int bits) : p_(new Impl) { p_->setWorld(world, bits); }
SpatialIndex3D::~SpatialIndex3D() { delete p_; }
SpatialIndex3D::SpatialIndex3D(SpatialIndex3D&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
SpatialIndex3D& SpatialIndex3D::operator=(SpatialIndex3D&& o) noexcept { if (this != &o) { delete p_; p_ = o.p_; o.p_ = nullptr; } return *this; }

SpatialIndex3D SpatialIndex3D::bulk_build(const AABB3* boxes, const Id* ids, std::size_t n) {
    SpatialIndex3D idx;
    if (n > 0) {
        AABB3 w{boxes[0].min_x, boxes[0].min_y, boxes[0].min_z, boxes[0].max_x, boxes[0].max_y, boxes[0].max_z};
        for (std::size_t i = 1; i < n; ++i) {
            w.min_x=std::min(w.min_x,boxes[i].min_x); w.min_y=std::min(w.min_y,boxes[i].min_y); w.min_z=std::min(w.min_z,boxes[i].min_z);
            w.max_x=std::max(w.max_x,boxes[i].max_x); w.max_y=std::max(w.max_y,boxes[i].max_y); w.max_z=std::max(w.max_z,boxes[i].max_z);
        }
        idx.p_->setWorld(w, 21);
    }
    idx.p_->entries.resize(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        idx.p_->entries[i] = 
            Entry{ idx.p_->toI(boxes[i]), ids[i] };
    }
    idx.p_->dirty = true; idx.p_->build();
    return idx;
}

void SpatialIndex3D::insert(const AABB3& box, Id id) { 
    p_->entries.push_back(Entry{ p_->toI(box), id });
    p_->dirty = true; 
}

void SpatialIndex3D::merge_apply(const AABB3* addBoxes, const Id* addIds, std::size_t addN,
                                 const Id* removeIds, std::size_t removeN) {
    p_->mergeApply(addBoxes, addIds, addN, removeIds, removeN);
}

bool SpatialIndex3D::remove(const AABB3& box, Id id) {
    IBox3 b = p_->toI(box);
    for (std::size_t i = 0; i < p_->entries.size(); ++i)
        if (p_->entries[i].id == id && ibEq(p_->entries[i].b, b)) {
            p_->entries[i] = p_->entries.back(); p_->entries.pop_back(); p_->dirty = true; return true;
        }
    return false;
}

void SpatialIndex3D::clear() { p_->entries.clear(); p_->dirty = true; }
std::size_t SpatialIndex3D::size() const { return p_->entries.size(); }
bool SpatialIndex3D::empty() const { return p_->entries.empty(); }

void SpatialIndex3D::query_intersects(const AABB3& region, std::vector<Id>& out) const {
    out.clear(); p_->query<0>(p_->toI(region), [&](Id id){ out.push_back(id); });
}
void SpatialIndex3D::query_within(const AABB3& region, std::vector<Id>& out) const {
    out.clear(); p_->query<1>(p_->toI(region), [&](Id id){ out.push_back(id); });
}

void SpatialIndex3D::nearest(Coord x, Coord y, Coord z, int k, std::vector<Id>& out) const {
    out.clear(); if (k <= 0 || p_->entries.empty()) return;
    p_->ensureBuilt();
    i32 qx = p_->qc(x,0), qy = p_->qc(y,1), qz = p_->qc(z,2);
    struct Item { i64 d; int ni; bool leafEnt; Id id; };
    struct Cmp { bool operator()(const Item& a, const Item& b) const { return a.d > b.d; } };
    std::priority_queue<Item, std::vector<Item>, Cmp> pq;
    pq.push(Item{0, p_->root, false, 0});
    std::vector<std::pair<i64, Id>> hits;
    while (!pq.empty() && (int)hits.size() < k) {
        Item it = pq.top(); pq.pop();
        if (it.leafEnt) { hits.emplace_back(it.d, it.id); continue; }
        const Node& n = p_->nodes[it.ni];
        if (n.leaf) for (int i = 0; i < n.count; ++i) pq.push(Item{Impl::dist2(n.lo[i], n.hi[i], qx, qy, qz), 0, true, n.pay[i]});
        else        for (int i = 0; i < n.count; ++i) { int c = (int)n.pay[i]; BOOSTER3_PREFETCH(&p_->nodes[c]); pq.push(Item{Impl::dist2(n.lo[i], n.hi[i], qx, qy, qz), c, false, 0}); }
    }
    std::sort(hits.begin(), hits.end(), [](const std::pair<i64,Id>& a, const std::pair<i64,Id>& b){ return a.first!=b.first ? a.first<b.first : a.second<b.second; });
    out.reserve(hits.size());
    for (const auto& h : hits) out.push_back(h.second);
}

AABB3 SpatialIndex3D::bounds() const {
    if (p_->entries.empty()) return AABB3{0,0,0,0,0,0};
    IBox3 m = p_->entries[0].b;
    for (const auto& e : p_->entries) for (int a = 0; a < 3; ++a) { m.mn[a]=std::min(m.mn[a],e.b.mn[a]); m.mx[a]=std::max(m.mx[a],e.b.mx[a]); }
    return AABB3{ p_->deq(m.mn[0],0), p_->deq(m.mn[1],1), p_->deq(m.mn[2],2), p_->deq(m.mx[0],0), p_->deq(m.mx[1],1), p_->deq(m.mx[2],2) };
}

} // namespace booster
