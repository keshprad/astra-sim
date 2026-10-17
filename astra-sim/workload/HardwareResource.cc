/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

// TODO: HardwareResource.cc should be moved to the system layer.

#include "astra-sim/workload/HardwareResource.hh"

using namespace std;
using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType;

HardwareResource::HardwareResource(uint32_t num_npus, int sys_id)
    : num_npus(num_npus),
      num_in_flight_cpu_ops(0),
      num_in_flight_gpu_comm_ops(0),
      num_in_flight_gpu_comp_ops(0),
      sys_id(sys_id) {

    num_cpu_ops = 0;
    num_gpu_ops = 0;
    num_gpu_comms = 0;

    tics_cpu_ops = 0;
    tics_gpu_ops = 0;
    tics_gpu_comms = 0;

    // cpu_ops_node = NULL;
    // gpu_ops_node = NULL;
    // gpu_comms_node = NULL;
}

void HardwareResource::occupy(
    const shared_ptr<Chakra::FeederV3::ETFeederNode> node) {
    if (leaf_accounting && node->type() == ChakraNodeType::COMM_COLL_NODE) return;
    if (node->is_cpu_op()) {
        assert(num_in_flight_cpu_ops == 0);
        ++num_in_flight_cpu_ops;
        ++num_cpu_ops;
        cpu_ops_node.emplace(node->id());
    } else {
        if (node->type() == ChakraNodeType::COMP_NODE) {
            assert(num_in_flight_gpu_comp_ops == 0);
            ++num_in_flight_gpu_comp_ops;
            ++num_gpu_ops;
            // gpu_ops_node = node;
            gpu_ops_node.emplace(node->id());
        } else {
            if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
                return;
            }
            if (!leaf_accounting) assert(num_in_flight_gpu_comm_ops == 0);
            ++num_in_flight_gpu_comm_ops;
            ++num_gpu_comms;
            // gpu_comms_node = node;
            gpu_comms_node.emplace(node->id());
        }
    }
}

void HardwareResource::release(
    const shared_ptr<Chakra::FeederV3::ETFeederNode> node) {
    if (leaf_accounting && node->type() == ChakraNodeType::COMM_COLL_NODE) return;
    if (node->is_cpu_op()) {
        --num_in_flight_cpu_ops;
        assert(num_in_flight_cpu_ops == 0);
        this->cpu_ops_node.erase(node->id());
    } else {
        if (node->type() == ChakraNodeType::COMP_NODE) {
            --num_in_flight_gpu_comp_ops;
            assert(num_in_flight_gpu_comp_ops == 0);
            this->gpu_ops_node.erase(node->id());
        } else {
            if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
                return;
            }
            --num_in_flight_gpu_comm_ops;
            if (!leaf_accounting) assert(num_in_flight_gpu_comm_ops == 0);
            this->gpu_comms_node.erase(node->id());
        }
    }
    notify_available();
}

bool HardwareResource::is_available(
    const shared_ptr<Chakra::FeederV3::ETFeederNode> node) const {
    if (leaf_accounting && node->type() == ChakraNodeType::COMM_COLL_NODE) return true;
    if (leaf_accounting && !node->is_cpu_op() && node->type() != ChakraNodeType::COMP_NODE)
        return true;
    if (node->is_cpu_op()) {
        if (num_in_flight_cpu_ops == 0) {
            return true;
        } else {
            return false;
        }
    } else {
        if (node->type() == ChakraNodeType::COMP_NODE) {
            if (num_in_flight_gpu_comp_ops == 0) {
                return true;
            } else {
                return false;
            }
        } else {
            if (num_in_flight_gpu_comm_ops == 0) {
                return true;
            } else {
                if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
                    return true;
                }
                if (num_in_flight_gpu_comm_ops == 0) {
                    return true;
                }
                return false;
            }
        }
    }
}

void HardwareResource::report() {
    cout << "num_cpu_ops: " << num_cpu_ops << endl;
    cout << "num_gpu_ops: " << num_gpu_ops << endl;
    cout << "num_gpu_comms: " << num_gpu_comms << endl;

    cout << "tics_cpu_ops: " << tics_cpu_ops << endl;
    cout << "tics_gpu_ops: " << tics_gpu_ops << endl;
    cout << "tics_gpu_comms: " << tics_gpu_comms << endl;
}

#include <stdexcept>
void HardwareResource::configure_programs() {
    leaf_accounting = true;
}
bool HardwareResource::acquire_gpu() {
    if (num_in_flight_gpu_comp_ops) return false;
    ++num_in_flight_gpu_comp_ops; ++num_gpu_ops; return true;
}
void HardwareResource::record_send() {
    // Accounting only: physical link queues govern send progress.
    ++num_in_flight_gpu_comm_ops; ++num_gpu_comms;
}
void HardwareResource::release_gpu() {
    if (!num_in_flight_gpu_comp_ops) throw std::runtime_error("GPU resource released twice");
    --num_in_flight_gpu_comp_ops; notify_available();
}
void HardwareResource::release_send() {
    if (!num_in_flight_gpu_comm_ops) throw std::runtime_error("send resource released twice");
    --num_in_flight_gpu_comm_ops; notify_available();
}
uint64_t HardwareResource::watch(std::function<void()> callback) {
    uint64_t id = next_watcher++; watchers.emplace(id, std::move(callback)); return id;
}
void HardwareResource::notify_available() {
    const auto callbacks = watchers;
    for (const auto &[id, fn] : callbacks) fn();
}
