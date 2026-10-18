#include "astra-sim/system/ComputeExecution.hh"
#include "astra-sim/system/CollectiveProgram.hh"
#include "astra-sim/system/Roofline.hh"
#include "astra-sim/system/Sys.hh"
#include "astra-sim/workload/Workload.hh"
#include <limits>
#include <memory>
#include <stdexcept>
namespace AstraSim {
class FunctionEvent final : public Callable {
  public:
    FunctionEvent(Sys *sys, std::function<void()> fn) : sys(sys), fn(std::move(fn)) {}
    void call(EventType, CallData *) override {
        std::unique_ptr<FunctionEvent> self(this);
        if (sys->compute_execution && sys->compute_execution->error) return;
        try { fn(); }
        catch (...) {
            if (!sys->compute_execution) throw;
            sys->compute_execution->error = std::current_exception();
        }
    }
  private:
    Sys *sys;
    std::function<void()> fn;
};
void defer_function(Sys *sys, uint64_t ns, std::function<void()> fn) {
    if (ns > std::numeric_limits<uint64_t>::max() - uint64_t(Sys::boostedTick()))
        throw std::overflow_error("simulation time overflow");
    sys->register_event(new FunctionEvent(sys, std::move(fn)), EventType::General, nullptr, ns);
}
uint64_t ComputeExecution::duration(uint64_t operations, uint64_t memory_bytes) const {
    if (!sys->roofline) throw std::runtime_error("program execution requires roofline");
    return sys->roofline->duration_ns(operations, memory_bytes);
}
void ComputeExecution::execute_reserved(uint64_t instance, uint64_t task, uint64_t operations,
                                       uint64_t memory_bytes, uint64_t ready_ns, std::string kind,
                                       std::function<void()> done) {
    uint64_t start = Sys::boostedTick(), delay = duration(operations, memory_bytes);
    sys->workload->hw_resource->tics_gpu_ops += delay;
    size_t index = records.size();
    records.push_back({instance, task, operations, memory_bytes, ready_ns, start, delay, 0, std::move(kind)});
    defer_function(sys, delay, [this, index, done = std::move(done)]() {
        records.at(index).finish_ns = Sys::boostedTick();
        done();
    });
}
}
