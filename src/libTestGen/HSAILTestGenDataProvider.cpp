/*
   Copyright 2013-2015 Heterogeneous System Architecture (HSA) Foundation

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "HSAILTestGenDataProvider.h"
#include "HSAILTestGenEmulatorTypes.h"
#include "HSAILTestGenUtilities.h"

#include <limits>
#include <iostream>
#include <vector>

using std::vector;
using std::numeric_limits;

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================
// Interface with container which holds test data

class OperandTestData
{
protected:
    static const unsigned MAX_RND_TEST_TRY = 256;  // Max number of attempts to generate next random value
    static const unsigned MAX_RND_TEST_NUM = 64;   // Max number of random values
    static unsigned rndTestNum;

    //==========================================================================
private:
    // Used to keep all instances of test data (except for predefined).
    // All these instances are deleted by 'clean' when test generation finishes.
    //
    // It is important to keep all instances of test data because they may be created
    // for temporary purposes and are not easy to track automatically.
    // For example:
    //      SRCT(S32.NEW(7).ADD(8))
    // "S32.NEW(7)" creates a temporary object used as a base for ".ADD(8)"
    //
    static vector<OperandTestData*> tmpData;

    //==========================================================================
public:
    OperandTestData() {}
    virtual ~OperandTestData() {}

public:
    virtual unsigned getType() = 0;
    virtual unsigned getSize() = 0;
    virtual Val getVal(unsigned idx) = 0;

    virtual void dump() = 0;

    //==========================================================================
protected:
    static void registerData(OperandTestData* td) { tmpData.push_back(td); }

public:
    static void init(unsigned rndNum) { assert(rndNum <= MAX_RND_TEST_NUM); rndTestNum = rndNum; }
    static void clean() { for (vector<OperandTestData*>::iterator it = tmpData.begin(); it != tmpData.end(); ++it) delete *it; tmpData.clear(); }
};

unsigned OperandTestData::rndTestNum = 0; // no random values by default

//==============================================================================
//==============================================================================
//==============================================================================

vector<OperandTestData*> OperandTestData::tmpData;

//==============================================================================
//==============================================================================
//==============================================================================
// Container implementation

template<typename T>
class OperandTestDataImpl : public OperandTestData // Container for test data
{
    //==========================================================================
private:
    unsigned size;      // number of standard test values (NOT including randomly-generated)
    unsigned rsize;     // number of randomly-generated test values

    T *values;          // test values [standard,random]

    //==========================================================================
public:
    OperandTestDataImpl(unsigned sz, const T *vs, OperandTestDataImpl<T>* base = 0) : size(sz), values(0)
    {
        assert(sz > 0 && vs);

        if (base) size += base->size;       // copy base values only (not including random)
        assert(size > 0);                   // expected at least one value

        values = new T[size + rndTestNum];  // NB: some elements at the end of array may be unused

        // Clear initial number of values to reflect current (empty) state of arrays
        // This is important because adding new data to these arrays
        // require searching through previously added elements
        size  = 0;
        rsize = 0;

        if (base) for (unsigned i = 0; i < base->size; ++i)  size = addStdValue(size, base->values[i]);
                  for (unsigned i = 0; i < sz; ++i)          size = addStdValue(size, vs[i]);
        assert(size > 0); // expected at least one value

        for (unsigned i = 0; i < rndTestNum; ++i) rsize = addRndValue(rsize);
    }

    ~OperandTestDataImpl()
    {
        delete[] values;
    }

    //==========================================================================
    // The following interface functions are used for description of test data in
    // the table labelled with BEGIN_TEST_DATA ... END_TEST_DATA.
    // These functions allow construction of custom sets of test data based on
    // predefined sets for standard data types.

    // Create a set of values of base type but replace base test values with specified values. Used by 'NEW'.
    OperandTestDataImpl<T>& reset(T x)                                { T values[] = {x};                  return resetValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& reset(T x1, T x2)                         { T values[] = {x1, x2};             return resetValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& reset(T x1, T x2, T x3)                   { T values[] = {x1, x2, x3};         return resetValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& reset(T x1, T x2, T x3, T x4)             { T values[] = {x1, x2, x3, x4};     return resetValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& reset(T x1, T x2, T x3, T x4, T x5)       { T values[] = {x1, x2, x3, x4, x5}; return resetValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& resetValues(unsigned sz, const T* values) { OperandTestDataImpl<T>* res = new OperandTestDataImpl<T>(sz, values); registerData(res); return *res; }
    OperandTestDataImpl<T>& resetList(unsigned sz, const T* values)   { OperandTestDataImpl<T>* res = new OperandTestDataImpl<T>(sz / sizeof(T), values); registerData(res); return *res; }

    // Create a set of values of base type; copy base test values and add specified values. Used by 'ADD'.
    OperandTestDataImpl<T>& clone(T x)                                { T values[] = {x};                  return cloneValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& clone(T x1, T x2)                         { T values[] = {x1, x2};             return cloneValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& clone(T x1, T x2, T x3)                   { T values[] = {x1, x2, x3};         return cloneValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& clone(T x1, T x2, T x3, T x4)             { T values[] = {x1, x2, x3, x4};     return cloneValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& clone(T x1, T x2, T x3, T x4, T x5)       { T values[] = {x1, x2, x3, x4, x5}; return cloneValues(sizeof(values) / sizeof(T), values); }
    OperandTestDataImpl<T>& cloneValues(unsigned sz, const T* values) { OperandTestDataImpl<T>* res = new OperandTestDataImpl<T>(sz, values, this); registerData(res); return *res; }
    OperandTestDataImpl<T>& cloneList(unsigned sz, const T* values)   { OperandTestDataImpl<T>* res = new OperandTestDataImpl<T>(sz / sizeof(T), values, this); registerData(res); return *res; }

    //==========================================================================

    unsigned getType()     { assert(size > 0 && values); return Val(*values).getType(); } // Is there another way to do this?
    unsigned getSize()     { return size + rsize; }
    Val getVal(unsigned i) { assert(i < getSize()); return Val(values[i]); }

    void dump()
    {
        std::cout << "======================================================\n";
        std::cout << "type = " << getType() << "\n\n";

        std::cout << "standard values = [";
        for (unsigned i = 0; i < size; ++i)
        {
            if (i > 0) std::cout << ", ";
            std::cout << "\n\t\t\t" << Val(values[i]).dump();
        }
        std::cout << "\n                  ]\n\n";

        std::cout << "random values   = [";
        for (unsigned i = 0; i < rsize; ++i)
        {
            if (i > 0) std::cout << ", ";
            std::cout << "\n\t\t\t" << Val(values[size + i]).dump();
        }
        std::cout << "\n                  ]\n\n";
    }

    //==========================================================================
private:
    bool isNan(T val) { return Val(val).isNan(); }

    bool eq(T val1, T val2)
    {
        if (isNan(val1) || isNan(val2))
        {
            return isNan(val1) && isNan(val2);
        }
        else if (Val(val1).isZero() && Val(val2).isZero())        // -0.0 != +0.0
        {
            return Val(val1).isPositiveZero() == Val(val2).isPositiveZero();
        }
        else
        {
            return val1 == val2;
        }
    }

    bool isStdValue(T val)
    {
        for (unsigned i = 0; i < size; ++i) if (eq(val, values[i])) return true;
        return false;
    }

    unsigned addStdValue(unsigned pos, T val)
    {
        if (!isStdValue(val)) values[pos++] = Val(val).normalize(false);
        return pos;
    }

    unsigned addRndValue(unsigned pos)
    {
        Val v(*values); // Create properly typed value
        unsigned cnt = MAX_RND_TEST_TRY;
        for (v = v.randomize(); isStdValue(v) && cnt > 0; v = v.randomize(), --cnt); // filter out duplicated values
        if (cnt > 0) values[size + pos++] = v;
        return pos;
    }
};

//==============================================================================
//==============================================================================
//==============================================================================
// Factory for generation of predefined sets of test data (for standard HSAIL types)

class OperandTestDataFactory
{
    //==========================================================================
    // Indices of standard data types
private:
    enum
    {
        IDX_b1_t  = 1,
        IDX_b8_t,
        IDX_b16_t,
        IDX_b32_t,
        IDX_b64_t,
        IDX_b128_t,

        IDX_u8_t,
        IDX_u16_t,
        IDX_u32_t,
        IDX_u64_t,

        IDX_s8_t,
        IDX_s16_t,
        IDX_s32_t,
        IDX_s64_t,

        IDX_f16_t,
        IDX_f32_t,
        IDX_f64_t,

        IDX_s8x4_t,
        IDX_s8x8_t,
        IDX_s8x16_t,
        IDX_s16x2_t,
        IDX_s16x4_t,
        IDX_s16x8_t,
        IDX_s32x2_t,
        IDX_s32x4_t,
        IDX_s64x2_t,

        IDX_u8x4_t,
        IDX_u8x8_t,
        IDX_u8x16_t,
        IDX_u16x2_t,
        IDX_u16x4_t,
        IDX_u16x8_t,
        IDX_u32x2_t,
        IDX_u32x4_t,
        IDX_u64x2_t,

        IDX_f16x2_t,
        IDX_f16x4_t,
        IDX_f16x8_t,
        IDX_f32x2_t,
        IDX_f32x4_t,
        IDX_f64x2_t,

        TSZ // Table size
    };

    static OperandTestData* predefined[TSZ]; // tets data for standard data types

    //==========================================================================
public:
    template<typename T>
    static OperandTestDataImpl<T>* create(unsigned sz, const T *vs)
    {
        return new OperandTestDataImpl<T>(sz, vs);
    }

    static void dump(OperandTestData* data)
    {
        data->dump();
    }

    static void init();
    static void clean();

    static OperandTestData& get(unsigned type)
    {
        switch(type)
        {
        case BRIG_TYPE_B1:  return *predefined[IDX_b1_t];
        case BRIG_TYPE_B8:  return *predefined[IDX_b8_t];
        case BRIG_TYPE_B16: return *predefined[IDX_b16_t];
        case BRIG_TYPE_B32: return *predefined[IDX_b32_t];
        case BRIG_TYPE_B64: return *predefined[IDX_b64_t];
        case BRIG_TYPE_B128:return *predefined[IDX_b128_t];

        case BRIG_TYPE_U8:  return *predefined[IDX_u8_t];
        case BRIG_TYPE_U16: return *predefined[IDX_u16_t];
        case BRIG_TYPE_U32: return *predefined[IDX_u32_t];
        case BRIG_TYPE_U64: return *predefined[IDX_u64_t];

        case BRIG_TYPE_S8:  return *predefined[IDX_s8_t];
        case BRIG_TYPE_S16: return *predefined[IDX_s16_t];
        case BRIG_TYPE_S32: return *predefined[IDX_s32_t];
        case BRIG_TYPE_S64: return *predefined[IDX_s64_t];

        case BRIG_TYPE_F16: return *predefined[IDX_f16_t];
        case BRIG_TYPE_F32: return *predefined[IDX_f32_t];
        case BRIG_TYPE_F64: return *predefined[IDX_f64_t];

        case BRIG_TYPE_S8X4:  return *predefined[IDX_s8x4_t];
        case BRIG_TYPE_S8X8:  return *predefined[IDX_s8x8_t];
        case BRIG_TYPE_S8X16: return *predefined[IDX_s8x16_t];
        case BRIG_TYPE_S16X2: return *predefined[IDX_s16x2_t];
        case BRIG_TYPE_S16X4: return *predefined[IDX_s16x4_t];
        case BRIG_TYPE_S16X8: return *predefined[IDX_s16x8_t];
        case BRIG_TYPE_S32X2: return *predefined[IDX_s32x2_t];
        case BRIG_TYPE_S32X4: return *predefined[IDX_s32x4_t];
        case BRIG_TYPE_S64X2: return *predefined[IDX_s64x2_t];

        case BRIG_TYPE_U8X4:  return *predefined[IDX_u8x4_t];
        case BRIG_TYPE_U8X8:  return *predefined[IDX_u8x8_t];
        case BRIG_TYPE_U8X16: return *predefined[IDX_u8x16_t];
        case BRIG_TYPE_U16X2: return *predefined[IDX_u16x2_t];
        case BRIG_TYPE_U16X4: return *predefined[IDX_u16x4_t];
        case BRIG_TYPE_U16X8: return *predefined[IDX_u16x8_t];
        case BRIG_TYPE_U32X2: return *predefined[IDX_u32x2_t];
        case BRIG_TYPE_U32X4: return *predefined[IDX_u32x4_t];
        case BRIG_TYPE_U64X2: return *predefined[IDX_u64x2_t];

        case BRIG_TYPE_F16X2: return *predefined[IDX_f16x2_t];
        case BRIG_TYPE_F16X4: return *predefined[IDX_f16x4_t];
        case BRIG_TYPE_F16X8: return *predefined[IDX_f16x8_t];
        case BRIG_TYPE_F32X2: return *predefined[IDX_f32x2_t];
        case BRIG_TYPE_F32X4: return *predefined[IDX_f32x4_t];
        case BRIG_TYPE_F64X2: return *predefined[IDX_f64x2_t];

        default:
            throw TestGenError("OperandTestDataFactory: unsupported data type");
        }
    }
};

OperandTestData* OperandTestDataFactory::predefined[TSZ];

void OperandTestDataFactory::clean()
{
    for (int i = 0; i < TSZ; ++i)
    {
        delete predefined[i];
        predefined[i] = 0;
    }
}

//==============================================================================
//==============================================================================
//==============================================================================
// Iterator implementation

TestDataIterator::TestDataIterator() : data(0), idx(0) {}
void     TestDataIterator::init(OperandTestData* td) { assert(td); data = td; }
         
void     TestDataIterator::reset()           { idx = 0; }
bool     TestDataIterator::next()            { if (exhausted()) return false; return ++idx < data->getSize(); }
bool     TestDataIterator::exhausted() const { return idx == data->getSize(); }
bool     TestDataIterator::empty()     const { return data == 0; }
Val      TestDataIterator::get()       const { assert(idx < data->getSize()); return data->getVal(idx); }
unsigned TestDataIterator::getSize()   const { return data->getSize(); }

//==============================================================================
//==============================================================================
//==============================================================================
// TestDataProvider implementation

TestDataProvider::TestDataProvider(unsigned opType) : type(opType), firstSrcOperand(1) {}

TestDataProvider* TestDataProvider::defIterators(unsigned n, unsigned first /*=1*/)
{
    assert(n > 0 && first + n <= MAX_OPERANDS_NUM);

    firstSrcOperand = first;

    for (unsigned i = 0; i < n; ++i)
    {
        initTestData(firstSrcOperand + i, &OperandTestDataFactory::get(type));
    }

    return this;
}

