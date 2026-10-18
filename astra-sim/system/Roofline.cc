/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/Roofline.hh"

#include <algorithm>

using namespace std;
using namespace AstraSim;

Roofline::Roofline(double peak_perf) : peak_perf(peak_perf) {}

Roofline::Roofline(double bandwidth, double peak_perf)
    : bandwidth(bandwidth),
      peak_perf(peak_perf) {}

void Roofline::set_bandwidth(double bandwidth) {
    this->bandwidth = bandwidth;
}

double Roofline::get_perf(double operational_intensity) {
    return min(bandwidth * operational_intensity, peak_perf);
}

#include <cmath>
#include <limits>
#include <stdexcept>
uint64_t Roofline::duration_ns(uint64_t operations, uint64_t memory_bytes) const {
    if (!std::isfinite(peak_perf) || peak_perf <= 0 || !std::isfinite(bandwidth) || bandwidth <= 0)
        throw std::runtime_error("roofline requires finite positive throughput and bandwidth");
    long double seconds = std::max((long double)operations / peak_perf,
                                   (long double)memory_bytes / bandwidth);
    long double ns = std::ceil(seconds * 1e9L);
    if (!std::isfinite(ns) || ns > std::numeric_limits<uint64_t>::max())
        throw std::overflow_error("roofline duration overflow");
    return uint64_t(ns);
}
