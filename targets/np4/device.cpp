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

#include "device.h"
#include "logger.h"
#include "p4dev.h"
#include "proto/pi/np4/np4_intel_device_config.pb.h"
#include "absl/memory/memory.h"

namespace pi {
namespace np4 {

Device::Device(pi_dev_id_t dev_id,
               const pi_p4info_t* p4_info)
  : dev_id_(dev_id),
    atom_id_(0),
    sync_atoms_(true),
    p4_info_(p4_info) {
}

Device::~Device() {
    // Stop the device
    Stop();
}

std::unique_ptr<Device> Device::CreateInstance(
    pi_dev_id_t dev_id,
    const pi_p4info_t* p4_info) {

    return absl::make_unique<Device>(dev_id, p4_info);
}

pi_status_t Device::LoadDevice(std::string data, size_t size) {

    (void)size;

    // Load NP4 Intel device config protobuf
    auto dev_config = ::pi::np4::NP4IntelDeviceConfig();
    if (!dev_config.ParseFromString(data)) {
        Logger::error("Dev " + std::to_string(dev_id_)
                        + ": invalid device config");
        return PI_STATUS_TARGET_ERROR;
    }

    // Allocate NP4 device
    try {
        switch (dev_config.np4_device().device_case()) {
        case ::pi::np4::NP4DeviceConfig::kPath: {
            // Allocate NP4 device in offline mode (i.e. device path)
            auto path = dev_config.np4_device().path();
            p4_dev_ = absl::make_unique<::np4::Device>(path);
            break;
        }

        case ::pi::np4::NP4DeviceConfig::kDaemon: {
            // Allocate NP4 device in online mode (i.e. daemon)
            auto dc = dev_config.np4_device().daemon();
            p4_dev_ = absl::make_unique<::np4::Device>(dc.host(), dc.port());
            break;
        }
        
        default:
            Logger::error("Dev " + std::to_string(dev_id_)
                + ": NP4 Device config not supported");
            return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ALLOCATE_ERROR);
        }

    } catch (::np4::Exception &e) {
        Logger::error("Dev " + std::to_string(dev_id_) 
            + ": NP4 Device allocate failed: " + e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ALLOCATE_ERROR);
    }

    // See if ATOM set
    if (dev_config.np4_device().has_atom()) {
        atom_id_ = dev_config.np4_device().atom().id();
        sync_atoms_ = false;
    }

    // TODO: need to allocate DPDK

    // TODO: 
    // - check current image on FPGA to see if same
    // - if not then load into FPGA and restart it

    return PI_STATUS_SUCCESS;
}

pi_status_t Device::Start() {

    // Reset all ATOMs
    Reset();

    return PI_STATUS_SUCCESS;
}

pi_status_t Device::Reset() {
    
    // Reset all atoms
    for (std::size_t i=0; i < p4_dev_->getAtomCount(); i++) {
        (*p4_dev_)[i].reset();
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t Device::Stop() {

    // Do we need to do anything for NP4 device? 

    // Stop the DPDK PMD

    return PI_STATUS_SUCCESS;
}

}   // np4
}   // pi