TestDataProvider* TestDataProvider::def(OperandTestData &d1) { return def(1, &d1, 0, 0, 0); }
TestDataProvider* TestDataProvider::def(OperandTestData &d1, OperandTestData &d2) { return def(1, &d1, &d2, 0, 0); }
TestDataProvider* TestDataProvider::def(OperandTestData &d1, OperandTestData &d2, OperandTestData &d3) { return def(1, &d1, &d2, &d3, 0); }
TestDataProvider* TestDataProvider::def(OperandTestData &d1, OperandTestData &d2, OperandTestData &d3, OperandTestData &d4) { return def(1, &d1, &d2, &d3, &d4); }

TestDataProvider* TestDataProvider::def(unsigned first, OperandTestData &d1) { return def(first, &d1, 0, 0, 0); }
TestDataProvider* TestDataProvider::def(unsigned first, OperandTestData &d1, OperandTestData &d2) { return def(first, &d1, &d2, 0, 0); }
TestDataProvider* TestDataProvider::def(unsigned first, OperandTestData &d1, OperandTestData &d2, OperandTestData &d3) { return def(first, &d1, &d2, &d3, 0); }
TestDataProvider* TestDataProvider::def(unsigned first, OperandTestData &d1, OperandTestData &d2, OperandTestData &d3, OperandTestData &d4) { return def(first, &d1, &d2, &d3, &d4); }

