#pragma once
// Generic in-memory collective input; recovery policy lives outside ASTRA-sim.
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include "astra-sim/system/Common.hh"
namespace AstraSim {
class Sys;
enum class ProgramTaskKind { Send, Receive, Compute };
struct ProgramTask {
    uint64_t id = 0, transfer = 0, bytes = 0, operations = 0, memory_bytes = 0;
    int peer = -1;
    ProgramTaskKind kind = ProgramTaskKind::Compute;
    std::vector<uint64_t> dependencies;
};
struct CollectiveProgram {
    uint64_t instance = 0, topology_version = 0, bytes = 0;
    ComType type = ComType::All_Reduce;
    std::map<int, std::vector<ProgramTask>> tasks;
    std::function<void(int, uint64_t, const char *, uint64_t)> observe;
    std::function<void(int)> completed;
};
// Deferred callbacks use ASTRA's network API and therefore its shared clock.
void defer_function(Sys *sys, uint64_t ns, std::function<void()> callback);
}
