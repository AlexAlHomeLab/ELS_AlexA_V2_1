/* Generated automatically by the program 'build/genpreds'
   from the machine description file '../../gcc/config/avr/avr.md'.  */

#ifndef GCC_TM_PREDS_H
#define GCC_TM_PREDS_H

#ifdef HAVE_MACHINE_MODES
extern bool general_operand (rtx, machine_mode);
extern bool address_operand (rtx, machine_mode);
extern bool register_operand (rtx, machine_mode);
extern bool pmode_register_operand (rtx, machine_mode);
extern bool scratch_operand (rtx, machine_mode);
extern bool immediate_operand (rtx, machine_mode);
extern bool const_int_operand (rtx, machine_mode);
extern bool const_double_operand (rtx, machine_mode);
extern bool nonimmediate_operand (rtx, machine_mode);
extern bool nonmemory_operand (rtx, machine_mode);
extern bool push_operand (rtx, machine_mode);
extern bool pop_operand (rtx, machine_mode);
extern bool memory_operand (rtx, machine_mode);
extern bool indirect_operand (rtx, machine_mode);
extern bool ordered_comparison_operator (rtx, machine_mode);
extern bool comparison_operator (rtx, machine_mode);
extern bool aligned_register_operand (rtx, machine_mode);
extern bool l_register_operand (rtx, machine_mode);
extern bool d_register_operand (rtx, machine_mode);
extern bool scratch_or_dreg_operand (rtx, machine_mode);
extern bool even_register_operand (rtx, machine_mode);
extern bool odd_register_operand (rtx, machine_mode);
extern bool stack_register_operand (rtx, machine_mode);
extern bool low_io_address_operand (rtx, machine_mode);
extern bool reg_or_low_io_operand (rtx, machine_mode);
extern bool high_io_address_operand (rtx, machine_mode);
extern bool io_address_operand (rtx, machine_mode);
extern bool nop_general_operand (rtx, machine_mode);
extern bool nox_general_operand (rtx, machine_mode);
extern bool const0_operand (rtx, machine_mode);
extern bool const1_operand (rtx, machine_mode);
extern bool const7_operand (rtx, machine_mode);
extern bool const15_operand (rtx, machine_mode);
extern bool const23_operand (rtx, machine_mode);
extern bool const31_operand (rtx, machine_mode);
extern bool const_0_to_7_operand (rtx, machine_mode);
extern bool const_0_to_15_operand (rtx, machine_mode);
extern bool const_0_to_23_operand (rtx, machine_mode);
extern bool const_0_to_31_operand (rtx, machine_mode);
extern bool const_2_to_7_operand (rtx, machine_mode);
extern bool const_1_to_3_operand (rtx, machine_mode);
extern bool const_1_to_6_operand (rtx, machine_mode);
extern bool const_2_to_6_operand (rtx, machine_mode);
extern bool const_m255_to_m1_operand (rtx, machine_mode);
extern bool abs1_abs2_operand (rtx, machine_mode);
extern bool reg_or_0_operand (rtx, machine_mode);
extern bool dreg_or_0_operand (rtx, machine_mode);
extern bool symbol_ref_operand (rtx, machine_mode);
extern bool const_0mod256_operand (rtx, machine_mode);
extern bool text_segment_operand (rtx, machine_mode);
extern bool single_one_operand (rtx, machine_mode);
extern bool single_zero_operand (rtx, machine_mode);
extern bool avr_sp_immediate_operand (rtx, machine_mode);
extern bool eqne_operator (rtx, machine_mode);
extern bool gelt_operator (rtx, machine_mode);
extern bool difficult_comparison_operator (rtx, machine_mode);
extern bool simple_comparison_operator (rtx, machine_mode);
extern bool signed_comparison_operator (rtx, machine_mode);
extern bool extend_operator (rtx, machine_mode);
extern bool op8_ZN_operator (rtx, machine_mode);
extern bool call_insn_operand (rtx, machine_mode);
extern bool pseudo_register_operand (rtx, machine_mode);
extern bool pseudo_register_or_const_int_operand (rtx, machine_mode);
extern bool combine_pseudo_register_operand (rtx, machine_mode);
extern bool const_8_16_24_operand (rtx, machine_mode);
extern bool u8_operand (rtx, machine_mode);
extern bool s8_operand (rtx, machine_mode);
extern bool o8_operand (rtx, machine_mode);
extern bool s9_operand (rtx, machine_mode);
extern bool register_or_s9_operand (rtx, machine_mode);
extern bool u16_operand (rtx, machine_mode);
extern bool s16_operand (rtx, machine_mode);
extern bool o16_operand (rtx, machine_mode);
extern bool const_operand (rtx, machine_mode);
extern bool nonmemory_or_const_operand (rtx, machine_mode);
extern bool const_or_immediate_operand (rtx, machine_mode);
extern bool set_some_operation (rtx, machine_mode);
#endif /* HAVE_MACHINE_MODES */

