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

#include <iostream>
#include "p4dev.h"
#include "device_mgr.h"
#include "device.h"
#include "proto/frontend/src/logger.h"
#include "rte_eal.h"
#include "rte_ethdev.h"
#include "rte_errno.h"

using pi::fe::proto::Logger;

namespace pi {
namespace np4 {

DeviceMgr* np4_device_mgr_ = nullptr;
bool dpdk_initialized_{false};

pi_status_t DeviceMgr::Init() {

    if (np4_device_mgr_ != nullptr) {
        Logger::get()->error("DeviceMgr has already been initialised");
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

// Note: this needs to be called by whatever component is using
//       PI library.  After the ::pi::fe::DeviceMgr() call but
//       before you start using any devices.

pi_status_t DeviceMgr::DPDKInit(int argc, char* argv[]) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::get()->error("DeviceMgr not initialised");
        return PI_STATUS_TARGET_ERROR;
    }

    // Make sure we only call this once
    std::unique_lock<std::mutex> lock(np4_device_mgr_->mutex);
    if (dpdk_initialized_) {
        Logger::get()->error("DPDK Init has already been called");
        return PI_STATUS_TARGET_ERROR;
    }

	// Initialize the Environment Abstraction Layer (EAL).
	int ret = rte_eal_init(argc, argv);
	if (ret < 0) {
        Logger::get()->error("DPDK EAL init failed: {}", rte_errno);
        return pi_status_t(PI_STATUS_TARGET_ERROR+rte_errno);
    }
    Logger::get()->info("EAL init successful");

    // not really necessary
	argc -= ret;
	argv += ret;

    // Check we've got at least one DPDK device
    auto nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 1) {
        Logger::get()->error("DPDK needs at least one DPDK device: {}/{}",
                             nb_ports, rte_errno);
        return pi_status_t(PI_STATUS_TARGET_ERROR+rte_errno);
    }
    Logger::get()->info("DPDK Init found {} devices", nb_ports);

    dpdk_initialized_ = true;

    return PI_STATUS_SUCCESS;
}

pi_dev_id_t DeviceMgr::GetDeviceCount() {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::get()->error("DeviceMgr not initialised");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

	return np4_device_mgr_->devices.size();
}

Device* DeviceMgr::GetDevice(pi_dev_id_t dev_id) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::get()->error("DeviceMgr not initialised");
        return nullptr;
    }

    std::unique_lock<std::mutex> lock(np4_device_mgr_->mutex);
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
        Logger::get()->error("DeviceMgr not initialised");
        return PI_STATUS_ALLOC_ERROR;
    }

    // Check to make sure this device id is not allocated
    if (np4_device_mgr_->GetDevice(dev_id) != nullptr) {
        Logger::get()->error("Dev {} already allocated", dev_id);
        return PI_STATUS_DEV_ALREADY_ASSIGNED;
    }

    // Allocate device 
    auto dev = Device::CreateInstance(dev_id, info);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: device create failed");
        return PI_STATUS_ALLOC_ERROR;
    }

    // Move into devices map
    std::unique_lock<std::mutex> lock(np4_device_mgr_->mutex);
    np4_device_mgr_->devices[dev_id] = std::move(dev);

    return PI_STATUS_SUCCESS;
}

pi_status_t DeviceMgr::UpdateDeviceStart(pi_dev_id_t dev_id,
                                            const pi_p4info_t *info,
                                            const char *device_data,
                                            size_t device_data_size) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::get()->error("DeviceMgr not initialised");
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
        Logger::get()->error("DeviceMgr not initialised");
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
        Logger::get()->error("DeviceMgr not initialised");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Check to make sure this device id is allocated
    auto dev =  np4_device_mgr_->GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {} not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Delete device
    np4_device_mgr_->devices.erase(dev_id);

    return PI_STATUS_SUCCESS;
}

pi_status_t
DeviceMgr::PacketOut(pi_dev_id_t dev_id, const char *pkt, size_t size) {

    // Check to make sure Device Manager initialised
    if (np4_device_mgr_ == nullptr) {
        Logger::get()->error("DeviceMgr not initialised");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Check to make sure this device id is allocated
    auto dev =  np4_device_mgr_->GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {} not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Call PacketOut
    return dev->PacketOut(pkt, size);
}

}   // np4
}   // pi
