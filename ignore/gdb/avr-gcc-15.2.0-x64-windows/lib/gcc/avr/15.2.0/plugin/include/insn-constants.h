/* Generated automatically by the program `genconstants'
   from the machine description file `md'.  */

#ifndef GCC_INSN_CONSTANTS_H
#define GCC_INSN_CONSTANTS_H

#define REG_10 10
#define REG_11 11
#define REG_12 12
#define REG_13 13
#define REG_14 14
#define REG_15 15
#define REG_16 16
#define REG_17 17
#define REG_18 18
#define REG_19 19
#define REG_30 30
#define REG_31 31
#define REG_36 36
#define REG_32 32
#define REG_0 0
#define REG_SP REG_32
#define ZERO_REGNO REG_1
#define TMP_REGNO_TINY REG_16
#define LPM_REGNO REG_0
#define GASISR_Epilogue 2
#define GASISR_Prologue 1
#define REG_20 20
#define REG_21 21
#define REG_22 22
#define REG_23 23
#define REG_24 24
#define REG_25 25
#define REG_26 26
#define REG_27 27
#define REG_28 28
#define REG_29 29
#define REG_8 8
#define REG_9 9
#define REG_CC REG_36
#define ZERO_REGNO_TINY REG_17
#define ACC_A 18
#define ACC_B 10
#define GASISR_Done 0
#define REG_1 1
#define REG_W REG_24
#define REG_X REG_26
#define REG_Y REG_28
#define REG_Z REG_30
#define TMP_REGNO REG_0
#define REG_2 2

enum unspec {
  UNSPEC_STRLEN = 0,
  UNSPEC_CPYMEM = 1,
  UNSPEC_INDEX_JMP = 2,
  UNSPEC_NZB = 3,
  UNSPEC_FMUL = 4,
  UNSPEC_FMULS = 5,
  UNSPEC_FMULSU = 6,
  UNSPEC_INSERT_BITS = 7,
  UNSPEC_ROUND = 8
};
#define NUM_UNSPEC_VALUES 9
extern const char *const unspec_strings[];

enum unspecv {
  UNSPECV_PROLOGUE_SAVES = 0,
  UNSPECV_EPILOGUE_RESTORES = 1,
  UNSPECV_WRITE_SP = 2,
  UNSPECV_GASISR = 3,
  UNSPECV_GOTO_RECEIVER = 4,
  UNSPECV_ENABLE_IRQS = 5,
  UNSPECV_MEMORY_BARRIER = 6,
  UNSPECV_NOP = 7,
  UNSPECV_SLEEP = 8,
  UNSPECV_WDR = 9,
  UNSPECV_DELAY_CYCLES = 10
};
#define NUM_UNSPECV_VALUES 11
extern const char *const unspecv_strings[];

#endif /* GCC_INSN_CONSTANTS_H */