TestDataProvider* TestDataProvider::def(unsigned first, OperandTestData* d1, OperandTestData* d2, OperandTestData* d3, OperandTestData* d4)
{
    assert(d1);
    assert(first < MAX_OPERANDS_NUM);

    firstSrcOperand = first;

    initTestData(firstSrcOperand + 0, d1);
    initTestData(firstSrcOperand + 1, d2);
    initTestData(firstSrcOperand + 2, d3);
    initTestData(firstSrcOperand + 3, d4);

    return this;
}

void TestDataProvider::initTestData(unsigned idx, OperandTestData* d1)
{
    if (d1)
    {
        assert(idx < MAX_OPERANDS_NUM);
        assert(idx == firstSrcOperand || testData[idx - 1].hasData()); // no gaps

        testData[idx].setData(d1);
        lastSrcOperand = idx;
    }
}

void TestDataProvider::registerOperand(unsigned i, unsigned dim, bool isConst, bool lockConst)
{
    TestDataGenerator* generator = 0;

    if      (isConst && groupImms && !lockConst) generator = &constOperands;
    else if (!isConst && groupTests)             generator = &mutableOperands;
    else                                         generator = &lockedOperands;

    testData[i].registerData(generator, dim);
}

bool TestDataProvider::next()
{
    if (constOperands.next()) {
        return true;
    } else if (mutableOperands.next()){
        constOperands.reset();
        return true;
    } else {
         assert(constOperands.exhausted());
         assert(mutableOperands.exhausted());
         return false;
    }
}

