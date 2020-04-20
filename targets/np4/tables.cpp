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

#include <PI/int/serialize.h>

#include "p4dev.h"
#include "tables.h"
#include "proto/frontend/src/logger.h"
#include "device_mgr.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

using pi::fe::proto::Logger;

namespace pi {
namespace np4 {

//---------------- Table Helper functions ---------------------------

// @brief Helper function to return a string representation of the 
//        given vector of uint8_t values.
//
// @param[in]   values      vector of uint8_t values
// @param[in]   hex         Print in hex flag (optional)
// return       Returns a string representation of the vector
//
std::string
StringValues(std::vector<uint8_t> values, bool hex=false)
{
    std::string str;
    std::stringstream st;

    st << "(";
    if (hex)
        st << std::setfill('0') << std::setw(2) << std::hex;

    for (size_t i=0; i < values.size(); i++) {
        if (i != 0) st << ", ";
        st << (int)values[i];
    } 
    st << ")";

    return st.str();
}

// @brief Helper function to return a string representation of the 
//        given data bytes
//
// @param[in]   data        bit field
// @param[in]   len         length of the data
// return       Returns a hex string representation of the data
//
std::string
StringData(const char* data, size_t len)
{
    const uint8_t *udata = reinterpret_cast<const uint8_t *>(data);
    std::string str;
    std::stringstream st;

    st << "0x";
    st << std::setfill('0') << std::hex;

    for (size_t i=0; i < len; i++) {
        st << std::setw(2) << (int)udata[i];
    } 

    return st.str();
}

// @brief Helper function to convert a bit field to a vector of uint8_t values
//
// @param[in]   data        data field
// @param[in]   bitwidth    bit width of the data field
// @param[in]   values      vector of uint8_t values
// return       Returns a string representation of the vector
//
size_t Bitfield2Vector(const char* data, size_t bitwidth,
                       std::vector<uint8_t>& values)
{
    const uint8_t *field = reinterpret_cast<const uint8_t *>(data);

    size_t bytewidth = (bitwidth + 7) / 8;

    // Apply bit mask to first byte
    uint8_t bitshift = bitwidth%8;
    uint8_t mask = 0xff;
    // not byte aligned
    if (bitshift != 0) mask = (1 << bitshift) - 1;

    // push first byte with first byte bit mask
    values.push_back(field[0] & mask);

    // then grab the rest if any
    for (size_t i=1; i < bytewidth; i++) {
        values.push_back(field[i]);
    }

    return(bytewidth);
}

// @brief Helper function to convert a vector of uint8_t values to a bit field
//
// @param[in]   values      vector of uint8_t values
// @param[in]   data        data field to be populated
// @param[in]   bitwidth    bit width of the data field
// return       Returns a string representation of the vector
//
size_t Vector2Bitfield(const std::vector<uint8_t>& values,
                       char* data, size_t bitwidth)
{
    uint8_t *field = reinterpret_cast<uint8_t *>(data);

    size_t bytewidth = (bitwidth + 7) / 8;
    if (bytewidth < values.size()) {
        Logger::get()->error("Vector2Bitfield bit field too small");
        return 0;
    }

    // Clear whole bitfield
    std::memset(field, 0, bytewidth);

    for (size_t i=0; i < values.size(); i++)
        field[i] = values[i];

    return(bytewidth);
}

using ParamMap = std::unordered_map<size_t, const ::np4::Parameter&>;

// @brief Helper function to create the param id to param object map
//
// @param[in]   info        P4 Info
// @param[in]   actionId    The action id of this action
// @param[in]   parameters  A vector of NP4 parameter objects
// @return      Function returns a map of NP4 Parameters indexed by id
//
ParamMap CreateParamMap(const pi_p4info_t* info,
                        const pi_p4_id_t actionId,
                        const std::vector<::np4::Parameter>& parameters) {

    // Create parameter map for this action
    ParamMap pmap;
    pmap.reserve(parameters.size());
    for (const ::np4::Parameter& param : parameters) {
        size_t id = pi_p4info_action_param_id_from_name(info, actionId,
                                                        param.name.c_str());
        if (id == PI_INVALID_ID) {
            Logger::get()->error("invalid param name {} for action id {}",
                                 param.name, actionId);
        } else {
            pmap.emplace(id, param);
        }
    }
    return pmap;
}


using KeyMap = std::unordered_map<size_t, const ::np4::KeyElem*>;

// @brief Helper function to create the key id to key object map
//
// @param[in]   info        P4 Info
// @param[in]   table_id    The action id of this action
// @param[in]   key         The key class
// @return      Function returns a map of NP4 KeyElems indexed by id
//
KeyMap CreateKeyMap(const pi_p4info_t* info,
                        const pi_p4_id_t table_id,
                        const ::np4::Key& key) {

    // Create key map for this key
    KeyMap kmap;
    kmap.reserve(key.size());
    for (const ::np4::KeyElem* keyElem : key) {
        size_t id = pi_p4info_table_match_field_id_from_name(info, table_id,
                                                        keyElem->name.c_str());
        if (id == PI_INVALID_ID) {
            Logger::get()->error("invalid match field name {} for table id {}",
                                 keyElem->name, table_id);
        } else {
            kmap[id] = keyElem;
        }
    }
    return kmap;
}


// @brief Set the match key data in the np4::Key object
//
// @param[in]   info        P4 Info
// @param[in]   table_id    Table Id
// @param[in]   match_key   The PI Match key data to be translated
// @param[out]  key         The NP4 Key object that will be set
// @return      Function returns a P4DEV status code
//
pi_status_t AddKey(const pi_p4info_t *info, pi_p4_id_t table_id, 
                   const pi_match_key_t *match_key, ::np4::Key& key) {

    const char *data = reinterpret_cast<const char *>(match_key->data);

    size_t matchFieldsSize = pi_p4info_table_num_match_fields(info, table_id);
    for (size_t i = 0; i < matchFieldsSize; i++) {

        const pi_p4info_match_field_info_t *fieldInfo = 
            pi_p4info_table_match_field_info(info, table_id, i);
        uint32_t prefixLen;
        const char *keyName = 
            pi_p4info_table_match_field_name_from_id(info, table_id, 
                                                     fieldInfo->mf_id);

        switch (fieldInfo->match_type) {
        case PI_P4INFO_MATCH_TYPE_VALID: {
            // Create value vector
            std::vector<uint8_t> keyValue;
            const char *keyData = data;
            data += Bitfield2Vector(data, fieldInfo->bitwidth, keyValue);

            try {
                // Create new key
                key.push_back(new ::np4::KeyElemValid(keyName, keyValue));
                Logger::get()->debug(
                    "adding Valid key {}, width {}, data {}, vector {}",
                    keyName, fieldInfo->bitwidth,
                    StringData(keyData, (data - keyData)),
                    StringValues(keyValue));

            } catch (::np4::Exception &e) {
                Logger::get()->error(
                    "error creating valid key {}, width {}, data {},"
                    " vector {}: {}",
                    keyName, fieldInfo->bitwidth,
                    StringData(keyData, (data - keyData)),
                    StringValues(keyValue), e.what());
                return pi_status_t(PI_STATUS_TARGET_ERROR + 
                                   P4DEV_KEY_NAME_ERROR);
            }
            break;
        }

        case PI_P4INFO_MATCH_TYPE_EXACT: {
            // Create value vector
            std::vector<uint8_t> keyValue;
            const char *keyData = data;
            data += Bitfield2Vector(data, fieldInfo->bitwidth, keyValue);

            try {
                // Create new key
                key.push_back(new ::np4::KeyElemExact(keyName, keyValue));
                Logger::get()->debug(
                    "adding Exact key {}, width {}, data {}, vector {}",
                    keyName, fieldInfo->bitwidth,
                    StringData(keyData, (data - keyData)),
                    StringValues(keyValue));

            } catch (::np4::Exception &e) {
                Logger::get()->error(
                    "error creating exact key {}, width {}, data {},"
                    " vector {}: {}",
                    keyName, fieldInfo->bitwidth,
                    StringData(keyData, (data - keyData)),
                    StringValues(keyValue), e.what());
                return pi_status_t(PI_STATUS_TARGET_ERROR + 
                                   P4DEV_KEY_NAME_ERROR);
            }
            break;
        }

        case PI_P4INFO_MATCH_TYPE_LPM: {
            // Create value vector
            std::vector<uint8_t> keyValue;
            const char *keyData = data;
            data += Bitfield2Vector(data, fieldInfo->bitwidth, keyValue);

            // Retrieve prefix length
            data += retrieve_uint32(data, &prefixLen);

            try {
                // Create new key
                key.push_back(new ::np4::KeyElemLPM(keyName, keyValue,
                                                       prefixLen));
                Logger::get()->debug(
                    "adding LPM key {}, width {}, data {}, vector {},"
                    " prefix len {}",
                    keyName, fieldInfo->bitwidth,
                    StringData(keyData, (data - keyData)),
                    StringValues(keyValue), prefixLen);

            } catch (::np4::Exception &e) {
                Logger::get()->error(
                    "error creating lpm key {}, width {}, data {},"
                    " vector {}, prefix len {}: {}",
                    keyName, fieldInfo->bitwidth,
                    StringData(keyData, (data - keyData)),
                    StringValues(keyValue), prefixLen, e.what());
                return pi_status_t(PI_STATUS_TARGET_ERROR + 
                                   P4DEV_KEY_NAME_ERROR);
            }
            break;
        }

        case PI_P4INFO_MATCH_TYPE_TERNARY: {
            // Create value vector
            std::vector<uint8_t> keyValue;
            const char *valueData = data;
            data += Bitfield2Vector(data, fieldInfo->bitwidth, keyValue);

            // Create mask vector
            std::vector<uint8_t> keyMask;
            const char *maskData = data;
            data += Bitfield2Vector(data, fieldInfo->bitwidth, keyMask);

            try {
                // Create new key
                // - TODO: priority support
                key.push_back(new ::np4::KeyElemTernary(keyName, keyValue, 
                                                        keyMask));
                //*requires_priority = true;
                Logger::get()->debug(
                    "adding Ternary key {}, bit width {}, value_data {}, "
                    "value_vector {}, mask_data {}, mask_vector {}",
                    keyName, fieldInfo->bitwidth,
                    StringData(valueData, (maskData - valueData)),
                    StringValues(keyValue),
                    StringData(maskData, (data - maskData)),
                    StringValues(keyMask));

            } catch (::np4::Exception &e) {
                Logger::get()->error(
                    "error creating ternary key {}, bit width {}, "
                    "value_data {}, value_vector {}, mask_data {}, "
                    "mask_vector {}: {}", keyName, fieldInfo->bitwidth,
                    StringData(valueData, (maskData - valueData)),
                    StringValues(keyValue),
                    StringData(maskData, (data - maskData)),
                    StringValues(keyMask), e.what());

                return pi_status_t(PI_STATUS_TARGET_ERROR + 
                                   P4DEV_KEY_NAME_ERROR);
            }
            break;
        }

        case PI_P4INFO_MATCH_TYPE_RANGE:
            Logger::get()->error(
                "error range key not implemented {}", keyName);
            return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_NOT_IMPLEMENTED);

        default:
            Logger::get()->error("error match type not supported {}", keyName);
            return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_NOT_IMPLEMENTED);
        }
    }

