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

#ifndef PI_NP4_DEVICE_MGR_H_
#define PI_NP4_DEVICE_MGR_H_

#include <PI/pi.h>
#include <PI/target/pi_imp.h>
#include <vector>
#include <mutex>
#include <map>
#include <device.h>

namespace pi {
namespace np4 {

extern bool dpdk_initialized_;

// @brief   The Device Manager class for the PI NP4 implementation
//
//  This class provides wrappers to access the NP4 and Packet In/Out DPDK
//  devices in C++ from their PI C implementations. It also provides a way
//  of mapping PI clients like Statum device ids into NP4 and DPDK devices.
//
class DeviceMgr {
  public:

    // @brief Initialise the device manager
    // 
    static pi_status_t Init();

    // @brief Destroy the device manager
    // 
    static pi_status_t Destroy();

    // @brief Initialise the DPDK EAL
    //
    // @param[in]   argc        The number of args
    // @param[in]   argv        Array of pointers to args (see EAL init)
    // @return      Function returns PI Status
    //
    // Note: this needs to be called by whatever component is using
    //       the PI library.  After the ::pi::fe::DeviceMgr() call but
    //       before you start using any DPDK devices.
    // 
    static pi_status_t DPDKInit(int argc, char* argv[]);

    // @brief Get the number of devices
    //
    // @return      Function returns the number of PI devices
    //
    static pi_dev_id_t GetDeviceCount();

    // @brief Get a pointer to the device object
    //
    // @return      Function returns the number of PI devices
    //
    static Device* GetDevice(pi_dev_id_t dev_id);

    // @brief Called by the PI assign device function
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   info          P4 Info
    // @return      Function returns PI Status
    //
	static pi_status_t AssignDevice(pi_dev_id_t dev_id,
                                    const pi_p4info_t* info); 

    // @brief The start of the device update
    //
    // @param[in]   dev_id             Device Id
    // @param[in]   info               P4 Info
    // @param[in]   device_data        NP4 config data
    // @param[in]   device_data_size   NP4 config data size
    // @return      Function returns PI Status
    //
    static pi_status_t UpdateDeviceStart(pi_dev_id_t dev_id,
                                         const pi_p4info_t *info,
                                         const char *device_data,
                                         size_t device_data_size);

    // @brief The end of the device update
    //
    // @param[in]   dev_id             Device Id
    //
    static pi_status_t UpdateDeviceEnd(pi_dev_id_t dev_id);

    // @brief Remove the device (called from PI)
    //
    // @param[in]   dev_id             Device Id
    //
	static pi_status_t RemoveDevice(pi_dev_id_t dev_id); 

    // @brief Send PacketOut to DPDK interface
    //
    // @param[in]   dev_id             Device Id
    //
    static pi_status_t PacketOut(pi_dev_id_t dev_id,
                                 const char *pkt, size_t size);

    // DeviceMgr is neither copyable or movable
    DeviceMgr(const DeviceMgr&) = delete;
    DeviceMgr& operator=(const DeviceMgr&) = delete;
    DeviceMgr(DeviceMgr&&) = delete;
    DeviceMgr& operator=(const DeviceMgr&&) = delete;

  private:
    // Private, need to use CreateInstance
    DeviceMgr() {};

    // Lock to protext devices map and dpdk_initialized
    mutable std::mutex mutex{};

    // Map of devices
    std::map<pi_dev_id_t, std::unique_ptr<Device>> devices;
};

}   // np4
}   // pi

#endif // PI_NP4_DEVICE_MGR_H_
