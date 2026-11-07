// This file is part of PGM-index <https://github.com/gvinciguerra/PGM-index>.
// Copyright (c) 2018 Giorgio Vinciguerra.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#else
#pragma message ("Compilation with -fopenmp is optional but recommended")
#define omp_get_num_procs() 1
#define omp_get_max_threads() 1
#endif

namespace pgm::internal {

template<typename T>
using LargeSigned = typename std::conditional_t<std::is_floating_point_v<T>,
                                                long double,
                                                std::conditional_t<(sizeof(T) < 8), int64_t, __int128>>;

template<typename X, typename Y>
class OptimalPiecewiseLinearModel {
private:
    using SX = LargeSigned<X>;
    using SY = LargeSigned<Y>;

    const Y epsilon;
    X first_x = 0;
    X last_x = 0;
    Y first_y = 0;
    size_t points_in_hull = 0;
    long double min_slope = 0;
    long double max_slope = 0;
    X min_x = 0;
    Y min_y = 0;
    X max_x = 0;
    Y max_y = 0;
    std::vector<std::pair<X, Y>> points;


public:

    class CanonicalSegment;

    explicit OptimalPiecewiseLinearModel(Y epsilon) : epsilon(epsilon) {
        if (epsilon < 0)
            throw std::invalid_argument("epsilon cannot be negative");
    }

    bool add_point(const X &x, const Y &y) {
        if (points_in_hull > 0 && x <= last_x)
            throw std::logic_error("Points must be increasing by x.");

        if (points_in_hull == 0) {
            points.clear();
            first_x = x;
            first_y = y;
            min_slope = -std::numeric_limits<long double>::infinity();
            max_slope = std::numeric_limits<long double>::infinity();
        } else {
            // A line within epsilon of the first point and of (x,y) must
            // have a slope in this interval.  The factor 2*epsilon is the
            // width of the two vertical error strips.
            const auto dx = static_cast<long double>(SX(x) - SX(first_x));
            const auto dy = static_cast<long double>(SY(y) - SY(first_y));
            const auto lower = (dy - 2 * static_cast<long double>(epsilon)) / dx;
            const auto upper = (dy + 2 * static_cast<long double>(epsilon)) / dx;

            const auto new_min_slope = std::max(min_slope, lower);
            const auto new_max_slope = std::min(max_slope, upper);

            if (new_min_slope > new_max_slope) {
                points_in_hull = 0;
                return false;
            }
            if (lower > min_slope) {
                min_x = x;
                min_y = y;
            }
            if (upper < max_slope) {
                max_x = x;
                max_y = y;
            }
            min_slope = new_min_slope;
            max_slope = new_max_slope;
        }

        last_x = x;
        points.emplace_back(x, y);
        ++points_in_hull;
        return true;
    }

    CanonicalSegment get_segment() {
        if (!std::isfinite(min_slope) || !std::isfinite(max_slope) ||
            min_slope == max_slope) {
            const auto slope = min_slope == max_slope ? min_slope : 0;
            return CanonicalSegment(first_x, first_x, first_y, slope, slope);
        }

        // Use the middle slope and the middle feasible intercept.  The
        // extremal points defining the cone are sufficient to compute the
        // intercept interval, and choosing an integral intercept keeps the
        // final index prediction within the requested integral error.
        const auto slope = (min_slope + max_slope) / 2;
        const auto x0 = static_cast<long double>(first_x);
        const auto y0 = static_cast<long double>(first_y);
        auto low = -std::numeric_limits<long double>::infinity();
        auto high = std::numeric_limits<long double>::infinity();
        for (const auto &[point_x, point_y] : points) {
            const auto px = static_cast<long double>(point_x);
            const auto py = static_cast<long double>(point_y);
            low = std::max(low, py - static_cast<long double>(epsilon) - slope * px);
            high = std::min(high, py + static_cast<long double>(epsilon) - slope * px);
        }
        auto intercept = std::round((low + high) / 2);
        if (intercept < low)
            intercept = std::ceil(low);
        if (intercept > high)
            intercept = std::floor(high);
        return CanonicalSegment(first_x, x0, intercept + slope * x0,
                                min_slope, max_slope);
    }

