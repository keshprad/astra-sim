#pragma once
#include "astra-sim/system/CollectiveProgram.hh"
#include "astra-sim/system/astraccl/Algorithm.hh"
#include "extern/graph_frontend/chakra/src/feeder_v3/dependancy_solver.h"
#include <map>
#include <set>
namespace AstraSim {
class ProgramAlgorithm final : public Algorithm {
  public:
    ProgramAlgorithm(std::shared_ptr<const CollectiveProgram> program, int rank);
    ~ProgramAlgorithm() override;
    void run(EventType, CallData *) override;
    void call(EventType, CallData *) override;
  private:
    std::shared_ptr<const CollectiveProgram> program;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    // Programs contain data dependencies only: reuse Chakra's existing layer
    // without duplicating it into data/control/combined graphs.
    Chakra::FeederV3::_DependancyLayer resolver;
    std::map<uint64_t, const ProgramTask *> tasks;
    std::map<uint64_t, uint64_t> ready_times, issue_times;
    size_t finished = 0;
    uint64_t watcher = 0;
    int rank;
    bool started = false, pump_queued = false;
    void request_pump();
    void pump();
    void complete(uint64_t id);
};
}
