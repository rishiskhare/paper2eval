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
    // The polygon of (slope, intercept-at-first_x) pairs which approximate
    // all the points seen so far.  Keeping this polygon is the two
    // dimensional version of the upper and lower convex hull construction
    // described in the paper.
    std::vector<std::pair<long double, long double>> feasible;

    template<typename T>
    static long double value(const T &v) { return static_cast<long double>(v); }

    std::vector<std::pair<long double, long double>> clipped(
            const std::vector<std::pair<long double, long double>> &polygon,
            long double a, long double c) const {
        // Keep a*m + b <= c.  Sutherland--Hodgman clipping is linear in the
        // number of hull vertices; the hulls grow only when an old edge is
        // removed, so the complete streaming construction is linear.
        std::vector<std::pair<long double, long double>> result;
        if (polygon.empty())
            return result;
        auto inside = [&](const auto &p) { return a * p.first + p.second <= c; };
        auto intersection = [&](const auto &p, const auto &q) {
            auto dp = a * p.first + p.second - c;
            auto dq = a * q.first + q.second - c;
            auto t = dp / (dp - dq);
            return std::make_pair(p.first + t * (q.first - p.first),
                                  p.second + t * (q.second - p.second));
        };
        auto previous = polygon.back();
        auto previous_inside = inside(previous);
        for (const auto &current : polygon) {
            auto current_inside = inside(current);
            if (current_inside != previous_inside)
                result.push_back(intersection(previous, current));
            if (current_inside)
                result.push_back(current);
            previous = current;
            previous_inside = current_inside;
        }
        return result;
    }

    bool add_constraint(const X &x, const Y &y,
                        std::vector<std::pair<long double, long double>> &polygon) const {
        auto dx = value(x) - value(first_x);
        auto dy = value(y) - value(first_y);
        // slope * dx + intercept <= dy + epsilon
        polygon = clipped(polygon, dx, dy + value(epsilon));
        if (polygon.empty())
            return false;
        // -(slope * dx + intercept) <= -(dy - epsilon)
        polygon = clipped(polygon, -dx, -dy + value(epsilon));
        return !polygon.empty();
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
            points_in_hull = 1;
            feasible.clear();
            return true;
        }

        auto candidate = feasible;
        if (points_in_hull == 1) {
            // At the first point the intercept is in [-epsilon, epsilon].
            // The second point bounds the slope, so this is a finite initial
            // polygon (and avoids artificial +/- infinity vertices).
            auto dx = value(x) - value(first_x);
            auto dy = value(y) - value(first_y);
            auto lo = (dy - 2 * value(epsilon)) / dx;
            auto hi = (dy + 2 * value(epsilon)) / dx;
            if (lo > hi)
                std::swap(lo, hi);
            candidate = {{lo, 0}, {hi, 0},
                         {hi, value(epsilon)}, {lo, value(epsilon)}};
        }

        if (!add_constraint(x, y, candidate)) {
            points_in_hull = 0;
            return false;
        }

        feasible.swap(candidate);
        last_x = x;
        ++points_in_hull;
        return true;
    }

    CanonicalSegment get_segment() {
        long double slope = 0;
        long double intercept = 0;
        if (!feasible.empty()) {
            for (const auto &p : feasible) {
                slope += p.first;
                intercept += p.second;
            }
            slope /= feasible.size();
            intercept /= feasible.size();
        }
        return CanonicalSegment(first_x, first_y, slope, intercept);
    }

    void reset() {
        points_in_hull = 0;
        feasible.clear();
    }
};

template<typename X, typename Y>
class OptimalPiecewiseLinearModel<X, Y>::CanonicalSegment {
    friend class OptimalPiecewiseLinearModel;

    X first;
    Y y;
    long double slope = 0;
    long double intercept = 0;

    CanonicalSegment(X first, Y y, long double slope, long double intercept)
        : first(first), y(y), slope(slope), intercept(intercept) {};

public:

    CanonicalSegment() = default;

    X get_first_x() const { return first; }

    std::pair<long double, long double> get_intersection() const {
        return {static_cast<long double>(first), static_cast<long double>(y) + intercept};
    }

    std::pair<long double, SY> get_floating_point_segment(const X &origin) const {
        auto b = static_cast<long double>(y) + intercept -
                 (static_cast<long double>(origin) - static_cast<long double>(first)) * slope;
        return {slope, static_cast<SY>(std::round(b))};
    }

    std::pair<long double, long double> get_slope_range() const {
        return {slope, slope};
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
