#include "astra-sim/system/ProgramAlgorithm.hh"
#include "astra-sim/system/ComputeExecution.hh"
#include "astra-sim/system/Sys.hh"
#include "astra-sim/system/DataSet.hh"
#include "astra-sim/system/StreamBaseline.hh"
#include "astra-sim/workload/Workload.hh"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace AstraSim {
ProgramAlgorithm::ProgramAlgorithm(std::shared_ptr<const CollectiveProgram> p, int r)
    : program(std::move(p)), rank(r) {
    id = r; name = Name::AllToAll; logical_topo = nullptr;
    data_size = final_data_size = program->bytes;
    comType = program->type; enabled = true;
    for (const auto &task : program->tasks.at(rank)) {
        if (!task.id || !tasks.emplace(task.id, task).second)
            throw std::runtime_error("duplicate/zero program task ID");
    }
    for (const auto &[task_id, task] : tasks) {
        ChakraProtoMsg::Node node; node.set_id(task_id);
        for (uint64_t parent : task.dependencies) {
            if (!tasks.count(parent) || parent >= task_id)
                throw std::runtime_error("invalid collective dependency");
            node.add_data_deps(parent);
        }
        resolver.add_node(node);
    }
    if (!tasks.empty()) resolver.resolve_dependancy_free_nodes();
}
ProgramAlgorithm::~ProgramAlgorithm() {
    *alive = false;
    if (watcher) stream->owner->workload->hw_resource->unwatch(watcher);
}
void ProgramAlgorithm::run(EventType, CallData *) {
    if (!started) {
        started = true;
        // This program has no PacketBundle/MemBus processing charge.
        stream->shared_request_counter = stream->mem_request_counter = 1;
        auto weak = std::weak_ptr<bool>(alive);
        watcher = stream->owner->workload->hw_resource->watch([this, weak]() {
            auto token = weak.lock();
            if (token && *token) request_pump();
        });
    }
    request_pump();
}
void ProgramAlgorithm::call(EventType, CallData *) { request_pump(); }
void ProgramAlgorithm::request_pump() {
    if (pump_queued) return;
    pump_queued = true;
    auto weak = std::weak_ptr<bool>(alive);
    defer_function(stream->owner, 0, [this, weak]() {
        auto token = weak.lock();
        if (token && *token) { pump_queued = false; pump(); }
    });
}
void ProgramAlgorithm::pump() {
    auto *sys = stream->owner;
    auto *hw = sys->workload->hw_resource;
    const auto &free = resolver.get_dependancy_free_nodes();
    std::vector<uint64_t> candidates(free.begin(), free.end());
    std::sort(candidates.begin(), candidates.end());
    for (uint64_t task_id : candidates) {
        const auto &task = tasks.at(task_id);
        ready_times.emplace(task_id, uint64_t(Sys::boostedTick()));
        if (task.kind == ProgramTaskKind::Send && !hw->acquire_send()) continue;
        if (task.kind == ProgramTaskKind::Compute && !hw->acquire_gpu()) continue;
        resolver.take_node(task_id);
        issue_times[task_id] = Sys::boostedTick();
        auto done = [this, task_id]() { complete(task_id); };
        if (task.kind == ProgramTaskKind::Compute) {
            sys->compute_execution->execute_reserved(program->instance, task_id, task.operations,
                task.memory_bytes, ready_times.at(task_id), "reduction", std::move(done));
        } else {
            sim_request request{}; request.transfer_id = task.transfer;
            request.srcRank = task.kind == ProgramTaskKind::Send ? rank : task.peer;
            request.dstRank = task.kind == ProgramTaskKind::Send ? task.peer : rank;
            request.reqType = UINT8; request.reqCount = task.bytes;
            auto *callback = new std::function<void()>(std::move(done));
            auto invoke = [](void *arg) {
                std::unique_ptr<std::function<void()>> cb(static_cast<std::function<void()> *>(arg));
                (*cb)();
            };
            if (task.kind == ProgramTaskKind::Send) {
                program->observe(rank, task.transfer, "send_ns", Sys::boostedTick());
                sys->front_end_sim_send(0, Sys::dummy_data, task.bytes, UINT8, task.peer, 0,
                    &request, Sys::FrontEndSendRecvType::COLLECTIVE, invoke, callback);
            } else {
                program->observe(rank, task.transfer, "receive_post_ns", Sys::boostedTick());
                sys->front_end_sim_recv(0, Sys::dummy_data, task.bytes, UINT8, task.peer, 0,
                    &request, Sys::FrontEndSendRecvType::COLLECTIVE, invoke, callback);
            }
        }
    }
    if (finished == tasks.size()) {
        program->completed(rank);
        exit(); // Algorithm/stream may delete this. Do not access it afterward.
    }
}
void ProgramAlgorithm::complete(uint64_t task_id) {
    const auto &task = tasks.at(task_id);
    auto *hw = stream->owner->workload->hw_resource;
    if (task.kind == ProgramTaskKind::Send) {
        hw->release_send();
        program->observe(rank, task.transfer, "arrival_ns", Sys::boostedTick());
    } else if (task.kind == ProgramTaskKind::Receive) {
        program->observe(rank, task.transfer, "receive_complete_ns", Sys::boostedTick());
        stream->net_message_counter++;
        stream->net_message_latency.back() += uint64_t(Sys::boostedTick()) - issue_times.at(task_id);
    } else {
        hw->release_gpu();
        auto *sys = stream->owner;
        defer_function(sys, 0, [sys]() {
            if (!sys->workload->is_finished) sys->workload->fire();
        });
    }
    --stream->owner->compute_execution->outstanding_tasks;
    resolver.finish_node(task_id);
    ++finished;
    request_pump();
}

void Sys::enable_program_execution(int send_slots) {
    if (compute_execution) throw std::runtime_error("program execution enabled twice");
    workload->hw_resource->configure_programs(send_slots);
    compute_execution = new ComputeExecution(this);
}
DataSet *Sys::generate_program(std::shared_ptr<const CollectiveProgram> program) {
    if (!compute_execution || program->instance > uint64_t(std::numeric_limits<int>::max()))
        throw std::runtime_error("invalid/unconfigured collective program");
    auto *dataset = new DataSet(1);
    auto *algorithm = new ProgramAlgorithm(program, id);
    compute_execution->outstanding_tasks += program->tasks.at(id).size();
    std::list<CollectivePhase> phases{CollectivePhase(this, 0, algorithm)};
    auto *new_stream = new StreamBaseline(this, dataset, int(program->instance), phases, get_priority(0));
    new_stream->current_queue_id = -1;
    // Defer insertion until Workload has installed its DataSet notifier.
    defer_function(this, 0, [this, new_stream]() { insert_into_ready_list(new_stream); });
    return dataset;
}
}
