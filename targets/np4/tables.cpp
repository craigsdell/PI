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
#include "logger.h"
#include "device_mgr.h"

namespace pi {
namespace np4 {

//---------------- Table Helper functions ---------------------------

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
		size_t bitwidth = fieldInfo->bitwidth;
		size_t bytewidth = (bitwidth + 7) / 8;
		uint32_t prefixLen;
		const char *keyName = 
            pi_p4info_table_match_field_name_from_id(info, table_id, 
                                                     fieldInfo->mf_id);

		switch (fieldInfo->match_type) {
		case PI_P4INFO_MATCH_TYPE_VALID:
            return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_NOT_IMPLEMENTED);
			break;

		case PI_P4INFO_MATCH_TYPE_EXACT: {
            // Create value vector
            std::vector<uint8_t> keyValue;
            for (size_t i=0; i < bytewidth; i++) {
                keyValue.push_back(*data++);
            }
			
            try {
                // Create new key
                key.push_back(new ::np4::KeyElemExact(keyName, keyValue));

            } catch (::np4::Exception &e) {
                Logger::error("error creating ternary key "
                                + std::string(keyName) + ": " +  e.what());
                return pi_status_t(PI_STATUS_TARGET_ERROR + 
                                   P4DEV_KEY_NAME_ERROR);
            }
			break;
        }

		case PI_P4INFO_MATCH_TYPE_LPM: {
            // Create value vector
            std::vector<uint8_t> keyValue;
            for (size_t i=0; i < bytewidth; i++) {
                keyValue.push_back(*data++);
            }

            // Retrieve prefix length
			data += retrieve_uint32(data, &prefixLen);
            // TODO: do we need this with the new ATOM library?
			//flipEndianness(value, bytewidth);

            try {
                // Create new key
                key.push_back(new ::np4::KeyElemLPM(keyName, keyValue,
                                                       prefixLen));

            } catch (::np4::Exception &e) {
                Logger::error("error creating ternary key "
                                + std::string(keyName) + ": " + e.what()); 
                return pi_status_t(PI_STATUS_TARGET_ERROR + 
                                   P4DEV_KEY_NAME_ERROR);
            }
			break;
        }

		case PI_P4INFO_MATCH_TYPE_TERNARY: {
            // Create value vector
            std::vector<uint8_t> keyValue;
            for (size_t i=0; i < bytewidth; i++) {
                keyValue.push_back(*data++);
            }

            // Create mask vector
            std::vector<uint8_t> keyMask;
            for (size_t i=0; i < bytewidth; i++) {
                keyMask.push_back(*data++);
            }

            try {
                // Create new key
                // - TODO: priority support
                key.push_back(new ::np4::KeyElemTernary(keyName, keyValue, 
                                                           keyMask));
			    //*requires_priority = true;

            } catch (::np4::Exception &e) {
                Logger::error("error creating ternary key "
                                + std::string(keyName) + ": " + e.what());
                return pi_status_t(PI_STATUS_TARGET_ERROR + 
                                   P4DEV_KEY_NAME_ERROR);
            }
			break;
        }

		case PI_P4INFO_MATCH_TYPE_RANGE:
            return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_NOT_IMPLEMENTED);
			break;

		default:
			assert(0);
			break;
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

	assert(info);
	assert(action_data);

