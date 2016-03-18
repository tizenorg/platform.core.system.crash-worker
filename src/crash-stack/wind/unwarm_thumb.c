/***************************************************************************
 * ARM Stack Unwinder, Michael.McTernan.2001@cs.bris.ac.uk
 *
 * This program is PUBLIC DOMAIN.
 * This means that there is no copyright and anyone is able to take a copy
 * for free and use it as they wish, with or without modifications, and in
 * any context, commercially or otherwise. The only limitation is that I
 * don't guarantee that the software is fit for any purpose or accept any
 * liability for it's use or misuse - this software is without warranty.
 ***************************************************************************
 * File Description: Abstract interpretation for Thumb mode.
 **************************************************************************/

#define MODULE_NAME "UNWARM_THUMB"

/***************************************************************************
 * Include Files
 **************************************************************************/

#include "system.h"
#if defined(UPGRADE_ARM_STACK_UNWIND)
#include <stdio.h>
#include "unwarm.h"

/***************************************************************************
 * Manifest Constants
 **************************************************************************/


/***************************************************************************
 * Type Definitions
 **************************************************************************/


/***************************************************************************
 * Variables
 **************************************************************************/


/***************************************************************************
 * Macros
 **************************************************************************/


/***************************************************************************
 * Local Functions
 **************************************************************************/

/** Sign extend an 11 bit value.
 * This function simply inspects bit 11 of the input \a value, and if
 * set, the top 5 bits are set to give a 2's compliment signed value.
 * \param value   The value to sign extend.
 * \return The signed-11 bit value stored in a 16bit data type.
 */
static SignedInt16 signExtend11(Int16 value)
{
    if(value & 0x400)
    {
        value |= 0xf800;
    }

    return value;
}


/***************************************************************************
 * Global Functions
 **************************************************************************/

static UnwResult Unw32DataProcessingModifiedImmediate (UnwState * const state, Int32 instr, Int32 instr2)
{
    Int8 op = (instr & (0xF << 5)) >> 5;
    Int8 Rn = instr & 0xF;
    Int8 S = (instr & (1 << 4)) >> 4;
    Int8 Rd = (instr2 & (0xF << 8)) >> 8;

    switch (op)
    {
      case 0:
          if (0xF==Rd)
          {
              if (0==S) return UNWIND_ILLEGAL_INSTR;

              UnwPrintd1("TST ...\n");
          }
          else
          {
              state->regData[Rd].o = REG_VAL_INVALID;
              UnwPrintd1("AND ...\n");
          }
          break;
      case 1:
          state->regData[Rd].o = REG_VAL_INVALID;
          UnwPrintd1("BIC ...\n");
          break;
      case 2:
          state->regData[Rd].o = REG_VAL_INVALID;
          if (0xF != Rn)
              UnwPrintd1("ORR ...\n");
          else
              UnwPrintd1("MOV ...\n");
          break;
      case 3:
          if (0xF != Rn)
          {
              state->regData[Rd].o = REG_VAL_INVALID;
              UnwPrintd1("ORN ...\n");
          }
          else
              UnwPrintd1("MVN ...\n");
          break;
      case 4:
          if (0xF != Rd)
          {
              UnwPrintd1("EOR ...\n");
              state->regData[Rd].o = REG_VAL_INVALID;
          }
          else
          {
              if (0==S) return UNWIND_ILLEGAL_INSTR;

              UnwPrintd1("TEQ ...\n");
          }
          break;
      case 8:
          if (0xF != Rd)
          {
              UnwPrintd1("ADD ...\n");
              state->regData[Rd].o = REG_VAL_INVALID;
          }
          else
          {
              if (0==S) return UNWIND_ILLEGAL_INSTR;

              UnwPrintd1("CMN ...\n");
          }
          break;
      case 10:
          state->regData[Rd].o = REG_VAL_INVALID;
          UnwPrintd1("ADC ...\n");
          break;
      case 11:
          state->regData[Rd].o = REG_VAL_INVALID;
          UnwPrintd1("SBC ...\n");
          break;
      case 13:
          if (0xF != Rd)
          {
              state->regData[Rd].o = REG_VAL_INVALID;
              UnwPrintd1("SUB ...\n");
          }
          else
          {
              if (0==S) return UNWIND_ILLEGAL_INSTR;

              UnwPrintd1("CMP ...\n");
          }
          break;
      case 14:
          state->regData[Rd].o = REG_VAL_INVALID;
          UnwPrintd1("RSB ...\n");
          break;
      default:
          return UNWIND_ILLEGAL_INSTR;
    }
    return UNWIND_SUCCESS;
}

