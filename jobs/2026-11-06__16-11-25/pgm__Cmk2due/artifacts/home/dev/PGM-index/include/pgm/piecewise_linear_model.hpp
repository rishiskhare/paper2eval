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
    long double min_slope = -std::numeric_limits<long double>::infinity();
    long double max_slope = std::numeric_limits<long double>::infinity();
    bool segment_closed = false;

public:

    class CanonicalSegment;

    explicit OptimalPiecewiseLinearModel(Y epsilon) : epsilon(epsilon) {
        if (epsilon < 0)
            throw std::invalid_argument("epsilon cannot be negative");
    }

    bool add_point(const X &x, const Y &y) {
        // The caller obtains the completed segment before adding the point
        // which did not fit.  Start the next segment lazily on that call.
        if (segment_closed) {
            reset();
            segment_closed = false;
        }

        if (points_in_hull > 0 && x <= last_x)
            throw std::logic_error("Points must be increasing by x.");

        if (points_in_hull == 0) {
            first_x = x;
            first_y = y;
            last_x = x;
            points_in_hull = 1;
            min_slope = -std::numeric_limits<long double>::infinity();
            max_slope = std::numeric_limits<long double>::infinity();
            return true;
        }

        // All lines used by a canonical segment pass through its first
        // point.  A point (x,y) therefore restricts the slope to
        // [(y-y0-epsilon)/(x-x0), (y-y0+epsilon)/(x-x0)].  Do the
        // subtractions in the widened signed types so unsigned keys and
        // positions cannot wrap before being converted to long double.
        const SX dx_wide = static_cast<SX>(x) - static_cast<SX>(first_x);
        const SY dy_wide = static_cast<SY>(y) - static_cast<SY>(first_y);
        const long double dx = static_cast<long double>(dx_wide);
        const long double dy = static_cast<long double>(dy_wide);
        const long double error = static_cast<long double>(epsilon);

        const long double point_min = (dy - error) / dx;
        const long double point_max = (dy + error) / dx;
        if (point_min > max_slope || point_max < min_slope) {
            segment_closed = true;
            return false;
        }

        min_slope = std::max(min_slope, point_min);
        max_slope = std::min(max_slope, point_max);
        last_x = x;
        ++points_in_hull;
        return true;
    }

    CanonicalSegment get_segment() {
        // With one point every slope is feasible.  Choosing zero also gives
        // the compressed representation a finite, deterministic interval.
        const auto slope = points_in_hull == 1 ? 0.0L :
                           min_slope + (max_slope - min_slope) / 2;
        const auto lower = points_in_hull == 1 ? 0.0L : min_slope;
        const auto upper = points_in_hull == 1 ? 0.0L : max_slope;
        return CanonicalSegment(first_x, first_y, slope, lower, upper);
    }

    void reset() {
        points_in_hull = 0;
        min_slope = -std::numeric_limits<long double>::infinity();
        max_slope = std::numeric_limits<long double>::infinity();
        segment_closed = false;
    }
};

template<typename X, typename Y>
class OptimalPiecewiseLinearModel<X, Y>::CanonicalSegment {
    friend class OptimalPiecewiseLinearModel;

    X first;
    Y y;
    long double slope;
    long double min_slope;
    long double max_slope;

    CanonicalSegment(X first, Y y, long double slope,
                     long double min_slope, long double max_slope)
        : first(first), y(y), slope(slope), min_slope(min_slope), max_slope(max_slope) {};

public:

    CanonicalSegment() = default;

    X get_first_x() const { return first; }

    std::pair<long double, long double> get_intersection() const {
        return {first, y};
    }

    std::pair<long double, SY> get_floating_point_segment(const X &origin) const {
        const auto intercept = static_cast<SY>(y) +
                               static_cast<SY>((static_cast<long double>(first) -
                                                static_cast<long double>(origin)) * slope);
        return {slope, intercept};
    }

    std::pair<long double, long double> get_slope_range() const {
        return {min_slope, max_slope};
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
    // Segmenting independent chunks can split a valid segment at every
    // chunk boundary, so it cannot preserve the minimum-segment guarantee.
    return make_segmentation(n, epsilon, in, out);
}

}