	pi_p4_id_t actionID = action_data->action_id;
	const char *actionData = action_data->data;
	const char *actionName = pi_p4info_action_name_from_id(info, actionID);
    if (actionName == nullptr) {
        Logger::error("can't find action name from id "
                      + std::to_string(actionID));
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ACTION_NAME_ERROR);
    }

    // Now add params
    std::vector<::np4::Parameter> params;
	size_t paramIdsSize;
	const pi_p4_id_t *paramIds = 
        pi_p4info_action_get_params(info, actionID, &paramIdsSize);

	for (size_t i = 0; i < paramIdsSize; i++) {

		size_t paramBitwidth = 
            pi_p4info_action_param_bitwidth(info, actionID, paramIds[i]);
		size_t paramBytewidth = (paramBitwidth + 7) / 8;

        // Get param name
		const char *paramName = 
            pi_p4info_action_param_name_from_id(info, actionID, paramIds[i]);
        if (paramName == nullptr) {
            Logger::error("Can't find param name for id "
                              + std::to_string(paramIds[i]));
            return pi_status_t(PI_STATUS_TARGET_ERROR + 
                               P4DEV_PARAMETER_NAME_ERROR);
        }

        // Create param value vector
        std::vector<uint8_t> value;
        for (size_t i=0; i < paramBytewidth; i++) {
            value.push_back(*actionData++);
        }

        // Add to parameter list
        params.push_back(::np4::Parameter(paramName, value));
	}

    try {
        // Add Action to rule
        action = ::np4::Action(actionName, params);

    } catch (::np4::Exception &e) {
        Logger::error("error adding action " + std::string(actionName)
                        + ": " + e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_ACTION_NAME_ERROR);
    }

	return PI_STATUS_SUCCESS;
}

