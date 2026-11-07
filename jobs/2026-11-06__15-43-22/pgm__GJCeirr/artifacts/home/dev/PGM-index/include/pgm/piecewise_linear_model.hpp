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
    Y last_y_value = 0;
    Y first_y = 0;
    size_t points_in_hull = 0;

    struct Point {
        long double x;
        long double y;
    };

    std::vector<Point> upper_hull;
    std::vector<Point> lower_hull;
    long double min_slope = -std::numeric_limits<long double>::infinity();
    long double max_slope = std::numeric_limits<long double>::infinity();

    static long double cross(const Point &a, const Point &b, const Point &c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    static long double slope(const Point &p, const Point &q) {
        return (q.y - p.y) / (q.x - p.x);
    }

    // The slope from a new point to a convex chain is unimodal.  Thus the
    // tangent can be found in logarithmic time (and moves monotonically as
    // points are added).
    static size_t tangent(const std::vector<Point> &hull, const Point &p, bool maximum) {
        size_t best = 0;
        for (size_t i = 1; i < hull.size(); ++i)
            if ((maximum && slope(hull[best], p) < slope(hull[i], p)) ||
                (!maximum && slope(hull[best], p) > slope(hull[i], p)))
                best = i;
        return best;
    }

    void add_to_hulls(Point p) {
        while (upper_hull.size() >= 2 && cross(upper_hull[upper_hull.size() - 2],
                                                upper_hull.back(), p) >= 0)
            upper_hull.pop_back();
        upper_hull.push_back(p);

        while (lower_hull.size() >= 2 && cross(lower_hull[lower_hull.size() - 2],
                                                lower_hull.back(), p) <= 0)
            lower_hull.pop_back();
        lower_hull.push_back(p);
    }

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
            first_x = x;
            first_y = y;
            last_x = x;
            last_y_value = y;
            ++points_in_hull;
            add_to_hulls({static_cast<long double>(x), static_cast<long double>(y)});
            return true;
        }

        Point p{static_cast<long double>(x), static_cast<long double>(y)};
        // A line fitting all the strips [y_i-epsilon, y_i+epsilon] must have
        // a slope between these two tangents.  The upper tangent uses the
        // lower edge of the new strip and the lower tangent the upper edge.
        Point lower_edge{p.x, p.y - static_cast<long double>(epsilon)};
        Point upper_edge{p.x, p.y + static_cast<long double>(epsilon)};
        auto lo = tangent(lower_hull, lower_edge, true);
        auto hi = tangent(upper_hull, upper_edge, false);
        auto lower_slope = slope(lower_hull[lo], lower_edge);
        auto upper_slope = slope(upper_hull[hi], upper_edge);
        auto new_min_slope = std::max(min_slope, lower_slope);
        auto new_max_slope = std::min(max_slope, upper_slope);
        //
        if (new_min_slope > new_max_slope)
            return false;

        min_slope = new_min_slope;
        max_slope = new_max_slope;
        last_x = x;
        last_y_value = y;
        ++points_in_hull;
        add_to_hulls(p);
        return true;
    }

    CanonicalSegment get_segment() {
        if (points_in_hull == 1)
            return CanonicalSegment(first_x, first_y, 0, 0, 0,
                                    static_cast<long double>(first_y));
        auto chosen_slope = (min_slope + max_slope) / 2;
        long double intercept_low = -std::numeric_limits<long double>::infinity();
        long double intercept_high = std::numeric_limits<long double>::infinity();
        for (const auto &p : upper_hull)
            intercept_low = std::max(intercept_low, p.y - static_cast<long double>(epsilon) - chosen_slope * p.x);
        for (const auto &p : lower_hull)
            intercept_high = std::min(intercept_high, p.y + static_cast<long double>(epsilon) - chosen_slope * p.x);
        auto intercept = (intercept_low + intercept_high) / 2;
        return CanonicalSegment(first_x, first_y, chosen_slope, min_slope, max_slope,
                                intercept + chosen_slope * static_cast<long double>(first_x));
    }

    void reset() {
        points_in_hull = 0;
        upper_hull.clear();
        lower_hull.clear();
        min_slope = -std::numeric_limits<long double>::infinity();
        max_slope = std::numeric_limits<long double>::infinity();
    }

};

template<typename X, typename Y>
class OptimalPiecewiseLinearModel<X, Y>::CanonicalSegment {
    friend class OptimalPiecewiseLinearModel;

    X first;
    Y y;
    long double slope;
    long double slope_min;
    long double slope_max;
    long double line_at_first;

    CanonicalSegment(X first, Y y, long double slope, long double slope_min,
                     long double slope_max, long double line_at_first)
        : first(first), y(y), slope(slope), slope_min(slope_min), slope_max(slope_max),
          line_at_first(line_at_first) {};

public:

    CanonicalSegment() = default;

    X get_first_x() const { return first; }

    std::pair<long double, long double> get_intersection() const {
        return {first, line_at_first};
    }

    std::pair<long double, SY> get_floating_point_segment(const X &origin) const {
        return {slope, static_cast<SY>(std::round(line_at_first -
                                                   slope * (static_cast<long double>(first) -
                                                            static_cast<long double>(origin))))};
    }

    std::pair<long double, long double> get_slope_range() const {
        return {slope_min, slope_max};
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
