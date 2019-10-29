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

#ifndef PI_NP4_DPDK_DEVICE_H_
#define PI_NP4_DPDK_DEVICE_H_

#include <PI/pi.h>
#include <PI/target/pi_imp.h>
#include <vector>
#include <mutex>
#include <thread>
#include <rte_config.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include "pi/np4/p4_device_config.pb.h"

#define NUM_RX_RINGS 1
#define NUM_TX_RINGS 1
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define MAX_PACKET_SIZE (RTE_MBUF_DEFAULT_BUF_SIZE)

namespace pi {
namespace np4 {

// @brief   The DPDK Device class
//
//  This class processes packets received from and sent to the DPDK device.
class DPDKDevice {
  public:
    // @brief Create a device object instance
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   dpdk_confg    ::pi::np4::DPDKConfig
    // @return      Function returns a unique pointer to a new device object
    //
    static std::unique_ptr<DPDKDevice> CreateInstance(
        pi_dev_id_t dev_id, const ::pi::np4::DPDKConfig& config);
    
    // @brief Initialise the port
    //
    // @param[in]   port          DPDK port id of the device
    // @return      Function returns status of the call
    //
    pi_status_t PortInit(const ::pi::np4::DPDKConfig& config);

    // @brief Clear stats
    //
    void ClearStats();

    // @brief Free the port resources
    //
    // @return      Function returns status of the call
    //
    pi_status_t PortFree();

    // @brief Start the PacketIn procesing
    //
    // @return      Function returns status of the call
    //
    pi_status_t Start();

    // @brief Stop the PacketIn procesing and wait for thread to finish
    //
    // @return      Function returns status of the call
    //
    pi_status_t Stop();

    // @brief PacketIn PMD thread
    //
    // @return      Function returns status of the call
    //
    void PacketInLoop();

    // @brief Called to process packets coming in from DPDK device
    //
    // @param[in]   pkt           packet from the device
    // @param[in]   size          size of the packet
    // @return      Function returns status of the call
    //
    pi_status_t PacketIn();

    // @brief Called to send packets out to DPDK device
    //
    // @param[in]   pkt           packet from the stratum upstream client
    // @param[in]   size          size of the packet
    // @return      Function returns status of the call
    //
    pi_status_t PacketOut(const char* pkt, size_t size);

    // DPDK Device is neither copyable or movable
    DPDKDevice(const DPDKDevice&) = delete;
    DPDKDevice& operator=(const DPDKDevice&) = delete;
    DPDKDevice(DPDKDevice&&) = delete;
    DPDKDevice& operator=(const DPDKDevice&&) = delete;

    ~DPDKDevice();

  private:
    DPDKDevice(pi_dev_id_t dev_id);

    uint16_t port_;
    pi_dev_id_t dev_id_;
    std::string dev_name_;
    std::thread recv_thread{};
    bool stop_recv_thread_{false};
    mutable std::mutex mutex{};
    bool port_initialized_{false};

    // DPDK mbuf and device info
    uint16_t nb_rxd_{0};            // rx ring size
    uint16_t nb_txd_{0};            // tx ring size
    uint16_t rx_rings_{0};
    uint16_t tx_rings_{0};
    uint16_t num_mbufs_{0};
    uint16_t mbuf_cache_size_{0};
    uint16_t burst_size_{0};
    struct rte_mempool* mbuf_pool_{nullptr};
    struct rte_eth_dev_info dev_info_;
    struct rte_eth_txconf txconf_;
    struct rte_eth_conf port_conf_{};

    struct rte_ether_hdr pktout_hdr_;
    struct rte_mbuf** pktin_bufs_;
    char* pktin_data_;

    // stats
    uint64_t pktin_errors;
    uint64_t pktin_rx_errors;
    uint64_t pktin_too_big;
    uint64_t pktin_too_small;
    uint64_t pktin_success;
    uint64_t pktout_too_big;
    uint64_t pktout_tx_errors;
    uint64_t pktout_alloc_fails;
    uint64_t pktout_success;
};

}   // np4
}   // pi

#endif // PI_NP4_DPDK_DEVICE_H_
