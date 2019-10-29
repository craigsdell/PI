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
#include "dpdk_device.h"
#include "proto/frontend/src/logger.h"
#include "pi/np4/p4_device_config.pb.h"
#include <rte_mempool.h>

using pi::fe::proto::Logger;

namespace pi {
namespace np4 {

DPDKDevice::DPDKDevice(pi_dev_id_t dev_id)
  : dev_id_(dev_id) {

    Logger::get()->trace("Dev {}: DPDK Device created", dev_id_);
}

DPDKDevice::~DPDKDevice() {

    // Free up port resources
    PortFree();

    Logger::get()->trace("Dev {}: DPDK Device deleted", dev_id_);
}

std::unique_ptr<DPDKDevice> DPDKDevice::CreateInstance(
        pi_dev_id_t dev_id, const ::pi::np4::DPDKConfig& config) {

    Logger::get()->trace("Dev {}: DPDK Device CreateInstance", dev_id);

    // Create device
    auto dpdk_dev =  new DPDKDevice(dev_id);

    // Now call init
    if (dpdk_dev->PortInit(config) != PI_STATUS_SUCCESS) {
        Logger::get()->error("Dev {}: Init failed", dev_id);
        delete dpdk_dev;
        return nullptr;
    }

    return std::unique_ptr<DPDKDevice>(dpdk_dev);
}

pi_status_t DPDKDevice::PortInit(const ::pi::np4::DPDKConfig& config) {

    int retval;
    uint16_t q;

    std::unique_lock<std::mutex> lock(mutex);
    if (port_initialized_) {
        Logger::get()->error("Dev {}: port already initialized", dev_id_);
        return PI_STATUS_TARGET_ERROR;
    }

    dev_name_ = config.device_name();

    // Find port id first
    uint16_t port;
    if (rte_eth_dev_get_port_by_name(dev_name_.c_str(), &port)) {
        Logger::get()->error("Dev {}: Can't find DPDK device {}",
                             dev_id_, dev_name_);
        return PI_STATUS_DEV_OUT_OF_RANGE;
    }
    port_ = port;

    // Is it a valid port
    if (!rte_eth_dev_is_valid_port(port_)) {
        Logger::get()->error("Dev {}: port {} not valid", dev_id_, port_);
        return pi_status_t(PI_STATUS_TARGET_ERROR);
    }

    // init config
    port_ = port;
    port_conf_.rxmode.max_rx_pkt_len = RTE_ETHER_MAX_LEN;
    rx_rings_ = (config.num_rx_rings() ? config.num_rx_rings(): NUM_RX_RINGS);
    tx_rings_ = (config.num_tx_rings() ? config.num_tx_rings(): NUM_TX_RINGS);
    nb_rxd_ = (config.rx_ring_size() ? config.rx_ring_size(): RX_RING_SIZE);
    nb_txd_ = (config.tx_ring_size() ? config.tx_ring_size(): TX_RING_SIZE);
    num_mbufs_ = (config.num_mbufs() ? config.num_mbufs(): NUM_MBUFS);
    mbuf_cache_size_ =
        (config.mbuf_cache_size() ? config.mbuf_cache_size(): MBUF_CACHE_SIZE);
    burst_size_ = (config.burst_size() ? config.burst_size(): BURST_SIZE);

    ClearStats();

    // Check if mbuf pool needing to be created
    if (mbuf_pool_ == nullptr) {
        // Creates a new mempool in memory to hold the mbufs.
        std::string pool_name = "Dev-" + std::to_string(dev_id_);
        mbuf_pool_ = 
            rte_pktmbuf_pool_create(pool_name.c_str(), num_mbufs_,
                                    mbuf_cache_size_, 0, 
                                    MAX_PACKET_SIZE, rte_socket_id());

        if (mbuf_pool_ == NULL) {
            Logger::get()->error("Dev {}: DPDK mbuf pool create error: {}", 
                                 dev_id_, rte_errno);
            return pi_status_t(PI_STATUS_TARGET_ERROR+rte_errno);
        }
        Logger::get()->info("Dev {}: created mbuf pool {}/{}/{}",
                            dev_id_, num_mbufs_, mbuf_cache_size_,
                            MAX_PACKET_SIZE);
    }
    Logger::get()->info("Dev {}: mbuf pool avail/in-use {}/{}", dev_id_,
                        rte_mempool_avail_count(mbuf_pool_),
                        rte_mempool_in_use_count(mbuf_pool_));

    // Grab device info
    rte_eth_dev_info_get(port_, &dev_info_);
    if (dev_info_.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
        port_conf_.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

    Logger::get()->info("Dev {}: Port init port {}: device name {}",
           dev_id_, port, dev_info_.device->name);

    // Configure the Ethernet device.
    retval = rte_eth_dev_configure(port_, rx_rings_, tx_rings_, &port_conf_);
    if (retval != 0) {
        Logger::get()->error("Dev {}: config eth failed: {}",
                             dev_id_, rte_errno);
        return pi_status_t(PI_STATUS_TARGET_ERROR + retval);
    }

    // enable promiscous mode
    rte_eth_promiscuous_enable(port_);

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_, &nb_rxd_, &nb_txd_);
    if (retval != 0) {
        Logger::get()->error("Dev {}: adjust nb rx/tx desc failed: {}",
                             dev_id_, rte_errno);
        return pi_status_t(PI_STATUS_TARGET_ERROR + retval);
    }

    // Allocate and set up RX queues per Ethernet port.
    // TODO: what happens second time thru?
    for (q = 0; q < rx_rings_; q++) {
        retval = rte_eth_rx_queue_setup(port_, q, nb_rxd_,
                rte_eth_dev_socket_id(port_), NULL, mbuf_pool_);
        if (retval < 0) {
            Logger::get()->error("Dev {}: rx q({}) setup failed: {}",
                             dev_id_, q, rte_errno);
            return pi_status_t(PI_STATUS_TARGET_ERROR + retval);
        }
    }

    txconf_ = dev_info_.default_txconf;
    txconf_.offloads = port_conf_.txmode.offloads;

    // Allocate and set up TX queue per Ethernet port.
    for (q = 0; q < tx_rings_; q++) {
        retval = rte_eth_tx_queue_setup(port_, q, nb_txd_,
                rte_eth_dev_socket_id(port_), &txconf_);
        if (retval < 0) {
            Logger::get()->error("Dev {}: tx q({}) setup failed: {}",
                             dev_id_, q, rte_errno);
            return pi_status_t(PI_STATUS_TARGET_ERROR + retval);
        }
    }

    // Preallocate packetin buffer and mbuf pointers
    // Note: don't forget to free them
    try {
        pktin_data_ = new char[MAX_PACKET_SIZE];
        pktin_bufs_ = new struct rte_mbuf*[burst_size_];
    } catch (std::bad_alloc&) {
        Logger::get()->error("Dev {}: failed to alloc pktin data", dev_id_);
        return PI_STATUS_ALLOC_ERROR;
    }
    // Setup dummy packet_out ethernet header
    rte_ether_unformat_addr("00:00:01:00:00:01", &(pktout_hdr_.s_addr));
    rte_ether_unformat_addr("00:00:02:00:00:02", &(pktout_hdr_.d_addr));
    pktout_hdr_.ether_type = RTE_ETHER_TYPE_MPLS;

    // Start the Ethernet port.
    retval = rte_eth_dev_start(port_);
    if (retval < 0) {
        delete[] pktin_data_;
        delete[] pktin_bufs_;
        Logger::get()->error("Dev {}: DPDK port start failed: {}",
                             dev_id_, retval);
        return pi_status_t(PI_STATUS_TARGET_ERROR + retval);
    }

    // Display the port MAC address. 
    struct rte_ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    Logger::get()->info("Port {} MAC: {}:{}:{}:{}:{}:{}",
            port,
            addr.addr_bytes[0], addr.addr_bytes[1],
            addr.addr_bytes[2], addr.addr_bytes[3],
            addr.addr_bytes[4], addr.addr_bytes[5]);

    // Enable RX in promiscuous mode for the Ethernet device. 
    rte_eth_promiscuous_enable(port);

    port_initialized_ = true;

    return PI_STATUS_SUCCESS;
}

void DPDKDevice::ClearStats() {

    // init stats
    pktin_errors = 0;
    pktin_rx_errors = 0;
    pktin_too_big = 0;
    pktin_success = 0;
    pktout_too_big = 0;
    pktout_tx_errors = 0;
    pktout_alloc_fails = 0;
    pktout_success = 0;
}

pi_status_t DPDKDevice::PortFree() {

    std::unique_lock<std::mutex> lock(mutex);
    if (port_initialized_) {
        // Stop device
        rte_eth_dev_stop(port_);

        // Free up memory
        delete[] pktin_data_;
        delete[] pktin_bufs_;

        // Free mbuf pool
        rte_mempool_free(mbuf_pool_);
        mbuf_pool_ = nullptr;

        port_initialized_ = false;
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t DPDKDevice::Start() {
    std::unique_lock<std::mutex> lock(mutex);
    stop_recv_thread_ = false;
    recv_thread = std::thread(&DPDKDevice::PacketInLoop, this);

    return PI_STATUS_SUCCESS;
}

pi_status_t DPDKDevice::Stop() {
    std::unique_lock<std::mutex> lock(mutex);
    if (stop_recv_thread_) {
        Logger::get()->warn("Dev {}: recv loop already stopped", dev_id_);
        return PI_STATUS_SUCCESS;
    }
    stop_recv_thread_ = true;

    // Wait for threads to stop
    recv_thread.join();

    return PI_STATUS_SUCCESS;
}

void DPDKDevice::PacketInLoop() {

    // Check if we're on same NUMA as ports
    if (rte_eth_dev_socket_id(port_) > 0 &&
        rte_eth_dev_socket_id(port_) !=
            (int)rte_socket_id()) {
            Logger::get()->warn("Dev {}: port {} is on remote NUMA node to "
                                "polling thread.\n\tPerformance will "
                                "not be optimal.\n", dev_id_, port_);
    }

    // run until stop flag set
    while (!stop_recv_thread_) {

        // Just check the packet in queues
        PacketIn();
    }
}

pi_status_t DPDKDevice::PacketIn() {

    // Get burst of RX packets, from first port of pair.
    for (uint16_t q=0; q < rx_rings_; q++) {
        const uint16_t nb_rx = 
            rte_eth_rx_burst(port_, q, pktin_bufs_, burst_size_);

        if (unlikely(nb_rx == 0)) {
            pktin_rx_errors++;
            continue;
        }

        // Process all packets received
        for (size_t i=0; i < nb_rx; i++) {
            rte_mbuf* m = pktin_bufs_[i];
            size_t size = static_cast<size_t>rte_pktmbuf_pkt_len(m);

            // Check to make sure the mbuf is not too long
            if (size > MAX_PACKET_SIZE) {
                Logger::get()->error("Dev {}: mbuf too bug: {}", dev_id_, size);
                pktin_too_big++;
                return PI_STATUS_TARGET_ERROR;
            }

            // Copy the packet from the DPDK mbuf
            size_t eth_size = sizeof(struct rte_ether_hdr);
            // We need at least ethernet header and 2 byte metadata
            if (size < eth_size+2) {
                Logger::get()->error("Dev {}: mbuff too short: {}", 
                                     dev_id_, size);
                pktin_too_small++;
                return PI_STATUS_TARGET_ERROR;
            }

            // Copy in the packet minus the dummy eth header
            rte_memcpy(pktin_data_, rte_pktmbuf_mtod_offset(
                m, void *, eth_size), (size - eth_size));

            // Free the mbuf
            rte_pktmbuf_free(m);

            // Now send it upstream
            // Note: might be a little bit better performance if both this
            //       process and the NIC are on the same NUMA.
            pi_status_t rc = pi_packetin_receive(dev_id_, pktin_data_, size);
            if (rc == PI_STATUS_SUCCESS) {
                pktin_success++;
            } else {
                pktin_errors++;
            }
        }
    }
    return PI_STATUS_SUCCESS;
}

pi_status_t DPDKDevice::PacketOut(const char* pkt, size_t size) {

    // Check MTU size
    if (size > dev_info_.max_mtu) {
        Logger::get()->error("Dev {}: PacketOut {} bigger than MTU {}", 
                             dev_id_, size, dev_info_.max_mtu);
        pktout_too_big++;
        return PI_STATUS_TARGET_ERROR;
    }

    // Check not bigger than MBuf len
    if (size+sizeof(rte_ether_hdr) > RTE_MBUF_DEFAULT_BUF_SIZE) {
        Logger::get()->error("Dev {}: PacketOut {} bigger than MBuf {}", 
                             dev_id_, size, RTE_MBUF_DEFAULT_BUF_SIZE);
        pktout_too_big++;
        return PI_STATUS_TARGET_ERROR;
    }

    // get a new mbuf
    struct rte_mbuf* m = rte_pktmbuf_alloc(mbuf_pool_);
    if (m == nullptr) {
        Logger::get()->error("Dev {}: mbuf alloc failed", dev_id_);
        pktout_alloc_fails++;
        return PI_STATUS_TARGET_ERROR;
    }
    // Copy dummy ethernet header first
    size_t offset = sizeof(rte_ether_hdr);
    rte_memcpy(rte_pktmbuf_mtod(m, void *), &pktout_hdr_, offset);
    // copy packet into mbuf
    rte_memcpy(rte_pktmbuf_mtod_offset(m, void *, offset), pkt, size);
    m->pkt_len = size;
    m->data_len = size;

    // Now send mbuf
    // Note: we'll just use queue 0 for now
    if (rte_eth_tx_burst(port_, 0, &m, 1) != 1) {
        rte_pktmbuf_free(m);
        Logger::get()->error("failed to senf mbuf");
        pktout_tx_errors++;
        return PI_STATUS_TARGET_ERROR;
    }
    pktout_success++;

    return PI_STATUS_SUCCESS;
}

}   // np4
}   // pi
