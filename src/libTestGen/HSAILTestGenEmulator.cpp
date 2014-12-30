//===-- HSAILTestGenEmulator.cpp - HSAIL Instructions Emulator ------------===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#include "HSAILTestGenEmulator.h"
#include "HSAILTestGenUtilities.h"

#include <cmath>
#include <limits>

using std::numeric_limits;

using Brig::BRIG_TYPE_B1;
using Brig::BRIG_TYPE_B8;
using Brig::BRIG_TYPE_B32;
using Brig::BRIG_TYPE_B64;
using Brig::BRIG_TYPE_B128;
using Brig::BRIG_TYPE_S8;
using Brig::BRIG_TYPE_S16;
using Brig::BRIG_TYPE_S32;
using Brig::BRIG_TYPE_S64;
using Brig::BRIG_TYPE_U8;
using Brig::BRIG_TYPE_U16;
using Brig::BRIG_TYPE_U32;
using Brig::BRIG_TYPE_U64;
using Brig::BRIG_TYPE_F16;
using Brig::BRIG_TYPE_F32;
using Brig::BRIG_TYPE_F64;
using Brig::BRIG_TYPE_U8X4;
using Brig::BRIG_TYPE_U16X2;

using HSAIL_ASM::InstBasic;
using HSAIL_ASM::InstSourceType;
using HSAIL_ASM::InstAtomic;
using HSAIL_ASM::InstCmp;
using HSAIL_ASM::InstCvt;
using HSAIL_ASM::InstImage;
using HSAIL_ASM::InstMem;
using HSAIL_ASM::InstMod;
using HSAIL_ASM::InstBr;

using HSAIL_ASM::isBitType;
using HSAIL_ASM::isFloatType;
using HSAIL_ASM::isPackedType;
using HSAIL_ASM::isSignedType;
using HSAIL_ASM::isUnsignedType;
using HSAIL_ASM::isIntType;
using HSAIL_ASM::getBrigTypeNumBits;
using HSAIL_ASM::getPacking;
using HSAIL_ASM::isSatPacking;
using HSAIL_ASM::getPackedDstDim;
using HSAIL_ASM::getPackedTypeDim;
using HSAIL_ASM::packedType2elementType;
using HSAIL_ASM::packedType2baseType;

//=================================================================================================
//=================================================================================================
//=======================      HOW TO ADD NEW INSTRUCTIONS TO EMULATOR      =======================
//=================================================================================================
//=================================================================================================
//
// Take a look at emulator interface defined in HSAILTestGenEmulator.h
// Each of these interface functions should be extended accordingly.
// The following notes describe how to implement emulation code.
//
// 1. Define a functor with appropriate semantics. ALL FUNCTORS MUST TAKE VALUES OF THE SAME TYPE.
//    This rule is enforced to minimize the number of functor selectors (they are described below).
//    If the instruction being emulated has different types of its arguments, some additional code
//    is required (see discussion of special cases below). The type of returned value MUST match
//    the type of result expected by instruction.
//
//    An example:
//
//          struct op_add {
//              template<typename T> T operator()(T val1, T val2) { return val1 + val2; }
//          };
//
// 2. Choose an appropriate functor selector. Functor selectors convert abstract source values
//    to specific type and invoke corresponding instantiation of functor. There are many kinds
//    of selectors which differ in number of arguments and types.
//    For example:
//
//          emulateUnrOpF       - for f 32/64 unary instructions
//          emulateBinOpF       - for f 32/64 binary instructions
//
//          emulateUnrOpB       - for b 1/32/64 unary instructions
//          emulateBinOpB       - for b 1/32/64 binary instructions
//          emulateTrnOpB       - for b 1/32/64 ternary instructions
//
//    Supported selector types must include all types accepted by instruction being emulated,
//    BUT MAY BE WIDER. Paths for types not supported by instruction will never be taken.
//    BEFORE WRITING A NEW SELECTOR, TRY USING EXISTING ONES WITH WIDER LIST OF SUPPORTED TYPES.
//
//    For example, emulateBinOpBSUF is a suitable selector for instruction "add", though this
//    instruction does not work with 'b' types.
//
// 3. Invoke functor selector passing it the type of source values together with source values
//    and the functor. For example:
//
//          static Val emulate_add(unsigned opcode, unsigned type, Val arg1, Val arg2)
//          {
//              return emulateBinOpBSUF(type, arg1, arg2, op_add());
//          }
//
//=================================================================================================
//
// SPECIAL CASES
//
// 1. Functors must take care of special values such as NaN if these values are not handled
//    by the runtime. For example:
//
//          struct op_max {
//              template<typename T>
//              T operator()(T val1, T val2) {
//                  return Val(val1).isNan()? val1 : Val(val2).isNan()? val2 : std::max(val1, val2);
//              }
//          };
//
// 2. Some instructions have unspecified behaviour for special values.
//    For example, ncos(x) has unspecified precision for values out of the range
//    [NSIN_NCOS_ARG_MIN, NSIN_NCOS_ARG_MAX]. In this case functor should have return
//    type Val and return undefValue. An example:
//
//          struct op_cos {
//              template<typename T>
//              Val operator()(T val) {
//                  return Val(val).isNan()?
//                              Val(val) :
//                              (val < NSIN_NCOS_ARG_MIN || NSIN_NCOS_ARG_MAX < val)? undefValue() : Val(cos(val));
//              }
//          };
//
// 3. Some instructions have several source arguments with different types.
//    To emulate these instructions, there are several possibilities:
//
//      a) Convert all arguments to some generic type before passing values to selector.
//
//      b) Implement emulation of this instruction w/o functors and selectors.
//         Appropriate for very irregular instructions.
//
//      c) Define a non-standard functor selector. Appropriate for very irregular instructions.
//
//=================================================================================================

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================
// Properties of integer types

template<typename T>
struct NumProps
{
    static unsigned width()     { return sizeof(T) * 8; }
    static unsigned shiftMask() { return width() - 1; }
};

template<typename T> static bool isSigned(T val)   { return ((T)-1 < (T)0); }

// Compute number of bits required to represent 'range' values
u32_t range2width(unsigned range) // log2
{
    switch(range)
    {
    case 2:     return 1;
    case 4:     return 2;
    case 8:     return 3;
    case 16:    return 4;
    case 32:    return 5;
    case 64:    return 6;
    default:
        assert(false);
        return 0;
    }
}

u64_t getSignMask(unsigned width)  { return 1ULL << (width - 1); }
u64_t getWidthMask(unsigned width) { return ((width == 64)? 0 : (1ULL << width)) - 1ULL; }
u64_t getRangeMask(unsigned range) { return getWidthMask(range2width(range)); }

//=============================================================================
//=============================================================================
//=============================================================================
// Min and max values for integer types (used for saturating rounding)

static const s64_t S8IB  = 0x7f;
static const s64_t S16IB = 0x7fff;
static const s64_t S32IB = 0x7fffffff;
static const s64_t S64IB = 0x7fffffffffffffffLL;
static const u64_t U8IB  = 0xff;
static const u64_t U16IB = 0xffff;
static const u64_t U32IB = 0xffffffff;
static const u64_t U64IB = 0xffffffffffffffffLL;