static UnwResult Unw32LoadWord (UnwState * const state, Int32 instr, Int32 instr2)
{
    Int8 op1 = (instr & (0x3 << 7)) >> 7;
    Int8 Rn = instr & 0xF;
//    Int8 op2 = (instr2 & (0x3F << 6)) >> 6;

    if (1 == op1 && 0xF != Rn)
    {
        /* LDR imm */
        Int8 Rt = (instr2 & (0xF << 12)) >> 12;
        Int32 imm12 = instr2 & 0xFFF;

        UnwPrintd4("LDR r%d, [r%d ,#0x%08x]\n", Rt, Rn, imm12);

        imm12 += state->regData[Rn].v;

        if(!UnwMemReadRegister(state, imm12, &state->regData[Rt]))
        {
            return UNWIND_DREAD_W_FAIL;
        }
    }
    else if (0 == op1 && 0xF != Rn)
    {
        Int8 Rt = (instr2 & (0xF << 12)) >> 12;
        Int32 imm8 = instr2 & 0xFF;
        Int8 U = (instr2 & (1 << 9)) >> 9;
        Int8 P = (instr2 & (1 << 10)) >> 10;
        Int8 W = (instr2 & (1 << 8)) >> 8;
        Int32 offset_addr;
        Int32 addr;

        UnwPrintd8("LDR r%d, [r%d%c,#%c0x%08x%c%c\n",
            Rt, Rn,
            P ? ' ' : ']',
            U ? '+' : '-',
            imm8,
            P ? ']' : ' ',
            W ? '!' : ' ');

        offset_addr = state->regData[Rn].v + (U ? 1 : -1) * imm8;
        addr = P ? offset_addr : state->regData[Rn].v;

        if(!UnwMemReadRegister(state, addr, &state->regData[Rt]))
        {
            return UNWIND_DREAD_W_FAIL;
        }

        if (W) state->regData[Rn].v = offset_addr;
    }
//            else if (op1 < 2 && 0xF == Rn)
//            {
      /* LDR literal */
//            }
    else
    {
      /* UNDEFINED */
        UnwPrintd1("????");
        UnwInvalidateRegisterFile(state->regData);
    }
    return UNWIND_SUCCESS;
}
static UnwResult Unw32LoadStoreMultiple (UnwState * const state, Int32 instr, Int32 instr2)
{
    Int8 op = (instr & (0x3 << 7)) >> 7;
    Int8 L = (instr & (0x1 << 4)) >> 4;
    Int8 Rn = instr & 0xF;

    UnwResult res = UNWIND_SUCCESS;

    switch (op)
    {
        case 0:
            if (0 == L) UnwPrintd1("SRS ...");
            else
            {
                state->regData[REG_PC].o = REG_VAL_INVALID;
                UnwPrintd1("LRE ...");
            }
            break;
        case 1:
            {
                Int8 bitCount = 0;
                Int16 register_list = (instr2 & 0x7FFF);
                int i;

                for (i = 0; i < 15; i++)
                {
                    if ((register_list & (0x1 << i)) != 0) bitCount++;
                }

                if (0 == L)
                {
                    Int8 W = (instr & (0x1 << 5)) >> 5;
                    if (W) state->regData[Rn].v += 4*bitCount;
                    UnwPrintd1("STM ...");
                }
                else
                {
                    Int8 W = (instr & (0x1 << 5)) >> 5;
                    for (i = 0; i < 15; i++)
                    {
                        if ((register_list & (0x1 << i)) != 0)
                            state->regData[i].o = REG_VAL_INVALID;
                    }
                    if (W)
                    {
                        if ((register_list & (1 << Rn)) == 0)
                            state->regData[Rn].v += 4*bitCount;
                        else
                            state->regData[Rn].o = REG_VAL_INVALID;
                    }
                    if (13 != Rn)
                    {
                        UnwPrintd1("LDM ...");
                    }
                    else
                    {
                        UnwPrintd1("POP ...");
                    }
                }
            }
            break;
        case 2:
            if (0 == L)
            {
              /* STMDB / PUSH if Rn == 13 */
              /* TODO */
                if (13 != Rn)
                {
                    UnwPrintd1("STMDB ...");
                }
                else
                {
                    UnwPrintd1("PUSH ...");
                }
            }
            else
            {
              /* LDMDB */
            }
            break;
        case 3:
            if (0 == L)
            {
              /* SRS */
            }
            else
            {
              /* RFE */
            }
            break;
    }

    return res;
}