    return PI_STATUS_SUCCESS;
}

// @brief Helper function to add action data to the ::np4::Key object
//
// @param[in]   info        P4 Info
// @param[in]   action_data Action data to be used
// @param[out]  action      The NP4 Action object that will be set
// @return      Function returns a P4DEV status code
//
pi_status_t AddAction(const pi_p4info_t *info, 
                      const pi_action_data_t *action_data,
                      ::np4::Action& action) {

    pi_p4_id_t actionID = action_data->action_id;
    const char *data = action_data->data;
    const char *actionName = pi_p4info_action_name_from_id(info, actionID);
    if (actionName == nullptr) {
        Logger::get()->error("can't find action name from id {}", actionID);
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ACTION_NAME_ERROR);
    }
    Logger::get()->debug("AddAction {}({})", actionName, actionID);

    // Now add params
    std::vector<::np4::Parameter> params;
    size_t paramIdsSize;
    const pi_p4_id_t *paramIds = 
        pi_p4info_action_get_params(info, actionID, &paramIdsSize);

    for (size_t i = 0; i < paramIdsSize; i++) {

        size_t bitwidth = 
            pi_p4info_action_param_bitwidth(info, actionID, paramIds[i]);

        // Get param name
        const char *paramName = 
            pi_p4info_action_param_name_from_id(info, actionID, paramIds[i]);
        if (paramName == nullptr) {
            Logger::get()->error("Can't find param name for id {}",
                                 paramIds[i]);
            return pi_status_t(PI_STATUS_TARGET_ERROR + 
                               P4DEV_PARAMETER_NAME_ERROR);
        }

        // Create param value vector
        std::vector<uint8_t> value;
        const char *actionData = data;
        data += Bitfield2Vector(data, bitwidth, value);

        // Add to parameter list
        params.push_back(::np4::Parameter(paramName, value));

        Logger::get()->debug("add action parameter {}, bit width {}, data {},"
            " vector {}", paramName, bitwidth,
            StringData(actionData, (data - actionData)),
            StringValues(value));
    }

    try {
        // Add Action to rule
        action = ::np4::Action(actionName, params);

    } catch (::np4::Exception &e) {
        Logger::get()->error(
            "error adding action {}: {}", actionName, e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ACTION_NAME_ERROR);
    }

    return PI_STATUS_SUCCESS;
}