// @brief Helper function to retrieve a table entry
//
// @param[in]   info        P4 Info
// @param[in]   action      The NP4 Action we'll use as the source
// @param[out]  table_entry The table entry we'll set
// @return      Function returns a P4DEV status code
//
pi_status_t RetrieveEntry(const pi_p4info_t *info,
                          ::np4::Action& action,
                          pi_table_entry_t *table_entry) {

	const pi_p4_id_t actionId = 
        pi_p4info_action_id_from_name(info, action.name.c_str());
	const size_t actionDataSize = pi_p4info_action_data_size(info, actionId);

	table_entry->entry_type = PI_ACTION_ENTRY_TYPE_DATA;

	char *data_ = new char[sizeof(pi_action_data_t) + actionDataSize];
	if (data_ == NULL) return PI_STATUS_ALLOC_ERROR;
	pi_action_data_t *actionData = (pi_action_data_t *)(data_);
	data_ += sizeof(pi_action_data_t);

	actionData->p4info = info;
	actionData->action_id = actionId;
	actionData->data_size = actionDataSize;
	actionData->data = data_;

	table_entry->entry.action_data = actionData;

	size_t paramCount;
	const pi_p4_id_t *paramIds =
        pi_p4info_action_get_params(info, actionId, &paramCount);

    // retrieve the action parameters
	size_t i = 0;
    for (auto param : action.parameters) {
		size_t bitwidth =
            pi_p4info_action_param_bitwidth(info, actionId, paramIds[i]);
		size_t bytewidth = (bitwidth + 7) / 8;
		assert(bytewidth >= param.value.size());

		size_t offset = bytewidth - param.value.size();
		memset(data_, 0, offset);

        // Copy in the value
        for (auto byte : param.value) {
            data_[offset++] = byte;
        }
		data_ += bytewidth;
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

// @brief Helper function to calculate a rule's data size
//
// @param[in]   info        P4 Info
// @param[in]   table_id    Table Id
// @param[in]   table       NP4 Table reference
// @param[in]   maxRuleIndex Max cpacity of the table
// @param[in]   actionMap   Action map holding action data sizes
// @param[out]  res         calculated results go here
// @return      Function returns the data size required for a rule
//
size_t CalcRuleDataSize(const pi_p4info_t *info,
                        pi_p4_id_t table_id,
                        ::np4::Table& table,
                        size_t maxRuleIndex,
                        ActionMap& actionMap,
                        pi_table_fetch_res_t *res) {

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

    size_t ruleIndex = 0;
	for (uint32_t i = 0; i < res->num_entries; i++) {
        ::np4::Rule rule;
        try {
            rule = table.getRule(ruleIndex);

        // Table is sparsely populated so should be ok
        } catch (::np4::Exception &e) {
            Logger::debug("table id " + std::to_string(table_id)
                            + ": rule fetch failed on index "
                            + std::to_string(ruleIndex) + ": %s" + e.what());
        }

        dataSize += actionMap.at(rule.action.name).size;
        dataSize += sizeof(s_pi_p4_id_t); // Action ID
        dataSize += sizeof(uint32_t); // Action params bytewidth

        // increment rule index and check we don't go over capacity
        if (++ruleIndex == maxRuleIndex) {
            Logger::error("Table Id " + std::to_string(table_id) 
                            + ": fetch rules has only found "
                            + std::to_string(i) + " rules");
            break;
        }
    }

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
char *CopyActionData(const pi_p4info_t *info,
                     char *data,
                     pi_p4_id_t actionId,
                     const ::np4::Action& action) {

	size_t paramCount;
	const pi_p4_id_t *paramIds =
        pi_p4info_action_get_params(info, actionId, &paramCount);

	size_t i = 0;
    for (auto param : action.parameters) {

        // Calc widths of param
		size_t bitwidth = pi_p4info_action_param_bitwidth(info, actionId, 
                                                          paramIds[i]);
		size_t bytewidth = (bitwidth + 7) / 8;

		assert(bytewidth >= param.value.size());

		size_t offset = bytewidth - param.value.size();
		memset(data, 0, offset);
        for (auto byte : param.value) {
            *data++ = byte;
        }
	}

	return data;
}

// @brief Copy in the rule to the given data pointer
//
// @param[in]   info        P4 Info
// @param[in]   data        pointer to the destination space
// @param[in]   ruleIndex   index of the rule to copy
// @param[in]   actionMap   Action map that gives us the sizes of actions
// @param[in]   rule        The rule we need to copy
// @return      Function returns the incremented data pointer
//
char *CopyRuleData(const pi_p4info_t *info,
                   char *data,
                   size_t ruleIndex,
                   ActionMap& actionMap,
                   const ::np4::Rule& rule) {


        // Entry rule number
		data += emit_entry_handle(data, ruleIndex);

		// We don't have priority yet
		data += emit_uint32(data, 0); // priority

        // Go through each key element
        for (auto keyElem : rule.key) {

			switch (keyElem->type) {
			case ::np4::info::MatchEngineType::Ternary: {
                ::np4::KeyElemTernary* keyElemTernary = 
                    reinterpret_cast<::np4::KeyElemTernary*>(keyElem);

                // copy in the value
                for (auto byte : keyElemTernary->value) {
                    *data++ = byte;
                }
				data += keyElemTernary->value.size();

                // copy in the mask
                for (auto byte : keyElemTernary->mask) {
                    *data++ = byte;
                }
				data += keyElemTernary->mask.size();
				break;
            }

			case ::np4::info::MatchEngineType::LPM: {
                auto keyElemLPM = 
                    reinterpret_cast<::np4::KeyElemLPM *>(keyElem);

                // copy in the value
                for (auto byte : keyElemLPM->value) {
                    *data++ = byte;
                }
				data += keyElemLPM->value.size();

                // copy in the prefix length
				data += emit_uint32(data, keyElemLPM->prefixLength);
				break;
            }

			case ::np4::info::MatchEngineType::Exact: {
                // copy in the value
                for (auto byte : keyElem->value) {
                    *data++ = byte;
                }
				data += keyElem->value.size();
				break;
            }

            default:
                Logger::error("Key type " + std::to_string(keyElem->type)
                                + " not handled");
			}
		}

		// Our actions are always direct
		data += emit_action_entry_type(data, PI_ACTION_ENTRY_TYPE_DATA);
		auto actionProperties = actionMap.at(rule.action.name);

		data += emit_p4_id(data, actionProperties.id);
		data += emit_uint32(data, actionProperties.size);
		data = CopyActionData(info, data, actionProperties.id, rule.action);

		data += emit_uint32(data, 0);  // properties
		data += emit_uint32(data, 0);  // TODO(antonin): direct resources

    return data;
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
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Now create the rule
    ::np4::Rule rule;

    // Add Key
    status = AddKey(info, table_id, match_key, rule.key);
    if (status != PI_STATUS_SUCCESS) return status;

    // Add Action
    status = AddAction(info, table_entry->entry.action_data, rule.action); 
    if (status != PI_STATUS_SUCCESS) return status;

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id)
                        + ": table id " + std::to_string(table_id)
                        + " not found in P4 Info");
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // Now insert the rule
    size_t ruleIndex = 0;

    // Insert rule
    try {

        // Do we write to all ATOMs
        if (dev->syncAtoms()) {
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                auto idx = table.insertRule(rule);
                if (i == 0) {
                    ruleIndex = idx;
                } else {
                    // If we're out of sync we'll just report it for now
                    if (idx != ruleIndex) {
                        Logger::error("ATOM rule indexes out of sync");
                    }
                }
            }

        // Or just one ATOM
        } else {
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            ruleIndex = table.insertRule(rule);
        }

    } catch (::np4::Exception &e) {
        Logger::error("Dev " + std::to_string(dev_id)
                        + ": insert rule failed: " + e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    *entry_handle = ruleIndex;

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::DefaultActionSet(pi_dev_id_t dev_id, 
                                     pi_p4_id_t table_id,
                                     const pi_table_entry_t *table_entry) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Now create the rule
    ::np4::Action action;

    // Add Action
    pi_status_t status;
    if ((status = AddAction(info, table_entry->entry.action_data, action)) 
                                                        != PI_STATUS_SUCCESS) {
        return pi_status_t(PI_STATUS_TARGET_ERROR + status);
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) 
                        + ": table id " + std::to_string(table_id)
                        + " not found in P4 Info");
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // Now set the default action
    try {

        // Do we write to all ATOMs
        if (dev->syncAtoms()) {
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                table.setDefaultAction(action);
            }

        // Or just one ATOM
        } else {
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            table.setDefaultAction(action);
        }

    } catch (::np4::Exception &e) {
        Logger::error("Dev " + std::to_string(dev_id)
                        + ": set default action failed: " + e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t Tables::DefaultActionReset(pi_dev_id_t dev_id, 
                                       pi_p4_id_t table_id) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": table id "
                        + std::to_string(table_id) + " not found in P4 Info"); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // Now reset the default action
    try {

        // Do we write to all ATOMs
        if (dev->syncAtoms()) {
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                table.clearDefaultAction();
            }

        // Or just one ATOM
        } else {
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            table.clearDefaultAction();
        }

    } catch (::np4::Exception &e) {
        Logger::error("Dev " + std::to_string(dev_id)
                        + ": clear default action failed: " + e.what());
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
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": table id "
                        + std::to_string(table_id) + " not found in P4 Info"); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // Now get the default action
    ::np4::Action action;

    try {
        // Now get the table
        // - note: if we're in all_atoms mode then we'll just
        //         get the first ATOM (assuming they're all in sync).
        ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
        action = table.getDefaultAction();

    } catch (::np4::Exception &e) {
        Logger::error("Dev " + std::to_string(dev_id)
                        + ": get default action failed: " +  e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    return RetrieveEntry(info, action, table_entry);;
}

pi_status_t EntryDelete(pi_dev_id_t dev_id,
                        pi_p4_id_t table_id,
                        const size_t ruleIndex) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": table id "
                      + std::to_string(table_id) + " not found in P4 Info"); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // Now delete rule
    try {

        // Do we delete rule in all ATOMs
        if (dev->syncAtoms()) {
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                table.deleteRule(ruleIndex);
            }

        // Or just one ATOM
        } else {
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            table.deleteRule(ruleIndex);
        }

    } catch (::np4::Exception &e) {
        Logger::error("Dev " + std::to_string(dev_id) + ": delete rule "
                     + std::to_string(ruleIndex) + " failed: " + e.what());
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    return PI_STATUS_SUCCESS;
}

