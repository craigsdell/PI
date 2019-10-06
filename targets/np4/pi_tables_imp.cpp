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

#include <PI/int/pi_int.h>
#include <PI/int/serialize.h>
#include <PI/p4info.h>
#include <PI/pi.h>
#include <cstring>
#include "tables.h"
#include "logger.h"


extern "C" {

//! Adds an entry to a table. Trying to add an entry that already exists should
//! return an error, unless the \p overwrite flag is set.
pi_status_t _pi_table_entry_add(pi_session_handle_t session_handle,
                                pi_dev_tgt_t dev_tgt,
                                pi_p4_id_t table_id,
                                const pi_match_key_t *match_key,
                                const pi_table_entry_t *table_entry,
                                int overwrite,
                                pi_entry_handle_t *entry_handle) {

	(void)(session_handle); ///< No support for sessions

	Logger::debug("PI_table_entry_add");
	
    return pi::np4::Tables::EntryAdd(dev_tgt.dev_id, table_id, match_key,
                                     table_entry, overwrite, entry_handle);
}

//! Sets the default entry for a table. Should return an error if the default
//! entry was statically configured and set as const in the P4 program.
pi_status_t _pi_table_default_action_set(pi_session_handle_t session_handle,
                                         pi_dev_tgt_t dev_tgt,
                                         pi_p4_id_t table_id,
                                         const pi_table_entry_t *table_entry) {

	(void)(session_handle);
	Logger::debug("PI_table_default_action_set");
	
    return pi::np4::Tables::DefaultActionSet(dev_tgt.dev_id, table_id,
                                             table_entry);
}

//! Resets the default entry for a table, as previously set with
//! pi_table_default_action_set, to the original default action (as specified in
//! the P4 program).
pi_status_t _pi_table_default_action_reset(pi_session_handle_t session_handle,
                                           pi_dev_tgt_t dev_tgt,
                                           pi_p4_id_t table_id) {

	(void)(session_handle);
	Logger::debug("PI_table_default_action_reset");

    return pi::np4::Tables::DefaultActionReset(dev_tgt.dev_id, table_id);
}

//! Retrieve the default entry for a table.
pi_status_t _pi_table_default_action_get(pi_session_handle_t session_handle,
                                         pi_dev_tgt_t dev_tgt,
                                         pi_p4_id_t table_id,
                                         pi_table_entry_t *table_entry) {

	(void)(session_handle);
	Logger::debug("PI_table_default_action_get");

	return pi::np4::Tables::DefaultActionGet(dev_tgt.dev_id, table_id,
                                             table_entry);
}

//! Need to be called after pi_table_default_action_get, once you wish the
//! memory to be released.
pi_status_t _pi_table_default_action_done(pi_session_handle_t session_handle,
                                          pi_table_entry_t *table_entry) {

	(void)(session_handle);
	Logger::debug("PI_table_default_action_done");

	if (table_entry->entry_type == PI_ACTION_ENTRY_TYPE_DATA) {
		pi_action_data_t *action_data = table_entry->entry.action_data;
		if (action_data) delete[] action_data;
	}

	return PI_STATUS_SUCCESS;
}

pi_status_t _pi_table_default_action_get_handle(
    pi_session_handle_t session_handle,
    pi_dev_tgt_t dev_tgt,
    pi_p4_id_t table_id,
    pi_entry_handle_t *entry_handle) {

  (void)session_handle;
  (void)dev_tgt;
  (void)table_id;
  (void)entry_handle;

  return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

//! Delete an entry from a table using the entry handle. Should return an error
//! if entry does not exist.
pi_status_t _pi_table_entry_delete(pi_session_handle_t session_handle,
                                   pi_dev_id_t dev_id,
                                   pi_p4_id_t table_id,
                                   pi_entry_handle_t entry_handle) {
	(void)(session_handle);
	Logger::debug("PI_table_entry_delete");

    const std::size_t ruleIndex(entry_handle);
    return pi::np4::Tables::EntryDelete(dev_id, table_id, ruleIndex);
}

//! Delete an entry from a table using the match key. Should return an error
//! if entry does not exist.
pi_status_t _pi_table_entry_delete_wkey(pi_session_handle_t session_handle, 
                                        pi_dev_tgt_t dev_tgt,
                                        pi_p4_id_t table_id,
                                        const pi_match_key_t *match_key) {

	(void)(session_handle);
	Logger::debug("PI_table_entry_delete_wkey");

    return pi::np4::Tables::EntryDeleteWKey(dev_tgt.dev_id, table_id,
                                            match_key);
}

//! Modify an existing entry using the entry handle. Should return an error if
//! entry does not exist.
pi_status_t _pi_table_entry_modify(pi_session_handle_t session_handle,
                                   pi_dev_id_t dev_id,
                                   pi_p4_id_t table_id,
                                   pi_entry_handle_t entry_handle,
                                   const pi_table_entry_t *table_entry) {

	(void)(session_handle);
	Logger::debug("PI_table_entry_modify");

    const std::size_t ruleIndex(entry_handle);
    return pi::np4::Tables::EntryModify(dev_id, table_id, ruleIndex,
                                       table_entry);
}

//! Modify an existing entry using the match key. Should return an error if
//! entry does not exist.
pi_status_t _pi_table_entry_modify_wkey(pi_session_handle_t session_handle,
                                        pi_dev_tgt_t dev_tgt,
                                        pi_p4_id_t table_id,
                                        const pi_match_key_t *match_key,
                                        const pi_table_entry_t *table_entry) {

	(void)(session_handle);
	Logger::debug("PI_table_entry_modify_wkey");

    return pi::np4::Tables::EntryModifyWKey(dev_tgt.dev_id, table_id,
                                            match_key, table_entry);
}

//! Retrieve all entries in table as one big blob.
pi_status_t _pi_table_entries_fetch(pi_session_handle_t session_handle,
                                    pi_dev_tgt_t dev_tgt,
                                    pi_p4_id_t table_id,
                                    pi_table_fetch_res_t *res) {

	(void)(session_handle);
	Logger::debug("PI_table_entries_fetch");

    return pi::np4::Tables::EntryFetch(dev_tgt.dev_id, table_id, res);
}

pi_status_t _pi_table_entries_fetch_one(pi_session_handle_t session_handle,
                                        pi_dev_id_t dev_id,
                                        pi_p4_id_t table_id,
                                        pi_entry_handle_t entry_handle,
                                        pi_table_fetch_res_t *res) {
  (void)session_handle;
  (void)dev_id;
  (void)table_id;
  (void)entry_handle;
  (void)res;

  return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

pi_status_t _pi_table_entries_fetch_wkey(pi_session_handle_t session_handle,
                                         pi_dev_tgt_t dev_tgt,
                                         pi_p4_id_t table_id,
                                         const pi_match_key_t *match_key,
                                         pi_table_fetch_res_t *res) {
  (void)session_handle;
  (void)dev_tgt;
  (void)table_id;
  (void)match_key;
  (void)res;

  return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

//! Need to be called after a pi_table_entries_fetch, once you wish the memory
//! to be released.
pi_status_t _pi_table_entries_fetch_done(pi_session_handle_t session_handle,
                                         pi_table_fetch_res_t *res) {

	(void)(session_handle);
	Logger::debug("PI_table_entries_fetch_done");
	
	delete[] res->entries;

	return PI_STATUS_SUCCESS;
}

pi_status_t _pi_table_idle_timeout_config_set(
    pi_session_handle_t session_handle,
    pi_dev_id_t dev_id,
    pi_p4_id_t table_id,
    const pi_idle_timeout_config_t *config) {

  (void)session_handle;
  (void)dev_id;
  (void)table_id;
  (void)config;

  return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

pi_status_t _pi_table_entry_get_remaining_ttl(
    pi_session_handle_t session_handle, pi_dev_id_t dev_id, pi_p4_id_t table_id,
    pi_entry_handle_t entry_handle, uint64_t *ttl_ns) {

  (void)session_handle;
  (void)dev_id;
  (void)table_id;
  (void)entry_handle;
  (void)ttl_ns;

  return PI_STATUS_NOT_IMPLEMENTED_BY_TARGET;
}

}