using ActionMap = std::unordered_map<std::string, ActionProperties>;

// @brief Helper function to compute the action data sizes
//
// @param[in]   info        P4 Info
// @param[in]   actionIds   List of Action Ids
// @param[out]  actionCount Number of actions
// @return      Function returns a map of ActionProperties indexed by name
//
ActionMap ComputeActionSizes(const pi_p4info_t* info,
                             const pi_p4_id_t* actionIds,
                             size_t actionCount) {

    ActionMap result;
    result.reserve(actionCount);

    for (size_t i = 0; i < actionCount; i++) {
        result.emplace(
            std::string(pi_p4info_action_name_from_id(info, actionIds[i])),
            ActionProperties(pi_p4info_action_data_size(info, actionIds[i]), 
                             actionIds[i])
        );
    }

    return result;
}


// @brief Helper function to calculate a rule's key size
//
// @param[in]   key         Rule key
// @return      Function returns the size required for the key
//
size_t CalcKeyDataSize(::np4::Key& key) {

    // Check the table key size because of bug in NP4 API
    // where all keys must be same match type.
    size_t dataSize = 0;
    for (auto keyElem : key) {
        switch (keyElem->type) {
        case ::np4::info::KeyElemType::Ternary: {
            ::np4::KeyElemTernary* keyElemTernary = 
                reinterpret_cast<::np4::KeyElemTernary*>(keyElem);
            dataSize += keyElemTernary->value.size();
            dataSize += keyElemTernary->mask.size();
            break;
        }

        case ::np4::info::KeyElemType::LPM: {
            auto keyElemLPM = reinterpret_cast<::np4::KeyElemLPM *>(keyElem);
            dataSize += keyElemLPM->value.size() + sizeof(uint32_t);
            break;
        }

        case ::np4::info::KeyElemType::Exact: {
            dataSize += keyElem->value.size();
            break;
        }

        case ::np4::info::KeyElemType::Valid: {
            dataSize += keyElem->value.size();
            break;
        }

        case ::np4::info::KeyElemType::Unknown:
            Logger::get()->error("Key type Unknown not handled");
            break;
        }
    }

    Logger::get()->debug("CalcKeyDataSize needs {} bytes", dataSize);

    return dataSize;
}


// @brief Helper function to calculate a rule's data size
//
// @param[in]   info        P4 Info
// @param[in]   table_id    Table Id
// @param[in]   table       NP4 Table reference
// @param[in]   ruleIndex   Zero if we want them all, else the rule index
// @param[in]   actionMap   Action map holding action data sizes
// @param[out]  res         calculated results go here
// @return      Function returns the data size required for a rule
//
size_t CalcRuleDataSize(::np4::Table& table,
                        size_t ruleIndex,
                        ActionMap& actionMap,
                        pi_table_fetch_res_t *res) {

    size_t dataSize = 0;

    try {
        ::np4::Rule rule;
        rule = table.getRule(ruleIndex);

        // Check the table key size because of bug in NP4 API
        // where all keys must be same match type.
        size_t keySize = CalcKeyDataSize(rule.key);
        if (keySize != res->mkey_nbytes) {
            Logger::get()->error("{} API Key size {} != P4 Info size {}",
                                 table.getInfo().name,
                                 keySize, res->mkey_nbytes);
            return 0;
        }

        dataSize += actionMap.at(rule.action.name).size;
        dataSize += sizeof(s_pi_p4_id_t); // Action ID
        dataSize += sizeof(uint32_t); // Action params bytewidth

    // Table is sparsely populated so should be ok
    } catch (::np4::Exception &e) {
        Logger::get()->debug("table {}: rule fetch failed on index {}: {}",
            table.getInfo().name, ruleIndex, e.what());
    }

    Logger::get()->debug("CalcRuleDataSize needs {} bytes", dataSize);

    return dataSize;
}

