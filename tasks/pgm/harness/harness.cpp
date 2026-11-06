// Trusted PGM harness. One source, two binaries, so the submission never shares a process with the checker:
//   -DHARNESS_BUILD  -I<submission>/include : builds PGMIndex<uint64_t, eps, 4> for eps in {32, 64, 128} with the
//                                             trusted pgm_index.hpp over the submission's segmentation and dumps it.
//   -DHARNESS_CHECK  -I<pristine>/include   : loads the dump into the pristine PGMIndex, validates its structure,
//                                             answers every present and absent query with the trusted search(),
//                                             compares against std::lower_bound and prints JSON.
// Usage: harness_build <keys.bin> <dump.bin>      harness_check <keys.bin> <dump.bin>
// keys.bin: little-endian uint64 keys, sorted, unique.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include "pgm/pgm_index.hpp"

using K = uint64_t;
static constexpr size_t EPS_REC = 4;

static std::vector<K> read_keys(const char *path) {
    std::vector<K> keys;
    FILE *f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); std::exit(2); }
    K buf[4096];
    size_t r;
    while ((r = std::fread(buf, sizeof(K), 4096, f)) > 0) keys.insert(keys.end(), buf, buf + r);
    std::fclose(f);
    return keys;
}

template<size_t E>
struct Access : pgm::PGMIndex<K, E, EPS_REC> {
    using Base = pgm::PGMIndex<K, E, EPS_REC>;
    using Seg = typename Base::Segment;
    using Base::Base;
    Access() = default;
    auto &segs() { return this->segments; }
    auto &offs() { return this->levels_offsets; }
    void set(size_t n, K first) { this->n = n; this->first_key = first; }
};

static_assert(sizeof(typename Access<32>::Seg) == 16, "unexpected Segment layout");

#ifdef HARNESS_BUILD
template<size_t E>
static void build_one(const std::vector<K> &keys, FILE *out) {
    auto t0 = std::chrono::steady_clock::now();
    Access<E> idx(keys.begin(), keys.end());
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    uint64_t hdr[4] = {E, idx.offs().size(), idx.segs().size(), 0};
    std::memcpy(&hdr[3], &ms, sizeof(ms));
    std::fwrite(hdr, sizeof(hdr), 1, out);
    for (auto o : idx.offs()) { uint64_t v = o; std::fwrite(&v, 8, 1, out); }
    std::fwrite(idx.segs().data(), 16, idx.segs().size(), out);
}

int main(int argc, char **argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: %s keys.bin dump.bin\n", argv[0]); return 2; }
    auto keys = read_keys(argv[1]);
    FILE *out = std::fopen(argv[2], "wb");
    if (!out) return 2;
    build_one<32>(keys, out);
    build_one<64>(keys, out);
    build_one<128>(keys, out);
    std::fclose(out);
    return 0;
}
#endif

#ifdef HARNESS_CHECK
struct Reader {
    FILE *f;
    bool get(void *p, size_t sz, size_t n) { return std::fread(p, sz, n, f) == n; }
};

static void fail(std::string &why, const std::string &msg) { if (why.empty()) why = msg; }