bool TestDataProvider::nextGroup()
{
    assert(constOperands.exhausted());
    assert(mutableOperands.exhausted());

    constOperands.reset();
    mutableOperands.reset();
    return lockedOperands.next();
}

void TestDataProvider::reset()
{
    constOperands.reset();
    mutableOperands.reset();
    lockedOperands.reset();
}

Val TestDataProvider::getSrcValue(unsigned argIdx)
{
    assert(argIdx < MAX_OPERANDS_NUM);

    return testData[argIdx].hasData()? testData[argIdx].get() : Val();
}

// Return properties of the current instruction
int TestDataProvider::getFirstSrcOperandIdx() { return firstSrcOperand; }     // First src operand index (usually 1)
int TestDataProvider::getDstOperandIdx()      { return firstSrcOperand - 1; } // Dst operand index, -1 if none

int TestDataProvider::getFirstOperandIdx()    { return 0; }                   // First operand index (usually this is the index of dst operand = 0)
int TestDataProvider::getLastOperandIdx()     { return lastSrcOperand; }      // Last operand index

void TestDataProvider::clean()                { OperandTestDataFactory::clean(); OperandTestData::clean(); }

void TestDataProvider::init(bool grpTests, bool grpImms, unsigned rndTestNum, unsigned ws, unsigned maxGridSz, bool testF16, bool testFtzF16)
{
    wavesize = ws;
    groupTests = grpTests;
    groupImms = grpTests && grpImms;
    maxGridSize = (maxGridSz > 0)? maxGridSz : MAX_GRID_SIZE;
    enableF16 = testF16;
    enableFtzF16 = testFtzF16;

    OperandTestDataFactory::init();
    OperandTestData::init(rndTestNum);
}