pi_status_t EntryDeleteWKey(pi_dev_id_t dev_id,
                            pi_p4_id_t table_id,
                            const pi_match_key_t *match_key) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": table id "
                      + std::to_string(table_id) + " not found in P4 Info");
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

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
        Logger::error("Dev " + std::to_string(dev_id)
                        + ": can't find rule: " + e.what());
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
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Create an actio
    ::np4::Action action;

    // Add Action
    pi_status_t status;
    if ((status = AddAction(info, table_entry->entry.action_data, action)) 
                                                    != PI_STATUS_SUCCESS) {
        return pi_status_t(PI_STATUS_TARGET_ERROR + status);
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": table id "
                        + std::to_string(table_id) + " not found in P4 Info");
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // Now modify rule
    try {

        // Do we write to all ATOMs
        if (dev->syncAtoms()) {
            for (size_t i=0; i < dev->GetP4Device()->getAtomCount(); i++) {
                ::np4::Table& table = dev->GetP4Atom(i).getTable(tableName);
                table.modifyRule(ruleIndex, action);
            }

        // Or just one ATOM
        } else {
            ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
            table.modifyRule(ruleIndex, action);
        }

    } catch (::np4::Exception &e) {
        Logger::error("Dev " + std::to_string(dev_id)
                        + ": modify rule failed: " + e.what());
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
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": table id "
                      + std::to_string(table_id) + " not found in P4 Info");
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

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
        Logger::error("Dev " + std::to_string(dev_id)
                        + ": can't find rule: " + e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_KEY_NAME_ERROR);
    }

    return EntryModify(dev_id, table_id, ruleIndex, table_entry);
}

