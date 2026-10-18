/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ROOFLINE_HH__
#define __ROOFLINE_HH__
#include <cstdint>

namespace AstraSim {

class Roofline {
  public:
    Roofline(double peak_perf);
    Roofline(double bandwidth, double peak_perf);
    void set_bandwidth(double bandwidth);
    double get_perf(double operational_intensity);
    uint64_t duration_ns(uint64_t operations, uint64_t memory_bytes) const;

  private:
    double bandwidth;
    double peak_perf;
};

}  // namespace AstraSim

#endif /* __ROOFLINE_HH__ */
