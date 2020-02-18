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

#ifndef PI_NP4_TABLES_H_
#define PI_NP4_TABLES_H_

#include <PI/int/pi_int.h>
#include <PI/pi.h>
#include <PI/target/pi_tables_imp.h>
#include <vector>
#include <np4atom.hpp>

// Netcope SDK doesn't support priorites so set a default
#define DEFAULT_PRIORITY 10

namespace pi {
namespace np4 {

// @brief   Class to hold the action properties
class ActionProperties {
  public:
    uint32_t size;
    pi_p4_id_t id;

    ActionProperties(uint32_t s, pi_p4_id_t i) : size(s), id(i) {}
};

// @brief   The NP4 Table class
//
//  This class implements the pi_tables functions in C++, providing
//  wrappers needed to make calls to the NP4 ATOM C++ API.
class Tables {
  public:
    // @brief Takes a pi rule and adds it to the NP4 table
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   match_key     The match key for the rule
    // @param[in]   table_entry   The table entry data
    // @param[in]   overwrite     Flag to allow/disallow overwriting a rule
    // @param[out]  entry_handle  Entry handle (i.e. rule index)
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryAdd(pi_dev_id_t dev_id, pi_p4_id_t table_id,
                                const pi_match_key_t *match_key,
                                const pi_table_entry_t *table_entry,
                                int overwrite,
                                pi_entry_handle_t *entry_handle);

    // @brief Sets the default action of a table rule
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   table_entry   The table entry data
    // @return      Function returns a PI status code
    //
    static pi_status_t DefaultActionSet(pi_dev_id_t dev_id,
                                        pi_p4_id_t table_id,
                                        const pi_table_entry_t *table_entry);

    // @brief Reset the default action of a table rule
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @return      Function returns a PI status code
    //
    static pi_status_t DefaultActionReset(pi_dev_id_t dev_id,
                                          pi_p4_id_t table_id);

    // @brief Get the default action of a table rule
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   table_entry   The returned table entry data
    // @return      Function returns a PI status code
    //
    static pi_status_t DefaultActionGet(pi_dev_id_t dev_id,
                                        pi_p4_id_t table_id,
                                        pi_table_entry_t *table_entry);

    // @brief Get the default action handle (i.e. row index)
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   table_entry   The returned table entry data
    // @param[out]  entry_handle  Entry handle (i.e. rule index)
    // @return      Function returns a PI status code
    //
    static pi_status_t DefaultActionGetHandle(pi_dev_id_t dev_id,
                                              pi_p4_id_t table_id,
                                              pi_entry_handle_t *entry_handle);

    // @brief Delete a table rule
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   ruleIndex     Rule index to be deleted
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryDelete(pi_dev_id_t dev_id,
                                   pi_p4_id_t table_id,
                                   const size_t ruleIndex);

    // @brief Delete a table rule using a key
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   match_key     The match key for the rule
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryDeleteWKey(pi_dev_id_t dev_id,
                                       pi_p4_id_t table_id,
                                       const pi_match_key_t *match_key);

    // @brief Modify a table rule
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   ruleIndex     The rule index of the rule to be modified
    // @param[in]   table_entry   Table entry data used to modify the rule
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryModify(pi_dev_id_t dev_id, 
                                   pi_p4_id_t table_id,
                                   pi_entry_handle_t entry_handle,
                                   const pi_table_entry_t *table_entry);

    // @brief Modify a table rule using a key
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   match_key     The PI match key data used to find a rule
    // @param[in]   table_entry   Table entry data used to modify the rule
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryModifyWKey(pi_dev_id_t dev_id, 
                                       pi_p4_id_t table_id,
                                       const pi_match_key_t *match_key,
                                       const pi_table_entry_t *table_entry);

    // @brief Fetch all the rules in a table
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[out]  res           The table rules returned
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryFetch(pi_dev_id_t dev_id,
                                  pi_p4_id_t table_id,
                                  pi_table_fetch_res_t *res);

    // @brief Fetch one rule in a table
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   ruleIndex     Rule index (0 gets all entries)
    // @param[out]  res           The table rules returned
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryFetchOne(pi_dev_id_t dev_id,
                                     pi_p4_id_t table_id,
                                     size_t ruleIndex,
                                     pi_table_fetch_res_t *res);

    // @brief Fetch all the rules in a table With Key
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   table_id      Table Id
    // @param[in]   match_key     Key to match on
    // @param[out]  res           The table rules returned
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryFetchWKey(pi_dev_id_t dev_id,
                                      pi_p4_id_t table_id,
                                      const pi_match_key_t *match_key,
                                      pi_table_fetch_res_t *res);

    // @brief Fetch is done, so free up memory
    //
    // @param[out]  res           The table rules to be freed
    // @return      Function returns a PI status code
    //
    static pi_status_t EntryFetchDone(pi_table_fetch_res_t *res);

    // Tables is neither copyable or movable
    Tables(const Tables&) = delete;
    Tables& operator=(const Tables&) = delete;
    Tables(Tables&&) = delete;
    Tables& operator=(const Tables&&) = delete;

  private:
    Tables();
};

}   // np4
}   // pi

#endif // PI_NP4_TABLES_H_