// @brief Helper function to calculate a table's data size
//
// @param[in]   info        P4 Info
// @param[in]   table_id    Table Id
// @param[in]   table       NP4 Table reference
// @param[in]   ruleIndex   Zero if we want them all, else the rule index
// @param[in]   actionMap   Action map holding action data sizes
// @param[out]  res         calculated results go here
// @return      Function returns the data size required for a rule
//
size_t CalcTableDataSize(const pi_p4info_t *info,
                        pi_p4_id_t table_id,
                        ::np4::Table& table,
                        size_t ruleIndex,
                        ActionMap& actionMap,
                        pi_table_fetch_res_t *res) {

    // Just make sure we've got something over 0
    if (res->num_entries == 0) return 0;

    // Calculate space needed
    size_t dataSize = 0U;
    res->p4info = info;
    res->num_direct_resources = res->num_entries;

    dataSize += res->num_entries * sizeof(s_pi_entry_handle_t);
    dataSize += res->num_entries * sizeof(s_pi_action_entry_type_t);
    dataSize += res->num_entries * sizeof(uint32_t);  // for priority
    dataSize += res->num_entries * sizeof(uint32_t);  // for properties
    dataSize += res->num_entries * sizeof(uint32_t);  // for dir resources

    res->mkey_nbytes = pi_p4info_table_match_key_size(info, table_id);
    dataSize += res->num_entries * res->mkey_nbytes;

    size_t num_actions;
    auto actionIds = pi_p4info_table_get_actions(info, table_id, &num_actions);
    actionMap = ComputeActionSizes(info, actionIds, num_actions);

    // Spin thru whole table
    size_t got = 0;
    if (res->num_entries > 1) {
        size_t maxRuleIndex = table.getCapacity();
        for (ruleIndex = 0; ruleIndex < maxRuleIndex; ruleIndex++) {
            size_t rc = CalcRuleDataSize(table, ruleIndex, actionMap, res);

            // Increment the size
            dataSize += rc;

            // Check to see if it was successful
            if (rc != 0 && ++got == res->num_entries) break;
        }

    // Only get size of one entry
    } else {
        size_t rc = CalcRuleDataSize(table, ruleIndex, actionMap, res);

        // Check to see if it was successful
        if (rc != 0) got++;
        dataSize += rc;
    }

    // Did we get enough entries
    if (got < res->num_entries) {
        Logger::get()->warn(
                    "Table Id {}: fetch rules has only found {} of {} rules",
                    table_id, got, res->num_entries);
    }

    Logger::get()->debug("CalcTableDataSize needs {} bytes", dataSize);

    return dataSize;
}

// @brief Helper function to copy actions into the data pointer
//
// @param[in]   info        P4 Info
// @param[in]   data        A pointer to the data space we're copying into
// @param[in]   actionId    The action id of the action
// @param[in]   params      The parameters of the action
// @return      Function returns the incremented data pointer
//
size_t CopyActionData(const pi_p4info_t *info,
                      char *data,
                      pi_p4_id_t actionId,
                      const ::np4::Action& action) {

    char *start = data;

    // Create parameter map for this action
    auto pmap = CreateParamMap(info, actionId, action.parameters);

    size_t paramCount;
    const pi_p4_id_t *paramIds =
        pi_p4info_action_get_params(info, actionId, &paramCount);

    for (size_t i=0; i < paramCount; i++) {

        // Grab param object
        auto id = paramIds[i];
        auto it = pmap.find(id);
        if (it == pmap.end()) {
            Logger::get()->error("CopyActionData can't find param for id {}",
                                 id);
            return 0;
        }
        const ::np4::Parameter& param = it->second;

        // Calc widths of param
        size_t bitwidth =
            pi_p4info_action_param_bitwidth(info, actionId, id);


        const char *valueData = data;
        data += Vector2Bitfield(param.value, data, bitwidth);
        Logger::get()->debug("CopyActionParam {} bit width {}, data {}, "
            "vector {}", param.name, bitwidth,
            StringData(valueData, (data - valueData)),
            StringValues(param.value));
    }
    Logger::get()->debug("CopyActionData {} data size {}",
        action.name, (data - start));

    return (data - start);
}

