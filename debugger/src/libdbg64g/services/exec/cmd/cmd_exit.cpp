/*
 *  Copyright 2018 Sergey Khabarov, sergeykhbr@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "cmd_exit.h"

namespace debugger {

CmdExit::CmdExit(ITap *tap) : ICommand ("exit", tap) {

    briefDescr_.make_string("Exit and close application");
    detailedDescr_.make_string(
        "Description:\n"
        "    Immediate close the application and exit.\n"
        "Example:\n"
        "    exit\n");
}

int CmdExit::isValid(AttributeType *args) {
    if (!cmdName_.is_equal((*args)[0u].to_string())) {
        return CMD_INVALID;
    }
    return CMD_VALID;
}

void CmdExit::exec(AttributeType *args, AttributeType *res) {
    RISCV_break_simulation();
}

}  // namespace debugger
