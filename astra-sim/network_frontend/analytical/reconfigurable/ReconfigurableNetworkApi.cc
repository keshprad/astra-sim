/******************************************************************************
MIT license; Opus analytical frontend adaptation. See integration provenance.
******************************************************************************/
#include "astra-sim/network_frontend/analytical/reconfigurable/ReconfigurableNetworkApi.hh"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace AstraSim {
int ReconfigurableNetworkApi::sim_send(void *, uint64_t bytes, int, int dst, int,
                                     sim_request *request, void (*handler)(void *), void *arg) {
    if (!request || !request->transfer_id) throw std::runtime_error("unregistered/rendezvous traffic unsupported");
    backend.post_send(request->transfer_id, rank, dst, bytes, handler, arg);
    return 0;
}
int ReconfigurableNetworkApi::sim_recv(void *, uint64_t bytes, int, int src, int,
                                     sim_request *request, void (*handler)(void *), void *arg) {
    if (!request || !request->transfer_id) throw std::runtime_error("unregistered/rendezvous traffic unsupported");
    backend.post_receive(request->transfer_id, src, rank, bytes, handler, arg);
    return 0;
}
void ReconfigurableNetworkApi::sim_schedule(timespec_t delta, void (*handler)(void *), void *arg) {
    if (delta.time_res != NS || !std::isfinite(delta.time_val) || delta.time_val < 0 ||
        delta.time_val > std::numeric_limits<uint64_t>::max() - backend.time_ns())
        throw std::runtime_error("invalid network event time");
    backend.schedule(backend.time_ns() + uint64_t(delta.time_val), handler, arg);
}
timespec_t ReconfigurableNetworkApi::sim_get_time() { return {NS, (long double)backend.time_ns()}; }
double ReconfigurableNetworkApi::get_BW_at_dimension(int dimension) {
    if (dimension != 0) throw std::runtime_error("planned network has one scheduling dimension");
    return backend.bandwidth_GBps();
}
}