#ifdef GCC_HARD_REG_SET_H
struct target_constraints {
  HARD_REG_SET register_filters[1];
};

extern struct target_constraints default_target_constraints;
#if SWITCHABLE_TARGET
extern struct target_constraints *this_target_constraints;
#else
#define this_target_constraints (&default_target_constraints)
#endif

#define TEST_REGISTER_FILTER_BIT(ID, REGNO) \
  ((void) (ID), (void) (REGNO), false)

inline bool
test_register_filters (unsigned int, unsigned int)
{
  return true;
}
#endif

#define CONSTRAINT_NUM_DEFINED_P 1
enum constraint_num
{
  CONSTRAINT__UNKNOWN = 0,
  CONSTRAINT_r,
  CONSTRAINT_t,
  CONSTRAINT_b,
  CONSTRAINT_e,
  CONSTRAINT_w,
  CONSTRAINT_d,
  CONSTRAINT_l,
  CONSTRAINT_a,
  CONSTRAINT_x,
  CONSTRAINT_y,
  CONSTRAINT_z,
  CONSTRAINT_q,
  CONSTRAINT_I,
  CONSTRAINT_J,
  CONSTRAINT_K,
  CONSTRAINT_L,
  CONSTRAINT_M,
  CONSTRAINT_N,
  CONSTRAINT_O,
  CONSTRAINT_P,
  CONSTRAINT_m,
  CONSTRAINT_o,
  CONSTRAINT_Q,
  CONSTRAINT_p,
  CONSTRAINT_G,
  CONSTRAINT_Cm2,
  CONSTRAINT_C03,
  CONSTRAINT_C04,
  CONSTRAINT_C05,
  CONSTRAINT_C06,
  CONSTRAINT_C07,
  CONSTRAINT_C7c,
  CONSTRAINT_C14,
  CONSTRAINT_C15,
  CONSTRAINT_C22,
  CONSTRAINT_C23,
  CONSTRAINT_C30,
  CONSTRAINT_C31,
  CONSTRAINT_Ca1,
  CONSTRAINT_Ca2,
  CONSTRAINT_Ca3,
  CONSTRAINT_Ca4,
  CONSTRAINT_Co1,
  CONSTRAINT_Co2,
  CONSTRAINT_Co3,
  CONSTRAINT_Co4,
  CONSTRAINT_Cb1,
  CONSTRAINT_Cb2,
  CONSTRAINT_Cb3,
  CONSTRAINT_Cb4,
  CONSTRAINT_Cx2,
  CONSTRAINT_Cx3,
  CONSTRAINT_Cx4,
  CONSTRAINT_CX2,
  CONSTRAINT_CX3,
  CONSTRAINT_CX4,
  CONSTRAINT_Csp,
  CONSTRAINT_Cxf,
  CONSTRAINT_C0f,
  CONSTRAINT_Cn8,
  CONSTRAINT_Cp8,
  CONSTRAINT_C2a,
  CONSTRAINT_C2r,
  CONSTRAINT_C2l,
  CONSTRAINT_C3a,
  CONSTRAINT_C3r,
  CONSTRAINT_C3l,
  CONSTRAINT_C4a,
  CONSTRAINT_C4r,
  CONSTRAINT_C4l,
  CONSTRAINT_C4R,
  CONSTRAINT_C4L,
  CONSTRAINT_Ynn,
  CONSTRAINT_Y00,
  CONSTRAINT_Yx2,
  CONSTRAINT_YIJ,
  CONSTRAINT_YMM,
  CONSTRAINT_V,
  CONSTRAINT__l,
  CONSTRAINT__g,
  CONSTRAINT_Yil,
  CONSTRAINT_i,
  CONSTRAINT_s,
  CONSTRAINT__c,
  CONSTRAINT_n,
  CONSTRAINT_E,
  CONSTRAINT_F,
  CONSTRAINT_X,
  CONSTRAINT_Y01,
  CONSTRAINT_Ym1,
  CONSTRAINT_Y02,
  CONSTRAINT_Ym2,
  CONSTRAINT_Yxx,
  CONSTRAINT__LIMIT
};

