// Checks the segmentation on held-out keys. Prints "<dataset> <eps> <segments>" for each pair and exits non-zero
// if any lookup is wrong, so a too-loose segmentation can't pass on a low count.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <vector>
#include "pgm/pgm_index.hpp"

using K = uint64_t;
constexpr K MAX_KEY = std::numeric_limits<K>::max() - 1;  // PGMIndex reserves the maximum as its sentinel
constexpr size_t N = 1000000;

template<typename Next>
std::vector<K> make_keys(uint64_t seed, Next next) {
    std::mt19937_64 r(seed);
    std::vector<K> keys(N);
    for (auto &k : keys)
        k = next(r);
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

template<size_t E>
void check(const char *name, const std::vector<K> &keys) {
    auto in = [&](size_t i) { return keys[i]; };
    size_t segments = pgm::internal::make_segmentation(keys.size(), E, in, [](auto) {});
    pgm::PGMIndex<K, E, 4> index(keys);
    auto lookup = [&](K q) {
        auto range = index.search(q);
        size_t rank = std::lower_bound(keys.begin(), keys.end(), q) - keys.begin();
        if (rank < range.lo || rank > range.hi || range.hi - range.lo > 2 * E + 2) {
            std::printf("wrong lookup: %s eps=%zu key=%llu\n", name, E, (unsigned long long) q);
            std::exit(1);
        }
    };
    for (size_t i = 0; i < keys.size(); ++i) {
        lookup(keys[i]);
        if (keys[i] > 0)
            lookup(keys[i] - 1);
        if (keys[i] < MAX_KEY)
            lookup(keys[i] + 1);
        if (i + 1 < keys.size())
            lookup(keys[i] + (keys[i + 1] - keys[i]) / 2);
    }
    std::printf("%s %zu %zu\n", name, E, segments);
}

void check_all(const char *name, const std::vector<K> &keys) {
    check<32>(name, keys);
    check<64>(name, keys);
    check<128>(name, keys);
}

int main() {
    check_all("uniform", make_keys(7310, [](auto &r) { return (K(1) << 62) + (r() >> 2); }));
    check_all("near_max", make_keys(7311, [](auto &r) { return MAX_KEY - (r() >> 24); }));
    std::mt19937_64 c(7312);
    std::vector<K> centers(37);
    for (auto &x : centers)
        x = c() >> 4;
    check_all("clustered", make_keys(7313, [&](auto &r) { return centers[r() % centers.size()] + (r() >> 34); }));
    K gap_key = 0;
    check_all("gaps", make_keys(7314, [&](auto &r) { return gap_key += 1 + (r() >> (24 + r() % 40)); }));
    K run_key = 0;
    check_all("dense_runs", make_keys(7315, [&](auto &r) { return run_key += r() % 500 == 0 ? r() >> 44 : 1; }));
    return 0;
}
