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

#include "p4dev.h"
#include "device_mgr.h"
#include "device.h"
#include "logger.h"

namespace pi {
namespace np4 {

DeviceMgr* np4_device_mgr_ = nullptr;

pi_status_t DeviceMgr::Init() {

    if (np4_device_mgr_ != nullptr) {
        Logger::error("DeviceMgr has already been initialised");
        return PI_STATUS_ALLOC_ERROR;
    }
    np4_device_mgr_ = new DeviceMgr();

    return PI_STATUS_SUCCESS;
}

pi_status_t DeviceMgr::Destroy() {

    if (np4_device_mgr_ != nullptr) {
        delete np4_device_mgr_;
        np4_device_mgr_ = nullptr;
    }

    return PI_STATUS_SUCCESS;
}

pi_dev_id_t DeviceMgr::GetDeviceCount() {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::error("DeviceMgr not initialised");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

	return np4_device_mgr_->devices.size();
}

Device* DeviceMgr::GetDevice(pi_dev_id_t dev_id) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::error("DeviceMgr not initialised");
        return nullptr;
    }

    if (np4_device_mgr_->devices.find(dev_id) 
            == np4_device_mgr_->devices.end()) {
        return nullptr;

    }
    return np4_device_mgr_->devices[dev_id].get();
}

pi_status_t DeviceMgr::AssignDevice(pi_dev_id_t dev_id, 
                                    const pi_p4info_t *info) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::error("DeviceMgr not initialised");
        return PI_STATUS_ALLOC_ERROR;
    }

    // Check to make sure this device id is not allocated
    auto dev =  np4_device_mgr_->GetDevice(dev_id);
    if (dev != nullptr) {
        Logger::error("Dev "+std::to_string(dev_id)+" already allocated");
        return PI_STATUS_DEV_ALREADY_ASSIGNED;
    }

    // Allocate NP4 device in online mode (i.e. Atom Daemon)
    np4_device_mgr_->devices[dev_id] = Device::CreateInstance(dev_id, info);

    // Did it work
    if (np4_device_mgr_->devices[dev_id] == nullptr)
        return PI_STATUS_ALLOC_ERROR;

    return PI_STATUS_SUCCESS;
}

pi_status_t DeviceMgr::UpdateDeviceStart(pi_dev_id_t dev_id,
                                            const pi_p4info_t *info,
                                            const char *device_data,
                                            size_t device_data_size) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::error("DeviceMgr not initialised");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Check to make sure this device id is allocated
    auto dev =  np4_device_mgr_->GetDevice(dev_id);
    if (dev == nullptr) {
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Update the p4 info
    dev->SetP4Info(info);

    // Store the Device Data passed down from FwdPipelineCfg
    return dev->LoadDevice(device_data, device_data_size);
}

pi_status_t DeviceMgr::UpdateDeviceEnd(pi_dev_id_t dev_id) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::error("DeviceMgr not initialised");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Check to make sure this device id is allocated
    auto dev =  np4_device_mgr_->GetDevice(dev_id);
    if (dev == nullptr) {
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Start the NP4 device
    auto rc = dev->Start();
    if (rc != PI_STATUS_SUCCESS) {
        return rc;
    }

    // TODO: Probably spin up the DPDK PMD for received pkts

    return PI_STATUS_SUCCESS;
}

pi_status_t DeviceMgr::RemoveDevice(pi_dev_id_t dev_id) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::error("DeviceMgr not initialised");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Check to make sure this device id is allocated
    auto dev =  np4_device_mgr_->GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::error("Dev "+std::to_string(dev_id)+" not allocated");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Delete device
    np4_device_mgr_->devices.erase(dev_id);

    return PI_STATUS_SUCCESS;
}

}   // np4
}   // pi
