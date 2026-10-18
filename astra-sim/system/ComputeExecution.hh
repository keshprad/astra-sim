#pragma once
#include <cstdint>
#include <functional>
#include <exception>
#include <string>
#include <vector>
namespace AstraSim {
class Sys;
struct ComputeRecord {
    uint64_t instance, task, operations, memory_bytes, ready_ns, start_ns, duration_ns, finish_ns;
    std::string kind;
};
class ComputeExecution {
  public:
    explicit ComputeExecution(Sys *sys) : sys(sys) {}
    uint64_t duration(uint64_t operations, uint64_t memory_bytes) const;
    // Caller holds the common GPU resource; its completion releases that handle.
    void execute_reserved(uint64_t instance, uint64_t task, uint64_t operations,
                          uint64_t memory_bytes, uint64_t ready_ns, std::string kind,
                          std::function<void()> done);
    std::vector<ComputeRecord> records;
    std::exception_ptr error;
    uint64_t outstanding_tasks = 0;
  private:
    Sys *sys;
};
}
