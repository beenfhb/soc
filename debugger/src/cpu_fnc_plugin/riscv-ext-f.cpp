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
 *
 * @brief      RISC-V extension-F (Floating-point Instructions).
 */

#include "api_core.h"
#include "riscv-isa.h"
#include "cpu_riscv_func.h"

namespace debugger {

#define CHECK_FPU_ALGORITHM

#ifdef CHECK_FPU_ALGORITHM
/** TODO: port this module from Hardware GNSS module */
void idiv53(int64_t inDivident,
            int64_t inDivisor,
            int *outBits,       // 106 bits value
            int &outShift,      // 7 bits value
            int &outOverBit,
            int &outZeroResid) {
    outShift = 0;
    outOverBit = 0;
    outZeroResid = 0;
}
#endif

/**
 * @brief The FDIV.D double precision division
 */
class FDIV_D : public RiscvInstruction {
 public:
    FDIV_D(CpuRiver_Functional *icpu) : RiscvInstruction(icpu,
        "FDIV_D", "0001101??????????????????1010011") {}

    virtual int exec(Reg64Type *payload) {
        ISA_R_type u;
        Reg64Type dest, src1, src2;
        u.value = payload->buf32[0];
        src1.val = R[u.bits.rs1];
        src2.val = R[u.bits.rs2];
        if (R[u.bits.rs2]) {
            dest.f64 = src1.f64 / src2.f64;
        } else {
            dest.val = 0;
        }
        R[u.bits.rd] = dest.val;

#ifdef CHECK_FPU_ALGORITHM
        Reg64Type A, B, fres;
        A.val = R[u.bits.rs1];
        B.val = R[u.bits.rs2];
        uint64_t zeroA = !A.f64bits.sign && !A.f64bits.exp ? 1: 0;
        uint64_t zeroB = !B.f64bits.sign && !B.f64bits.exp ? 1: 0;

        int64_t mantA = A.f64bits.mant;
        mantA |= A.f64bits.exp ? 0x0010000000000000ull: 0;

        int64_t mantB = B.f64bits.mant;
        mantB |= B.f64bits.exp ? 0x0010000000000000ull: 0;

        // multiplexer for operation with zero expanent
        int preShift = 0;
        while (preShift < 52 && ((mantB >> (52 - preShift)) & 0x1) == 0) {
            preShift++;
        }
        int64_t divisor = mantB << preShift;

        // IDiv53 module:
        int idivResult[106], idivLShift, idivOverBit, idivZeroResid;
        idiv53(mantA, divisor, idivResult, idivLShift,
               idivOverBit, idivZeroResid);

        // easy in HDL
        int mantAlign[105];
        for (int i = 0; i < 105; i++) {
            if ((i - idivLShift) >= 0) {
                mantAlign[i] = idivResult[i - idivLShift];
            } else {
                mantAlign[i] = 0;
            }
        }

        int64_t expAB = A.f64bits.exp - B.f64bits.exp + 1023;
        int expShift;
        if (B.f64bits.exp == 0 && A.f64bits.exp != 0) {
            expShift = preShift - idivLShift - 1;
        } else {
            expShift = preShift - idivLShift;
        }

        int64_t expAlign = expAB + expShift;
        int64_t postShift = 0;
        if (expAlign <= 0) {
            postShift = -expAlign;
            if (B.f64bits.exp != 0 && A.f64bits.exp != 0) {
                postShift += 1;
            }
        }

        int mantPostScale[105];
        for (int i = 0; i < 105; i++) {
            if ((i + postShift) < 105) {
                mantPostScale[i] = mantAlign[i + postShift];
            } else {
                mantPostScale[i] = 0;
            }
        }

        int64_t mantShort = 0;
        int64_t tmpMant05 = 0;
        for (int i = 0; i < 53; i++) {
            mantShort |= static_cast<int64_t>(mantPostScale[52 + i]) << i;
        }
        for (int i = 0; i < 52; i++) {
            tmpMant05 |= static_cast<int64_t>(mantPostScale[i]) << i;
        }

        int64_t mantOnes = mantShort == 0x001fffffffffffff ? 1: 0;

        // rounding bit
        int mantEven = mantPostScale[52];
        int mant05 = tmpMant05 == 0x0008000000000000 ? 1: 0;
        int64_t rndBit = mantPostScale[51] & !(mant05 & !mantEven);

        // Exceptions:
        int64_t nanRes = expAlign == 0x7ff ? 1: 0;
        int64_t overflow = !((expAlign >> 12) & 0x1) & ((expAlign >> 11) & 0x1);
        int64_t underflow = ((expAlign >> 12) & 0x1) & ((expAlign >> 11) & 0x1);

        // Check borders:
        int nanA = A.f64bits.exp == 0x7ff ? 1: 0;
        int nanB = B.f64bits.exp == 0x7ff ? 1: 0;
        int mantZeroA = A.f64bits.mant ? 0: 1;
        int mantZeroB = B.f64bits.mant ? 0: 1;
        int divOnZero = zeroB || (mantB == 0) ? 1: 0;

        // Result multiplexers:
        if ((nanA & mantZeroA) & (nanB & mantZeroB)) {
            fres.f64bits.sign = 1;
        } else if (nanA & !mantZeroA) {
            fres.f64bits.sign = A.f64bits.sign;
        } else if (nanB & !mantZeroB) {
            fres.f64bits.sign = B.f64bits.sign;
        } else if (divOnZero && zeroA) {
            fres.f64bits.sign = 1;
        } else {
            fres.f64bits.sign = A.f64bits.sign ^ B.f64bits.sign; 
        }

        if (nanB & !mantZeroB) {
            fres.f64bits.exp = B.f64bits.exp;
        } else if ((underflow | zeroA | zeroB) & !divOnZero) {
            fres.f64bits.exp = 0x0;
        } else if (overflow | divOnZero) {
            fres.f64bits.exp = 0x7FF;
        } else if (nanA) {
            fres.f64bits.exp = A.f64bits.exp;
        } else if ((nanB & mantZeroB) || expAlign < 0) {
            fres.f64bits.exp = 0x0;
        } else {
            fres.f64bits.exp = expAlign + (mantOnes & rndBit & !overflow);
        }

        if ((zeroA & zeroB) | (nanA & mantZeroA & nanB & mantZeroB)) {
            fres.f64bits.mant = 0x8000000000000;
        } else if (nanA & !mantZeroA) {
            fres.f64bits.mant = A.f64bits.mant | 0x8000000000000;
        } else if (nanB & !mantZeroB) {
            fres.f64bits.mant = B.f64bits.mant | 0x8000000000000;
        } else if (overflow | nanRes | (nanA & mantZeroA) | (nanB & mantZeroB)) {
            fres.f64bits.mant = 0x0;
        } else {
            fres.f64bits.mant = mantShort + rndBit;
        }

        if (fres.f64 != dest.f64) {
            RISCV_printf(0, 1, "FDIF.D %016" RV_PRI64 "x != %016" RV_PRI64 "x",
                        fres.val, dest.val);
        }
#endif
        return 4;
    }
};


void CpuRiver_Functional::addIsaExtensionF() {
    // TODO
    /*
    addInstr("FADD_S",             "0000000??????????????????1010011", NULL, out);
    addInstr("FSUB_S",             "0000100??????????????????1010011", NULL, out);
    addInstr("FMUL_S",             "0001000??????????????????1010011", NULL, out);
    addInstr("FDIV_S",             "0001100??????????????????1010011", NULL, out);
    addInstr("FSGNJ_S",            "0010000??????????000?????1010011", NULL, out);
    addInstr("FSGNJN_S",           "0010000??????????001?????1010011", NULL, out);
    addInstr("FSGNJX_S",           "0010000??????????010?????1010011", NULL, out);
    addInstr("FMIN_S",             "0010100??????????000?????1010011", NULL, out);
    addInstr("FMAX_S",             "0010100??????????001?????1010011", NULL, out);
    addInstr("FSQRT_S",            "010110000000?????????????1010011", NULL, out);
    addInstr("FADD_D",             "0000001??????????????????1010011", NULL, out);
    addInstr("FSUB_D",             "0000101??????????????????1010011", NULL, out);
    addInstr("FMUL_D",             "0001001??????????????????1010011", NULL, out);
    addInstr("FDIV_D",             "0001101??????????????????1010011", NULL, out);
    addInstr("FSGNJ_D",            "0010001??????????000?????1010011", NULL, out);
    addInstr("FSGNJN_D",           "0010001??????????001?????1010011", NULL, out);
    addInstr("FSGNJX_D",           "0010001??????????010?????1010011", NULL, out);
    addInstr("FMIN_D",             "0010101??????????000?????1010011", NULL, out);
    addInstr("FMAX_D",             "0010101??????????001?????1010011", NULL, out);
    addInstr("FCVT_S_D",           "010000000001?????????????1010011", NULL, out);
    addInstr("FCVT_D_S",           "010000100000?????????????1010011", NULL, out);
    addInstr("FSQRT_D",            "010110100000?????????????1010011", NULL, out);
    addInstr("FLE_S",              "1010000??????????000?????1010011", NULL, out);
    addInstr("FLT_S",              "1010000??????????001?????1010011", NULL, out);
    addInstr("FEQ_S",              "1010000??????????010?????1010011", NULL, out);
    addInstr("FLE_D",              "1010001??????????000?????1010011", NULL, out);
    addInstr("FLT_D",              "1010001??????????001?????1010011", NULL, out);
    addInstr("FEQ_D",              "1010001??????????010?????1010011", NULL, out);
    addInstr("FCVT_W_S",           "110000000000?????????????1010011", NULL, out);
    addInstr("FCVT_WU_S",          "110000000001?????????????1010011", NULL, out);
    addInstr("FCVT_L_S",           "110000000010?????????????1010011", NULL, out);
    addInstr("FCVT_LU_S",          "110000000011?????????????1010011", NULL, out);
    addInstr("FMV_X_S",            "111000000000?????000?????1010011", NULL, out);
    addInstr("FCLASS_S",           "111000000000?????001?????1010011", NULL, out);
    addInstr("FCVT_W_D",           "110000100000?????????????1010011", NULL, out);
    addInstr("FCVT_WU_D",          "110000100001?????????????1010011", NULL, out);
    addInstr("FCVT_L_D",           "110000100010?????????????1010011", NULL, out);
    addInstr("FCVT_LU_D",          "110000100011?????????????1010011", NULL, out);
    addInstr("FMV_X_D",            "111000100000?????000?????1010011", NULL, out);
    addInstr("FCLASS_D",           "111000100000?????001?????1010011", NULL, out);
    addInstr("FCVT_S_W",           "110100000000?????????????1010011", NULL, out);
    addInstr("FCVT_S_WU",          "110100000001?????????????1010011", NULL, out);
    addInstr("FCVT_S_L",           "110100000010?????????????1010011", NULL, out);
    addInstr("FCVT_S_LU",          "110100000011?????????????1010011", NULL, out);
    addInstr("FMV_S_X",            "111100000000?????000?????1010011", NULL, out);
    addInstr("FCVT_D_W",           "110100100000?????????????1010011", NULL, out);
    addInstr("FCVT_D_WU",          "110100100001?????????????1010011", NULL, out);
    addInstr("FCVT_D_L",           "110100100010?????????????1010011", NULL, out);
    addInstr("FCVT_D_LU",          "110100100011?????????????1010011", NULL, out);
    addInstr("FMV_D_X",            "111100100000?????000?????1010011", NULL, out);
    addInstr("FLW",                "?????????????????010?????0000111", NULL, out);
    addInstr("FLD",                "?????????????????011?????0000111", NULL, out);
    addInstr("FSW",                "?????????????????010?????0100111", NULL, out);
    addInstr("FSD",                "?????????????????011?????0100111", NULL, out);
    addInstr("FMADD_S",            "?????00??????????????????1000011", NULL, out);
    addInstr("FMSUB_S",            "?????00??????????????????1000111", NULL, out);
    addInstr("FNMSUB_S",           "?????00??????????????????1001011", NULL, out);
    addInstr("FNMADD_S",           "?????00??????????????????1001111", NULL, out);
    addInstr("FMADD_D",            "?????01??????????????????1000011", NULL, out);
    addInstr("FMSUB_D",            "?????01??????????????????1000111", NULL, out);
    addInstr("FNMSUB_D",           "?????01??????????????????1001011", NULL, out);
    addInstr("FNMADD_D",           "?????01??????????????????1001111", NULL, out);
    def FRFLAGS            = BitPat("b00000000000100000010?????1110011")
    def FSFLAGS            = BitPat("b000000000001?????001?????1110011")
    def FSFLAGSI           = BitPat("b000000000001?????101?????1110011")
    def FRRM               = BitPat("b00000000001000000010?????1110011")
    def FSRM               = BitPat("b000000000010?????001?????1110011")
    def FSRMI              = BitPat("b000000000010?????101?????1110011")
    def FSCSR              = BitPat("b000000000011?????001?????1110011")
    def FRCSR              = BitPat("b00000000001100000010?????1110011")
    */
    uint64_t isa = portCSR_.read(CSR_misa).val;
    isa |= (1LL << ('F' - 'A'));
    portCSR_.write(CSR_misa, isa);
}

}  // namespace debugger
