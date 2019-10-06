/* Copyright 2013-present Barefoot Networks, Inc.
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

/*
 * Jakub Neruda (xnerud01@stud.fit.vutbr.cz)
 *
 */

#include <PI/p4info.h>
#include <PI/pi.h>
#include <PI/target/pi_counter_imp.h>
#include "counter.h"

extern "C" {

/*
NOTE: This could come in handy later (as defined in  PI/include/PI/pi_counter.h):
#define PI_COUNTER_FLAGS_NONE 0
// do a sync with the hw when reading a counter
#define PI_COUNTER_FLAGS_HW_SYNC (1 << 0)
*/

pi_status_t _pi_counter_read(pi_session_handle_t session_handle,
                             pi_dev_tgt_t dev_tgt,
                             pi_p4_id_t counter_id,
                             size_t index,
                             int flags,
                             pi_counter_data_t *counter_data) {

	(void)(session_handle);
	(void)(flags);
	
    return pi::np4::Counter::Read(dev_tgt.dev_id, counter_id,
                                  index, counter_data);
}

pi_status_t _pi_counter_write(pi_session_handle_t session_handle,
                              pi_dev_tgt_t dev_tgt,
                              pi_p4_id_t counter_id,
                              size_t index,
                              const pi_counter_data_t *counter_data) {

	(void)(session_handle);
	(void)(dev_tgt);
	(void)(counter_id);
	(void)(index);
	(void)(counter_data);
	
	return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

pi_status_t _pi_counter_read_direct(pi_session_handle_t session_handle,
                                    pi_dev_tgt_t dev_tgt,
                                    pi_p4_id_t counter_id,
                                    pi_entry_handle_t entry_handle,
                                    int flags,
                                    pi_counter_data_t *counter_data) {

	(void)(session_handle);
	(void)(dev_tgt);
	(void)(counter_id);
	(void)(entry_handle);
	(void)(flags);
	(void)(counter_data);
	
	return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

pi_status_t _pi_counter_write_direct(pi_session_handle_t session_handle,
                                     pi_dev_tgt_t dev_tgt,
                                     pi_p4_id_t counter_id,
                                     pi_entry_handle_t entry_handle,
                                     const pi_counter_data_t *counter_data) {

	(void)(session_handle);
	(void)(dev_tgt);
	(void)(counter_id);
	(void)(entry_handle);
	(void)(counter_data);
	
	return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

pi_status_t _pi_counter_hw_sync(pi_session_handle_t session_handle,
                                pi_dev_tgt_t dev_tgt,
                                pi_p4_id_t counter_id,
                                PICounterHwSyncCb cb, void *cb_cookie) {

	(void)(session_handle);
	(void)(dev_tgt);
	(void)(counter_id);
	(void)(cb);
	(void)(cb_cookie);
	
	return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

}