// @brief Copy in the key data to the given data pointer
//
// @param[in]   data        pointer to the destination space
// @param[in]   table_id    The table id
// @param[in]   key         The key we need to copy
// @return      Function returns the incremented data pointer
//
size_t CopyKeyData(const pi_p4info_t *info, char *data,
                  const pi_p4_id_t table_id, const ::np4::Key& key) {

    char* start = data;
    Logger::get()->trace("CopyKeyData key size {}", key.size());

    // Create parameter map for this action
    auto kmap = CreateKeyMap(info, table_id, key);

    size_t keyCount;
    const pi_p4_id_t *keyIds =
        pi_p4info_table_get_match_fields(info, table_id, &keyCount);

    for (size_t i=0; i < keyCount; i++) {

        // Grab key element object
        auto id = keyIds[i];
        auto it = kmap.find(id);
        if (it == kmap.end()) {
            Logger::get()->error("CopyKeyData can't find key with id {}",
                                 id);
            return 0;
        }
        const ::np4::KeyElem* keyElem = it->second;
        size_t bitwidth = pi_p4info_table_match_field_bitwidth(
            info, table_id, id);

        switch (keyElem->type) {
        case ::np4::info::KeyElemType::Valid: {

            // copy in the value
            char *valueData = data;
            data += Vector2Bitfield(keyElem->value, data, bitwidth);

            Logger::get()->debug("copying Valid key {} bit width {}, "
                "value_data {}, value_vector {}", keyElem->name, bitwidth, 
                StringData(valueData, (data - valueData)),
                StringValues(keyElem->value));
            break;
        }

        case ::np4::info::KeyElemType::Exact: {

            // copy in the value
            char *valueData = data;
            data += Vector2Bitfield(keyElem->value, data, bitwidth);

            Logger::get()->debug("copying Exact key {} bit width {}, "
                "value_data {}, value_vector {}", keyElem->name, bitwidth, 
                StringData(valueData, (data - valueData)),
                StringValues(keyElem->value));
            break;
        }

        case ::np4::info::KeyElemType::LPM: {
            auto keyElemLPM = 
                reinterpret_cast<const ::np4::KeyElemLPM *>(keyElem);

            // copy in the value
            const char *valueData = data;
            data += Vector2Bitfield(keyElemLPM->value, data, bitwidth);

            // copy in the prefix length
            data += emit_uint32((char *)data, keyElemLPM->prefixLength);

            Logger::get()->debug("copying LPM key {} bit width {}, "
                "value_data {}, value_vector {}, prefix len {}",
                keyElem->name, bitwidth, 
                StringData(valueData, (data - valueData)),
                StringValues(keyElem->value), keyElemLPM->prefixLength);
            break;
        }

        case ::np4::info::KeyElemType::Ternary: {
            auto keyElemTernary = 
                reinterpret_cast<const ::np4::KeyElemTernary*>(keyElem);

            // copy in the value & mask
            const char *valueData = data;
            data += Vector2Bitfield(keyElemTernary->value, data, bitwidth);
            const char *maskData = data;
            data += Vector2Bitfield(keyElemTernary->mask, data, bitwidth);

            Logger::get()->debug("copying Ternary key {} bit width {}, "
                "value_data {}, value_vector {}, mask_data {}, "
                "mask_value {}", keyElem->name, bitwidth, 
                StringData(valueData, (maskData - valueData)),
                StringValues(keyElem->value), 
                StringData(maskData, (data - maskData)), 
                StringValues(keyElemTernary->mask)); 
            break;
        }

        case ::np4::info::KeyElemType::Unknown:
            Logger::get()->error("Key type Unknown not handled");
            return 0;
        }
    }
    Logger::get()->debug("CopyKeyData data size {}", (data - start));

    return (data - start);
}

// @brief Determine if priority is required
//
// @param[in]   info        P4 Info
// @param[in]   table_id    The table id
// @param[in]   key         The key we need to copy
// @return      Function returns true or false
//
bool NeedsPriority(const pi_p4info_t *info,
                   const pi_p4_id_t table_id,
                   const ::np4::Key& key) {

    size_t keyCount;
    const pi_p4_id_t *keyIds =
        pi_p4info_table_get_match_fields(info, table_id, &keyCount);

    // Spin thru all the keys
    for (size_t i=0; i < keyCount; i++) {

        // Grab key info
        auto mf_id = keyIds[i];
        auto kinfo = pi_p4info_table_match_field_info(info, table_id, i);
        assert(mf_id == kinfo->mf_id);

        switch (kinfo->match_type) {

        // Don't need priority for these match types
        case PI_P4INFO_MATCH_TYPE_EXACT:
        case PI_P4INFO_MATCH_TYPE_END:
        case PI_P4INFO_MATCH_TYPE_VALID:
            break;

        // Need priority for these match types
        case PI_P4INFO_MATCH_TYPE_LPM:
        case PI_P4INFO_MATCH_TYPE_TERNARY:
        case PI_P4INFO_MATCH_TYPE_RANGE:
            return(true);
        }
    }

    // Didn't find any keys that need a priority
    return(false);
}

// @brief Copy in the rule to the given data pointer
//
// @param[in]   info        P4 Info
// @param[in]   table       Table reference
// @param[in]   data        pointer to the destination space
// @param[in]   ruleIndex   index of the rule to copy
// @param[in]   actionMap   Action map that gives us the sizes of actions
// @param[in]   rule        The rule we need to copy
// @return      Function returns the incremented data pointer
//
size_t CopyRuleData(const pi_p4info_t *info,
                   pi_p4_id_t table_id,
                   ::np4::Table& table,
                   char *data,
                   size_t ruleIndex,
                   ActionMap& actionMap) {


    char *start = data;

    // Grab the rule
    ::np4::Rule rule;
    try {
        rule = table.getRule(ruleIndex);

    // Table is sparsely populated so should be ok
    } catch (::np4::Exception &e) {
        Logger::get()->debug("rule fetch failed on index {}: {}",
                                 ruleIndex, e.what());
        return 0;
    }

    // Entry rule number
    pi_entry_handle_t handle(ruleIndex);
    data += emit_entry_handle(data, handle);

    // We don't have priority support in Netcope SDK yet so if
    // needed we'll just set the default that was used on insert
    if (NeedsPriority(info, table_id, rule.key)) {
        data += emit_uint32(data, DEFAULT_PRIORITY);
    } else {
        data += emit_uint32(data, 0);
    }

    // Copy the key data
    data += CopyKeyData(info, data, table_id, rule.key);

    // Our actions are always direct
    data += emit_action_entry_type(data, PI_ACTION_ENTRY_TYPE_DATA);
    auto actionProperties = actionMap.at(rule.action.name);

    data += emit_p4_id(data, actionProperties.id);
    data += emit_uint32(data, actionProperties.size);
    data += CopyActionData(info, data, actionProperties.id, rule.action);
    data += emit_uint32(data, 0);  // properties
    data += emit_uint32(data, 0);  // TODO(antonin): direct resources

    Logger::get()->debug("CopyRuleData data size {}", (data - start));

    return (data - start);
}

//---------------- Tables class ---------------------------
//  This class implements the pi_tables functions in C++, providing
//  wrappers needed to make calls to the NP4 ATOM C++ API.