pi_status_t EntryFetch(pi_dev_id_t dev_id,
                       pi_p4_id_t table_id,
                       pi_table_fetch_res_t *res) {

    // Check to make sure this device id is allocated
    auto dev =  DeviceMgr::GetDevice(dev_id);
    if (dev == nullptr) {
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Make sure we have P4 info
    auto info = dev->GetP4Info();
    if (info == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": no P4 Info");
        return PI_STATUS_DEV_NOT_ASSIGNED;
    }

    // Retrieve table name
    const char* tableName = pi_p4info_table_name_from_id(info, table_id);
    if (tableName == nullptr) {
        Logger::error("Dev " + std::to_string(dev_id) + ": table id "
                        + std::to_string(table_id) + " not found in P4 Info"); 
        return PI_STATUS_INVALID_ENTRY_PROPERTY;
    }

    // Grab the table reference and table size and capacity
    ::np4::Table& table = dev->GetP4Atom().getTable(tableName);
    size_t maxRuleIndex = 0;
    try {

	    res->num_entries = table.getSize();
        maxRuleIndex = table.getCapacity();

    // Couldn't find entry
    } catch (::np4::Exception &e) {
        Logger::error("Dev " + std::to_string(dev_id) + ": can't get table "
                        + tableName + ": "  + e.what());
        return pi_status_t(PI_STATUS_TARGET_ERROR + P4DEV_KEY_NAME_ERROR);
    }

    // Calculate space needed
    ActionMap actionMap;
    size_t dataSize = CalcRuleDataSize(info, table_id, table,
                                       maxRuleIndex, actionMap, res);

    // Now allocate the space
	char *data = new char[dataSize];
	if (data == NULL) return PI_STATUS_ALLOC_ERROR;

	// in some cases, we do not use the whole buffer
	std::fill(data, data + dataSize, 0);
	res->entries_size = dataSize;
	res->entries = data;

    // Dump each rule into the reserved table data
    size_t ruleIndex = 0;
	for (uint32_t i = 0; i < res->num_entries; i++) {

        // Grab the rule
        ::np4::Rule rule;
        try {
            rule = table.getRule(ruleIndex);


        // Table is sparsely populated so should be ok
        } catch (::np4::Exception &e) {
            Logger::debug("Dev " + std::to_string(dev_id)
                            + ": rule fetch failed on index "
                            + std::to_string(ruleIndex) + "%d: " + e.what());
        }

        // Copy in the rule data
        data = CopyRuleData(info, data, ruleIndex, actionMap, rule);
	}

    return PI_STATUS_SUCCESS;
}

}   // np4
}   // pi
