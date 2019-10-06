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

#ifndef PI_NP4_COUNTER_H_
#define PI_NP4_COUNTER_H_

#include <PI/pi.h>
#include <PI/target/pi_counter_imp.h>
#include <vector>
#include <np4atom.hpp>

namespace pi {
namespace np4 {

// @brief   The NP4 Counter class
//
//  This class implements the pi counter functions in C++, providing
//  wrappers needed to make calls to the NP4 ATOM C++ API.
class Counter {
  public:
    // @brief Read a counter
    //
    // @param[in]   dev_id        Device Id
    // @param[in]   counter_id    Counter Id
    // @param[in]   index         Index into a counter array
    // @param[out]  counter_data  Returned counter data
    // @return      Function returns a PI status code
    //
    static pi_status_t Read(pi_dev_id_t dev_id, 
                            pi_p4_id_t counter_id,
                            std::size_t index,
                            pi_counter_data_t *counter_data);

    // Counter is neither copyable or movable
    Counter(const Counter&) = delete;
    Counter& operator=(const Counter&) = delete;
    Counter(Counter&&) = delete;
    Counter& operator=(const Counter&&) = delete;

  private:
    Counter();
};

}   // np4
}   // pi

#endif // PI_NP4_COUNTER_H_