pi_status_t Tables::EntryAdd(pi_dev_id_t dev_id, pi_p4_id_t table_id,
                             const pi_match_key_t *match_key,
                             const pi_table_entry_t *table_entry,
                             int overwrite,
                             pi_entry_handle_t *entry_handle) {

    (void)(overwrite);

    pi_status_t status;

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id);
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: EntryAdd table {}", dev_id, tableName);

    // Now create the rule
    ::np4::Rule rule;

    // Add Key
    status = AddKey(info, table_id, match_key, rule.key);
    if (status != PI_STATUS_SUCCESS) return status;

    // Add Action
    status = AddAction(info, table_entry->entry.action_data, rule.action); 
    if (status != PI_STATUS_SUCCESS) return status;

    // Now insert the rule
    size_t ruleIndex = 0;

    // Insert rule
    try {

        // Do we write to all ATOMs
        if (dev->syncAtoms()) {
            Logger::get()->debug("insert table {}: sync {} atoms",
                                 tableName,
                                 dev->GetP4Device()->getAtomCount());
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                Logger::get()->debug("insert for atom {}", i);
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                auto idx = table.insertRule(rule);
                if (i == 0) {
                    ruleIndex = idx;
                } else {
                    // If we're out of sync we'll just report it for now
                    if (idx != ruleIndex) {
                        Logger::get()->error("Dev {}: {} ATOM rule indexes out"
                                             " of sync", dev_id, tableName);
                    }
                }
            }

        // Or just one ATOM
        } else {
            Logger::get()->debug("insert table {}:", tableName);
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            ruleIndex = table.insertRule(rule);
        }

    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} insert rule failed: {}",
                             dev_id, tableName, e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    Logger::get()->debug("rule inserted at index {}", ruleIndex);
    pi_entry_handle_t handle(ruleIndex);
    *entry_handle = handle;

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::DefaultActionSet(pi_dev_id_t dev_id, 
                                     pi_p4_id_t table_id,
                                     const pi_table_entry_t *table_entry) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id);
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: DefaultActionSet table {}",
                         dev_id, tableName);

    // Now create the rule
    ::np4::Action action;

    // Add Action
    pi_status_t status;
    if ((status = AddAction(info, table_entry->entry.action_data, action)) 
                                                        != PI_STATUS_SUCCESS) {
        return pi_status_t(PI_STATUS_TARGET_ERROR + status);
    }

    // Now set the default action
    try {

        // Do we write to all ATOMs
        if (dev->syncAtoms()) {
            Logger::get()->debug("set default action for  {}: sync {} atoms",
                                 tableName,
                                 dev->GetP4Device()->getAtomCount());
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                Logger::get()->debug("set default action for atom {}", i);
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                table.setDefaultAction(action);
            }

        // Or just one ATOM
        } else {
            Logger::get()->debug("set default action for {}", tableName);
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            table.setDefaultAction(action);
        }

    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} set default action failed: {}",
                             dev_id, tableName, e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::DefaultActionReset(pi_dev_id_t dev_id, 
                                       pi_p4_id_t table_id) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: DefaultActionReset table {}",
                         dev_id, tableName);

    // Now reset the default action
    try {

        // Do we write to all ATOMs
        if (dev->syncAtoms()) {
            Logger::get()->debug("clear default action for  {}: sync {} atoms",
                                 tableName,
                                 dev->GetP4Device()->getAtomCount());
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                Logger::get()->debug("clear default action for atom {}", i);
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                table.clearDefaultAction();
            }

        // Or just one ATOM
        } else {
            Logger::get()->debug("clear default action for {}", tableName);
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            table.clearDefaultAction();
        }

    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} clear default action failed: {}",
                             dev_id, tableName, e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::DefaultActionGet(pi_dev_id_t dev_id, 
                                     pi_p4_id_t table_id,
                                     pi_table_entry_t *table_entry) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: DefaultActionGet table {}",
                         dev_id, tableName);

    // Now get the default action
    ::np4::Action action;

    try {
        // Now get the table
        // - note: if we're in all_atoms mode then we'll just
        //         get the first ATOM (assuming they're all in sync).
        ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
        action = table.getDefaultAction();

    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} get default action failed: {}",
                             dev_id, tableName, e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // Allocate space for the action data
    const pi_p4_id_t actionId = 
        pi_p4info_action_id_from_name(info, action.name.c_str());
    if (actionId == PI_INVALID_ID) {
        Logger::get()->error("Dev {}: {} invalid action name {}",
                             dev_id, tableName, action.name);
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    const size_t actionDataSize = pi_p4info_action_data_size(info, actionId);

    table_entry->entry_type = PI_ACTION_ENTRY_TYPE_DATA;

    char *data = new char[sizeof(pi_action_data_t) + actionDataSize];
    if (data == NULL) {
        Logger::get()->error("Dev {}: default action data alloc failed",
                             dev_id);
        return PI_STATUS_ALLOC_ERROR;
    }
    char *start = data;
    char *end = data + sizeof(pi_action_data_t) + actionDataSize;
    pi_action_data_t *actionData = (pi_action_data_t *)(data);
    data += sizeof(pi_action_data_t);

    actionData->p4info = info;
    actionData->action_id = actionId;
    actionData->data_size = actionDataSize;
    actionData->data = data;

    table_entry->entry.action_data = actionData;

    data += CopyActionData(info, data, actionId, action);

    // Check we haven't overrun our data buffer
    assert(end >= data);
    Logger::get()->debug("DefaultActionGet data size {}", (data - start));

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::DefaultActionGetHandle(pi_dev_id_t dev_id,
                                           pi_p4_id_t table_id,
                                           pi_entry_handle_t *entry_handle) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id);
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: DefaultActionGetHandle table {}",
                         dev_id, tableName);

    // Get the max table capacity and return as the
    // default action rule index
    size_t ruleIndex = 0;
    try {
        ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
        ruleIndex = table.getCapacity();

    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} get capacity failed: {}",
                             dev_id, tableName, e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    Logger::get()->debug("Dev {}: Table {}: default action ruleIndex is: {}",
                         dev_id, tableName, ruleIndex);

    pi_entry_handle_t handle(ruleIndex);
    *entry_handle = handle;

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::EntryDelete(pi_dev_id_t dev_id,
                                pi_p4_id_t table_id,
                                const size_t ruleIndex) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: EntryDelete table {} index {}",
                         dev_id, tableName, ruleIndex);

    // Now delete rule
    try {

        // Do we delete rule in all ATOMs
        if (dev->syncAtoms()) {
            Logger::get()->debug("delete rule for {}[{}]: sync {} atoms",
                                 tableName, ruleIndex,
                                 dev->GetP4Device()->getAtomCount());
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                Logger::get()->debug("delete rule for atom {}", i);
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                table.deleteRule(ruleIndex);
            }

        // Or just one ATOM
        } else {
            Logger::get()->debug("delete rule for {}[{}]", 
                                 tableName, ruleIndex);
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            table.deleteRule(ruleIndex);
        }

    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} delete rule {} failed: {}",
                             dev_id, tableName, ruleIndex, e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::EntryDeleteWKey(pi_dev_id_t dev_id,
                                    pi_p4_id_t table_id,
                                    const pi_match_key_t *match_key) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id);
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: EntryDeleteWKey table {}",
                         dev_id, tableName);

    // Add Key
    ::np4::Key key;
    pi_status_t status;
    if ((status = AddKey(info, table_id, match_key, key)) 
                                                != PI_STATUS_SUCCESS) {
        return pi_status_t(PI_STATUS_TARGET_ERROR + status);
    }

    // Try and find the rule index
    size_t ruleIndex;
    try {
        ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
        ruleIndex = table.findRuleIndex(key);

    // Couldn't find entry
    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} can't find rule: {}",
                             dev_id, tableName, e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_KEY_NAME_ERROR);
    }

    // Now delete rule
    return EntryDelete(dev_id, table_id, ruleIndex);
}

