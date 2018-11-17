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

#include "iservice.h"
#include "cmd_loadelf.h"
#include "coreservices/ielfreader.h"

namespace debugger {

CmdLoadElf::CmdLoadElf(ITap *tap, ISocInfo *info) 
    : ICommand ("loadelf", tap, info) {

    briefDescr_.make_string("Load ELF-file");
    detailedDescr_.make_string(
        "Description:\n"
        "    Load ELF-file to SOC target memory. Optional key 'nocode'\n"
        "    allows to read debug information from the elf-file without\n"
        "    target programming.\n"
        "Usage:\n"
        "    loadelf filename [nocode]\n"
        "Example:\n"
        "    loadelf /home/riscv/image.elf\n"
        "    loadelf /home/riscv/image.elf nocode\n");
}

bool CmdLoadElf::isValid(AttributeType *args) {
    if ((*args)[0u].is_equal("loadelf") 
        && (args->size() == 2 || args->size() == 3)) {
        return CMD_VALID;
    }
    return CMD_INVALID;
}

void CmdLoadElf::exec(AttributeType *args, AttributeType *res) {
    res->make_nil();
    if (!isValid(args)) {
        generateError(res, "Wrong argument list");
        return;
    }
    bool program = true;
    if (args->size() == 3 
        && (*args)[2].is_string() && (*args)[2].is_equal("nocode")) {
        program = false;
    }

    /**
     *  @todo Elf Loader service change on elf-reader
     */
    AttributeType lstServ;
    RISCV_get_services_with_iface(IFACE_ELFREADER, &lstServ);
    if (lstServ.size() == 0) {
        generateError(res, "Elf-service not found");
        return;
    }

    IService *iserv = static_cast<IService *>(lstServ[0u].to_iface());
    IElfReader *elf = static_cast<IElfReader *>(
                        iserv->getInterface(IFACE_ELFREADER));
    elf->readFile((*args)[1].to_string());

    if (!program) {
        return;
    }

    DsuMapType *dsu = info_->getpDsu();
    uint64_t soft_reset = 1;
    uint64_t addr = reinterpret_cast<uint64_t>(&dsu->ulocal.v.soft_reset);
    tap_->write(addr, 8, reinterpret_cast<uint8_t *>(&soft_reset));

    uint64_t sec_addr;
    int sec_sz;
    for (unsigned i = 0; i < elf->loadableSectionTotal(); i++) {
        sec_addr = elf->sectionAddress(i);
        sec_sz = static_cast<int>(elf->sectionSize(i));
        tap_->write(sec_addr, sec_sz, elf->sectionData(i));
    }

    soft_reset = 0;
    tap_->write(addr, 8, reinterpret_cast<uint8_t *>(&soft_reset));
}


}  // namespace debugger