template<size_t E>
static bool check_one(const std::vector<K> &keys, const std::vector<K> &queries, Reader &rd, bool first) {
    using Seg = typename Access<E>::Seg;
    uint64_t hdr[4];
    std::string why;
    std::vector<size_t> per_level;
    double ms = 0;
    Access<E> idx;
    size_t checked = 0;

    if (!rd.get(hdr, sizeof(hdr), 1) || hdr[0] != E) fail(why, "dump truncated or eps mismatch");
    else {
        std::memcpy(&ms, &hdr[3], sizeof(ms));
        size_t n_offs = hdr[1], n_segs = hdr[2];
        if (n_offs < 2 || n_offs > 64 || n_segs > 4 * keys.size() + 64) fail(why, "implausible dump sizes");
        else {
            idx.offs().resize(n_offs);
            idx.segs().resize(n_segs);
            for (auto &o : idx.offs()) { uint64_t v; if (!rd.get(&v, 8, 1)) fail(why, "dump truncated"); o = v; }
            if (why.empty() && !rd.get(idx.segs().data(), 16, n_segs)) fail(why, "dump truncated");
        }
    }

    // Structural validation, so the trusted search() never reads out of bounds.
    if (why.empty()) {
        auto &off = idx.offs();
        auto &seg = idx.segs();
        if (off[0] != 0 || off.back() != seg.size()) fail(why, "levels_offsets do not span the segment array");
        for (size_t l = 0; why.empty() && l + 1 < off.size(); ++l) {
            if (off[l + 1] < off[l] + 2) { fail(why, "level " + std::to_string(l) + " has no segment"); break; }
            size_t lo = off[l], hi = off[l + 1];
            if (seg[hi - 1].key != std::numeric_limits<K>::max())
                fail(why, "level " + std::to_string(l) + " does not end with the sentinel");
            for (size_t i = lo + 1; why.empty() && i < hi; ++i)
                if (!(seg[i - 1].key < seg[i].key)) fail(why, "level " + std::to_string(l) + " keys not increasing");
            if (l == 0 && seg[lo].key > keys.front()) fail(why, "first segment starts after the first key");
            if (l > 0 && seg[lo].key > seg[off[l - 1]].key) fail(why, "level " + std::to_string(l) + " starts after its children");
            per_level.push_back(hi - lo - 1);
        }
        if (why.empty() && off.size() - 1 > 40) fail(why, "too many levels");
    }

    if (why.empty()) {
        idx.set(keys.size(), keys.front());
        for (auto q : queries) {
            auto r = idx.search(q);
            size_t truth = std::lower_bound(keys.begin(), keys.end(), q) - keys.begin();
            ++checked;
            if (r.lo > r.hi || r.hi > keys.size() || r.hi - r.lo > 2 * E + 2) {
                fail(why, "query " + std::to_string(q) + ": range [" + std::to_string(r.lo) + "," +
                          std::to_string(r.hi) + ") wider than 2*eps+2 or out of bounds");
                break;
            }
            if (truth < r.lo || truth > r.hi) {
                fail(why, "query " + std::to_string(q) + ": lower_bound rank " + std::to_string(truth) +
                          " outside [" + std::to_string(r.lo) + "," + std::to_string(r.hi) + "]");
                break;
            }
        }
    }

    size_t total = 0;
    for (auto c : per_level) total += c;
    std::printf("%s    {\"eps\": %zu, \"correct\": %s, \"reason\": \"%s\", \"queries_checked\": %zu, "
                "\"segments_per_level\": [", first ? "" : ",\n", E, why.empty() ? "true" : "false", why.c_str(), checked);
    for (size_t i = 0; i < per_level.size(); ++i) std::printf("%s%zu", i ? ", " : "", per_level[i]);
    std::printf("], \"total_segments\": %zu, \"build_ms\": %.1f}", total, ms);
    return why.empty();
}

int main(int argc, char **argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: %s keys.bin dump.bin\n", argv[0]); return 2; }
    auto keys = read_keys(argv[1]);
    if (keys.empty()) { std::fprintf(stderr, "empty workload\n"); return 2; }

    // Every key, its neighbours (absent unless adjacent), gap midpoints, the extremes, and random probes.
    std::vector<K> queries;
    queries.reserve(keys.size() * 4 + 4096);
    queries.push_back(0);
    queries.push_back(std::numeric_limits<K>::max() - 1);
    for (size_t i = 0; i < keys.size(); ++i) {
        queries.push_back(keys[i]);
        if (keys[i] > 0) queries.push_back(keys[i] - 1);
        if (keys[i] < std::numeric_limits<K>::max() - 1) queries.push_back(keys[i] + 1);
        if (i + 1 < keys.size()) queries.push_back(keys[i] + (keys[i + 1] - keys[i]) / 2);
    }
    std::mt19937_64 rng(12345);
    for (int i = 0; i < 4096; ++i) queries.push_back(std::min<K>(rng(), std::numeric_limits<K>::max() - 1));

    FILE *f = std::fopen(argv[2], "rb");
    if (!f) { std::printf("{\"levels\": [], \"error\": \"no dump\"}\n"); return 1; }
    Reader rd{f};
    std::printf("{\"n\": %zu, \"queries\": %zu, \"results\": [\n", keys.size(), queries.size());
    bool ok = check_one<32>(keys, queries, rd, true);
    ok = check_one<64>(keys, queries, rd, false) && ok;
    ok = check_one<128>(keys, queries, rd, false) && ok;
    char extra;
    bool trailing = std::fread(&extra, 1, 1, f) == 1;
    std::fclose(f);
    std::printf("\n  ], \"trailing_bytes\": %s, \"correct\": %s}\n", trailing ? "true" : "false",
                ok && !trailing ? "true" : "false");
    return ok && !trailing ? 0 : 1;
}
#endif