unsigned TestDataProvider::wavesize    = TestDataProvider::DEFAULT_WAVESIZE;
unsigned TestDataProvider::maxGridSize = TestDataProvider::DEFAULT_GRID_SIZE;
bool     TestDataProvider::groupTests  = true;
bool     TestDataProvider::groupImms   = true;
bool     TestDataProvider::enableF16   = false;
bool     TestDataProvider::enableFtzF16= false;

//=============================================================================
//=============================================================================
//=============================================================================

#define B1T   (static_cast<OperandTestDataImpl<b1_t>&> (OperandTestDataFactory::get(BRIG_TYPE_B1)))
#define B8T   (static_cast<OperandTestDataImpl<b8_t>&> (OperandTestDataFactory::get(BRIG_TYPE_B8)))
#define B16T  (static_cast<OperandTestDataImpl<b16_t>&>(OperandTestDataFactory::get(BRIG_TYPE_B16)))
#define B32T  (static_cast<OperandTestDataImpl<b32_t>&>(OperandTestDataFactory::get(BRIG_TYPE_B32)))
#define B64T  (static_cast<OperandTestDataImpl<b64_t>&>(OperandTestDataFactory::get(BRIG_TYPE_B64)))
#define B128T (static_cast<OperandTestDataImpl<b128_t>&>(OperandTestDataFactory::get(BRIG_TYPE_B128)))

#define U8T  (static_cast<OperandTestDataImpl<u8_t>&> (OperandTestDataFactory::get(BRIG_TYPE_U8)))
#define U16T (static_cast<OperandTestDataImpl<u16_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U16)))
#define U32T (static_cast<OperandTestDataImpl<u32_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U32)))
#define U64T (static_cast<OperandTestDataImpl<u64_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U64)))