    void reset() {
        points_in_hull = 0;
        points.clear();
    }
};

template<typename X, typename Y>
class OptimalPiecewiseLinearModel<X, Y>::CanonicalSegment {
    friend class OptimalPiecewiseLinearModel;

    X first;
    long double intersection_x;
    long double intersection_y;
    long double slope_low;
    long double slope_high;

    CanonicalSegment(X first, long double intersection_x, long double intersection_y,
                     long double slope_low, long double slope_high)
        : first(first), intersection_x(intersection_x), intersection_y(intersection_y),
          slope_low(slope_low), slope_high(slope_high) {};

public:

    CanonicalSegment() = default;

    X get_first_x() const { return first; }

    std::pair<long double, long double> get_intersection() const {
        return {intersection_x, intersection_y};
    }

    std::pair<long double, SY> get_floating_point_segment(const X &origin) const {
        const auto slope = (slope_low + slope_high) / 2;
        const auto intercept = intersection_y - slope *
                               (intersection_x - static_cast<long double>(origin));
        return {slope, static_cast<SY>(std::round(intercept))};
    }

    std::pair<long double, long double> get_slope_range() const {
        return {slope_low, slope_high};
    }
};

template<typename K>
K next_key(K x) {
    if constexpr (std::is_floating_point_v<K>)
        return std::nextafter(x, std::numeric_limits<K>::infinity());
    else
        return x + 1;
}

template<typename Fin, typename Fout>
size_t make_segmentation(size_t n, size_t start, size_t end, size_t epsilon, Fin in, Fout out) {
    using K = typename std::invoke_result_t<Fin, size_t>;
    size_t c = 0;
    OptimalPiecewiseLinearModel<K, size_t> opt(epsilon);
    auto add_point = [&](K x, size_t y) {
        if (!opt.add_point(x, y)) {
            out(opt.get_segment());
            opt.add_point(x, y);
            ++c;
        }
    };

    // placeholder
    for (size_t i = start; i < end; ++i) {
        if (i == start || in(i) != in(i - 1))
            add_point(in(i), i);
        else if (i + 1 < end && next_key(in(i)) < in(i + 1))
            add_point(next_key(in(i)), i + 1);
    }

    if (end == n)
        add_point(next_key(in(n - 1)), n); // Keys greater than the last one map to n

    out(opt.get_segment());
    return ++c;
}

template<typename Fin, typename Fout>
size_t make_segmentation(size_t n, size_t epsilon, Fin in, Fout out) {
    return make_segmentation(n, 0, n, epsilon, in, out);
}

template<typename Fin, typename Fout>
size_t make_segmentation_par(size_t n, size_t epsilon, Fin in, Fout out) {
    auto parallelism = std::min(std::min(omp_get_num_procs(), omp_get_max_threads()), 20);
    auto chunk_size = n / parallelism;
    auto c = 0ull;

    if (parallelism == 1 || n < 1ull << 15)
        return make_segmentation(n, epsilon, in, out);

    using K = typename std::invoke_result_t<Fin, size_t>;
    using canonical_segment = typename OptimalPiecewiseLinearModel<K, size_t>::CanonicalSegment;
    std::vector<std::vector<canonical_segment>> results(parallelism);

    #pragma omp parallel for reduction(+:c) num_threads(parallelism)
    for (auto i = 0; i < parallelism; ++i) {
        auto first = i * chunk_size;
        auto last = i == parallelism - 1 ? n : first + chunk_size;
        if (first > 0) {
            for (; first < last; ++first)
                if (in(first) != in(first - 1))
                    break;
            if (first == last)
                continue;
        }

        auto in_fun = [in](auto j) { return in(j); };
        auto out_fun = [&results, i](const auto &cs) { results[i].emplace_back(cs); };
        results[i].reserve(chunk_size / (epsilon > 0 ? epsilon * epsilon : 16));
        c += make_segmentation(n, first, last, epsilon, in_fun, out_fun);
    }

    for (auto &v : results)
        for (auto &cs : v)
            out(cs);

    return c;
}

}