static u64_t getIntBoundary(unsigned type, bool low)
{
    switch (type)
    {
    case BRIG_TYPE_S8:  return low? -S8IB  - 1 : S8IB;
    case BRIG_TYPE_S16: return low? -S16IB - 1 : S16IB;
    case BRIG_TYPE_S32: return low? -S32IB - 1 : S32IB;
    case BRIG_TYPE_S64: return low? -S64IB - 1 : S64IB;
    case BRIG_TYPE_U8:  return low? 0          : U8IB;
    case BRIG_TYPE_U16: return low? 0          : U16IB;
    case BRIG_TYPE_U32: return low? 0          : U32IB;
    case BRIG_TYPE_U64: return low? 0          : U64IB;

    default:
        assert(false);
        return 0;
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
//

static const u32_t MAX_U32_F32H = 0x4f7fffff; // max float value (4294967040.0f) which does not exceed U32IB = 4294967295
static const u32_t MAX_U64_F32H = 0x5f7fffff; // max float value (18446742974197923840.0f) which does not exceed U64IB = 18446744073709551615

static const u32_t MAX_S32_F32H = 0x4effffff; // max float value (2147483520.0f) which does not exceed S32IB = 2147483647
static const u32_t MAX_S64_F32H = 0x5effffff; // max float value (18446742974197923840.0f) which does not exceed S64IB = 9223372036854775807

static const u32_t MIN_S32_F32H = 0xcf000000; // min float value (2147483520.0f) which is greater than or equal to -(S32IB+1) = -2147483648
static const u32_t MIN_S64_F32H = 0xdf000000; // min float value (18446742974197923840.0f) which is greater than or equal to -(S64IB+1) = -9223372036854775808

static const u64_t MAX_U64_F64H = 0x43efffffffffffff; // max float value which does not exceed U64IB = 18446744073709551615
static const u64_t MAX_S64_F64H = 0x43dfffffffffffff; // max float value which does not exceed S64IB = 9223372036854775807
static const u64_t MIN_S64_F64H = 0xc3e0000000000000; // min float value which is greater than or equal to -(S64IB+1) = -9223372036854775808

#define HEX2F32(x) (*reinterpret_cast<const float*>(&x))
#define HEX2F64(x) (*reinterpret_cast<const double*>(&x))

static const float MIN_S8_F32  = -128.0f;
static const float MAX_S8_F32  =  127.0f;

static const float MIN_S16_F32 = -32768.0f;
static const float MAX_S16_F32 =  32767.0f;

static const float MIN_S32_F32 = HEX2F32(MIN_S32_F32H);
static const float MAX_S32_F32 = HEX2F32(MAX_S32_F32H);

static const float MIN_S64_F32 = HEX2F32(MIN_S64_F32H);
static const float MAX_S64_F32 = HEX2F32(MAX_S64_F32H);

static const float MAX_U8_F32  = 255.0f;
static const float MAX_U16_F32 = 65535.0f;
static const float MAX_U32_F32 = HEX2F32(MAX_U32_F32H);
static const float MAX_U64_F32 = HEX2F32(MAX_U64_F32H);

static const float MIN_U8_F32  = 0.0f;
static const float MIN_U16_F32 = 0.0f;
static const float MIN_U32_F32 = 0.0f;
static const float MIN_U64_F32 = 0.0f;

static const double MIN_S8_F64  = -128.0;
static const double MAX_S8_F64  =  127.0;

static const double MIN_S16_F64 = -32768.0;
static const double MAX_S16_F64 =  32767.0;

static const double MIN_S32_F64 = -2147483648.0;
static const double MAX_S32_F64 =  2147483647.0;

static const double MIN_S64_F64 = HEX2F64(MIN_S64_F64H);
static const double MAX_S64_F64 = HEX2F64(MAX_S64_F64H);

static const double MAX_U8_F64  = 255.0;
static const double MAX_U16_F64 = 65535.0;
static const double MAX_U32_F64 = 4294967295.0;
static const double MAX_U64_F64 = HEX2F64(MAX_U64_F64H);

static const double MIN_U8_F64  = 0.0;
static const double MIN_U16_F64 = 0.0;
static const double MIN_U32_F64 = 0.0;
static const double MIN_U64_F64 = 0.0;

//=============================================================================
//=============================================================================
//=============================================================================
//

static float getTypeBoundary_f32(unsigned type, bool isLo)
{
    switch (type)
    {
    case BRIG_TYPE_S8:   return isLo? MIN_S8_F32  : MAX_S8_F32;
    case BRIG_TYPE_S16:  return isLo? MIN_S16_F32 : MAX_S16_F32;
    case BRIG_TYPE_S32:  return isLo? MIN_S32_F32 : MAX_S32_F32;
    case BRIG_TYPE_S64:  return isLo? MIN_S64_F32 : MAX_S64_F32;
    case BRIG_TYPE_U8:   return isLo? MIN_U8_F32  : MAX_U8_F32;
    case BRIG_TYPE_U16:  return isLo? MIN_U16_F32 : MAX_U16_F32;
    case BRIG_TYPE_U32:  return isLo? MIN_U32_F32 : MAX_U32_F32;
    case BRIG_TYPE_U64:  return isLo? MIN_U64_F32 : MAX_U64_F32;

    default:
        assert(false);
        return false;
    }
}

static double getTypeBoundary_f64(unsigned type, bool isLo)
{
    switch (type)
    {
    case BRIG_TYPE_S8:   return isLo? MIN_S8_F64  : MAX_S8_F64;
    case BRIG_TYPE_S16:  return isLo? MIN_S16_F64 : MAX_S16_F64;
    case BRIG_TYPE_S32:  return isLo? MIN_S32_F64 : MAX_S32_F64;
    case BRIG_TYPE_S64:  return isLo? MIN_S64_F64 : MAX_S64_F64;
    case BRIG_TYPE_U8:   return isLo? MIN_U8_F64  : MAX_U8_F64;
    case BRIG_TYPE_U16:  return isLo? MIN_U16_F64 : MAX_U16_F64;
    case BRIG_TYPE_U32:  return isLo? MIN_U32_F64 : MAX_U32_F64;
    case BRIG_TYPE_U64:  return isLo? MIN_U64_F64 : MAX_U64_F64;

    default:
        assert(false);
        return false;
    }
}

template<typename T>
static T getTypeBoundary(unsigned type, bool isLo)
{
    if (sizeof(T) == 4) return (T)getTypeBoundary_f32(type, isLo);
    if (sizeof(T) == 8) return (T)getTypeBoundary_f64(type, isLo);

    assert(false);
    return (T)0.0f;
}

// Return true if integer part of val (i.e. val w/o fractional part)
// is within boundaries of the specified type.
// For example, -0,999 is within bounds of u8 [0..255]
template<typename T>
static bool checkTypeBoundaries(unsigned type, T val)
{
    return ((getTypeBoundary<T>(type, true)     <= val)  || // case a: boundary is too large for mantissa
            (getTypeBoundary<T>(type, true) - 1 <  val)) && // case b: boundary is less than max mantissa, so take care of fractional part of val
           ((val <= getTypeBoundary<T>(type, false)    ) ||
            (val <  getTypeBoundary<T>(type, false) + 1));
}

//=============================================================================
//=============================================================================
//=============================================================================

static const unsigned ROUNDING_TESTS_NUM = 12;

unsigned getRoundingTestsNum(unsigned dstType)
{
    return (isSignedType(dstType) || isUnsignedType(dstType))? ROUNDING_TESTS_NUM : 1;
}

template<typename T>
const void makeRoundingTestsData(unsigned dstType, AluMod aluMod, T* dst)
{
    if (getRoundingTestsNum(dstType) == 1)
    {
        dst[0] = (T)0.0f; // dummy test data (cannot return empty list)
        return;
    }

    T lo = getTypeBoundary<T>(dstType, true);
    T hi = getTypeBoundary<T>(dstType, false);

    switch(aluMod.getRounding())
    {
    case AluMod::ROUNDING_NEARI:
    case AluMod::ROUNDING_NEARI_SAT:
    case AluMod::ROUNDING_SNEARI:
    case AluMod::ROUNDING_SNEARI_SAT:
                                        lo += 0.5f;
                                        hi += 0.5f;
                                        break;

    case AluMod::ROUNDING_ZEROI:
    case AluMod::ROUNDING_ZEROI_SAT:
    case AluMod::ROUNDING_SZEROI:
    case AluMod::ROUNDING_SZEROI_SAT:
                                        if (lo > 0) lo += 1.0f;
                                        if (hi > 0) hi += 1.0f;
                                        break;

    case AluMod::ROUNDING_DOWNI:
    case AluMod::ROUNDING_DOWNI_SAT:
    case AluMod::ROUNDING_SDOWNI:
    case AluMod::ROUNDING_SDOWNI_SAT:
                                        lo += 1.0f;
                                        hi += 1.0f;
                                        break;

    case AluMod::ROUNDING_UPI:
    case AluMod::ROUNDING_UPI_SAT:
    case AluMod::ROUNDING_SUPI:
    case AluMod::ROUNDING_SUPI_SAT:
                                        break;

    default:
        assert(false);
        break;
    }

    dst[0] = lo - 1;
    dst[1] = Val(lo - 1).ulp(1);
    dst[2] = Val(lo).ulp(-1);
    dst[3] = lo;
    dst[4] = Val(lo).ulp(1);
    dst[5] = lo + 1;
    dst[6] = hi - 1;
    dst[7] = Val(hi).ulp(-1);
    dst[8] = hi;
    dst[9] = Val(hi).ulp(1);
    dst[10] = Val(hi + 1).ulp(-1);
    dst[11] = hi + 1;

}

static float  f_rounding_tests[ROUNDING_TESTS_NUM];
static double d_rounding_tests[ROUNDING_TESTS_NUM];

const float* getF32RoundingTestsData(unsigned dstType, AluMod aluMod)
{
    makeRoundingTestsData(dstType, aluMod, f_rounding_tests);
    return f_rounding_tests;
}

const double* getF64RoundingTestsData(unsigned dstType, AluMod aluMod)
{
    makeRoundingTestsData(dstType, aluMod, d_rounding_tests);
    return d_rounding_tests;
}

//=============================================================================
//=============================================================================
//=============================================================================

static const f64_t PI       = 3.1415926535897932384626433832795;
static const f64_t LN2      = 0.693147180559945309417;        // ln(2)

// Limits of normalized floats.
// Values between (FLT_MAX_NEG_NORM, 0) and (0,FLT_MIN_POS_NORM) are denormals.
static const unsigned FLT_MAX_NEG_NORM_BITS_ = 0x80800000;
static const unsigned FLT_MIN_POS_NORM_BITS_ = 0x00800000;
#define FLT_MAX_NEG_NORM (*reinterpret_cast<const float*>(&FLT_MAX_NEG_NORM_BITS_))
#define FLT_MIN_POS_NORM (*reinterpret_cast<const float*>(&FLT_MIN_POS_NORM_BITS_))
#define IS_FLT_DENORM(x) (FLT_MAX_NEG_NORM < (x) && (x) < FLT_MIN_POS_NORM && (x) != 0.0F)

//=============================================================================
//=============================================================================
//=============================================================================

static Val emulationFailed()                    // This should not happen
{
    assert(false);
    return Val();
}

static Val undefValue()    { return Val(); }    // Value returned for unspecified behaviour
static Val unimplemented() { return Val(); }    // Value returned for unimplemented features
static Val emptyDstValue() { return Val(); }    // Value returned when instruction has no destination register(s)
static Val emptyMemValue() { return Val(); }    // Value returned when instruction does not affect memory

//=============================================================================
//=============================================================================
//=============================================================================
// Functor Selectors for Unary Ops

template<class T>
static Val emulateUnrOpUS(unsigned type, Val arg, T op)
{
    assert(arg.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_U8:  return op(arg.u8());
    case BRIG_TYPE_S8:  return op(arg.s8());
    case BRIG_TYPE_U16: return op(arg.u16());
    case BRIG_TYPE_S16: return op(arg.s16());
    case BRIG_TYPE_U32: return op(arg.u32());
    case BRIG_TYPE_S32: return op(arg.s32());
    case BRIG_TYPE_U64: return op(arg.u64());
    case BRIG_TYPE_S64: return op(arg.s64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateUnrOpSF(unsigned type, Val arg, T op)
{
    assert(arg.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_S32: return op(arg.s32());
    case BRIG_TYPE_S64: return op(arg.s64());
    case BRIG_TYPE_F32: return op(arg.f32());
    case BRIG_TYPE_F64: return op(arg.f64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateUnrOpF(unsigned type, Val arg, T op)
{
    assert(arg.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_F32: return op(arg.f32());
    case BRIG_TYPE_F64: return op(arg.f64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateUnrOpB(unsigned type, Val arg, T op)
{
    assert(arg.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_B1:  return op(arg.b1());
    case BRIG_TYPE_B32: return op(arg.b32());
    case BRIG_TYPE_B64: return op(arg.b64());

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Functor Selectors for Binary Ops

template<class T>
static Val emulateBinOpBSUF(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_B1:  return op(arg1.b1(),  arg2.b1());
    case BRIG_TYPE_B32: return op(arg1.b32(), arg2.b32());
    case BRIG_TYPE_B64: return op(arg1.b64(), arg2.b64());

    case BRIG_TYPE_S32: return op(arg1.s32(), arg2.s32());
    case BRIG_TYPE_S64: return op(arg1.s64(), arg2.s64());

    case BRIG_TYPE_U32: return op(arg1.u32(), arg2.u32());
    case BRIG_TYPE_U64: return op(arg1.u64(), arg2.u64());

    case BRIG_TYPE_F32: return op(arg1.f32(), arg2.f32());
    case BRIG_TYPE_F64: return op(arg1.f64(), arg2.f64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpF(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_F32: return op(arg1.f32(), arg2.f32());
    case BRIG_TYPE_F64: return op(arg1.f64(), arg2.f64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpB(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_B1:  return op(arg1.b1(),  arg2.b1());
    case BRIG_TYPE_B32: return op(arg1.b32(), arg2.b32());
    case BRIG_TYPE_B64: return op(arg1.b64(), arg2.b64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpSU(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_S32: return op(arg1.s32(), arg2.s32());
    case BRIG_TYPE_U32: return op(arg1.u32(), arg2.u32());

    case BRIG_TYPE_S64: return op(arg1.s64(), arg2.s64());
    case BRIG_TYPE_U64: return op(arg1.u64(), arg2.u64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpSU_U32(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == BRIG_TYPE_U32);

    switch (type)
    {
    case BRIG_TYPE_S32: return op(arg1.s32(), arg2.u32());
    case BRIG_TYPE_U32: return op(arg1.u32(), arg2.u32());

    case BRIG_TYPE_S64: return op(arg1.s64(), arg2.u32());
    case BRIG_TYPE_U64: return op(arg1.u64(), arg2.u32());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpSat(unsigned elementType, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == elementType);
    assert(arg2.getType() == elementType);

    switch (elementType)
    {
    case BRIG_TYPE_S8:  return op(elementType, arg1.s8(), arg2.s8());
    case BRIG_TYPE_U8:  return op(elementType, arg1.u8(), arg2.u8());

    case BRIG_TYPE_S16: return op(elementType, arg1.s16(), arg2.s16());
    case BRIG_TYPE_U16: return op(elementType, arg1.u16(), arg2.u16());

    case BRIG_TYPE_S32: return op(elementType, arg1.s32(), arg2.s32());
    case BRIG_TYPE_U32: return op(elementType, arg1.u32(), arg2.u32());

    case BRIG_TYPE_S64: return op(elementType, arg1.s64(), arg2.s64());
    case BRIG_TYPE_U64: return op(elementType, arg1.u64(), arg2.u64());

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Functor Selectors for Ternary Ops

template<class T>
static Val emulateTrnOpSUF(unsigned type, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_S32: return op(arg1.s32(), arg2.s32(), arg3.s32());
    case BRIG_TYPE_S64: return op(arg1.s64(), arg2.s64(), arg3.s64());

    case BRIG_TYPE_U32: return op(arg1.u32(), arg2.u32(), arg3.u32());
    case BRIG_TYPE_U64: return op(arg1.u64(), arg2.u64(), arg3.u64());

    case BRIG_TYPE_F32: return op(arg1.f32(), arg2.f32(), arg3.f32());
    case BRIG_TYPE_F64: return op(arg1.f64(), arg2.f64(), arg3.f64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateTrnOpSU(unsigned type, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_S32: return op(arg1.s32(), arg2.s32(), arg3.s32());
    case BRIG_TYPE_S64: return op(arg1.s64(), arg2.s64(), arg3.s64());

    case BRIG_TYPE_U32: return op(arg1.u32(), arg2.u32(), arg3.u32());
    case BRIG_TYPE_U64: return op(arg1.u64(), arg2.u64(), arg3.u64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateTrnOpB(unsigned type, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_B1:  return op(arg1.b1(),  arg2.b1(),  arg3.b1());
    case BRIG_TYPE_B32: return op(arg1.b32(), arg2.b32(), arg3.b32());
    case BRIG_TYPE_B64: return op(arg1.b64(), arg2.b64(), arg3.b64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateTrnOpSU_U32_U32(unsigned type, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == BRIG_TYPE_U32);
    assert(arg3.getType() == BRIG_TYPE_U32);

    switch (type)
    {
    case BRIG_TYPE_S32: return op(arg1.s32(), arg2.u32(), arg3.u32());
    case BRIG_TYPE_S64: return op(arg1.s64(), arg2.u32(), arg3.u32());

    case BRIG_TYPE_U32: return op(arg1.u32(), arg2.u32(), arg3.u32());
    case BRIG_TYPE_U64: return op(arg1.u64(), arg2.u32(), arg3.u32());

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Functor Selectors for Quaternary Ops

template<class T>
static Val emulateQrnOpSU_U32_U32(unsigned type, Val arg1, Val arg2, Val arg3, Val arg4, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == BRIG_TYPE_U32);
    assert(arg4.getType() == BRIG_TYPE_U32);

    switch (type)
    {
    case BRIG_TYPE_S32: return op(arg1.s32(), arg2.s32(), arg3.u32(), arg4.u32());
    case BRIG_TYPE_S64: return op(arg1.s64(), arg2.s64(), arg3.u32(), arg4.u32());

    case BRIG_TYPE_U32: return op(arg1.u32(), arg2.u32(), arg3.u32(), arg4.u32());
    case BRIG_TYPE_U64: return op(arg1.u64(), arg2.u64(), arg3.u32(), arg4.u32());

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Functors Emulating HSAIL Instructions

template<typename T> T undef_div_rem(T val1, T val2) // Identify special cases for integer div/rem
{
    if (!Val(val1).isInt()) return false;
    if (val2 == (T)0) return true;
    return Val(val1).isSignedInt() && val1 == numeric_limits<T>::min() && val2 == (T)-1;
}

struct op_abs    { template<typename T> T operator()(T val)                  { return std::abs(val); }};
struct op_neg    { template<typename T> T operator()(T val)                  { return -val; }};

struct op_not    { template<typename T> T operator()(T val)                  { return static_cast<T>(val ^ 0xffffffffffffffffULL); }};

struct op_add    { template<typename T> T   operator()(T val1, T val2)       { return val1 + val2; }};
struct op_sub    { template<typename T> T   operator()(T val1, T val2)       { return val1 - val2; }};
struct op_mul    { template<typename T> T   operator()(T val1, T val2)       { return val1 * val2; }};
struct op_div    { template<typename T> Val operator()(T val1, T val2)       { return undef_div_rem(val1, val2)? undefValue() : Val(val1 / val2); }};
struct op_rem    { template<typename T> Val operator()(T val1, T val2)       { return undef_div_rem(val1, val2)? ((val2 == (T)0)? undefValue() : Val((T)0)) : Val(val1 % val2); }};
struct op_max    { template<typename T> T   operator()(T val1, T val2)       { return Val(val1).isNan()? val2 : Val(val2).isNan()? val1 : std::max(val1, val2); }};
struct op_min    { template<typename T> T   operator()(T val1, T val2)       { return Val(val1).isNan()? val2 : Val(val2).isNan()? val1 : std::min(val1, val2); }};
struct op_arg1   { template<typename T> T   operator()(T val1, T val2)       { return val1; }};
struct op_arg2   { template<typename T> T   operator()(T val1, T val2)       { return val2; }};

struct op_and    { template<typename T> T operator()(T val1, T val2)         { return val1 & val2; }};
struct op_or     { template<typename T> T operator()(T val1, T val2)         { return val1 | val2; }};
struct op_xor    { template<typename T> T operator()(T val1, T val2)         { return val1 ^ val2; }};

struct op_inc    { template<typename T> T operator()(T mem, T max)           { return (mem >= max)? 0 : mem + 1; }};
struct op_dec    { template<typename T> T operator()(T mem, T max)           { return (mem == 0 || mem > max)? max : mem - 1; }};

struct op_cas    { template<typename T> T operator()(T mem, T val1, T val2)  { return (mem == val1)? val2 : mem; }};

struct op_cmov   { template<typename T> T operator()(T val1, T val2, T val3) { return val1? val2 : val3; }};

struct op_cmp    { template<typename T> int operator()(T val1, T val2)       { return val1 < val2 ? -1 : val1 > val2 ? 1 : 0; }}; // NB: NaNs are handled separately

struct op_copysign { template<typename T> T operator()(T val1, T val2)       { return Val(val1).copySign(Val(val2)); }};

struct op_carry  { template<typename T> T operator()(T val1, T val2)         { assert(!isSigned(val1)); T res = val1 + val2; return res < val1; }};
struct op_borrow { template<typename T> T operator()(T val1, T val2)         { assert(!isSigned(val1)); return val1 < val2; }};

struct op_shl    { template<typename T> T operator()(T val, unsigned shift){ return val << (shift & NumProps<T>::shiftMask()); }};
struct op_shr    { template<typename T> T operator()(T val, unsigned shift){ return val >> (shift & NumProps<T>::shiftMask()); }};

struct op_mad    { template<typename T> T operator()(T val1, T val2, T val3) { return val1 * val2 + val3; }};

struct op_sqrt   { template<typename T> T   operator()(T val)                { return sqrt(val); }};
struct op_log2   { template<typename T> T   operator()(T val)                { return log(val) / static_cast<T>(LN2); }};
struct op_exp2   { template<typename T> T   operator()(T val)                { return exp(val * static_cast<T>(LN2)); }};
struct op_rsqrt  { template<typename T> T   operator()(T val)                { return static_cast<T>(1.0) / sqrt(val); }};
struct op_rcp    { template<typename T> T   operator()(T val)                { return static_cast<T>(1.0) / val; }};

struct op_i2f32  { template<typename T> f32_t operator()(T val)              { return 0.0f + val; }};
struct op_i2f64  { template<typename T> f64_t operator()(T val)              { return 0.0  + val; }};

//=============================================================================
//=============================================================================
//=============================================================================
// Functors Emulating Native Sin and Cos

/// HSAIL spec set no requirements for nsin/ncos WRT range of arguments and precision.
/// Actual traits of nsin/ncos depend on HSA JIT implementation.
/// The following values describe existing implementation and set boundaries for testing:
static const unsigned NSIN_NCOS_RESULT_PRECISION_ULPS = 8192 + 1;

/// Precision is guaranteed only for inputs within:
static const f64_t NSIN_NCOS_ARG_MAX =  PI;
static const f64_t NSIN_NCOS_ARG_MIN = -PI;

/// Precision is unspecified when argument or result is very close to the zero.
/// \note HSA JIT implementation details: These values ensure that denorms would
/// not appear at V_SIN/COS_F32 inputs and outputs.

#define IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(x) \
  ((FLT_MAX_NEG_NORM*2*(float)PI) < (x) && (x) < (FLT_MIN_POS_NORM*2*(float)PI) && (x) != 0.0F)

#define IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(x) \
  (IS_FLT_DENORM(x))

class op_cos
{
    /// [atamazov 24 Mar 2014] C/C++ cos(~(N+0.5)*PI) is too rough. For example,
    /// HSAIL ncos(0.5*PI) returns 0, while C/C++ cos() returns something like 8e-08.
    /// The deviation is hundreds thousands ULPs which is obviously too much.
    /// The same problem occus with nsin(~N*PI). cos_precise_near_zero() and
    /// sin_precise_near_zero() are attempts to fix the problem, which seem
    /// acceptable for now.
    float cos_precise_near_zero(const float x)
    {
        const float pi      = (float)PI;
        const float half_pi = (float)(0.5*PI);

        // find integer N for which N*PI < x <= (N+1)*PI

        const float x_offset = (x >=0) ? 0 : -pi;
        const int n = (int)((x + x_offset) / pi);

        // find error of regular cos() in the middle, at (N+0.5)*PI

        const float middle = n*pi + half_pi;
        const float errN = 0 - cos(middle);

        // linear error correction in the [N*PI, (N+1)*PI] range.
        // compensation should be 0.0 at the ends and errN in the middle.

        const float distance_from_the_middle = (x >= middle) ? (x - middle) : (middle - x);
        const float compensation = errN * (1 - distance_from_the_middle / (half_pi));

        return cos(x) + compensation;
    }

public:
    template<typename T> Val operator()(T x)
    {
        const float val = (float)x;

        if (Val(val).isNan()) return Val(val);
        if (val < NSIN_NCOS_ARG_MIN || NSIN_NCOS_ARG_MAX < val) return undefValue();
        if (IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(val)) return undefValue();

        const float cosine = cos_precise_near_zero(val);
        if (IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(cosine)) return undefValue();

        return Val(cosine);
    }
};

class op_sin
{
    float sin_precise_near_zero(const float x)
    {
        const float pi      = (float)PI;
        const float half_pi = (float)(0.5*PI);

        // find integer N for which (N-0.5)*PI < x <= (N+0.5)*PI

        const float x_offset = (x >=0) ? half_pi : -half_pi;
        const int n = (int)((x + x_offset) / pi); // rounded to nearest, e.g. (int)1.2*PI/PI == 1

        // find error of regular sin() in the middle, at N*PI

        const float middle = n*pi;
        const float errN = 0 - sin(middle);

        // linear error correction in the [(N-0.5)*PI, (N+0.5)*PI] range.
        // compensation should be 0.0 at the ends and errN in the middle.

        const float distance_from_the_middle = (x >= middle) ? (x - middle) : (middle - x);
        const float compensation = errN * (1 - distance_from_the_middle / (half_pi));

        return sin(x) + compensation;
    }

public:
    template<typename T> Val operator()(T x)
    {
        const float val = (float)x;

        if (Val(val).isNan()) return Val(val);
        if (val < NSIN_NCOS_ARG_MIN || NSIN_NCOS_ARG_MAX < val) return undefValue();
        if (IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(val)) return undefValue();

        const float sine = (float)sin_precise_near_zero(val);
        if (IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(sine)) return undefValue();

        return Val(sine);
    }
};

//=============================================================================
//=============================================================================
//=============================================================================
// Saturating versions of add, sub and mul (used for packed operands)

struct op_add_sat
{
    template<typename T>
    T operator()(unsigned type, T val1, T val2)
    {
        assert(getBrigTypeNumBits(type) == sizeof(T) * 8);

        T res  = (T)(val1 + val2);

        int sat = 0;

        if      (!isSigned(val1) && res < val1)                         sat =  1;
        else if (isSigned(val1)  && val1 >= 0 && val2 >= 0 && res <  0) sat =  1;
        else if (isSigned(val1)  && val1 <  0 && val2 <  0 && res >= 0) sat = -1;

        if (sat != 0) res = (T)getIntBoundary(type, sat == -1);

        return res;
    }
};

struct op_sub_sat
{
    template<typename T>
    T operator()(unsigned type, T val1, T val2)
    {
        assert(getBrigTypeNumBits(type) == sizeof(T) * 8);

        T res  = (T)(val1 - val2);

        int sat = 0;

        if      (!isSigned(val1) && res > val1)                         sat = -1;
        else if (isSigned(val1)  && val1 >= 0 && val2 <  0 && res <  0) sat =  1;
        else if (isSigned(val1)  && val1 <  0 && val2 >= 0 && res >= 0) sat = -1;

        if (sat != 0) res = (T)getIntBoundary(type, sat == -1);

        return res;
    }
};

struct op_mul_sat
{
    template<typename T>
    T operator()(unsigned type, T val1, T val2)
    {
        assert(getBrigTypeNumBits(type) == sizeof(T) * 8);

        T res = (T)(val1 * val2);

        int sat = 0;

        if (isSigned(val1))
        {
            T min = (T)getSignMask(getBrigTypeNumBits(type));

            if ((val1 < 0 && val2 == min) || // min negative value is a special case
                (val1 != 0 && ((T)(res / val1) != val2)))
            {
                sat = ((val1 < 0) != (val2 < 0))? -1 : 1;
            }
        }
        else // unsigned
        {
            if (val1 != 0 && (res / val1) != val2) sat = 1;
        }

        if (sat != 0) res = (T)getIntBoundary(type, sat == -1);

        return res;
    }
};

//=============================================================================
//=============================================================================
//=============================================================================

template<typename DT>
struct op_bitmask
{
    template<typename T>
    Val operator()(T val1, T val2)
    {
        u64_t offset = val1 & NumProps<DT>::shiftMask();
        u64_t width  = val2 & NumProps<DT>::shiftMask();
        u64_t mask   = (1ULL << width) - 1;

        if (offset + width > NumProps<DT>::width()) return undefValue();

        unsigned dstType = Val(DT(0)).getType();
        return Val(dstType, mask << offset);
    }
};

struct op_bitsel { template<typename T> T operator()(T val1, T val2, T val3) { return (val2 & val1) | (val3 & (~val1)); }};

struct op_bitextract
{
    template<typename T>
    Val operator()(T val1, unsigned val2, unsigned val3)
    {
        u64_t offset    = val2 & NumProps<T>::shiftMask();
        u64_t width     = val3 & NumProps<T>::shiftMask();

        if (width == 0) return Val(static_cast<T>(0));
        if (width + offset > NumProps<T>::width()) return undefValue();

        u64_t shift = NumProps<T>::width() - width;
        return Val(static_cast<T>((val1 << (shift - offset)) >> shift));
    }
};

struct op_bitinsert
{
    template<typename T>
    Val operator()(T val1, T val2, unsigned val3, unsigned val4)
    {
        u64_t offset    = val3 & NumProps<T>::shiftMask();
        u64_t width     = val4 & NumProps<T>::shiftMask();
        u64_t mask      = (1ULL << width) - 1;

        if (width + offset > NumProps<T>::width()) return undefValue();

        u64_t res = (val1 & ~(mask << offset)) | ((val2 & mask) << offset);
        return Val(static_cast<T>(res));
    }
};

struct op_bitrev
{
    template<typename T>
    T operator()(T val)
    {
        T res = 0;
        for (unsigned i = 0; i < sizeof(T) * 8; i++, val = val >> 1) res = (res << 1) | (val & 0x1);
        return res;
    }
};

struct op_bitalign
{
    unsigned shift_mask;
    unsigned element_width;

    op_bitalign(unsigned mask, unsigned width = 1) : shift_mask(mask), element_width(width) {}

    template<typename T>
    T operator()(T val0, T val1, T val2)
    {
        assert(sizeof(T) == 4);

        uint32_t shift = (val2 & shift_mask) * element_width;
        uint64_t value = (static_cast<uint64_t>(val1) << 32) | val0;

        return static_cast<uint32_t>((value >> shift) & 0xffffffff);
    }
};

//=============================================================================
//=============================================================================
//=============================================================================

template<typename T> static bool isSU24(T val)
{
    if (isSigned(val)) {
        return -0x400000 <= (signed) val && (signed) val <= 0x3FFFFF;
    } else {
        return val <= 0x7FFFFF;
    }
}

struct op_mad24
{
    unsigned res_shift;

    op_mad24(unsigned shift) : res_shift(shift) {}

    template<typename T>
    Val operator()(T val1, T val2, T val3)
    {
        assert(sizeof(T) == 4);

        if (isSU24(val1) && isSU24(val2) && isSU24(val3))
        {
            if (isSigned(val1)) {
                return static_cast<T>(((static_cast<s64_t>(val1) * val2) >> res_shift) + val3);
            } else {
                return static_cast<T>(((static_cast<u64_t>(val1) * val2) >> res_shift) + val3);
            }
        }
        else
        {
            return undefValue();
        }
    }
};

/// Multiplies x, by a uint64_t integer and places the result into dest.
static void mul64hi(uint64_t *res, uint64_t x, uint64_t y)
{
    if (x == 0 || y == 0)
    {
        res[0] = 0;
        res[1] = 0;
        return;
    }

    // Split y into high 32-bit part (hy)  and low 32-bit part (ly)
    uint64_t ly = y & 0xffffffffULL, hy = y >> 32;
    uint64_t carry = 0;

    // For each digit of x.
    // Split x into high and low words
    uint64_t lx = x & 0xffffffffULL;
    uint64_t hx = x >> 32;
    // hasCarry - A flag to indicate if there is a carry to the next digit.
    // hasCarry == 0, no carry
    // hasCarry == 1, has carry
    // hasCarry == 2, no carry and the calculation result == 0.
    uint8_t hasCarry = 0;
    res[0] = carry + lx * ly;
    // Determine if the add above introduces carry.
    hasCarry = (res[0] < carry) ? 1 : 0;
    carry = hx * ly + (res[0] >> 32) + (hasCarry ? (1ULL << 32) : 0);
    // The upper limit of carry can be (2^32 - 1)(2^32 - 1) +
    // (2^32 - 1) + 2^32 = 2^64.
    hasCarry = (!carry && hasCarry) ? 1 : (!carry ? 2 : 0);

    carry += (lx * hy) & 0xffffffffULL;
    res[0] = (carry << 32) | (res[0] & 0xffffffffULL);
    carry = (((!carry && hasCarry != 2) || hasCarry == 1) ? (1ULL << 32) : 0) +
            (carry >> 32) + ((lx * hy) >> 32) + hx * hy;

    res[1] = carry;
}

static void neg64(uint64_t *val)
{
    bool borrow = false;
    for (unsigned i = 0; i < 2; ++i)
    {
        uint64_t x_tmp = borrow ? -1 : 0;
        borrow = (val[i] > x_tmp) || borrow;
        val[i] = x_tmp - val[i];
    }
}

struct op_mulhi
{
    template<typename T>
    T operator()(T val1, T val2)
    {
        uint64_t res;
        bool sgn = isSigned(val1);
        unsigned bits = sizeof(T) * 8;

        if (bits <= 32)
        {
            uint64_t x = sgn ? (uint64_t)(int64_t)(val1) : (uint64_t)val1;
            uint64_t y = sgn ? (uint64_t)(int64_t)(val2) : (uint64_t)val2;
            res = ((x * y) >> bits);
        }
        else
        {
            uint64_t x = sgn ? (val1 < 0? (uint64_t)(0 - val1) : (uint64_t)val1) : (uint64_t)val1;
            uint64_t y = sgn ? (val2 < 0? (uint64_t)(0 - val2) : (uint64_t)val2) : (uint64_t)val2;
            bool isNegativeRes = sgn && (val1 < 0) != (val2 < 0) && val1 != 0 && val2 != 0;

            uint64_t dst[2];
            mul64hi(dst, x, y);
            if (isNegativeRes) neg64(dst);

            res = dst[1];
        }

        return *(const T*)&res;
    }
};

//=============================================================================
//=============================================================================
//=============================================================================


//F must return POS_ZERO when src is POS_INF
//F must return NEG_ZERO when src is NEG_INF
struct op_fract
{
    template<typename T>
    Val operator()(T val)
    {
        if (Val(val).isNan()) return Val(val); // preserve NaN payload

        // Handling of infinity is not specified by HSAIL but required by OpenCL 2.0.
        // See bug 10282
        if (Val(val).isPositiveInf()) return Val(val).getPositiveZero();
        if (Val(val).isNegativeInf()) return Val(val).getNegativeZero();

        T unused;
        T res = std::modf(val, &unused);
        T one = 1; // NB: avoid casting to double

        if (val > 0)       return Val(res);
        if (res == (T)0)   return Val((T)0); // NB: res may be -0!

        T x = one + res;
        if (x < one) return Val(x);

        // Fractional part is so small that (1 + res) have to be rounded to 1.
        // Return the largest representable number which is less than 1.0
        if (sizeof(T) == 4) return Val(BRIG_TYPE_F32, 0x3F7FFFFFULL);
        if (sizeof(T) == 8) return Val(BRIG_TYPE_F64, 0x3FEFFFFFFFFFFFFFULL);

        assert(false);
        return emulationFailed();
    }
};

struct op_ceil
{
    template<typename T>
    Val operator()(T val)
    {
        if (Val(val).isNan() || Val(val).isInf()) return Val(val);

        T res;
        T fract = std::modf(val, &res);

        if (fract != 0 && val >= 0) return Val(res + 1);
        return Val(res);
    }
};

struct op_floor
{
    template<typename T>
    Val operator()(T val)
    {
        if (Val(val).isNan() || Val(val).isInf()) return Val(val);

        T res;
        T fract = std::modf(val, &res);

        if (fract != 0 && val < 0) return Val(res - 1);
        return Val(res);
    }
};

struct op_trunc
{
    template<typename T>
    Val operator()(T val)
    {
        if (Val(val).isNan() || Val(val).isInf()) return Val(val);

        T res;
        std::modf(val, &res);
        return Val(res);
    }
};

struct op_rint
{
    template<typename T>
    Val operator()(T val)
    {
        if (Val(val).isNan() || Val(val).isInf()) return Val(val);

        T res;
        T fract = std::abs(std::modf(val, &res));
        bool isEven = (static_cast<uint64_t>(res) & 1) == 0;

        if (fract < 0.5 || (fract == 0.5 && isEven)) fract = 0;
        else fract = static_cast<T>((val < 0)? -1 : 1);

        return Val(res + fract);
    }
};

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of 'class' Instruction

static Val emulate_class(unsigned stype, Val arg1, Val arg2)
{
    assert(arg1.isFloat());
    assert(arg1.getType() == stype);
    assert(arg2.getType() == BRIG_TYPE_U32);

    u32_t res = 0;
    u32_t flags = arg2.u32();

    if (arg1.isSpecialFloat())
    {
        if ((flags & 0x001) && arg1.isSignalingNan())      res = 1;
        if ((flags & 0x002) && arg1.isQuietNan())          res = 1;
        if ((flags & 0x004) && arg1.isNegativeInf())       res = 1;
        if ((flags & 0x200) && arg1.isPositiveInf())       res = 1;
    }
    else if (arg1.isSubnormal())
    {
        if ((flags & 0x010) && arg1.isNegativeSubnormal()) res = 1;
        if ((flags & 0x080) && arg1.isPositiveSubnormal()) res = 1;
    }
    else if (arg1.isZero())
    {
        if ((flags & 0x020) &&  arg1.isNegativeZero())     res = 1;
        if ((flags & 0x040) &&  arg1.isPositiveZero())     res = 1;
    }
    else
    {
        if ((flags & 0x100) &&  arg1.isPositive())         res = 1;
        if ((flags & 0x008) && !arg1.isPositive())         res = 1;
    }

    return Val(BRIG_TYPE_B1, res);
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Bit String operations

static Val emulate_popcount(unsigned stype, Val arg)
{
    assert(arg.getType() == stype);
    assert(isBitType(stype));

    u32_t count = 0;
    for (u64_t val = arg.getAsB64(); val > 0; val >>= 1)
    {
        if ((val & 0x1) != 0) count++;
    }
    return Val(count);
}

static Val emulate_firstbit(unsigned stype, Val arg)
{
    assert(arg.getType() == stype);

    u64_t firstBit = 0x1ULL << (arg.getSize() - 1);
    s64_t val = arg.getAsS64(); // zero/sign-extend as necessary

    if (arg.isSignedInt() && val < 0) val = ~val;
    if (val == 0) return Val(BRIG_TYPE_U32, -1);

    u32_t res = 0;
    for (; (val & firstBit) == 0; ++res, val <<= 1);
    return Val(res);
}

static Val emulate_lastbit(unsigned stype, Val arg)
{
    assert(arg.getType() == stype);

    u64_t val = arg.getAsB64(); // Disable sign-extension

    if (val == 0) return Val(BRIG_TYPE_U32, -1);

    u32_t res = 0;
    for (; (val & 0x1) == 0; ++res, val >>= 1);
    return Val(res);
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of combine/expand Instruction

static Val emulate_combine(unsigned type, unsigned stype, Val arg)
{
    assert(arg.isVector());
    assert(arg.getVecType() == stype);

    if (type == BRIG_TYPE_B64)
    {
        assert(arg.getDim() == 2);
        assert(stype == BRIG_TYPE_B32);

        return Val(type, (arg[1].getAsB64() << 32) | arg[0].b32());
    }

    assert(type == BRIG_TYPE_B128);

    if (stype == BRIG_TYPE_B32)
    {
        assert(arg.getDim() == 4);

        return Val(type,
                   b128((arg[1].getAsB64() << 32) | arg[0].b32(),
                        (arg[3].getAsB64() << 32) | arg[2].b32()));
    }
    else
    {
        assert(arg.getDim() == 2);
        assert(stype == BRIG_TYPE_B64);

        return Val(type, b128(arg[0].b64(), arg[1].b64()));
    }
}

static Val emulate_expand(unsigned type, unsigned stype, Val arg)
{
    assert(!arg.isVector());
    assert(arg.getType() == stype);

    if (stype == BRIG_TYPE_B64)
    {
        assert(type == BRIG_TYPE_B32);

        return Val(2,
                   Val(type, arg.getAsB32(0)),
                   Val(type, arg.getAsB32(1)),
                   Val(),
                   Val());
    }
    else
    {
        assert(stype == BRIG_TYPE_B128);

        if (type == BRIG_TYPE_B32)
        {
            return Val(4,
                       Val(type, arg.getAsB32(0)),
                       Val(type, arg.getAsB32(1)),
                       Val(type, arg.getAsB32(2)),
                       Val(type, arg.getAsB32(3)));
        }
        else
        {
            assert(type == BRIG_TYPE_B64);
            return Val(2,
                       Val(type, arg.getAsB64(0)),
                       Val(type, arg.getAsB64(1)),
                       Val(),
                       Val());
        }
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of cmp Instruction

static Val emulateCmp(unsigned type, unsigned stype, AluMod aluMod, unsigned op, Val arg1, Val arg2)
{
    using namespace Brig;

    assert(arg1.getType() == stype);
    assert(arg2.getType() == stype);

    if (type == BRIG_TYPE_F16) return unimplemented();

    bool isNan = arg1.isNan() || arg2.isNan();
    int cmp = emulateBinOpBSUF(stype, arg1, arg2, op_cmp());

    bool res;
    bool signaling = false;

    switch (op)
    {
    case BRIG_COMPARE_EQ:   res = (cmp == 0) && !isNan;                      break;
    case BRIG_COMPARE_SEQ:  res = (cmp == 0) && !isNan;   signaling = true;  break;

    case BRIG_COMPARE_EQU:  res = (cmp == 0) || isNan;                       break;
    case BRIG_COMPARE_SEQU: res = (cmp == 0) || isNan;    signaling = true;  break;

    case BRIG_COMPARE_NE:   res = (cmp != 0) && !isNan;                      break;
    case BRIG_COMPARE_SNE:  res = (cmp != 0) && !isNan;   signaling = true;  break;
                                                                             
    case BRIG_COMPARE_NEU:  res = (cmp != 0) || isNan;                       break;
    case BRIG_COMPARE_SNEU: res = (cmp != 0) || isNan;    signaling = true;  break;
                                                                             
    case BRIG_COMPARE_LT:   res = (cmp == -1) && !isNan;                     break;
    case BRIG_COMPARE_SLT:  res = (cmp == -1) && !isNan;  signaling = true;  break;
                                                                             
    case BRIG_COMPARE_LTU:  res = (cmp == -1) || isNan;                      break;
    case BRIG_COMPARE_SLTU: res = (cmp == -1) || isNan;   signaling = true;  break;
                                                                             
    case BRIG_COMPARE_LE:   res = (cmp != 1) && !isNan;                      break;
    case BRIG_COMPARE_SLE:  res = (cmp != 1) && !isNan;   signaling = true;  break;
                                                                             
    case BRIG_COMPARE_LEU:  res = (cmp != 1) || isNan;                       break;
    case BRIG_COMPARE_SLEU: res = (cmp != 1) || isNan;    signaling = true;  break;
                                                                             
    case BRIG_COMPARE_GT:   res = (cmp == 1) && !isNan;                      break;
    case BRIG_COMPARE_SGT:  res = (cmp == 1) && !isNan;   signaling = true;  break;

    case BRIG_COMPARE_GTU:  res = (cmp == 1) || isNan;                       break;
    case BRIG_COMPARE_SGTU: res = (cmp == 1) || isNan;    signaling = true;  break;
                                                                             
    case BRIG_COMPARE_GE:   res = (cmp != -1) && !isNan;                     break;
    case BRIG_COMPARE_SGE:  res = (cmp != -1) && !isNan;  signaling = true;  break;
                                                                             
    case BRIG_COMPARE_GEU:  res = (cmp != -1) || isNan;                      break;
    case BRIG_COMPARE_SGEU: res = (cmp != -1) || isNan;   signaling = true;  break;
                                                                             
    case BRIG_COMPARE_NUM:  res = !isNan;                                    break;
    case BRIG_COMPARE_SNUM: res = !isNan;                 signaling = true;  break;

    case BRIG_COMPARE_NAN:  res = isNan;                                     break;
    case BRIG_COMPARE_SNAN: res = isNan;                  signaling = true;  break;

    default: 
        assert(false);
        return emulationFailed();
    }

    if (signaling && isNan) return unimplemented();

    switch (type)
    {
    case BRIG_TYPE_B1:  return Val(type, res? 1 : 0);

    case BRIG_TYPE_S32:
    case BRIG_TYPE_S64:
    case BRIG_TYPE_U32:
    case BRIG_TYPE_U64: return Val(type, res? -1 : 0);

    case BRIG_TYPE_F32: return Val(res? 1.0f : 0.0f);
    case BRIG_TYPE_F64: return Val(res? 1.0  : 0.0 );

    default:
        return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of cvt Instruction

// Compute delta d for rounding of val so that (val+d)
// will be rounded to proper value when converted to integer
static int f2i_round(Val val, unsigned rounding)
{
    assert(val.isFloat());
    assert(!val.isNan());

    int round = 0;

    switch (rounding)
    {
    case AluMod::ROUNDING_NEARI:
    case AluMod::ROUNDING_NEARI_SAT:
    case AluMod::ROUNDING_SNEARI:
    case AluMod::ROUNDING_SNEARI_SAT:
        if (val.getNormalizedFract() > Val(0.5f).getNormalizedFract())          // Rounds to the nearest representable value
        {
            round = val.isNegative() ? -1 : 1;
        }
        else if (val.getNormalizedFract()  == Val(0.5f).getNormalizedFract() &&  // If there is a tie, round to an even least significant digit
                 val.getNormalizedFract(-1) > Val(0.5f).getNormalizedFract())
        {
            round = val.isNegative() ? -1 : 1;
        }
        break;
    case AluMod::ROUNDING_ZEROI:
    case AluMod::ROUNDING_ZEROI_SAT:
    case AluMod::ROUNDING_SZEROI:
    case AluMod::ROUNDING_SZEROI_SAT:
        break;
    case AluMod::ROUNDING_UPI:
    case AluMod::ROUNDING_UPI_SAT:
    case AluMod::ROUNDING_SUPI:
    case AluMod::ROUNDING_SUPI_SAT:
        if (val.isRegularPositive() && !val.isNatural()) round = 1;
        break;
    case AluMod::ROUNDING_DOWNI:
    case AluMod::ROUNDING_DOWNI_SAT:
    case AluMod::ROUNDING_SDOWNI:
    case AluMod::ROUNDING_SDOWNI_SAT:
        if (val.isRegularNegative() && !val.isNatural()) round = -1;
        break;

    default:
        assert(false);
        return 0;
    }

    return round;
}

static Val f2i_saturate(unsigned type, bool lowBound)
{
    return Val(type, getIntBoundary(type, lowBound));
}

static bool isIntegral(Val val)
{
    //F Should check if exception policy is enabled for the inexact exception

    Val fract = emulateUnrOpF(val.getType(), val, op_fract());
    return fract.isZero();
}

template<typename T>
static Val cvt_f2i(unsigned type, AluMod aluMod, Val val)
{
    assert(isIntType(type));

    if (val.isNan()) return aluMod.isSat()? Val(type, 0) : undefValue();

    int round = f2i_round(val, aluMod.getRounding());
    T v = static_cast<T>(val) + round;

    if (!checkTypeBoundaries(type, v)) return aluMod.isSat() ? f2i_saturate(type, v <= 0) : undefValue();

    if (aluMod.isSignaling() && !isIntegral(val)) return unimplemented(); // generate an inexact exception

    return isSignedType(type)? Val(type, static_cast<s64_t>(v)) : Val(type, static_cast<u64_t>(v));
}

static Val cvt_f2x(unsigned type, unsigned stype, AluMod aluMod, Val arg)
{
    assert(isFloatType(stype));

    if (isFloatType(type))
    {
        if (type == BRIG_TYPE_F64 && stype == BRIG_TYPE_F32)
        {
            return static_cast<f64_t>(arg.f32());
        }
        else if (type == BRIG_TYPE_F32 && stype == BRIG_TYPE_F64)
        {
            if (aluMod.getRounding() == AluMod::ROUNDING_NEAR)  // This is the default MS C++ rounding mode
            {
                return static_cast<f32_t>(arg.f64());
            }
            else
            {
                return unimplemented(); // FIXME: fp rounding is not implemented yet
            }
        }

        return unimplemented(); // f16 is not supported yet
    }
    else
    {
        assert(isIntType(type));

        if (stype == BRIG_TYPE_F32) return cvt_f2i<f32_t>(type, aluMod, arg);
        if (stype == BRIG_TYPE_F64) return cvt_f2i<f64_t>(type, aluMod, arg);

        return unimplemented();  // f16 is not supported yet
    }
}

static Val cvt_i2f(unsigned type, Val val, AluMod aluMod)
{
    assert(isFloatType(type));

    if (aluMod.getRounding() == AluMod::ROUNDING_NEAR) // This is the default MS C++ rounding mode
    {
        if (type == BRIG_TYPE_F32) return emulateUnrOpUS(val.getType(), val, op_i2f32());
        if (type == BRIG_TYPE_F64) return emulateUnrOpUS(val.getType(), val, op_i2f64());
        return unimplemented(); // f16 is not supported yet
    }
    return unimplemented(); // other fp rounding modes are not supported yet
}

static Val cvt_i2x(unsigned type, unsigned stype, AluMod aluMod, Val arg)
{
    assert(isIntType(stype));

    return isIntType(type)?
            Val(type, arg.getAsS64()) : // zero/sign-extend as necessary
            cvt_i2f(type, arg, aluMod);
}

static Val cvt_x2b1(unsigned type, unsigned stype, AluMod aluMod, Val arg)
{
    return isIntType(stype)?
            Val(type, arg.getAsB64() != 0) :
            Val(type, !arg.isZero());
}

static Val emulateCvt(unsigned type, unsigned stype, AluMod aluMod, Val arg)
{
    assert(arg.getType() == stype);
    assert(type != stype);

    if (type == BRIG_TYPE_F16) return unimplemented();

    if (stype == BRIG_TYPE_B1) // to avoid handling this type in other places
    {
        stype = BRIG_TYPE_U32;
        arg = Val(stype, arg.getAsB64());
    }

    if (type == BRIG_TYPE_B1)  return cvt_x2b1(type, stype, aluMod, arg);
    else                       return isFloatType(stype)?
                                      cvt_f2x(type, stype, aluMod, arg) :
                                      cvt_i2x(type, stype, aluMod, arg);
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of atomic Instruction

static Val emulateAtomicMem(unsigned type, unsigned atomicOperation, Val arg1, Val arg2, Val arg3)
{
    using namespace Brig;

    assert(arg1.getType() == type);
    assert(BRIG_ATOMIC_ST || arg2.getType() == type);

    switch (atomicOperation)
    {
    case BRIG_ATOMIC_AND:     return emulateBinOpB(type, arg1, arg2, op_and());
    case BRIG_ATOMIC_OR:      return emulateBinOpB(type, arg1, arg2, op_or());
    case BRIG_ATOMIC_XOR:     return emulateBinOpB(type, arg1, arg2, op_xor());

    case BRIG_ATOMIC_ADD:     return emulateBinOpSU(type, arg1, arg2, op_add());
    case BRIG_ATOMIC_SUB:     return emulateBinOpSU(type, arg1, arg2, op_sub());
    case BRIG_ATOMIC_MAX:     return emulateBinOpSU(type, arg1, arg2, op_max());
    case BRIG_ATOMIC_MIN:     return emulateBinOpSU(type, arg1, arg2, op_min());

    case BRIG_ATOMIC_WRAPINC: return emulateBinOpSU(type, arg1, arg2, op_inc());
    case BRIG_ATOMIC_WRAPDEC: return emulateBinOpSU(type, arg1, arg2, op_dec());
    case BRIG_ATOMIC_EXCH:    return emulateBinOpB(type, arg1, arg2, op_arg2());
    case BRIG_ATOMIC_CAS:     return emulateTrnOpB(type, arg1, arg2, arg3, op_cas());

    case BRIG_ATOMIC_LD:      assert(arg1.getType() == type); return arg1;
    case BRIG_ATOMIC_ST:      assert(arg2.getType() == type); return arg2;

    default: return emulationFailed();
    }
}

static Val emulateAtomicDst(unsigned opcode, Val arg1)
{
    return (opcode == Brig::BRIG_OPCODE_ATOMIC)? arg1 : emptyDstValue();
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of carry/borrow

template<class T>
static Val emulateAluFlag(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    unsigned utype = type;

    if (isSignedType(type)) // Convert args to unsigned (to simplify functor implementation)
    {
        utype = (getBrigTypeNumBits(type) == 32)? BRIG_TYPE_U32 : BRIG_TYPE_U64;

        arg1 = Val(utype, arg1.getAsB64()); // copy bits w/o sign-extension
        arg2 = Val(utype, arg2.getAsB64()); // copy bits w/o sign-extension
    }

    Val res = emulateBinOpSU(utype, arg1, arg2, op);

    // Convert back to original type
    // NB: result is either 0 or 1, sign-extension is not necessary
    return Val(type, res.getAsB64());
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Irregular Instructions operating with packed data

static Val emulate_shuffle(unsigned type, Val arg1, Val arg2, Val arg3)
{
    assert(arg1.isPacked());
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(isBitType(arg3.getType()) && arg3.getSize() == 32);

    Val dst(type, 0);

    u32_t ctl   = arg3.getAsB32();          // value that controls shuffling
    u32_t dim   = getPackedTypeDim(type);   // number of elements in packed data
    u32_t width = range2width(dim);         // number of control bits per element
    u64_t mask  = getWidthMask(width);      // mask for extracting control bits

    for (unsigned i = 0; i < dim; ++i)
    {
        unsigned idx = (ctl & mask);
        u64_t x = (i < dim / 2)? arg1.getElement(idx) : arg2.getElement(idx);
        dst.setElement(i, x);
        ctl >>= width;
    }

    return dst;
}

static Val emulate_unpackHalf(unsigned type, bool lowHalf, Val arg1, Val arg2)
{
    assert(arg1.isPacked());
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    Val dst(type, 0);

    unsigned dim = getPackedTypeDim(type);
    unsigned srcPos = lowHalf? 0 : (dim / 2);

    for (unsigned dstPos = 0; dstPos < dim; ++srcPos)
    {
        dst.setElement(dstPos++, arg1.getElement(srcPos));
        dst.setElement(dstPos++, arg2.getElement(srcPos));
    }

    return dst;
}

static Val emulate_pack(unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3)
{
    assert(isPackedType(type));
    assert(!isPackedType(stype));
    assert(arg1.getType() == type);
    assert(arg2.getType() == stype);
    assert(arg3.getType() == BRIG_TYPE_U32);

    u32_t dim   = getPackedTypeDim(type);   // number of elements in packed data
    u32_t width = range2width(dim);         // number of control bits per element
    u64_t mask  = getWidthMask(width);      // mask for extracting control bits

    Val dst = arg1;
    dst.setElement(arg3.u32() & mask, arg2.getAsB64());
    return dst;
}

static Val emulate_unpack(unsigned type, unsigned stype, Val arg1, Val arg2)
{
    assert(!isPackedType(type));
    assert(isPackedType(stype));
    assert(arg1.getType() == stype);
    assert(arg2.getType() == BRIG_TYPE_U32);

    u32_t dim   = getPackedTypeDim(stype);  // number of elements in packed data
    u32_t width = range2width(dim);         // number of control bits per element
    u64_t mask  = getWidthMask(width);      // mask for extracting control bits

    // Extract specified element in native type
    Val res(arg1.getElementType(), arg1.getElement(arg2.u32() & mask));

    // The required type may be wider than extracted (for s/u).
    // In this case sign-extend or zero-extend the value as required.
    if (res.getType() != type)
    {
        assert(!res.isFloat());
        assert(!isFloatType(type));

        res = res.isSignedInt()? Val(type, res.getAsS64()) : Val(type, res.getAsB64());
    }

    return res;
}

static Val emulate_lerp(unsigned type, Val arg1, Val arg2, Val arg3)
{
    assert(type == BRIG_TYPE_U8X4);
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    Val res(type, 0);

    for (unsigned i = 0; i < 4; ++i)
    {
        res.setElement(i, (arg1.getElement(i) + arg2.getElement(i) + (arg3.getElement(i) & 0x1)) / 2);
    }

    return res;
}

static Val emulate_packcvt(unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3, Val arg4)
{
    assert(type == BRIG_TYPE_U8X4);
    assert(stype == BRIG_TYPE_F32);
    assert(arg1.getType() == stype);
    assert(arg2.getType() == stype);
    assert(arg3.getType() == stype);
    assert(arg4.getType() == stype);

    Val x1 = emulateCvt(BRIG_TYPE_U8, stype, AluMod(AluMod::ROUNDING_NEARI_SAT), arg1);
    Val x2 = emulateCvt(BRIG_TYPE_U8, stype, AluMod(AluMod::ROUNDING_NEARI_SAT), arg2);
    Val x3 = emulateCvt(BRIG_TYPE_U8, stype, AluMod(AluMod::ROUNDING_NEARI_SAT), arg3);
    Val x4 = emulateCvt(BRIG_TYPE_U8, stype, AluMod(AluMod::ROUNDING_NEARI_SAT), arg4);

    if (x1.empty() || x2.empty() || x3.empty() || x4.empty()) return undefValue();

    Val res(type, 0);
    res.setElement(0, x1.u8());
    res.setElement(1, x2.u8());
    res.setElement(2, x3.u8());
    res.setElement(3, x4.u8());

    return res;
}

static Val emulate_unpackcvt(unsigned type, unsigned stype, Val arg1, Val arg2)
{
    assert(type == BRIG_TYPE_F32);
    assert(stype == BRIG_TYPE_U8X4);
    assert(arg1.getType() == stype);
    assert(arg2.getType() == BRIG_TYPE_U32);

    Val val(BRIG_TYPE_U8, arg1.getElement(arg2.u32() & 0x3));
    return emulateCvt(type, BRIG_TYPE_U8, AluMod(AluMod::ROUNDING_NEAR), val);
}

static Val emulate_cmov(unsigned type, Val arg1, Val arg2, Val arg3)
{
    assert(arg1.isPacked());
    assert(isUnsignedType(arg1.getElementType()));
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);
    assert(arg1.getSize() == arg2.getSize());
    assert(arg1.getElementSize() == arg2.getElementSize());

    Val dst = arg2;

    u32_t dim = getPackedTypeDim(type);   // number of elements in packed data

    for (unsigned i = 0; i < dim; ++i)
    {
        dst.setElement(i, arg1.getElement(i)? arg2.getElement(i) : arg3.getElement(i));
    }

    return dst;
}

static u64_t sad(u64_t a, u64_t b) { return a < b ? b - a : a - b; }

static Val emulate_sad(unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3)
{
    assert(type == BRIG_TYPE_U32);
    assert(stype == BRIG_TYPE_U32 || stype == BRIG_TYPE_U16X2 || stype == BRIG_TYPE_U8X4);
    assert(arg1.getType() == stype);
    assert(arg2.getType() == stype);
    assert(arg3.getType() == BRIG_TYPE_U32);

    uint64_t res = arg3.u32();

    if (stype == BRIG_TYPE_U32)
    {
        res += sad(arg1.u32(), arg2.u32());
    }
    else
    {
        assert(isPackedType(stype));
        u32_t dim = getPackedTypeDim(stype);
        for (unsigned i = 0; i < dim; ++i)
        {
            res += sad(arg1.getElement(i), arg2.getElement(i));
        }
    }

    return Val(type, res);
}

static Val emulate_sadhi(unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3)
{
    assert(type == BRIG_TYPE_U16X2);
    assert(stype == BRIG_TYPE_U8X4);
    assert(arg1.getType() == stype);
    assert(arg2.getType() == stype);
    assert(arg3.getType() == BRIG_TYPE_U16X2);

    uint64_t res = arg3.getElement(1);
    u32_t dim = getPackedTypeDim(stype);
    for (unsigned i = 0; i < dim; ++i)
    {
        res += sad(arg1.getElement(i), arg2.getElement(i));
    }

    Val dst = arg3;
    dst.setElement(1, res);
    return dst;
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Instructions in Basic/Mod Formats

static Val emulateMod(unsigned opcode, unsigned type, AluMod aluMod, Val arg1, Val arg2, Val arg3 = Val(), Val arg4 = Val())
{
    using namespace Brig;

    if (aluMod.getRounding() != AluMod::ROUNDING_NONE &&
        aluMod.getRounding() != AluMod::ROUNDING_NEAR) return unimplemented();

    switch (opcode)
    {
    case BRIG_OPCODE_ABS:       return emulateUnrOpSF(type, arg1, op_abs());
    case BRIG_OPCODE_NEG:       return emulateUnrOpSF(type, arg1, op_neg());

    case BRIG_OPCODE_NOT:       return emulateUnrOpB(type, arg1, op_not());

    case BRIG_OPCODE_ADD:       return emulateBinOpBSUF(type, arg1, arg2, op_add());
    case BRIG_OPCODE_SUB:       return emulateBinOpBSUF(type, arg1, arg2, op_sub());
    case BRIG_OPCODE_MUL:       return emulateBinOpBSUF(type, arg1, arg2, op_mul());
    case BRIG_OPCODE_MULHI:     return emulateBinOpBSUF(type, arg1, arg2, op_mulhi());
    case BRIG_OPCODE_DIV:       return emulateBinOpBSUF(type, arg1, arg2, op_div());
    case BRIG_OPCODE_MAX:       return emulateBinOpBSUF(type, arg1, arg2, op_max());
    case BRIG_OPCODE_MIN:       return emulateBinOpBSUF(type, arg1, arg2, op_min());

    case BRIG_OPCODE_REM:       return emulateBinOpSU(type, arg1, arg2, op_rem());

    case BRIG_OPCODE_MUL24:     return emulateTrnOpSU(type, arg1, arg2, Val(type, 0), op_mad24(0));
    case BRIG_OPCODE_MUL24HI:   return emulateTrnOpSU(type, arg1, arg2, Val(type, 0), op_mad24(32));

    case BRIG_OPCODE_MAD24:     return emulateTrnOpSU(type, arg1, arg2, arg3, op_mad24(0));
    case BRIG_OPCODE_MAD24HI:   return emulateTrnOpSU(type, arg1, arg2, arg3, op_mad24(32));

    case BRIG_OPCODE_AND:       return emulateBinOpB(type, arg1, arg2, op_and());
    case BRIG_OPCODE_OR:        return emulateBinOpB(type, arg1, arg2, op_or());
    case BRIG_OPCODE_XOR:       return emulateBinOpB(type, arg1, arg2, op_xor());

    case BRIG_OPCODE_COPYSIGN:  return emulateBinOpF(type, arg1, arg2, op_copysign());

    case BRIG_OPCODE_CARRY:     return emulateAluFlag(type, arg1, arg2, op_carry());
    case BRIG_OPCODE_BORROW:    return emulateAluFlag(type, arg1, arg2, op_borrow());

    case BRIG_OPCODE_SHL:       return emulateBinOpSU_U32(type, arg1, arg2, op_shl());
    case BRIG_OPCODE_SHR:       return emulateBinOpSU_U32(type, arg1, arg2, op_shr());

    case BRIG_OPCODE_FRACT:     return emulateUnrOpF(type, arg1, op_fract());
    case BRIG_OPCODE_CEIL:      return emulateUnrOpF(type, arg1, op_ceil());
    case BRIG_OPCODE_FLOOR:     return emulateUnrOpF(type, arg1, op_floor());
    case BRIG_OPCODE_RINT:      return emulateUnrOpF(type, arg1, op_rint());
    case BRIG_OPCODE_TRUNC:     return emulateUnrOpF(type, arg1, op_trunc());

    case BRIG_OPCODE_SQRT:      return emulateUnrOpF(type, arg1, op_sqrt());
    case BRIG_OPCODE_NCOS:      return emulateUnrOpF(type, arg1, op_cos());
    case BRIG_OPCODE_NSIN:      return emulateUnrOpF(type, arg1, op_sin());
    case BRIG_OPCODE_NEXP2:     return emulateUnrOpF(type, arg1, op_exp2());
    case BRIG_OPCODE_NLOG2:     return emulateUnrOpF(type, arg1, op_log2());
    case BRIG_OPCODE_NSQRT:     return emulateUnrOpF(type, arg1, op_sqrt());
    case BRIG_OPCODE_NRSQRT:    return emulateUnrOpF(type, arg1, op_rsqrt());
    case BRIG_OPCODE_NRCP:      return emulateUnrOpF(type, arg1, op_rcp());
    case BRIG_OPCODE_NFMA:      return emulateTrnOpSUF(type, arg1, arg2, arg3, op_mad());

    case BRIG_OPCODE_MAD:       return emulateTrnOpSUF(type, arg1, arg2, arg3, op_mad());
    case BRIG_OPCODE_FMA:       return emulateTrnOpSUF(type, arg1, arg2, arg3, op_mad());  // the same as mad but fp type

    case BRIG_OPCODE_MOV:       assert(arg1.getType() == type);         return arg1;
    case BRIG_OPCODE_CMOV:      assert(arg1.getType() == BRIG_TYPE_B1); return emulateTrnOpB(type, Val(type, arg1.getAsB32()), arg2, arg3, op_cmov());

    case BRIG_OPCODE_BITMASK:   return (type == BRIG_TYPE_B32)?
                                       emulateBinOpB(arg1.getType(), arg1, arg2, op_bitmask<b32_t>()) :
                                       emulateBinOpB(arg1.getType(), arg1, arg2, op_bitmask<b64_t>());
    case BRIG_OPCODE_BITSELECT: return emulateTrnOpB(type, arg1, arg2, arg3, op_bitsel());

    case BRIG_OPCODE_BITREV:    return emulateUnrOpB(type, arg1, op_bitrev());
    case BRIG_OPCODE_BITEXTRACT:return emulateTrnOpSU_U32_U32(type, arg1, arg2, arg3, op_bitextract());
    case BRIG_OPCODE_BITINSERT: return emulateQrnOpSU_U32_U32(type, arg1, arg2, arg3, arg4, op_bitinsert());

    case BRIG_OPCODE_BITALIGN:  return emulateTrnOpB(type, arg1, arg2, arg3, op_bitalign(31, 1));
    case BRIG_OPCODE_BYTEALIGN: return emulateTrnOpB(type, arg1, arg2, arg3, op_bitalign( 3, 8));

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Instructions in SourceType Format

static Val emulateSourceType(unsigned opcode, unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3)
{
    using namespace Brig;

    switch (opcode)
    {
    case BRIG_OPCODE_CLASS:     return emulate_class(stype, arg1, arg2);
    case BRIG_OPCODE_POPCOUNT:  return emulate_popcount(stype, arg1);
    case BRIG_OPCODE_FIRSTBIT:  return emulate_firstbit(stype, arg1);
    case BRIG_OPCODE_LASTBIT:   return emulate_lastbit(stype, arg1);

    case BRIG_OPCODE_COMBINE:   return emulate_combine(type, stype, arg1);
    case BRIG_OPCODE_EXPAND:    return emulate_expand(type, stype, arg1);

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Instructions in Mem Format (ld/st)

static Val emulateMemDst(unsigned segment, unsigned opcode, Val arg)
{
    using namespace Brig;

    switch (opcode)
    {
    case BRIG_OPCODE_LD:  return arg;
    case BRIG_OPCODE_ST:  return emptyDstValue();

    default: return emulationFailed();
    }
}

static Val emulateMemMem(unsigned segment, unsigned opcode, Val arg0, Val arg1)
{
    using namespace Brig;

    switch (opcode)
    {
    case BRIG_OPCODE_LD:  return arg1;
    case BRIG_OPCODE_ST:  return arg0;

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Helpers

// Arrays declared at TOPLEVEL must belong to global, group or private segments.
// NB: Readonly cannot be initialized and so unsuitable for tesing.
static bool isSupportedSegment(unsigned segment)
{
    using namespace Brig;

    switch (segment)
    {
    case BRIG_SEGMENT_GLOBAL:
    case BRIG_SEGMENT_GROUP:
    case BRIG_SEGMENT_PRIVATE:
        return true;
    default:
        return false;
    }
}

static bool emulateFtz(Inst inst, Val& arg0, Val& arg1, Val& arg2, Val& arg3, Val& arg4)
{
    bool ftz = false;

    if      (InstMod i = inst)  ftz = i.modifier().ftz();
    else if (InstCmp i = inst)  ftz = i.modifier().ftz();
    else if (InstCvt i = inst)  ftz = i.modifier().ftz();

    if (ftz)
    {
        arg0 = arg0.ftz();
        arg1 = arg1.ftz();
        arg2 = arg2.ftz();
        arg3 = arg3.ftz();
        arg4 = arg4.ftz();
    }

    return ftz;
}

static bool discardNanSign(unsigned opcode)
{
    using namespace Brig;

    switch (opcode)
    {
    case BRIG_OPCODE_ABS:
    case BRIG_OPCODE_NEG:
    case BRIG_OPCODE_CLASS:
    case BRIG_OPCODE_COPYSIGN:
        return false;
    default:
        return true;
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Helpers for instructions with packed operands

// Identify regular operations with packed data.
// Most of these operations may be reduced to non-packed operations.
static bool isCommonPacked(Inst inst)
{
    using namespace Brig;

    return (getPacking(inst) != BRIG_PACK_NONE) ||
           (isPackedType(inst.type()) &&
                (inst.opcode() == BRIG_OPCODE_SHL  ||
                 inst.opcode() == BRIG_OPCODE_SHR));
}

// Identify special (irregular) operations with packed data
// which cannot be reduced to non-packed operations
static bool isSpecialPacked(Inst inst)
{
    using namespace Brig;

    switch (inst.opcode())
    {
    case BRIG_OPCODE_SHUFFLE:   // Packed Data Operations
    case BRIG_OPCODE_UNPACKHI:
    case BRIG_OPCODE_UNPACKLO:
    case BRIG_OPCODE_PACK:
    case BRIG_OPCODE_UNPACK:
        return true;
    case BRIG_OPCODE_CMOV:
        return isPackedType(inst.type());
    case BRIG_OPCODE_PACKCVT:   // Multimedia Operations
    case BRIG_OPCODE_UNPACKCVT:
    case BRIG_OPCODE_LERP:
    case BRIG_OPCODE_SAD:
    case BRIG_OPCODE_SADHI:
        return true;
    default:
        return false;
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of packed operations

// MulHi for packed types must be handled separately because there are 2 cases
// depending on element type:
// - subword types: use 'mul' and extract result from high bits of the product;
// - 32/64 bit types: use regular 'mulhi'
static Val emulateMulHiPacked(unsigned type, unsigned baseType, Val arg1, Val arg2)
{
    assert(isPackedType(type));
    assert(arg1.getType() == baseType);
    assert(arg2.getType() == baseType);

    using namespace Brig;

    unsigned elementType = packedType2elementType(type);
    unsigned opcode = (getBrigTypeNumBits(elementType) < 32)? BRIG_OPCODE_MUL : BRIG_OPCODE_MULHI;

    Val res = emulateMod(opcode, baseType, AluMod(), arg1, arg2);

    if (opcode == BRIG_OPCODE_MUL)
    {
        res = Val(baseType, res.getAsB64() >> getBrigTypeNumBits(elementType));
    }

    return res;
}

static Val emulateSat(unsigned opcode, unsigned type, Val arg1, Val arg2)
{
    assert(isPackedType(type));
    assert(!isFloatType(packedType2elementType(type)));

    using namespace Brig;

    // Repack from base type to element type
    unsigned baseType    = packedType2baseType(type);
    unsigned elementType = packedType2elementType(type);
    arg1 = Val(elementType, arg1.getAsB64());
    arg2 = Val(elementType, arg2.getAsB64());

    Val res;

    switch (opcode)
    {
    case BRIG_OPCODE_ADD: res = emulateBinOpSat(elementType, arg1, arg2, op_add_sat()); break;
    case BRIG_OPCODE_SUB: res = emulateBinOpSat(elementType, arg1, arg2, op_sub_sat()); break;
    case BRIG_OPCODE_MUL: res = emulateBinOpSat(elementType, arg1, arg2, op_mul_sat()); break;
    default:
        return emulationFailed();
    }

    return res.isSignedInt()? Val(baseType, res.getAsS64()) : Val(baseType, res.getAsB64());
}

static Val emulateDstValPackedRegular(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    using namespace Brig;

    assert(arg0.empty());
    assert(arg3.empty());
    assert(arg4.empty());

    assert(!arg1.empty());
    assert(!arg1.isVector());
    assert(isPackedType(arg1.getType()));

    unsigned type        = inst.type();
    unsigned stype       = InstCmp(inst)? getSrcType(inst) : type;
    unsigned packing     = getPacking(inst);
    unsigned opcode      = inst.opcode();

    if (opcode == BRIG_OPCODE_SHL || opcode == BRIG_OPCODE_SHR) packing = BRIG_PACK_PP;

    unsigned baseType    = packedType2baseType(type);
    unsigned baseSrcType = packedType2baseType(stype);
    unsigned typeDim     = getPackedDstDim(stype, packing);

    // NB: operations with 's' packing control must preserve all elements
    // except for the lowest one, but this cannot be emulated.
    // So we just erase everything before emulation.
    Val dst(type, b128(0, 0));

    for (unsigned idx = 0; idx < typeDim; ++idx)
    {
        Val res;

        Val x1 = arg1.getPackedElement(idx, packing, 0);
        Val x2 = arg2.getPackedElement(idx, packing, 1);

        if (opcode == BRIG_OPCODE_SHL || opcode == BRIG_OPCODE_SHR) // Mask out insignificant shift bits for shl/shr
        {
            assert(x2.getType() == BRIG_TYPE_U32);

            unsigned elementSize = getBrigTypeNumBits(type) / typeDim;
            x2 = Val(BRIG_TYPE_U32, x2.u32() & getRangeMask(elementSize));
        }

        if (opcode == BRIG_OPCODE_MULHI) res = emulateMulHiPacked(  type,        baseType,                                    x1, x2);
        else if (isSatPacking(packing))  res = emulateSat(opcode,   type,                                                     x1, x2);
        else if (InstBasic i = inst)     res = emulateMod(opcode,   baseType,    AluMod(),                                    x1, x2);
        else if (InstMod   i = inst)     res = emulateMod(opcode,   baseType,    AluMod(i.modifier().allBits()),              x1, x2);
        else if (InstCmp   i = inst)     res = emulateCmp(baseType, baseSrcType, AluMod(i.modifier().allBits()), i.compare(), x1, x2);
        else                             { assert(false); }

        if (res.empty())
        {
            assert(idx == 0);
            return unimplemented();
        }

        dst.setPackedElement(idx, res);
    }

    return dst;
}

static Val emulateDstValPackedSpecial(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    using namespace Brig;

    switch (inst.opcode())
    {
    // Packed Data Operations
    case BRIG_OPCODE_SHUFFLE:   return emulate_shuffle   (inst.type(),                    arg1, arg2, arg3);
    case BRIG_OPCODE_UNPACKHI:  return emulate_unpackHalf(inst.type(), false,             arg1, arg2);
    case BRIG_OPCODE_UNPACKLO:  return emulate_unpackHalf(inst.type(), true,              arg1, arg2);
    case BRIG_OPCODE_PACK:      return emulate_pack      (inst.type(), getSrcType(inst),  arg1, arg2, arg3);
    case BRIG_OPCODE_UNPACK:    return emulate_unpack    (inst.type(), getSrcType(inst),  arg1, arg2);
    case BRIG_OPCODE_CMOV:      return emulate_cmov      (inst.type(),                    arg1, arg2, arg3);

    // Multimedia Operations
    case BRIG_OPCODE_PACKCVT:   return emulate_packcvt   (inst.type(), getSrcType(inst),  arg1, arg2, arg3, arg4);
    case BRIG_OPCODE_UNPACKCVT: return emulate_unpackcvt (inst.type(), getSrcType(inst),  arg1, arg2);
    case BRIG_OPCODE_LERP:      return emulate_lerp      (inst.type(),                    arg1, arg2, arg3);
    case BRIG_OPCODE_SAD:       return emulate_sad       (inst.type(), getSrcType(inst),  arg1, arg2, arg3);
    case BRIG_OPCODE_SADHI:     return emulate_sadhi     (inst.type(), getSrcType(inst),  arg1, arg2, arg3);

    default:
        return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of common (non-packed) operations

static Val emulateDstValCommon(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    if      (InstBasic      i = inst)  return emulateMod(i.opcode(), i.type(), AluMod(),                       arg1, arg2, arg3, arg4);
    else if (InstMod        i = inst)  return emulateMod(i.opcode(), i.type(), AluMod(i.modifier().allBits()), arg1, arg2, arg3, arg4);
    else if (InstCmp        i = inst)  return emulateCmp(i.type(),   i.sourceType(), AluMod(i.modifier().allBits()), i.compare(), arg1, arg2);
    else if (InstCvt        i = inst)  return emulateCvt(i.type(),   i.sourceType(), AluMod(i.modifier().allBits()), arg1);
    else if (InstSourceType i = inst)  return emulateSourceType(i.opcode(), i.type(), i.sourceType(), arg1, arg2, arg3);
    else if (InstAtomic     i = inst)  return emulateAtomicDst(inst.opcode(), arg1);
    else if (InstMem        i = inst)  return emulateMemDst(i.segment(), i.opcode(), arg1);
    else                               return emulationFailed();
}

//=============================================================================
//=============================================================================
//=============================================================================
// Public interface with Emulator

// Check generic limitations on instruction being tested
// NB: Most limitations should be encoded in HSAILTestGenTestData.h
//     This function shall only check limitations which cannot be expressed there
bool testableInst(Inst inst)
{
    using namespace Brig;
    assert(inst);

    if (InstAtomic instAtomic = inst)
    {
        if (!isSupportedSegment(instAtomic.segment())) return false;
        if (instAtomic.equivClass() != 0) return false;
        //if (instAtomic.memoryOrder() == ...) return false;
        //if (instAtomic.memoryScope() == ...) return false;
    }
    else if (InstMem instMem = inst)
    {
        if (!isSupportedSegment(instMem.segment())) return false;
        if (instMem.width() != BRIG_WIDTH_NONE && instMem.width() != BRIG_WIDTH_1) return false;
        if (instMem.modifier().isConst()) return false;
        if (instMem.equivClass() != 0) return false;
    }

    return true;
}

// Emulate execution of instruction 'inst' using provided input values.
// Return value stored into destination register or an empty value
// if there is no destination or if emulation failed.
Val emulateDstVal(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    Val res;

    bool ftz = emulateFtz(inst, arg0, arg1, arg2, arg3, arg4);

    if (isCommonPacked(inst))           // regular operations on packed data
    {
        res = emulateDstValPackedRegular(inst, arg0, arg1, arg2, arg3, arg4);
    }
    else if (isSpecialPacked(inst))     // irregular operations on packed data
    {
        res = emulateDstValPackedSpecial(inst, arg0, arg1, arg2, arg3, arg4);
    }
    else                                // operations with non-packed data types
    {
        res = emulateDstValCommon(inst, arg0, arg1, arg2, arg3, arg4);
    }

    if (ftz) res = res.ftz();
    return res.normalize(discardNanSign(inst.opcode())); // Clear NaN payload and sign
}

// Emulate execution of instruction 'inst' using provided input values.
// Return value stored into memory or an empty value if this
// instruction does not modify memory or if emulation failed.
Val emulateMemVal(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    using namespace Brig;

    Val res;

    if (InstAtomic i = inst)
    {
        switch (i.opcode())
        {
        case BRIG_OPCODE_ATOMIC:      res = emulateAtomicMem(i.type(), i.atomicOperation(), arg1, arg2, arg3); break;
        case BRIG_OPCODE_ATOMICNORET: res = emulateAtomicMem(i.type(), i.atomicOperation(), arg0, arg1, arg2); break;
        default:                      res = emulationFailed();                                                 break;
        }
    }
    else if (InstMem i = inst)
    {
        res = emulateMemMem(i.segment(), i.opcode(), arg0, arg1);
    }
    else
    {
        res = emptyMemValue();
    }

    return res;
}

// Return precision of result computation for this instruction.
// If the value is 0, the precision is infinite.
// If the value is between 0 and 1, the precision is relative.
// If the value is greater than or equals to 1, the precision is specified in ULPS.
// This is a property of target HW, not emulator!
double getPrecision(Inst inst)
{
    using namespace Brig;

    switch(inst.opcode()) // Instructions with HW-specific precision
    {
    case BRIG_OPCODE_NRCP:
    case BRIG_OPCODE_NSQRT:
    case BRIG_OPCODE_NRSQRT:
    case BRIG_OPCODE_NEXP2:
    case BRIG_OPCODE_NLOG2:
        if (inst.type() == BRIG_TYPE_F32) return 0.0000005;  // Relative
        if (inst.type() == BRIG_TYPE_F64) return 0.00000002; // Relative
        break;

    case BRIG_OPCODE_NSIN:
    case BRIG_OPCODE_NCOS:
        return NSIN_NCOS_RESULT_PRECISION_ULPS;

    default:
        break;
    }

    return 1; // 0.5 ULP (infinite precision)
}

//=============================================================================
//=============================================================================
//=============================================================================

} // namespace TESTGEN