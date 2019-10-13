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

#include <memory>
#include "device.h"
#include "proto/frontend/src/logger.h"
#include "p4dev.h"
#include "pi/np4/np4_intel_device_config.pb.h"

using pi::fe::proto::Logger;

namespace pi {
namespace np4 {

Device::Device(pi_dev_id_t dev_id,
               const pi_p4info_t* p4_info)
  : dev_id_(dev_id),
    atom_id_(0),
    sync_atoms_(true),
    p4_info_(p4_info) {

    Logger::get()->trace("Dev {}: Device created", dev_id_);
}

Device::~Device() {
    Logger::get()->trace("Dev {}: Device deleted", dev_id_);

    // Stop the device
    Stop();
}

std::unique_ptr<Device> Device::CreateInstance(
    pi_dev_id_t dev_id,
    const pi_p4info_t* p4_info) {

    Logger::get()->trace("Dev {}: Device CreateInstance", dev_id);

    return std::unique_ptr<Device>(new Device(dev_id, p4_info));
}

pi_status_t Device::LoadDevice(std::string data, size_t size) {

    (void)size;

    // Load NP4 Intel device config protobuf
    auto dev_config = ::pi::np4::NP4IntelDeviceConfig();
    if (!dev_config.ParseFromString(data)) {
        Logger::get()->error("Dev {}: invalid device config", dev_id_);
        return PI_STATUS_TARGET_ERROR;
    }

    // Allocate NP4 device
    try {
        switch (dev_config.np4_device().device_case()) {
        case ::pi::np4::NP4DeviceConfig::kPath: {
            // Allocate NP4 device in offline mode (i.e. device path)
            auto path = dev_config.np4_device().path();
            Logger::get()->debug("Dev {}: connecting to {}", dev_id_, path);
            p4_dev_ = std::unique_ptr<::np4::Device>(new ::np4::Device(path));
            break;
        }

        case ::pi::np4::NP4DeviceConfig::kDaemon: {
            // Allocate NP4 device in online mode (i.e. daemon)
            auto dc = dev_config.np4_device().daemon();
            Logger::get()->debug("Dev {}: connecting to {}:{}",
                                 dev_id_, dc.host(), dc.port());
            p4_dev_ = std::unique_ptr<::np4::Device>(
                new ::np4::Device(dc.host(), dc.port()));
            break;
        }
        
        case ::pi::np4::NP4DeviceConfig::DEVICE_NOT_SET:
            Logger::get() ->error("Dev {}: device config not set", dev_id_);
            return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ALLOCATE_ERROR);
        }

    } catch (::np4::Exception &e) {
        Logger::get()
          ->error("Dev {}: NP4 Device allocate failed: {}", dev_id_, e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ALLOCATE_ERROR);
    }

    // See if ATOM set
    if (dev_config.np4_device().has_atom()) {
        atom_id_ = dev_config.np4_device().atom().id();
        sync_atoms_ = false;
    }
    Logger::get()->debug("Dev {}: sync atoms is {}", dev_id_, sync_atoms_);

    // TODO: need to allocate DPDK

    // TODO: 
    // - check current image on FPGA to see if same
    // - if not then load into FPGA and restart it

    return PI_STATUS_SUCCESS;
}

pi_status_t Device::Start() {

    Logger::get()->trace("Dev {}: Start", dev_id_);

    // Reset all ATOMs
    Reset();

    return PI_STATUS_SUCCESS;
}

pi_status_t Device::Reset() {
    
    Logger::get()->trace("Dev {}: Reset", dev_id_);

    // Reset all atoms
    for (std::size_t i=0; i < p4_dev_->getAtomCount(); i++) {
        Logger::get()->debug("Dev {}: resetting atom {}", dev_id_, i);
        (*p4_dev_)[i].reset();
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t Device::Stop() {

    Logger::get()->trace("Dev {}: Stop", dev_id_);

    // Do we need to do anything for NP4 device? 

    // Stop the DPDK PMD

    return PI_STATUS_SUCCESS;
}

}   // np4
}   // pi