pi_status_t Tables::EntryModify(pi_dev_id_t dev_id, 
                                pi_p4_id_t table_id,
                                const size_t ruleIndex,
                                const pi_table_entry_t *table_entry) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                              dev_id, table_id);
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: EntryModify table {}, index {}",
                         dev_id, tableName, ruleIndex);

    // Create an actio
    ::np4::Action action;

    // Add Action
    pi_status_t status;
    if ((status = AddAction(info, table_entry->entry.action_data, action)) 
                                                    != PI_STATUS_SUCCESS) {
        return pi_status_t(PI_STATUS_TARGET_ERROR + status);
    }

    // Now modify rule
    try {

        // Do we write to all ATOMs
        if (dev->syncAtoms()) {
            Logger::get()->debug("modify rule for {}[{}]: sync {} atoms",
                                 tableName, ruleIndex,
                                 dev->GetP4Device()->getAtomCount());
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                Logger::get()->debug("modify rule for atom {}", i);
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                table.modifyRule(ruleIndex, action);
            }

        // Or just one ATOM
        } else {
            Logger::get()->debug("modify rule for {}[{}]",
                                 tableName, ruleIndex);
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            table.modifyRule(ruleIndex, action);
        }

    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} modify rule failed: {}",
                             dev_id, tableName, e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ERROR);
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::EntryModifyWKey(pi_dev_id_t dev_id, 
                                    pi_p4_id_t table_id,
                                    const pi_match_key_t *match_key,
                                    const pi_table_entry_t *table_entry) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id);
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: EntryModifyWKey table {}",
                         dev_id, tableName);

    // Add Key
    ::np4::Key key;
    pi_status_t status;
    if ((status = AddKey(info, table_id, match_key, key)) 
                                            != PI_STATUS_SUCCESS) {
        return pi_status_t(PI_STATUS_TARGET_ERROR + status);
    }

    // Try and find the rule index
    size_t ruleIndex;
    try {
        ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
        ruleIndex = table.findRuleIndex(key);

    // Couldn't find entry
    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} can't find rule: {}",
                             dev_id, tableName, e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_KEY_NAME_ERROR);
    }

    return EntryModify(dev_id, table_id, ruleIndex, table_entry);
}