#define S8T  (static_cast<OperandTestDataImpl<s8_t>&> (OperandTestDataFactory::get(BRIG_TYPE_S8)))
#define S16T (static_cast<OperandTestDataImpl<s16_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S16)))
#define S32T (static_cast<OperandTestDataImpl<s32_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S32)))
#define S64T (static_cast<OperandTestDataImpl<s64_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S64)))

#define F16T (static_cast<OperandTestDataImpl<f16_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F16)))
#define F32T (static_cast<OperandTestDataImpl<f32_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F32)))
#define F64T (static_cast<OperandTestDataImpl<f64_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F64)))

#define S8X4T  (static_cast<OperandTestDataImpl<s8x4_t >&>(OperandTestDataFactory::get(BRIG_TYPE_S8X4 )))
#define S8X8T  (static_cast<OperandTestDataImpl<s8x8_t >&>(OperandTestDataFactory::get(BRIG_TYPE_S8X8 )))
#define S8X16T (static_cast<OperandTestDataImpl<s8x16_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S8X16)))
#define S16X2T (static_cast<OperandTestDataImpl<s16x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S16X2)))
#define S16X4T (static_cast<OperandTestDataImpl<s16x4_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S16X4)))
#define S16X8T (static_cast<OperandTestDataImpl<s16x8_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S16X8)))
#define S32X2T (static_cast<OperandTestDataImpl<s32x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S32X2)))
#define S32X4T (static_cast<OperandTestDataImpl<s32x4_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S32X4)))
#define S64X2T (static_cast<OperandTestDataImpl<s64x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_S64X2)))

#define U8X4T  (static_cast<OperandTestDataImpl<u8x4_t >&>(OperandTestDataFactory::get(BRIG_TYPE_U8X4 )))
#define U8X8T  (static_cast<OperandTestDataImpl<u8x8_t >&>(OperandTestDataFactory::get(BRIG_TYPE_U8X8 )))
#define U8X16T (static_cast<OperandTestDataImpl<u8x16_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U8X16)))
#define U16X2T (static_cast<OperandTestDataImpl<u16x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U16X2)))
#define U16X4T (static_cast<OperandTestDataImpl<u16x4_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U16X4)))
#define U16X8T (static_cast<OperandTestDataImpl<u16x8_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U16X8)))
#define U32X2T (static_cast<OperandTestDataImpl<u32x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U32X2)))
#define U32X4T (static_cast<OperandTestDataImpl<u32x4_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U32X4)))
#define U64X2T (static_cast<OperandTestDataImpl<u64x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_U64X2)))

#define F16X2T (static_cast<OperandTestDataImpl<f16x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F16X2)))
#define F16X4T (static_cast<OperandTestDataImpl<f16x4_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F16X4)))
#define F16X8T (static_cast<OperandTestDataImpl<f16x8_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F16X8)))
#define F32X2T (static_cast<OperandTestDataImpl<f32x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F32X2)))
#define F32X4T (static_cast<OperandTestDataImpl<f32x4_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F32X4)))
#define F64X2T (static_cast<OperandTestDataImpl<f64x2_t>&>(OperandTestDataFactory::get(BRIG_TYPE_F64X2)))

#define BEGIN_TEST_DATA \
    TestDataProvider* TestDataProvider::getProvider(unsigned opcode, unsigned dstType, unsigned srcType, AluMod aluMod, unsigned srcNum)\
    {\
        switch(opcode)\
        {\
        default: {{

#define END_TEST_DATA \
            } return 0; }\
        }\
        return 0;\
    }

#define INST(inst)  } return 0; } case BRIG_OPCODE_##inst: { switch(srcType) {

#define TYPE(t) case BRIG_TYPE_##t:
#define SRC_TYPE(t) case BRIG_TYPE_##t:
#define DST_TYPE(t) case BRIG_TYPE_##t:

#define BEGIN_DST switch(dstType) {
#define END_DST   }

#define SX32TYPES   \
    TYPE(S8X4)      \
    TYPE(S16X2)

#define SX64TYPES   \
    TYPE(S8X8)      \
    TYPE(S16X4)     \
    TYPE(S32X2)

#define SX128TYPES  \
    TYPE(S8X16)     \
    TYPE(S16X8)     \
    TYPE(S32X4)     \
    TYPE(S64X2)

#define UX32TYPES   \
    TYPE(U8X4)      \
    TYPE(U16X2)