extern enum constraint_num lookup_constraint_1 (const char *);
extern const unsigned char lookup_constraint_array[];

/* Return the constraint at the beginning of P, or CONSTRAINT__UNKNOWN if it
   isn't recognized.  */

static inline enum constraint_num
lookup_constraint (const char *p)
{
  unsigned int index = lookup_constraint_array[(unsigned char) *p];
  return (index == UCHAR_MAX
          ? lookup_constraint_1 (p)
          : (enum constraint_num) index);
}

extern bool (*constraint_satisfied_p_array[]) (rtx);

/* Return true if X satisfies constraint C.  */

static inline bool
constraint_satisfied_p (rtx x, enum constraint_num c)
{
  int i = (int) c - (int) CONSTRAINT_I;
  return i >= 0 && constraint_satisfied_p_array[i] (x);
}

static inline bool
insn_extra_register_constraint (enum constraint_num c)
{
  return c >= CONSTRAINT_r && c <= CONSTRAINT_q;
}

static inline bool
insn_extra_memory_constraint (enum constraint_num c)
{
  return c >= CONSTRAINT_m && c <= CONSTRAINT_Q;
}

static inline bool
insn_extra_special_memory_constraint (enum constraint_num)
{
  return false;
}

static inline bool
insn_extra_relaxed_memory_constraint (enum constraint_num)
{
  return false;
}

static inline bool
insn_extra_address_constraint (enum constraint_num c)
{
  return c >= CONSTRAINT_p && c <= CONSTRAINT_p;
}

static inline void
insn_extra_constraint_allows_reg_mem (enum constraint_num c,
				      bool *allows_reg, bool *allows_mem)
{
  if (c >= CONSTRAINT_G && c <= CONSTRAINT_YMM)
    return;
  if (c >= CONSTRAINT_V && c <= CONSTRAINT_Yil)
    {
      *allows_mem = true;
      return;
    }
  (void) c;
  *allows_reg = true;
  *allows_mem = true;
}

static inline size_t
insn_constraint_len (char fc, const char *str ATTRIBUTE_UNUSED)
{
  switch (fc)
    {
    case 'C': return 3;
    case 'Y': return 3;
    default: break;
    }
  return 1;
}

#define CONSTRAINT_LEN(c_,s_) insn_constraint_len (c_,s_)

extern enum reg_class reg_class_for_constraint_1 (enum constraint_num);

static inline enum reg_class
reg_class_for_constraint (enum constraint_num c)
{
  if (insn_extra_register_constraint (c))
    return reg_class_for_constraint_1 (c);
  return NO_REGS;
}

extern bool insn_const_int_ok_for_constraint (HOST_WIDE_INT, enum constraint_num);
#define CONST_OK_FOR_CONSTRAINT_P(v_,c_,s_) \
    insn_const_int_ok_for_constraint (v_, lookup_constraint (s_))

enum constraint_type
{
  CT_REGISTER,
  CT_CONST_INT,
  CT_MEMORY,
  CT_SPECIAL_MEMORY,
  CT_RELAXED_MEMORY,
  CT_ADDRESS,
  CT_FIXED_FORM
};

static inline enum constraint_type
get_constraint_type (enum constraint_num c)
{
  if (c >= CONSTRAINT_p)
    {
      if (c >= CONSTRAINT_G)
        return CT_FIXED_FORM;
      return CT_ADDRESS;
    }
  if (c >= CONSTRAINT_m)
    return CT_MEMORY;
  if (c >= CONSTRAINT_I)
    return CT_CONST_INT;
  return CT_REGISTER;
}

#ifdef GCC_HARD_REG_SET_H
static inline const HARD_REG_SET *
get_register_filter (constraint_num)
{
  return nullptr;
}
#endif

static inline int
get_register_filter_id (constraint_num)
{
  return -1;
}
#endif /* tm-preds.h */