pi_status_t Tables::EntryFetch(pi_dev_id_t dev_id,
                               pi_p4_id_t table_id,
                               pi_table_fetch_res_t *res) {

    // Need to init these now
    res->entries_size = 0;
    res->entries = nullptr;

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: EntryFetch table {}",
                         dev_id, tableName);

    // Grab the table reference and table size and capacity
    ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
    size_t tableSize = 0;
    size_t maxRuleIndex = 0;
    try {
        tableSize = table.getSize();
        maxRuleIndex = table.getCapacity();

    // Couldn't find entry
    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} can't get table: {}",
                             dev_id, tableName, e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_KEY_NAME_ERROR);
    }
    Logger::get()->info("Dev {}: EntryFetch size: {}, capacity: {}",
                        dev_id, tableSize,  maxRuleIndex);

    // fetch all the entries
    res->num_entries = tableSize;

    // return if got no entries
    if (tableSize == 0) {
        Logger::get()->info("Dev {}: no entries, returning now", dev_id);
        return PI_STATUS_SUCCESS;
    }

    // Calculate space needed
    ActionMap actionMap;
    size_t dataSize = CalcTableDataSize(info, table_id, table, 0,
                                        actionMap, res);
    if (dataSize == 0) {
        Logger::get()->error("Dev {}: Calc of rule size failed", dev_id);
        return PI_STATUS_ALLOC_ERROR;
    }

    // Now allocate the space
    char *data = new char[dataSize];
    if (data == NULL) {
        Logger::get()->error("Dev {}: alloc of fetch space failed", dev_id);
        return PI_STATUS_ALLOC_ERROR;
    }
    char *start = data;
    char *end = data + dataSize;

    // in some cases, we do not use the whole buffer
    std::fill(data, data + dataSize, 0);
    res->entries_size = dataSize;
    res->entries = data;

    // Dump each rule into the reserved table data
    size_t got = 0;
    for (size_t ruleIndex = 0; ruleIndex < maxRuleIndex; ruleIndex++) {

        // Copy in the rule data
        size_t rule_sz = CopyRuleData(info, table_id, table, data,
                                      ruleIndex, actionMap);

        // zero increase means rule get failed, which is ok because
        // it's a sparse table.
        if (rule_sz != 0) {

            // increment data ptr
            data += rule_sz;

            // Have we got enough
            if (++got == tableSize) break;
        }
    }
    // Check to make sure we got tableSize entries
    if (got < res->num_entries) {
        Logger::get()->warn("Dev {}: only found {} out of {} entries",
                            dev_id, got, res->num_entries);
    }

    Logger::get()->debug("EntryFetch data used/alloc {}/{}",
                         (data - start), (end - start));

    // Just make sure we didn't go over the end of the allocated data
    assert(end >= data);

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::EntryFetchOne(pi_dev_id_t dev_id,
                                  pi_p4_id_t table_id,
                                  size_t ruleIndex,
                                  pi_table_fetch_res_t *res) {

    // Need to init these now
    res->entries_size = 0;
    res->entries = nullptr;

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: EntryFetchOne table {}",
                         dev_id, tableName);

    // Grab the table reference and table size and capacity
    ::np4::Table& table = dev->GetP4Atom().getTable(tableName);

    res->num_entries = 1;

    // Calculate space needed
    ActionMap actionMap;
    size_t dataSize = CalcTableDataSize(info, table_id, table, ruleIndex,
                                        actionMap, res);
    if (dataSize == 0) {
        Logger::get()->error("Dev {}: {} calc of rule size failed",
                             dev_id, tableName);
        return PI_STATUS_ALLOC_ERROR;
    }

    // Now allocate the space
    char *data = new char[dataSize];
    if (data == NULL) {
        Logger::get()->error("Dev {}: {} alloc of fetch space failed",
                             dev_id, tableName);
        return PI_STATUS_ALLOC_ERROR;
    }

    // in some cases, we do not use the whole buffer
    std::fill(data, data + dataSize, 0);
    res->entries_size = dataSize;
    res->entries = data;

    // Copy in the rule data
    size_t rule_sz = CopyRuleData(info, table_id, table, data,
                                  ruleIndex, actionMap);
    // get of this rule index failed
    if (rule_sz == 0) {
        Logger::get()->error("Dev {}: failed to get rule {}",
                             dev_id, ruleIndex);
        return PI_STATUS_OUT_OF_BOUND_IDX;
    }
    data += rule_sz;

    Logger::get()->debug("EntryFetchOne data used/alloc {}/{}",
                         rule_sz, dataSize);

    // Just make sure we didn't go over the end of the allocated data
    assert(rule_sz < dataSize);

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::EntryFetchWKey(pi_dev_id_t dev_id,
                                   pi_p4_id_t table_id,
                                   const pi_match_key_t *match_key,
                                   pi_table_fetch_res_t *res) {

    // Need to init these now
    res->entries_size = 0;
    res->entries = nullptr;

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        Logger::get()->error("Dev {}: not allocated", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::get()->error("Dev {}: no P4 Info", dev_id);
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::get()->error("Dev {}: table id {} not found in P4 Info",
                             dev_id, table_id); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }
    Logger::get()->trace("Dev {}: EntryFetchWKey table {}",
                         dev_id, tableName);

    // Get Key
    ::np4::Key key;
    pi_status_t status;
    if ((status = AddKey(info, table_id, match_key, key)) 
                                            != PI_STATUS_SUCCESS) {
        return pi_status_t(PI_STATUS_TARGET_ERROR + status);
    }

    // Grab the table reference and table size and capacity
    ::np4::Table& table = dev->GetP4Atom().getTable(tableName);

    // Grab the rule index
    size_t ruleIndex = 0;
    try {
        ruleIndex = table.findRuleIndex(key);

    // Table is sparsely populated so should be ok
    } catch (::np4::Exception &e) {
        Logger::get()->error("Dev {}: {} rule fetch failed on key: {}",
                              dev_id, tableName, e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_KEY_NAME_ERROR);
    }

    // Calculate space needed (we only need one rule)
    ActionMap actionMap;
    size_t dataSize = CalcTableDataSize(info, table_id, table, ruleIndex,
                                        actionMap, res);

    // Now allocate the space
    char *data = new char[dataSize];
    if (data == NULL) {
        Logger::get()->error("Dev {}: {} alloc of fetch space failed",
                             dev_id, tableName);
        return PI_STATUS_ALLOC_ERROR;
    }

    // in some cases, we do not use the whole buffer
    std::fill(data, data + dataSize, 0);
    res->entries_size = dataSize;
    res->entries = data;

    // Copy in the rule data
    size_t rule_sz = CopyRuleData(info, table_id, table, data, 
                                  ruleIndex, actionMap);
    // get on this index failed
    if (rule_sz == 0) {
        Logger::get()->error("Dev {}: failed to get rule {}",
                             dev_id, ruleIndex);
        return PI_STATUS_OUT_OF_BOUND_IDX;
    }
    data += rule_sz;

    Logger::get()->debug("EntryFetchWKey data used/alloc {}/{}",
                         rule_sz, dataSize);

    // Just make sure we didn't go over the end of the allocated data
    assert(rule_sz < dataSize);

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::EntryFetchDone(pi_table_fetch_res_t *res) {

    // If we've allocated data free it up
    if (res->entries_size > 0 && res->entries) delete[] res->entries;

    return PI_STATUS_SUCCESS;
}

}   // np4
}   // pi