#define UX64TYPES   \
    TYPE(U8X8)      \
    TYPE(U16X4)     \
    TYPE(U32X2)

#define UX128TYPES  \
    TYPE(U8X16)     \
    TYPE(U16X8)     \
    TYPE(U32X4)     \
    TYPE(U64X2)

#define FX32TYPES   \
    TYPE(F16X2)

#define FX64TYPES   \
    TYPE(F16X4)     \
    TYPE(F32X2)

#define FX128TYPES  \
    TYPE(F16X8)     \
    TYPE(F32X4)     \
    TYPE(F64X2)

#define SXTYPES  \
    SX32TYPES    \
    SX64TYPES    \
    SX128TYPES

#define UXTYPES  \
    UX32TYPES    \
    UX64TYPES    \
    UX128TYPES

#define FXTYPES  \
    FX32TYPES    \
    FX64TYPES    \
    FX128TYPES

#define X32TYPES \
    SX32TYPES    \
    UX32TYPES

#define X64TYPES \
    SX64TYPES    \
    UX64TYPES    \
    FX64TYPES

#define XTYPES   \
    SXTYPES      \
    UXTYPES      \
    FXTYPES

#define SRCN return (new TestDataProvider(srcType))->defIterators
#define SRCT return (new TestDataProvider(srcType))->def
#define ADD clone
#define ADDL(x) cloneList(sizeof(x), x)
#define ADD_RF16() cloneValues(getRoundingTestsNum(dstType), getF16RoundingTestsData(dstType, aluMod))
#define ADD_RF32() cloneValues(getRoundingTestsNum(dstType), getF32RoundingTestsData(dstType, aluMod))
#define ADD_RF64() cloneValues(getRoundingTestsNum(dstType), getF64RoundingTestsData(dstType, aluMod))
#define NEW reset
#define NEWL(x) resetList(sizeof(x), x)

