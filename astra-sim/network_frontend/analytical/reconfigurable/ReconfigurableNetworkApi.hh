#pragma once
// Opus-derived reconfigurable frontend boundary, adapted for descriptor IDs.
// Physical Device/Link/Chunk implementations are supplied by the linked backend.
#include "astra-sim/common/AstraNetworkAPI.hh"
#include <cstdint>
#include <vector>
namespace AstraSim {
struct TransferDescriptor {
    uint64_t id, bytes, topology_version;
    int plane, source, destination;
    std::vector<int> route;
};
struct CircuitDescriptor { int plane, source, destination; double bandwidth_GBps, latency_ns; };
class ReconfigurableNetworkBackend {
  public:
    virtual ~ReconfigurableNetworkBackend() = default;
    virtual void register_transfer(const TransferDescriptor &) = 0;
    virtual void post_send(uint64_t, int, int, uint64_t, void (*)(void *), void *) = 0;
    virtual void post_receive(uint64_t, int, int, uint64_t, void (*)(void *), void *) = 0;
    virtual uint64_t request_transition(const std::vector<CircuitDescriptor> &, uint64_t, uint64_t) = 0;
    virtual bool drained() const = 0;
    virtual void begin_collective() = 0;
    virtual void end_collective() = 0;
    virtual void schedule(uint64_t, void (*)(void *), void *) = 0;
    virtual uint64_t time_ns() const = 0;
    virtual double bandwidth_GBps() const = 0;
    virtual void rank_finished(int) = 0;
};
class ReconfigurableNetworkApi final : public AstraNetworkAPI {
  public:
    ReconfigurableNetworkApi(int rank, ReconfigurableNetworkBackend &backend) : AstraNetworkAPI(rank), backend(backend) {}
    int sim_send(void *, uint64_t, int, int, int, sim_request *, void (*)(void *), void *) override;
    int sim_recv(void *, uint64_t, int, int, int, sim_request *, void (*)(void *), void *) override;
    void sim_schedule(timespec_t, void (*)(void *), void *) override;
    timespec_t sim_get_time() override;
    BackendType get_backend_type() override { return BackendType::Analytical; }
    double get_BW_at_dimension(int dimension) override;
    void sim_notify_finished() override { backend.rank_finished(rank); }
    void register_transfer(const TransferDescriptor &transfer) { backend.register_transfer(transfer); }
    uint64_t reconfigure(const std::vector<CircuitDescriptor> &circuits, uint64_t delay, uint64_t version) {
        return backend.request_transition(circuits, delay, version);
    }
    bool drained() const { return backend.drained(); }
    void begin_collective() { backend.begin_collective(); }
    void end_collective() { backend.end_collective(); }
  private:
    ReconfigurableNetworkBackend &backend;
};
}
