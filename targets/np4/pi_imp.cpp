/* Copyright 2013-present Barefoot Networks, Inc.
 * Copyright 2019 Dell EMC
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

#include <PI/int/pi_int.h>
#include <PI/pi.h>
#include <PI/target/pi_imp.h>

#include <iostream>
#include <string>

#include "device_mgr.h"

#include <cstring>  // for memset

extern "C" {

pi_status_t _pi_init(int *abi_version, void *extra) {

  (void) extra;

  *abi_version = PI_ABI_VERSION;

  return pi::np4::DeviceMgr::Init();
}

pi_status_t _pi_assign_device(pi_dev_id_t dev_id, const pi_p4info_t *p4info,
                              pi_assign_extra_t *extra) {

  (void)extra;

  return pi::np4::DeviceMgr::AssignDevice(dev_id, p4info);
}

pi_status_t _pi_update_device_start(pi_dev_id_t dev_id,
                                    const pi_p4info_t *info,
                                    const char *device_data,
                                    size_t device_data_size) {

  return pi::np4::DeviceMgr::UpdateDeviceStart(dev_id, info, device_data, 
                                               device_data_size);
}

pi_status_t _pi_update_device_end(pi_dev_id_t dev_id) {

  return pi::np4::DeviceMgr::UpdateDeviceEnd(dev_id);
}

pi_status_t _pi_remove_device(pi_dev_id_t dev_id) {

  return pi::np4::DeviceMgr::RemoveDevice(dev_id);
}

pi_status_t _pi_destroy() {

  return pi::np4::DeviceMgr::Destroy();
}

// np4 does not support transactions and has no use for the session_handle
pi_status_t _pi_session_init(pi_session_handle_t *session_handle) {
  *session_handle = 0;
  return PI_STATUS_SUCCESS;
}

pi_status_t _pi_session_cleanup(pi_session_handle_t session_handle) {
  (void) session_handle;
  return PI_STATUS_SUCCESS;
}

pi_status_t _pi_batch_begin(pi_session_handle_t session_handle) {
  (void) session_handle;
  return PI_STATUS_SUCCESS;
}

pi_status_t _pi_batch_end(pi_session_handle_t session_handle, bool hw_sync) {
  (void) session_handle;
  (void) hw_sync;
  return PI_STATUS_SUCCESS;
}

// TODO: need to send via DPDK library
pi_status_t _pi_packetout_send(pi_dev_id_t dev_id, const char *pkt,
                               size_t size) {
  (void)dev_id;
  (void)pkt;
  (void)size;

  return PI_STATUS_SUCCESS;
}

// TODO: need to get port status?
pi_status_t _pi_port_status_get(pi_dev_id_t dev_id, pi_port_t port,
                                pi_port_status_t *status) {
  (void)dev_id;
  (void)port;
  *status = PI_PORT_STATUS_UP;
  return PI_STATUS_SUCCESS;
}

}