UnwResult UnwStartThumb(UnwState * const state)
{
    Boolean  found = FALSE;
    Int16    t = UNW_MAX_INSTR_COUNT;

    do
    {
        Int16 instr;

        /* Attempt to read the instruction */
        if(!state->cb->readH(state->regData[15].v & (~0x1), &instr))
        {
            return UNWIND_IREAD_H_FAIL;
        }

        UnwPrintd4("T %x %x %04x:",
                   state->regData[13].v, state->regData[15].v, instr);

        /* Check that the PC is still on Thumb alignment */
        if(!UnwIsAddrThumb(state->regData[REG_PC].v, state->regData[REG_SPSR].v))
        {
            UnwPrintd1("\nError: PC misalignment\n");
            return UNWIND_INCONSISTENT;
        }

        /* Check that the SP and PC have not been invalidated */
        if(!M_IsOriginValid(state->regData[13].o) || !M_IsOriginValid(state->regData[15].o))
        {
            UnwPrintd1("\nError: PC or SP invalidated\n");
            return UNWIND_INCONSISTENT;
        }

        /* Format 1: Move shifted register
         *  LSL Rd, Rs, #Offset5
         *  LSR Rd, Rs, #Offset5
         *  ASR Rd, Rs, #Offset5
         */
        if((instr & 0xe000) == 0x0000 && (instr & 0x1800) != 0x1800)
        {
            Boolean signExtend;
            Int8    op      = (instr & 0x1800) >> 11;
            Int8    offset5 = (instr & 0x07c0) >>  6;
            Int8    rs      = (instr & 0x0038) >>  3;
            Int8    rd      = (instr & 0x0007);

            switch(op)
            {
                case 0: /* LSL */
                    UnwPrintd6("LSL r%d, r%d, #%d\t; r%d %s", rd, rs, offset5, rs, M_Origin2Str(state->regData[rs].o));
                    state->regData[rd].v = state->regData[rs].v << offset5;
                    state->regData[rd].o = state->regData[rs].o;
                    state->regData[rd].o |= REG_VAL_ARITHMETIC;
                    break;

                case 1: /* LSR */
                    UnwPrintd6("LSR r%d, r%d, #%d\t; r%d %s", rd, rs, offset5, rs, M_Origin2Str(state->regData[rs].o));
                    state->regData[rd].v = state->regData[rs].v >> offset5;
                    state->regData[rd].o = state->regData[rs].o;
                    state->regData[rd].o |= REG_VAL_ARITHMETIC;
                    break;

                case 2: /* ASR */
                    UnwPrintd6("ASL r%d, r%d, #%d\t; r%d %s", rd, rs, offset5, rs, M_Origin2Str(state->regData[rs].o));

                    signExtend = (state->regData[rs].v & 0x8000) ? TRUE : FALSE;
                    state->regData[rd].v = state->regData[rs].v >> offset5;
                    if(signExtend)
                    {
                        state->regData[rd].v |= 0xffffffff << (32 - offset5);
                    }
                    state->regData[rd].o = state->regData[rs].o;
                    state->regData[rd].o |= REG_VAL_ARITHMETIC;
                    break;
            }
        }
        /* Format 2: add/subtract
         *  ADD Rd, Rs, Rn
         *  ADD Rd, Rs, #Offset3
         *  SUB Rd, Rs, Rn
         *  SUB Rd, Rs, #Offset3
         */
        else if((instr & 0xf800) == 0x1800)
        {
            Boolean I  = (instr & 0x0400) ? TRUE : FALSE;
            Boolean op = (instr & 0x0200) ? TRUE : FALSE;
            Int8    rn = (instr & 0x01c0) >> 6;
            Int8    rs = (instr & 0x0038) >> 3;
            Int8    rd = (instr & 0x0007);

            /* Print decoding */
            UnwPrintd6("%s r%d, r%d, %c%d\t;",
                       op ? "SUB" : "ADD",
                       rd, rs,
                       I ? '#' : 'r',
                       rn);
            UnwPrintd5("r%d %s, r%d %s",
                       rd, M_Origin2Str(state->regData[rd].o),
                       rs, M_Origin2Str(state->regData[rs].o));
            if(!I)
            {
                UnwPrintd3(", r%d %s", rn, M_Origin2Str(state->regData[rn].o));

                /* Perform calculation */
                if(op)
                {
                    state->regData[rd].v = state->regData[rs].v - state->regData[rn].v;
                }
                else
                {
                    state->regData[rd].v = state->regData[rs].v + state->regData[rn].v;
                }

                /* Propagate the origin */
                if(M_IsOriginValid(state->regData[rs].v) &&
                   M_IsOriginValid(state->regData[rn].v))
                {
                    state->regData[rd].o = state->regData[rs].o;
                    state->regData[rd].o |= REG_VAL_ARITHMETIC;
                }
                else
                {
                    state->regData[rd].o = REG_VAL_INVALID;
                }
            }
            else
            {
                /* Perform calculation */
                if(op)
                {
                    state->regData[rd].v = state->regData[rs].v - rn;
                }
                else
                {
                    state->regData[rd].v = state->regData[rs].v + rn;
                }

                /* Propagate the origin */
                state->regData[rd].o = state->regData[rs].o;
                state->regData[rd].o |= REG_VAL_ARITHMETIC;
            }
        }
        /* Format 3: move/compare/add/subtract immediate
         *  MOV Rd, #Offset8
         *  CMP Rd, #Offset8
         *  ADD Rd, #Offset8
         *  SUB Rd, #Offset8
         */
        else if((instr & 0xe000) == 0x2000)
        {
            Int8    op      = (instr & 0x1800) >> 11;
            Int8    rd      = (instr & 0x0700) >>  8;
            Int8    offset8 = (instr & 0x00ff);

            switch(op)
            {
                case 0: /* MOV */
                    UnwPrintd3("MOV r%d, #0x%x", rd, offset8);
                    state->regData[rd].v = offset8;
                    state->regData[rd].o = REG_VAL_FROM_CONST;
                    break;

                case 1: /* CMP */
                    /* Irrelevant to unwinding */
                    UnwPrintd1("CMP ???");
                    break;

                case 2: /* ADD */
                    UnwPrintd5("ADD r%d, #0x%x\t; r%d %s",
                               rd, offset8, rd, M_Origin2Str(state->regData[rd].o));
                    state->regData[rd].v += offset8;
                    state->regData[rd].o |= REG_VAL_ARITHMETIC;
                    break;

                case 3: /* SUB */
                    UnwPrintd5("SUB r%d, #0x%d\t; r%d %s",
                               rd, offset8, rd, M_Origin2Str(state->regData[rd].o));
                    state->regData[rd].v += offset8;
                    state->regData[rd].o |= REG_VAL_ARITHMETIC;
                    break;
            }
        }
        /* Format 4: ALU operations
         *  AND Rd, Rs
         *  EOR Rd, Rs
         *  LSL Rd, Rs
         *  LSR Rd, Rs
         *  ASR Rd, Rs
         *  ADC Rd, Rs
         *  SBC Rd, Rs
         *  ROR Rd, Rs
         *  TST Rd, Rs
         *  NEG Rd, Rs
         *  CMP Rd, Rs
         *  CMN Rd, Rs
         *  ORR Rd, Rs
         *  MUL Rd, Rs
         *  BIC Rd, Rs
         *  MVN Rd, Rs
         */
        else if((instr & 0xfc00) == 0x4000)
        {
            Int8 op = (instr & 0x03c0) >> 6;
            Int8 rs = (instr & 0x0038) >> 3;
            Int8 rd = (instr & 0x0007);
#if defined(UNW_DEBUG)
            static const char * const mnu[16] =
            { "AND", "EOR", "LSL", "LSR",
              "ASR", "ADC", "SBC", "ROR",
              "TST", "NEG", "CMP", "CMN",
              "ORR", "MUL", "BIC", "MVN" };
#endif
            /* Print the mnemonic and registers */
            switch(op)
            {
                case 0: /* AND */
                case 1: /* EOR */
                case 2: /* LSL */
                case 3: /* LSR */
                case 4: /* ASR */
                case 7: /* ROR */
                case 9: /* NEG */
                case 12: /* ORR */
                case 13: /* MUL */
                case 15: /* MVN */
                    UnwPrintd8("%s r%d ,r%d\t; r%d %s, r%d %s",
                               mnu[op],
                               rd, rs,
                               rd, M_Origin2Str(state->regData[rd].o),
                               rs, M_Origin2Str(state->regData[rs].o));
                    break;

                case 5: /* ADC */
                case 6: /* SBC */
                    UnwPrintd4("%s r%d, r%d", mnu[op], rd, rs);
                    break;

                case 8: /* TST */
                case 10: /* CMP */
                case 11: /* CMN */
                    /* Irrelevant to unwinding */
                    UnwPrintd2("%s ???", mnu[op]);
                    break;

                case 14: /* BIC */
                    UnwPrintd5("r%d ,r%d\t; r%d %s",
                                rd, rs,
                                rs, M_Origin2Str(state->regData[rs].o));
                    state->regData[rd].v &= !state->regData[rs].v;
                    break;
            }


            /* Perform operation */
            switch(op)
            {
                case 0: /* AND */
                    state->regData[rd].v &= state->regData[rs].v;
                    break;

                case 1: /* EOR */
                    state->regData[rd].v ^= state->regData[rs].v;
                    break;

                case 2: /* LSL */
                    state->regData[rd].v <<= state->regData[rs].v;
                    break;

                case 3: /* LSR */
                    state->regData[rd].v >>= state->regData[rs].v;
                    break;

                case 4: /* ASR */
                    if(state->regData[rd].v & 0x80000000)
                    {
                        state->regData[rd].v >>= state->regData[rs].v;
                        state->regData[rd].v |= 0xffffffff << (32 - state->regData[rs].v);
                    }
                    else
                    {
                        state->regData[rd].v >>= state->regData[rs].v;
                    }

                    break;

                case 5: /* ADC */
                case 6: /* SBC */
                case 8: /* TST */
                case 10: /* CMP */
                case 11: /* CMN */
                    break;
                case 7: /* ROR */
                    state->regData[rd].v = (state->regData[rd].v >> state->regData[rs].v) |
                                    (state->regData[rd].v << (32 - state->regData[rs].v));
                    break;

                case 9: /* NEG */
                    state->regData[rd].v = -state->regData[rs].v;
                    break;

                case 12: /* ORR */
                    state->regData[rd].v |= state->regData[rs].v;
                    break;

                case 13: /* MUL */
                    state->regData[rd].v *= state->regData[rs].v;
                    break;

                case 14: /* BIC */
                    state->regData[rd].v &= !state->regData[rs].v;
                    break;

                case 15: /* MVN */
                    state->regData[rd].v = !state->regData[rs].v;
                    break;
            }

            /* Propagate data origins */
            switch(op)
            {
                case 0: /* AND */
                case 1: /* EOR */
                case 2: /* LSL */
                case 3: /* LSR */
                case 4: /* ASR */
                case 7: /* ROR */
                case 12: /* ORR */
                case 13: /* MUL */
                case 14: /* BIC */
                    if(M_IsOriginValid(state->regData[rs].o) && M_IsOriginValid(state->regData[rs].o))
                    {
                        state->regData[rd].o = state->regData[rs].o;
                        state->regData[rd].o |= REG_VAL_ARITHMETIC;
                    }
                    else
                    {
                        state->regData[rd].o = REG_VAL_INVALID;
                    }
                    break;

                case 5: /* ADC */
                case 6: /* SBC */
                    /* C-bit not tracked */
                    state->regData[rd].o = REG_VAL_INVALID;
                    break;

                case 8: /* TST */
                case 10: /* CMP */
                case 11: /* CMN */
                    /* Nothing propagated */
                    break;

                case 9: /* NEG */
                case 15: /* MVN */
                    state->regData[rd].o = state->regData[rs].o;
                    state->regData[rd].o |= REG_VAL_ARITHMETIC;
                    break;

            }

        }
        /* Format 5: Hi register operations/branch exchange
         *  ADD Rd, Hs
         *  ADD Hd, Rs
         *  ADD Hd, Hs
         *  CMP Hd
         *  MOV Rd
         *  MOV Hd
         *  BX
         *  BLX
         */
        else if((instr & 0xfc00) == 0x4400)
        {
            Int8    op  = (instr & 0x0300) >> 8;
            Boolean h1  = (instr & 0x0080) ? TRUE: FALSE;
            Boolean h2  = (instr & 0x0040) ? TRUE: FALSE;
            Int8    rhs = (instr & 0x0038) >> 3;
            Int8    rhd = (instr & 0x0007);

            /* Adjust the register numbers */
            if(h2) rhs += 8;
            if(h1) rhd += 8;

            if(op == 1 && !h1 && !h2)
            {
                UnwPrintd1("\nError: h1 or h2 must be set for ADD, CMP or MOV\n");
                return UNWIND_ILLEGAL_INSTR;
            }

            switch(op)
            {
                case 0: /* ADD */
                    UnwPrintd5("ADD r%d, r%d\t; r%d %s",
                               rhd, rhs, rhs, M_Origin2Str(state->regData[rhs].o));
                    state->regData[rhd].v += state->regData[rhs].v;
                    state->regData[rhd].o =  state->regData[rhs].o;
                    state->regData[rhd].o |= REG_VAL_ARITHMETIC;
                    break;

                case 1: /* CMP */
                    /* Irrelevant to unwinding */
                    UnwPrintd1("CMP ???");
                    break;

                case 2: /* MOV */
                    UnwPrintd5("MOV r%d, r%d\t; r%d %s",
                               rhd, rhs, rhd, M_Origin2Str(state->regData[rhs].o));
                    state->regData[rhd].v  = state->regData[rhs].v;
                    state->regData[rhd].o  = state->regData[rhd].o;
                    break;

                case 3: /* BX */
                    UnwPrintd4("BX r%d\t; r%d %s\n",
                               rhs, rhs, M_Origin2Str(state->regData[rhs].o));

                    /* Only follow BX if the data was from the stack */
                    if(state->regData[rhs].o == REG_VAL_FROM_STACK)
                    {
                        UnwPrintd2(" Return PC=0x%x\n", state->regData[rhs].v & (~0x1));

                        /* Report the return address, including mode bit */
                        if(!UnwReportRetAddr(state, state->regData[rhs].v))
                        {
                            return UNWIND_TRUNCATED;
                        }

                        /* Update the PC */
                        state->regData[15].v = state->regData[rhs].v;

                        /* Determine the new mode */
                        if(UnwIsAddrThumb(state->regData[rhs].v, state->regData[REG_SPSR].v))
                        {
                            /* Branching to THUMB */

                            /* Account for the auto-increment which isn't needed */
                            state->regData[15].v -= 2;
                        }
                        else
                        {
                            /* Branch to ARM */
                            return UnwStartArm(state);
                        }
                    }
                    else
                    {
                        UnwPrintd4("\nError: BX to invalid register: r%d = 0x%x (%s)\n",
                                   rhs, state->regData[rhs].o, M_Origin2Str(state->regData[rhs].o));
                        return UNWIND_FAILURE;
                    }
            }
        }
        /* Format 9: load/store with immediate offset
         *  LDR/STR Rd, [Rb, #imm]
         */
        else if ((instr & 0xe000) == 0x6000)
        {
            Int8 rd = instr & 0x7;
            Int8 rb = (instr & (0x7 << 3)) >> 3;
            Int32 offset5 = (instr & (0x1f << 6)) >> 6;

            offset5 += state->regData[rb].v;

            if ((instr & 0x0400) != 0)
            {
              /* This is LDR */

              UnwPrintd3("LDR r%d, 0x%08x", rd, offset5);

              if (!UnwMemReadRegister (state, offset5, &state->regData[rd]))
              {
                return UNWIND_DREAD_W_FAIL;
              }
            }
            else
            {
            /* in STR case, ignore it (for now) */
              UnwPrintd3("STR r%d, 0x%08x", rd, offset5);
            }
        }
        /* Format 9: PC-relative load
         *  LDR Rd,[PC, #imm]
         */
        else if((instr & 0xf800) == 0x4800)
        {
            Int8  rd    = (instr & 0x0700) >> 8;
            Int8  word8 = (instr & 0x00ff);
            Int32 address;

            /* Compute load address, adding a word to account for prefetch */
            address = (state->regData[15].v & (~0x3)) + 4 + (word8 << 2);

            UnwPrintd3("LDR r%d, 0x%08x", rd, address);

            if(!UnwMemReadRegister(state, address, &state->regData[rd]))
            {
                return UNWIND_DREAD_W_FAIL;
            }
        }
        else if((instr & 0xf800) == 0x4000)
        {
            /* in STR case, ignore it (for now) */
            UnwPrintd1("STR ???");
        }
        /* Format 13: add offset to Stack Pointer
         *  ADD sp,#+imm
         *  ADD sp,#-imm
         */
        else if((instr & 0xff00) == 0xB000)
        {
            Int8 value = (instr & 0x7f) * 4;

            /* Check the negative bit */
            if((instr & 0x80) != 0)
            {
                UnwPrintd2("SUB sp,#0x%x", value);
                state->regData[13].v -= value;
            }
            else
            {
                UnwPrintd2("ADD sp,#0x%x", value);
                state->regData[13].v += value;
            }
        }
        /* Format 14: push/pop registers
         *  PUSH {Rlist}
         *  PUSH {Rlist, LR}
         *  POP {Rlist}
         *  POP {Rlist, PC}
         */
        else if((instr & 0xf600) == 0xb400)
        {
            Boolean  L     = (instr & 0x0800) ? TRUE : FALSE;
            Boolean  R     = (instr & 0x0100) ? TRUE : FALSE;
            Int8     rList = (instr & 0x00ff);

            if(L)
            {
                Int8 r;

                /* Load from memory: POP */
                UnwPrintd2("POP {Rlist%s}\n", R ? ", PC" : "");

                for(r = 0; r < 8; r++)
                {
                    if(rList & (0x1 << r))
                    {
                        /* Read the word */
                        if(!UnwMemReadRegister(state, state->regData[13].v, &state->regData[r]))
                        {
                            return UNWIND_DREAD_W_FAIL;
                        }

                        /* Alter the origin to be from the stack if it was valid */
                        if(M_IsOriginValid(state->regData[r].o))
                        {
                            state->regData[r].o = REG_VAL_FROM_STACK;
                        }

                        state->regData[13].v += 4;

                        UnwPrintd3("  r%d = 0x%08x\n", r, state->regData[r].v);
                    }
                }

                /* Check if the PC is to be popped */
                if(R)
                {
                    /* Get the return address */
                    if(!UnwMemReadRegister(state, state->regData[13].v, &state->regData[15]))
                    {
                        return UNWIND_DREAD_W_FAIL;
                    }

                    /* Alter the origin to be from the stack if it was valid */
                    if(!M_IsOriginValid(state->regData[15].o))
                    {
                        /* Return address is not valid */
                        UnwPrintd1("PC popped with invalid address\n");
                        return UNWIND_FAILURE;
                    }
                    else
                    {
                        /* The bottom bit should have been set to indicate that
                         *  the caller was from Thumb.  This would allow return
                         *  by BX for interworking APCS.
                         */
                        if(!UnwIsAddrThumb(state->regData[REG_PC].v, state->regData[REG_SPSR].v))
                        {
                            UnwPrintd2("Warning: Return address not to Thumb: 0x%08x\n",
                                       state->regData[15].v);

                            /* Pop into the PC will not switch mode */
                            return UNWIND_INCONSISTENT;
                        }

                        /* Store the return address */
                        if(!UnwReportRetAddr(state, state->regData[15].v))
                        {
                            return UNWIND_TRUNCATED;
                        }

                        /* Now have the return address */
                        UnwPrintd2(" Return PC=%x\n", state->regData[15].v);

                        /* Update the pc */
                        state->regData[13].v += 4;

                        /* Compensate for the auto-increment, which isn't needed here */
                        state->regData[15].v -= 2;
                    }
                }

            }
            else
            {
                SignedInt8 r;

                /* Store to memory: PUSH */
                UnwPrintd2("PUSH {Rlist%s}", R ? ", LR" : "");

                /* Check if the LR is to be pushed */
                if(R)
                {
                    UnwPrintd3("\n  lr = 0x%08x\t; %s",
                               state->regData[14].v, M_Origin2Str(state->regData[14].o));

                    state->regData[13].v -= 4;

                    /* Write the register value to memory */
                    if(!UnwMemWriteRegister(state, state->regData[13].v, &state->regData[14]))
                    {
                        return UNWIND_DWRITE_W_FAIL;
                    }
                }

                for(r = 7; r >= 0; r--)
                {
                    if(rList & (0x1 << r))
                    {
                        UnwPrintd4("\n  r%d = 0x%08x\t; %s",
                                   r, state->regData[r].v, M_Origin2Str(state->regData[r].o));

                        state->regData[13].v -= 4;

                        if(!UnwMemWriteRegister(state, state->regData[13].v, &state->regData[r]))
                        {
                            return UNWIND_DWRITE_W_FAIL;
                        }
                    }
                }
            }
        }
        /* Format 18: unconditional branch
         *  B label
         */
        else if((instr & 0xf800) == 0xe000)
        {
            SignedInt16 branchValue = signExtend11(instr & 0x07ff);

            /* Branch distance is twice that specified in the instruction. */
            branchValue *= 2;

            UnwPrintd2("B %d \n", branchValue);

            /* Update PC */
            state->regData[15].v += branchValue;

            /* Need to advance by a word to account for pre-fetch.
             *  Advance by a half word here, allowing the normal address
             *  advance to account for the other half word.
             */
            state->regData[15].v += 2;

            /* Display PC of next instruction */
            UnwPrintd2(" New PC=%x", state->regData[15].v + 2);

        }
        /* 32-bit instructions */
        else if (((instr & 0xe000) == 0xe000) && ((instr & 0xf800) != 0xe00))
        {
            Int8 op1 = (instr & (0x3 << 11)) >> 11;
            Int8 op2 = (instr & (0x7F << 4)) >> 4;
            Int8 op;
            UnwResult res = UNWIND_SUCCESS;

            Int16 instr2;
            /* read second part of this 32-bit instruction */
            if(!state->cb->readH((state->regData[15].v + 2) & (~0x1), &instr2))
            {
                return UNWIND_IREAD_H_FAIL;
            }

            op = (instr2 & (1 << 15)) >> 15;

            switch (op1)
            {
                case 1:
                    if ((op2 & 0x64) == 0)
                    {
                      /* Load/store multiple */
                      res = Unw32LoadStoreMultiple(state, instr, instr2);
                    }
                    else if ((op2 & 0x64) == 4)
                    {
                      /* Load/store dual, load/store exclusive, table branch */
                    }
                    else if ((op2 & 0x60) == 0x20)
                    {
                      /* Data-processing (shifted register) */
                    }
                    else /* if (op2 & 0x40 == 0x40) */
                    {
                      /* Coprocessor instructions */
                    }
                    break;
                case 2:
                    if (0 == op)
                    {
                        if ((op2 & 0x20) == 0)
                        {
                            /* Data processing (modified immediate) */
                            res = Unw32DataProcessingModifiedImmediate(state, instr, instr2);
                        }
                        else
                        {
                            /* Data-processing (plain binary immediate) */
                        }
                    }
                    else
                    {
                        /* Branches and miscellaneous control */
                    }
                    break;
                case 3:
                    if ((op2 & 0x71) == 0)
                    {
                        /* Store single data item */
                    }
                    else if ((op2 & 0x71) == 0x10)
                    {
                        /* Advanced SIMD element or structure load/store instructions */
                    }
                    else if ((op2 & 0x67) == 1)
                    {
                        /* Load byte, memory hints */
                    }
                    else if ((op2 & 0x67) == 3)
                    {
                        /* Load halfword, memory hints */
                    }
                    else if ((op2 & 0x67) == 5)
                    {
                        /* Load word */
                        res = Unw32LoadWord (state, instr, instr2);
                    }
                    else if ((op2 & 0x67) == 7)
                    {
                        res = UNWIND_ILLEGAL_INSTR;
                    }
                    else if ((op2 & 0x70) == 0x20)
                    {
                        /* Data-processing (register) */
                    }
                    else if ((op2 & 0x78) == 0x30)
                    {
                        /* Multiply, multiply accumulate, and absolute difference */
                    }
                    else if ((op2 & 0x78) == 0x38)
                    {
                        /* Long multiply, long multiply accumulate, and divide */
                    }
                    else if ((op2 & 0x80) == 0x80)
                    {
                        /* Coprocessor instructions */
                    }
                    else
                        res = UNWIND_ILLEGAL_INSTR;
            }

            state->regData[REG_PC].v += 2;

            if (UNWIND_SUCCESS != res)
                return res;
        }
        else
        {
            UnwPrintd1("????");

            /* Unknown/undecoded.  May alter some register, so invalidate file */
            UnwInvalidateRegisterFile(state->regData);
        }

        UnwPrintd1("\n");

        /* Should never hit the reset vector */
        if(state->regData[15].v == 0) return UNWIND_RESET;

        /* Check next address */
        state->regData[15].v += 2;

        /* Garbage collect the memory hash (used only for the stack) */
        UnwMemHashGC(state);

        t--;
        if(t == 0)
        {
            /* TODO: try scanning prologue - here */
            return UNWIND_EXHAUSTED;
        }

    }
    while(!found);

    return UNWIND_SUCCESS;
}

#endif /* UPGRADE_ARM_STACK_UNWIND */

/* END OF FILE */

