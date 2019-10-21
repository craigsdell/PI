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

#ifndef PI_NP4_DEVICE_H_
#define PI_NP4_DEVICE_H_

#include <PI/pi.h>
#include <PI/target/pi_imp.h>
#include <vector>
#include <np4atom.hpp>
#include <dpdk_device.h>

#define DEF_ATOM_PORT   6660
#define MAX_ATOM_PORT   65535

namespace pi {
namespace np4 {

// @brief   The NP4 Device class
//
//  This class holds both NP4 and DPDK information for P4 devices
//  as well as the functions used to act on these.  There are a lot
//  of static functions that provide wrappers around the NP4 ATOM C++
//  API.
class Device {
  public:
    // @brief Create a device object instance
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   p4_info       P4 Info
    // @return      Function returns a unique pointer to a new device object
    //
    static std::unique_ptr<Device> CreateInstance(
        pi_dev_id_t dev_id,
        const pi_p4info_t* p4_info);
    
    // Get the P4 Device
    // @brief Get a reference to the NP4 Device
    //
    // @return      Function returns a reference to the NP4 Device
    //
    ::np4::Device* GetP4Device() { return p4_dev_.get(); }

    // @brief Flag to indicate if we're in Atom sync mode
    //
    // @return      Function returns a true if in Atom sync mode
    //
    bool syncAtoms() { return sync_atoms_; }

    // @brief Get a reference to the NP4 ATOM
    //
    // @return      Function returns a reference to the NP4 ATOM
    //
    ::np4::Atom& GetP4Atom() { return (*p4_dev_)[atom_id_]; }

    // @brief Get a reference to the NP4 ATOM given an ATOM index
    //
    // @param[in]   atom_id       The ATOM id
    // @return      Function returns a reference to the NP4 ATOM
    //
    ::np4::Atom& GetP4Atom(std::size_t atom_id) { return (*p4_dev_)[atom_id]; }

    // @brief Get a pointer to the P4 Info
    //
    // @return      Function returns a pointer to the P4 Info
    //
    const pi_p4info_t* GetP4Info() { return p4_info_; }

    // @brief Set the P4 Info pointer
    //
    // @param[in]   info          Pointer to the P4 Info
    //
    void SetP4Info(const pi_p4info_t* p4_info) { p4_info_ = p4_info; }

    // @brief Load the FPGA device using the ForwardPipeline data
    //
    // @param[in]   data          The forwarding pipeline data
    // @param[in]   size          Size of the data
    // @return      Function returns the PI status
    //
    pi_status_t LoadDevice(const char *data, size_t size);

    // @brief Starts the NP4 and Packet In/Out DPDK devices
    // @return      Function returns the PI status
    //
    pi_status_t Start();

    // @brief Resets the NP4 and Packet In/Out DPDK devices
    // @return      Function returns the PI status
    //
    pi_status_t Reset();

    // @brief Stops the NP4 and Packet In/Out DPDK devices
    // @return      Function returns the PI status
    //
    pi_status_t Stop();

    // @brief Sends a PacketOut
    // @param[in]   pkt           Packet to send
    // @param[in]   size          Size of the packet
    // @return      Function returns the PI status
    //
    pi_status_t PacketOut(const char *pkt, size_t size);

    Device(pi_dev_id_t dev_id, const pi_p4info_t* p4_info);

    // Device is neither copyable or movable
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(const Device&&) = delete;

    ~Device();

  private:
    pi_dev_id_t  dev_id_;         // PI Device ID
    std::unique_ptr<::np4::Device> p4_dev_;  // NP4 Device
    std::unique_ptr<DPDKDevice> dpdk_dev_;  // DPDK Device
    std::size_t  atom_id_;        // ATOM id if only using one ATOM
    bool         sync_atoms_;     // keep atoms in sync flag
    const pi_p4info_t* p4_info_;
    std::string  dev_path_;       // File name for fpga device
    std::string  dev_host_;       // Hostname for ATOM Daemon
    unsigned     dev_port_;       // Port # for ATOM Daemon (default 6660)
    std::string  pktio_bdf_;      // Packet In/Out Bus:Dev:Func
    std::string  device_data_;    // Device data from FwdPipelineCfg
};

}   // np4
}   // pi

#endif // PI_NP4_DEVICE_H_