#define REGISTER_TEST_VALUES(type) \
    predefined[IDX_##type] = create<type>(sizeof(vals_##type) / sizeof(type), vals_##type); /* predefined[IDX_##type]->dump(); */

#define DCL_TEST_SET(type) \
    const type vals_##type[]

//=============================================================================
//=============================================================================
//=============================================================================
// Description of test values

#include "HSAILTestGenTestData.h"

//==============================================================================
//==============================================================================
//==============================================================================
// Initialization of test data for standard data types

void OperandTestDataFactory::init()
{
    for (int i = 0; i < TSZ; ++i) predefined[i] = 0;

    DCL_TEST_SET(b1_t)   = TEST_DATA_b1_t;
    DCL_TEST_SET(b8_t)   = TEST_DATA_b8_t;
    DCL_TEST_SET(b16_t)  = TEST_DATA_b16_t;
    DCL_TEST_SET(b32_t)  = TEST_DATA_b32_t;
    DCL_TEST_SET(b64_t)  = TEST_DATA_b64_t;
    DCL_TEST_SET(b128_t) = TEST_DATA_b128_t;


    DCL_TEST_SET(u8_t)   = TEST_DATA_u8_t;
    DCL_TEST_SET(u16_t)  = TEST_DATA_u16_t;
    DCL_TEST_SET(u32_t)  = TEST_DATA_u32_t;
    DCL_TEST_SET(u64_t)  = TEST_DATA_u64_t;

    DCL_TEST_SET(s8_t)   = TEST_DATA_s8_t;
    DCL_TEST_SET(s16_t)  = TEST_DATA_s16_t;
    DCL_TEST_SET(s32_t)  = TEST_DATA_s32_t;
    DCL_TEST_SET(s64_t)  = TEST_DATA_s64_t;

    DCL_TEST_SET(f16_t)  = TEST_DATA_f16_t;
    DCL_TEST_SET(f32_t)  = TEST_DATA_f32_t;
    DCL_TEST_SET(f64_t)  = TEST_DATA_f64_t;

    DCL_TEST_SET(s8x4_t ) = TEST_DATA_s8x4_t ;
    DCL_TEST_SET(s8x8_t ) = TEST_DATA_s8x8_t ;
    DCL_TEST_SET(s8x16_t) = TEST_DATA_s8x16_t;
    DCL_TEST_SET(s16x2_t) = TEST_DATA_s16x2_t;
    DCL_TEST_SET(s16x4_t) = TEST_DATA_s16x4_t;
    DCL_TEST_SET(s16x8_t) = TEST_DATA_s16x8_t;
    DCL_TEST_SET(s32x2_t) = TEST_DATA_s32x2_t;
    DCL_TEST_SET(s32x4_t) = TEST_DATA_s32x4_t;
    DCL_TEST_SET(s64x2_t) = TEST_DATA_s64x2_t;

    DCL_TEST_SET(u8x4_t ) = TEST_DATA_u8x4_t ;
    DCL_TEST_SET(u8x8_t ) = TEST_DATA_u8x8_t ;
    DCL_TEST_SET(u8x16_t) = TEST_DATA_u8x16_t;
    DCL_TEST_SET(u16x2_t) = TEST_DATA_u16x2_t;
    DCL_TEST_SET(u16x4_t) = TEST_DATA_u16x4_t;
    DCL_TEST_SET(u16x8_t) = TEST_DATA_u16x8_t;
    DCL_TEST_SET(u32x2_t) = TEST_DATA_u32x2_t;
    DCL_TEST_SET(u32x4_t) = TEST_DATA_u32x4_t;
    DCL_TEST_SET(u64x2_t) = TEST_DATA_u64x2_t;

    DCL_TEST_SET(f16x2_t) = TEST_DATA_f16x2_t;
    DCL_TEST_SET(f16x4_t) = TEST_DATA_f16x4_t;
    DCL_TEST_SET(f16x8_t) = TEST_DATA_f16x8_t;
    DCL_TEST_SET(f32x2_t) = TEST_DATA_f32x2_t;
    DCL_TEST_SET(f32x4_t) = TEST_DATA_f32x4_t;
    DCL_TEST_SET(f64x2_t) = TEST_DATA_f64x2_t;


    REGISTER_TEST_VALUES(b1_t);
    REGISTER_TEST_VALUES(b8_t);
    REGISTER_TEST_VALUES(b16_t);
    REGISTER_TEST_VALUES(b32_t);
    REGISTER_TEST_VALUES(b64_t);
    REGISTER_TEST_VALUES(b128_t);

    REGISTER_TEST_VALUES(u8_t);
    REGISTER_TEST_VALUES(u16_t);
    REGISTER_TEST_VALUES(u32_t);
    REGISTER_TEST_VALUES(u64_t);

    REGISTER_TEST_VALUES(s8_t);
    REGISTER_TEST_VALUES(s16_t);
    REGISTER_TEST_VALUES(s32_t);
    REGISTER_TEST_VALUES(s64_t);

    REGISTER_TEST_VALUES(f16_t);
    REGISTER_TEST_VALUES(f32_t);
    REGISTER_TEST_VALUES(f64_t);

    REGISTER_TEST_VALUES(s8x4_t );
    REGISTER_TEST_VALUES(s8x8_t );
    REGISTER_TEST_VALUES(s8x16_t);
    REGISTER_TEST_VALUES(s16x2_t);
    REGISTER_TEST_VALUES(s16x4_t);
    REGISTER_TEST_VALUES(s16x8_t);
    REGISTER_TEST_VALUES(s32x2_t);
    REGISTER_TEST_VALUES(s32x4_t);
    REGISTER_TEST_VALUES(s64x2_t);

    REGISTER_TEST_VALUES(u8x4_t );
    REGISTER_TEST_VALUES(u8x8_t );
    REGISTER_TEST_VALUES(u8x16_t);
    REGISTER_TEST_VALUES(u16x2_t);
    REGISTER_TEST_VALUES(u16x4_t);
    REGISTER_TEST_VALUES(u16x8_t);
    REGISTER_TEST_VALUES(u32x2_t);
    REGISTER_TEST_VALUES(u32x4_t);
    REGISTER_TEST_VALUES(u64x2_t);

    REGISTER_TEST_VALUES(f16x2_t);
    REGISTER_TEST_VALUES(f16x4_t);
    REGISTER_TEST_VALUES(f16x8_t);
    REGISTER_TEST_VALUES(f32x2_t);
    REGISTER_TEST_VALUES(f32x4_t);
    REGISTER_TEST_VALUES(f64x2_t);
}

//==============================================================================
//==============================================================================
//==============================================================================

} // namespace TESTGEN
