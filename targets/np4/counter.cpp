/* Copyright 2019-present Dell EMC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "counter.h"

namespace pi {
namespace np4 {

//---------------- Counter Helper functions ---------------------------


// @brief Add the given np4::CounerValue to the total packet and byte counters
//
// @param[in]   value           np4::Counter reference (source)
// @param[out]  totalPackets    total packet counter (destination)
// @param[out]  totalBytes      total byte counter (destination)
//
void AddCounterValues(np4::CounterValue& value, 
                      pi_counter_value_t *totalPackets,
                      pi_counter_value_t *totalBytes) {

    pi_counter_value_t packets;
    pi_counter_value_t bytes;
    char* data;

    // Copy in packet value
    data = (char *)&packets;
    std::offset = sizeof(packets) - value.packets.size();
    memset(data, 0, offset);
    for (auto byte : value.packets) {
        data[offset++] = byte;
    }
    *totalPackets += packets;

    // Copy in bytes value
    data = (char *)&bytes;
    std::offset = sizeof(bytes) - value.bytes.size();
    memset(data, 0, offset);
    for (auto byte : value.bytes) {
        data[offset++] = byte;
    }
    *totalBytes += bytes;
}
	

//---------------- Counter class ---------------------------
//  This class implements the pi_counter functions in C++, providing
//  wrappers needed to make calls to the NP4 ATOM C++ API.

static pi_status_t Counter::Read(pi_dev_id_t dev_id, 
                        pi_p4__id_t counter_id,
                        std::size_t index,
                        pi_counter_data_t *counter_data) {

    // Check to make sure this device id is allocated
    auto dev =  np4_device_mgr.GetDevice(dev_id);
    if (dev == nullptr) {
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev.GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev %d: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Find counter name
	const char *counterName = pi_p4info_counter_name_from_id(info, counter_id);
    if (counterName == nullptr) {
        Logger::error("Dev %d: counter id %d not found in P4 Info", 
                      dev_id, counter);
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // We'll aggregate all the counter values here
    pi_counter_value_t totalPackets = 0UL;
    pi_counter_value_t totalBytes = 0UL;

    // Grab the counter reference
    try {

        // Do we read and aggregate all ATOMs
        if (dev.offlineMode()) {
            for (int i=0; i < dev.getAtomCount(); i++) {
                auto counter = dev.GetP4Atom(atom_id).getCounter(counterName);
                auto value = counter.read(index);
                AddCounterValues(value, &totalPackets, &totalBytes);
            }

        // Or just one ATOM
        } else {
            auto counter = dev.GetP4Atom(atom_id).getCounter(counterName);
            auto value = counter.read(index);
            AddCounterValues(value, &totalPackets, &totalBytes);
        }

    } catch (np4::Exception &e) {
        Logger:error("Dev %d: read counter failed: %s", dev_id. e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

	counter_data->valid = PI_COUNTER_UNIT_PACKETS | PI_COUNTER_UNIT_BYTES;
    counter_data->packets = totalPackets;
    counter_data->bytes = totalBytes;

    return PI_STATUS_SUCCESS;
}

}   // np4
}   // pi
