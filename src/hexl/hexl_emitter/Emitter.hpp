/*
   Copyright 2014 Heterogeneous System Architecture (HSA) Foundation

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

#ifndef HEXL_EMITTER_HPP
#define HEXL_EMITTER_HPP

#include "Arena.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include "MObject.hpp"
#include "HSAILItems.h"
#include "EmitterCommon.hpp"
#include "Grid.hpp"
#include "MObject.hpp"
#include "HexlTest.hpp"
#include "Scenario.hpp"
#include "Sequence.hpp"
#include "CoreConfig.hpp"
#include "Utils.hpp"
#include "Image.hpp"

namespace hexl {

namespace Bools {
  hexl::Sequence<bool>* All();
  hexl::Sequence<bool>* Value(bool val);
}

std::string Dir2Str(BrigControlDirective d);

template <>
inline const char *EmptySequenceName<BrigControlDirective>() { return "ND"; }

template <>
inline void PrintSequenceItem<BrigControlDirective>(std::ostream& out, const BrigControlDirective& d) {
  out << Dir2Str(d);
}

namespace emitter {

Sequence<Location>* CodeLocations();
Sequence<Location>* KernelLocation();

enum BufferType {
  HOST_INPUT_BUFFER,
  HOST_RESULT_BUFFER,
  MODULE_BUFFER,
  KERNEL_BUFFER,
};

class EmitterObject {
  ARENAMEM

protected:
  EmitterObject(const EmitterObject& other) { }

public:
  EmitterObject() { }
  virtual void Name(std::ostream& out) const { assert(0); }
  virtual void Print(std::ostream& out) const { Name(out); }
  virtual ~EmitterObject() { }
};

class Emittable : public EmitterObject {
protected:
  TestEmitter* te;

public:
  explicit Emittable(TestEmitter* te_ = 0)
    : te(te_) { }
  virtual ~Emittable() { }

  Grid Geometry();

  virtual void Reset(TestEmitter* te) { this->te = te; }

  virtual bool IsValid() const { return true; }

  virtual void Name(std::ostream& out) const { }

  virtual void Test() { }

  virtual void Init() { }
  virtual void Finish() { }

  virtual void StartProgram() { }
  virtual void EndProgram() { }

  virtual void StartModule() { }
  virtual void ModuleDirectives() { }
  virtual void ModuleVariables() { }
  virtual void EndModule() { }

  virtual void StartFunction() { }
  virtual void FunctionFormalOutputArguments() { }
  virtual void FunctionFormalInputArguments() { }
  virtual void StartFunctionBody() { }
  virtual void FunctionDirectives() { }
  virtual void FunctionVariables() { }
  virtual void FunctionInit() { }
  virtual void EndFunction() { }

  virtual void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs) { }

  virtual void StartKernel() { }
  virtual void KernelArguments() { }
  virtual void StartKernelBody() { }
  virtual void KernelDirectives() { }
  virtual void KernelVariables() { }
  virtual void KernelInit() { }
  virtual void EndKernel() { }

  virtual void SetupDispatch(DispatchSetup* dispatch) { }

  virtual void ScenarioInit() { }
  virtual void ScenarioCodes() { }
  virtual void ScenarioDispatches() { }
  virtual void ScenarioValidation() { }
  virtual void ScenarioEnd() { }
};

class ETypedReg : public EmitterObject {
public:
  ETypedReg()
    : type(BRIG_TYPE_NONE) { }
  ETypedReg(BrigType16_t type_)
    : type(type_) { }
  ETypedReg(HSAIL_ASM::OperandRegister reg, BrigType16_t type_)
    : type(type_) { Add(reg); }

  HSAIL_ASM::OperandRegister Reg() const { assert(Count() == 1); return regs[0]; }
  HSAIL_ASM::OperandRegister Reg(size_t i) const { return regs[(int) i]; }
  HSAIL_ASM::ItemList&  Regs() { return regs; }
  const HSAIL_ASM::ItemList& Regs() const { return regs; }
  BrigType16_t Type() const { return type; }
  unsigned TypeSizeBytes() const { return HSAIL_ASM::getBrigTypeNumBytes(type); }
  unsigned TypeSizeBits() const { return HSAIL_ASM::getBrigTypeNumBits(type); }
  size_t Count() const { return regs.size(); }
  void Add(HSAIL_ASM::OperandRegister reg) { regs.push_back(reg); }

private:
  HSAIL_ASM::ItemList regs;
  BrigType16_t type;
};

class ETypedRegList : public EmitterObject {
private:
  std::vector<TypedReg> tregs;

public:
  size_t Count() const { return tregs.size(); }
  TypedReg Get(size_t i) { return tregs[i]; }
  void Add(TypedReg treg) { tregs.push_back(treg); }
  void Clear() { tregs.clear(); }
};

class EPointerReg : public ETypedReg {
public:
  static BrigType GetSegmentPointerType(BrigSegment8_t segment, bool large);

  EPointerReg(HSAIL_ASM::OperandRegister reg_, BrigType16_t type_, BrigSegment8_t segment_)
    : ETypedReg(reg_, type_), segment(segment_) { }

  BrigSegment8_t Segment() const { return segment; }
  bool IsLarge() const { return Type() == BRIG_TYPE_U64; }

private:
  BrigSegment8_t segment;
};

class EVariableSpec : public Emittable {
protected:
  Location location;
  BrigSegment segment;
  BrigType type;
  BrigAlignment align;
  uint64_t dim;
  bool isConst;
  bool output;

  bool IsValidVar() const;
  bool IsValidAt(Location location) const;

public:
  EVariableSpec(BrigSegment segment_, BrigType type_, Location location_ = AUTO, BrigAlignment align_ = BRIG_ALIGNMENT_NONE, uint64_t dim_ = 0, bool isConst_ = false, bool output_ = false)
    : location(location_), segment(segment_), type(type_), align(align_), dim(dim_), isConst(isConst_), output(output_) { }
  EVariableSpec(const EVariableSpec& spec, bool output_)
    : location(spec.location), segment(spec.segment), type(spec.type), align(spec.align), dim(spec.dim), isConst(spec.isConst), output(output_) { }

  bool IsValid() const;
  void Name(std::ostream& out) const;
  BrigSegment Segment() const { return segment; }
  BrigType Type() const { return type; }
  ValueType VType() const { return Brig2ValueType(type); }
  BrigAlignment Align() const { return align; }
  unsigned AlignNum() const { return HSAIL_ASM::align2num(align); }
  uint64_t Dim() const { return dim; }
  uint32_t Dim32() const { assert(dim <= UINT32_MAX); return (uint32_t) dim; }
  uint32_t Count() const { return (std::max)(Dim32(), (uint32_t) 1); }
  bool IsArray() const { return dim > 0; }
};


class EVariable : public EVariableSpec {
private:
  std::string id;
  HSAIL_ASM::DirectiveVariable var;
  hexl::Values data;

  Location RealLocation() const;

public:
  EVariable(TestEmitter* te_, const std::string& id_, BrigSegment segment_, BrigType type_, Location location_, BrigAlignment align_ = BRIG_ALIGNMENT_NONE, uint64_t dim_ = 0, bool isConst_ = false, bool output_ = false)
    : EVariableSpec(segment_, type_, location_, align_, dim_, isConst_, output_), id(id_) { te = te_;  }
  EVariable(TestEmitter* te_, const std::string& id_, const EVariableSpec* spec_)
    : EVariableSpec(*spec_), id(id_) { te = te_; }
  EVariable(TestEmitter* te_, const std::string& id_, const EVariableSpec* spec_, bool output)
    : EVariableSpec(*spec_, output), id(id_) { te = te_; }

  std::string VariableName() const;
  HSAIL_ASM::DirectiveVariable Variable() { assert(var != 0); return var; }

  void PushBack(hexl::Value val);
  void WriteData(hexl::Value val, size_t pos); 

  void Name(std::ostream& out) const;

  TypedReg AddDataReg();
  void EmitDefinition();
  void EmitInitializer();
  void EmitLoadTo(TypedReg dst, bool useVectorInstructions = true);
  void EmitStoreFrom(TypedReg src, bool useVectorInstructions = true);

  void ModuleVariables();
  void FunctionFormalOutputArguments();
  void FunctionFormalInputArguments();
  void FunctionVariables();
  void KernelArguments();
  void KernelVariables();

  void SetupDispatch(DispatchSetup* setup);
};


class EFBarrier : public Emittable {
private:
  std::string id;
  Location location;
  bool output;
  HSAIL_ASM::DirectiveFbarrier fb;

public:
  EFBarrier(TestEmitter* te, const std::string& id, Location location = Location::KERNEL, bool output = false);

  std::string FBarrierName() const;
  HSAIL_ASM::DirectiveFbarrier FBarrier() const { assert(fb); return fb; }

  void EmitDefinition();
  void EmitInitfbar();
  void EmitInitfbarInFirstWI();
  void EmitJoinfbar();
  void EmitWaitfbar();
  void EmitArrivefbar();
  void EmitLeavefbar();
  void EmitReleasefbar();
  void EmitReleasefbarInFirstWI();
  void EmitLdf(TypedReg dest);

  void Name(std::ostream& out) const override;

  void ModuleVariables() override;
  void FunctionFormalOutputArguments() override;
  void FunctionFormalInputArguments() override;
  void FunctionVariables() override;
  void KernelVariables() override;
};


class EAddressSpec : public Emittable {
protected:
  VariableSpec varSpec;

public:
  BrigType Type() { return varSpec->Type(); }
  ValueType VType() { return varSpec->VType(); }
};

class EAddress : public EAddressSpec {
public:
  struct Spec {
    VariableSpec varSpec;
    bool hasOffset;
    bool hasRegister;
  };

private:
  Spec spec;
  Variable var;

public:
  HSAIL_ASM::OperandAddress Address();
};


class EControlDirectives : public Emittable {
private:
  const hexl::Sequence<BrigControlDirective>* spec;

  void Emit();

public:
  EControlDirectives(const hexl::Sequence<BrigControlDirective>* spec_)
    : spec(spec_) { }

  void Name(std::ostream& out) const;

  const hexl::Sequence<BrigControlDirective>* Spec() const { return spec; }
  bool Has(BrigControlDirective d) const { return spec->Has(d); }
  void FunctionDirectives();
  void KernelDirectives();
};

class EmittableContainer : public Emittable {
private:
  std::vector<Emittable*> list;

public:
  EmittableContainer(TestEmitter* te = 0)
    : Emittable(te) { } 

  void Add(Emittable* e) { list.push_back(e); }
  void Name(std::ostream& out) const;
  void Reset(TestEmitter* te) { Emittable::Reset(te); for (Emittable* e : list) { e->Reset(te); } }

  void Init() { for (Emittable* e : list) { e->Init(); } }
  void StartModule() { for (Emittable* e : list) { e->StartModule(); } }
  void ModuleVariables() { for (Emittable* e : list) { e->ModuleVariables(); } }
  void EndModule() { for (Emittable* e : list) { e->EndModule(); } }

  void FunctionFormalInputArguments() { for (Emittable* e : list) { e->FunctionFormalInputArguments(); } }
  void FunctionFormalOutputArguments() { for (Emittable* e : list) { e->FunctionFormalOutputArguments(); } }
  void FunctionVariables() { for (Emittable* e : list) { e->FunctionVariables(); } }
  void FunctionDirectives() { for (Emittable* e : list) { e->FunctionDirectives(); }}
  void FunctionInit() { for (Emittable* e : list) { e->FunctionInit(); }}
  void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs) { for (Emittable* e : list) { e->ActualCallArguments(inputs, outputs); } }

  void KernelArguments() { for (Emittable* e : list) { e->KernelArguments(); } }
  void KernelVariables() { for (Emittable* e : list) { e->KernelVariables(); } }
  void KernelDirectives() { for (Emittable* e : list) { e->KernelDirectives(); }}
  void KernelInit() { for (Emittable* e : list) { e->KernelInit(); }}
  void StartKernelBody() { for (Emittable* e : list) { e->StartKernelBody(); } }

  void SetupDispatch(DispatchSetup* dispatch) { for (Emittable* e : list) { e->SetupDispatch(dispatch); } }
  void ScenarioInit() { for (Emittable* e : list) { e->ScenarioInit(); } }
  void ScenarioCodes() { for (Emittable* e : list) { e->ScenarioCodes(); } }
  void ScenarioDispatches() { for (Emittable* e : list) { e->ScenarioDispatches(); } }

  void ScenarioEnd() { for (Emittable* e : list) { e->ScenarioEnd(); } }

  Variable NewVariable(const std::string& id, BrigSegment segment, BrigType type, Location location = AUTO, BrigAlignment align = BRIG_ALIGNMENT_NONE, uint64_t dim = 0, bool isConst = false, bool output = false);
  Variable NewVariable(const std::string& id, VariableSpec varSpec);
  Variable NewVariable(const std::string& id, VariableSpec varSpec, bool output);
  FBarrier NewFBarrier(const std::string& id, Location location = Location::KERNEL, bool output = false);
  Buffer NewBuffer(const std::string& id, BufferType type, ValueType vtype, size_t count);
  UserModeQueue NewQueue(const std::string& id, UserModeQueueType type);
  Kernel NewKernel(const std::string& id);
  Function NewFunction(const std::string& id);
  Image NewImage(const std::string& id, ImageSpec spec);
  Sampler NewSampler(const std::string& id, SamplerSpec spec);
};

class EBuffer : public Emittable {
private:
  std::string id;
  BufferType type;
  ValueType vtype;
  size_t count;
  std::unique_ptr<Values> data;
  HSAIL_ASM::DirectiveVariable variable;
  PointerReg address[2];
  PointerReg dataOffset;

  HSAIL_ASM::DirectiveVariable EmitAddressDefinition(BrigSegment segment);
  void EmitBufferDefinition();

  HSAIL_ASM::OperandAddress DataAddress(TypedReg index, bool flat = false, uint64_t count = 1);

public:
  EBuffer(TestEmitter* te, const std::string& id_, BufferType type_, ValueType vtype_, size_t count_)
    : Emittable(te), id(id_), type(type_), vtype(vtype_), count(count_), data(new Values()), dataOffset(0) {
    address[0] = 0; address[1] = 0;
  }

  std::string IdData() const { return id + ".data"; }
  void AddData(Value v) { data->push_back(v); }
  void SetData(Values* values) { data.reset(values); }
  Values* ReleaseData() { return data.release(); }

  HSAIL_ASM::DirectiveVariable Variable();
  PointerReg Address(bool flat = false);

  size_t Count() const { return count; }
  size_t TypeSize() const { return ValueTypeSize(vtype); }
  size_t Size() const;

  void KernelArguments();
  void KernelVariables();
  void SetupDispatch(DispatchSetup* dsetup);
  void ScenarioInit();
  void Validation();

  TypedReg AddDataReg();
  PointerReg AddAReg(bool flat = false);
  void EmitLoadData(TypedReg dest, TypedReg index, bool flat = false);
  void EmitLoadData(TypedReg dest, bool flat = false);
  void EmitStoreData(TypedReg src, TypedReg index, bool flat = false);
  void EmitStoreData(TypedReg src, bool flat = false);
};

class EUserModeQueue : public Emittable {
private:
  std::string id;
  UserModeQueueType type;
  HSAIL_ASM::DirectiveVariable queueKernelArg;
  PointerReg address;
  PointerReg serviceQueue;
  TypedReg doorbellSignal;
  TypedReg size;
  PointerReg baseAddress;

public:
  EUserModeQueue(TestEmitter* te, const std::string& id_, UserModeQueueType type_)
    : Emittable(te), id(id_), type(type_), address(0), doorbellSignal(0), size(0), baseAddress(0) { }
  EUserModeQueue(TestEmitter* te, const std::string& id_, PointerReg address_ = 0)
    : Emittable(te), id(id_), type(USER_PROVIDED), address(address_), doorbellSignal(0), size(0), baseAddress(0) { }

//  void Name(std::ostream& out) { out << UserModeQueueType2Str(type); }

  PointerReg Address(BrigSegment segment = BRIG_SEGMENT_GLOBAL);

  void KernelArguments();
  void StartKernelBody();
  void SetupDispatch(hexl::DispatchSetup* setup);
  void ScenarioInit();

  void EmitLdQueueReadIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest);
  void EmitLdQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest);
  void EmitStQueueReadIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg src);
  void EmitStQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg src);  
  void EmitAddQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest, HSAIL_ASM::Operand src);
  void EmitCasQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest, HSAIL_ASM::Operand src0, HSAIL_ASM::Operand src1);

  TypedReg DoorbellSignal();
  TypedReg EmitLoadDoorbellSignal();
  TypedReg Size();
  TypedReg EmitLoadSize();
  PointerReg ServiceQueue();
  PointerReg EmitLoadServiceQueue();
  PointerReg BaseAddress();
  PointerReg EmitLoadBaseAddress();
};

class ESignal : public Emittable {
private:
  std::string id;
  uint64_t initialValue;
  HSAIL_ASM::DirectiveVariable kernelArg;

public:
  ESignal(TestEmitter* te, const std::string& id_, uint64_t initialValue_)
    : Emittable(te), id(id_), initialValue(initialValue_) { }

  const std::string& Id() const { return id; }
  HSAIL_ASM::DirectiveVariable KernelArg() { assert(kernelArg != 0); return kernelArg; }

  void ScenarioInit();
  void KernelArguments();
  void SetupDispatch(DispatchSetup* dispatch);

  TypedReg AddReg();
  TypedReg AddValueReg();
};

class EImageSpec : public EVariableSpec {
protected:
  BrigImageGeometry geometry;
  BrigImageChannelOrder channel_order;
  BrigImageChannelType channel_type;
  size_t width;
  size_t height;
  size_t depth;
  size_t rowPitch;
  size_t slicePitch;
  size_t array_size;

  bool IsValidSegment() const;
  bool IsValidType() const;
  
public:
  explicit EImageSpec(
    BrigSegment brigseg_ = BRIG_SEGMENT_GLOBAL, 
    BrigType imageType_ = BRIG_TYPE_RWIMG, 
    Location location_ = Location::KERNEL, 
    uint64_t dim_ = 0, 
    bool isConst_ = false, 
    bool output_ = false,
    BrigImageGeometry geometry_ = BRIG_GEOMETRY_1D,
    BrigImageChannelOrder channel_order_ = BRIG_CHANNEL_ORDER_A, 
    BrigImageChannelType channel_type_ = BRIG_CHANNEL_TYPE_SNORM_INT8,
    size_t width_ = 0, 
    size_t height_ = 0, 
    size_t depth_ = 0, 
    size_t array_size_ = 0
    );

  bool IsValid() const;

  BrigImageGeometry Geometry() { return geometry; }
  BrigImageChannelOrder ChannelOrder() { return channel_order; }
  BrigImageChannelType ChannelType() { return channel_type; }
  size_t Width() { return width; }
  size_t Height() { return height; }
  size_t Depth() { return depth; }
  size_t RowPitch() { return rowPitch; }
  size_t SlicePitch() { return slicePitch; }
  size_t ArraySize() { return array_size; }
  
  void Geometry(BrigImageGeometry geometry_) { geometry = geometry_; }
  void ChannelOrder(BrigImageChannelOrder channel_order_) { channel_order = channel_order_; }
  void ChannelType(BrigImageChannelType channel_type_) { channel_type = channel_type_; }
  void Width(size_t width_) { width = width_; }
  void Height(size_t height_) { height = height_; }
  void Depth(size_t depth_) { depth = depth_; }
  void RowPitch(size_t rowPitch_) { rowPitch = rowPitch_; }
  void SlicePitch(size_t slicePitch_) { slicePitch = slicePitch_; }
  void ArraySize(size_t array_size_) { array_size = array_size_; }

  hexl::ImageGeometry ImageGeometry() { return hexl::ImageGeometry((unsigned)width, (unsigned)height, (unsigned)depth, (unsigned)array_size); }
};

class EImageCalc {
private:
  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;
  BrigSamplerCoordNormalization samplerCoord;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

  Value color_zero;
  Value color_one;
  bool bWithoutSampler;
  Value existVal;

  void SetupDefaultColors();
  float UnnormalizeCoord(Value* c, unsigned dimSize) const;
  float UnnormalizeArrayCoord(Value* c) const;
  int round_downi(float f) const;
  int round_neari(float f) const;
  int clamp_i(int a, int min, int max) const;
  float clamp_f(float a, float min, float max) const;
  int GetTexelIndex(float f, unsigned _dimSize) const;
  int GetTexelArrayIndex(float f, unsigned dimSize) const;
  void LoadBorderData(Value* _color) const;
  uint32_t GetRawColorData() const;
  int32_t SignExtend(uint32_t c, unsigned int bit_size) const;
  float ConvertionSignedNormalize(uint32_t c, unsigned int bit_size) const;
  float ConvertionUnsignedNormalize(uint32_t c, unsigned int bit_size) const;
  int32_t ConvertionSignedClamp(uint32_t c, unsigned int bit_size) const;
  uint32_t ConvertionUnsignedClamp(uint32_t c, unsigned int bit_size) const;
  float ConvertionHalfFloat(uint32_t data) const;
  Value ConvertRawData(uint32_t data) const;
  float GammaCorrection(float f) const;
  void LoadColorData(int x_ind, int y_ind, int z_ind, Value* _color) const;
  void LoadTexel(int x_ind, int y_ind, int z_ind, Value* _color) const;
  void LoadFloatTexel(int x, int y, int z, double* const f) const;
  void EmulateReadColor(Value* _coords, Value* _color) const;

public:
  EImageCalc(EImage * eimage, ESampler* esampler, Value val);
  void ReadColor(Value* _coords, Value* _color) const;
};

class EImage : public EImageSpec {
private:
  std::string id;
  HSAIL_ASM::DirectiveVariable var;
  MImage* image;
  std::unique_ptr<Values> data;
  bool bLimitTestOn;
  ImageCalc calculator;

  HSAIL_ASM::DirectiveVariable EmitAddressDefinition(BrigSegment segment);
  void EmitInitializer();
  void EmitDefinition();

  Location RealLocation() const;

public:
  EImage(TestEmitter* te_, const std::string& id_, const EImageSpec* spec) : EImageSpec(*spec), id(id_), data(new Values()), bLimitTestOn(false), calculator(NULL) { te = te_; }
  ~EImage() { if (calculator) delete calculator; }

  const std::string& Id() const { return id; }

  void KernelArguments();
  void ModuleVariables();
  void FunctionFormalOutputArguments();
  void FunctionFormalInputArguments();
  void FunctionVariables();
  void KernelVariables();

  void SetupDispatch(DispatchSetup* dispatch);
  void EmitImageRd(HSAIL_ASM::OperandOperandList dest, BrigType destType, TypedReg image, TypedReg sampler, TypedReg coord);
  void EmitImageRd(HSAIL_ASM::OperandOperandList dest, BrigType destType, TypedReg image, TypedReg sampler, HSAIL_ASM::OperandOperandList coord, BrigType coordType);
  void EmitImageRd(TypedReg dest, TypedReg image, TypedReg sampler, HSAIL_ASM::OperandOperandList coord, BrigType coordType);
  void EmitImageLd(HSAIL_ASM::OperandOperandList dest, BrigType destType, TypedReg image, TypedReg coord);
  void EmitImageLd(HSAIL_ASM::OperandOperandList dest, BrigType destType, TypedReg image, HSAIL_ASM::OperandOperandList coord, BrigType coordType);
  void EmitImageLd(TypedReg dest, TypedReg image, HSAIL_ASM::OperandOperandList coord, BrigType coordType);
  void EmitImageSt(HSAIL_ASM::OperandOperandList src, BrigType srcType, TypedReg image, TypedReg coord);
  void EmitImageSt(HSAIL_ASM::OperandOperandList src, BrigType srcType, TypedReg image, HSAIL_ASM::OperandOperandList coord, BrigType coordType);
  void EmitImageSt(TypedReg src, TypedReg image, HSAIL_ASM::OperandOperandList coord, BrigType coordType);
  void EmitImageQuery(TypedReg dest, TypedReg image, BrigImageQuery query);

  HSAIL_ASM::DirectiveVariable Variable() { assert(var != 0); return var; }

  void AddData(Value v) { data->push_back(v); }
  void SetData(Values* values) { data.reset(values); }
  Values* ReleaseData() { return data.release(); }
  Value GetRawData() { return (*data).at(0); }
  void LimitEnable(bool bEnable) { bLimitTestOn = bEnable; }
  void InitImageCalculator(Sampler pSampler, Value val) { calculator = new EImageCalc(this, pSampler, val); }
  void ReadColor(Value* _coords, Value* _color) const { assert(calculator); calculator->ReadColor(_coords, _color); }
};

class ESamplerSpec : public EVariableSpec {
protected:
  BrigSamplerCoordNormalization coord;
  BrigSamplerFilter filter;
  BrigSamplerAddressing addressing;

  bool IsValidSegment() const;
  
public:
  explicit ESamplerSpec(
    BrigSegment brigseg_ = BRIG_SEGMENT_GLOBAL, 
    Location location_ = Location::KERNEL, 
    uint64_t dim_ = 0, 
    bool isConst_ = false, 
    bool output_ = false,
    BrigSamplerCoordNormalization coord_ = BRIG_COORD_UNNORMALIZED,
    BrigSamplerFilter filter_ = BRIG_FILTER_NEAREST,
    BrigSamplerAddressing addressing_ = BRIG_ADDRESSING_UNDEFINED
  ) 
  : EVariableSpec(brigseg_, BRIG_TYPE_SAMP, location_, BRIG_ALIGNMENT_8, dim_, isConst_, output_), 
  coord(coord_), filter(filter_), addressing(addressing_) {}

  bool IsValid() const;

  BrigSamplerCoordNormalization CoordNormalization() { return coord; }
  BrigSamplerFilter Filter() { return filter; }
  BrigSamplerAddressing Addresing() { return addressing; }
  
  void CoordNormalization(BrigSamplerCoordNormalization coord_) { coord = coord_; }
  void Filter(BrigSamplerFilter filter_) { filter = filter_; }
  void Addresing(BrigSamplerAddressing addressing_) { addressing = addressing_; }
};

class ESampler : public ESamplerSpec {
private:
  std::string id;
  HSAIL_ASM::DirectiveVariable var;
  MSampler* sampler;

  HSAIL_ASM::DirectiveVariable EmitAddressDefinition(BrigSegment segment);
  void EmitInitializer();
  void EmitDefinition();

  bool IsValidSegment() const;
  Location RealLocation() const;

public:
  ESampler(TestEmitter* te_, const std::string& id_, const ESamplerSpec* spec_): ESamplerSpec(*spec_), id(id_) { te = te_; }
  
  const std::string& Id() const { return id; }

  void KernelArguments();
  void ModuleVariables();
  void FunctionFormalOutputArguments();
  void FunctionFormalInputArguments();
  void FunctionVariables();
  void KernelVariables();

  void SetupDispatch(DispatchSetup* dispatch);
  void EmitSamplerQuery(TypedReg dest, TypedReg sampler, BrigSamplerQuery query);
  HSAIL_ASM::DirectiveVariable Variable() { assert(var != 0); return var; }
  TypedReg AddReg();
  TypedReg AddValueReg();
};


class EKernel : public EmittableContainer {
private:
  std::string id;
  HSAIL_ASM::DirectiveKernel kernel;

public:
  EKernel(TestEmitter* te, const std::string& id_)
    : EmittableContainer(te), id(id_) { }

  std::string KernelName() const { return "&" + id; }
  HSAIL_ASM::DirectiveKernel Directive() { assert(kernel != 0); return kernel; }
  HSAIL_ASM::Offset BrigOffset() { return Directive().brigOffset(); }
  void StartKernel();
  void StartKernelBody();
  void EndKernel();
};

class EFunction : public EmittableContainer {
private:
  std::string id;
  HSAIL_ASM::DirectiveFunction function;

public:
  EFunction(TestEmitter* te, const std::string& id_)
    : EmittableContainer(te), id(id_) { }

  std::string FunctionName() const { return "&" + id; }
  HSAIL_ASM::DirectiveFunction Directive() { assert(function != 0); return function; }
  HSAIL_ASM::Offset BrigOffset() { return Directive().brigOffset(); }
  void StartFunction();
  void StartFunctionBody();
  void EndFunction();
};

const char* ConditionType2Str(ConditionType type);
const char* ConditionInput2Str(ConditionInput input);

class ECondition : public Emittable {
private:
  std::string id;
  ConditionType type;
  ConditionInput input;
  BrigType itype;
  BrigWidth width;

  HSAIL_ASM::DirectiveVariable kernarg, funcarg;
  TypedReg kerninp, funcinp;
  Buffer condBuffer;
  std::string lThen, lElse, lEnd;
  std::vector<std::string> labels;
  
  std::string KernargName();
  TypedReg InputData();
  
  std::string Id();

public:
  ECondition(ConditionType type_, ConditionInput input_, BrigWidth width_)
    : type(type_), input(input_), itype(BRIG_TYPE_U32), width(width_),
      kerninp(0), funcinp(0), condBuffer(0)
      { }
  ECondition(ConditionType type_, ConditionInput input_, BrigType itype_, BrigWidth width_)
    : type(type_), input(input_), itype(itype_), width(width_),
      kerninp(0), funcinp(0), condBuffer(0)
      { }
    
  ConditionInput Input() { return input; }

  void Name(std::ostream& out) const;
  void Reset(TestEmitter* te);

  void Init();
  void KernelArguments();
  void KernelVariables();
  void KernelInit();
  void FunctionFormalInputArguments();
  void FunctionInit();
  void SetupDispatch(DispatchSetup* dsetup);
  void ScenarioInit();
  void Validation();
  void ActualCallArguments(TypedRegList inputs, TypedRegList outputs);

  bool IsTrueFor(uint64_t wi);
  bool IsTrueFor(const Dim& point) { return IsTrueFor(Geometry()->WorkitemFlatAbsId(point)); }

  HSAIL_ASM::Operand CondOperand();

  HSAIL_ASM::Operand EmitIfCond();
  void EmitIfThenStart();
  void EmitIfThenEnd();
  bool ExpectThenPath(uint64_t wi);
  bool ExpectThenPath(const Dim& point) { return ExpectThenPath(Geometry()->WorkitemFlatAbsId(point)); }

  void EmitIfThenElseStart();
  void EmitIfThenElseOtherwise();
  void EmitIfThenElseEnd();

  HSAIL_ASM::Operand EmitSwitchCond();
  void EmitSwitchStart();
  void EmitSwitchBranchStart(unsigned i);
  void EmitSwitchEnd();
  unsigned SwitchBranchCount();
  unsigned ExpectedSwitchPath(uint64_t i);

  uint32_t InputValue(uint64_t id, BrigWidth width = BRIG_WIDTH_NONE);
};

class BrigEmitter;
class CoreConfig;

class TestEmitter {
private:
  Arena ap;
  std::unique_ptr<BrigEmitter> be;
  std::unique_ptr<Context> initialContext;
  std::unique_ptr<hexl::scenario::Scenario> scenario;
  CoreConfig* coreConfig;

public:
  TestEmitter();

  void SetCoreConfig(CoreConfig* cc);

  Arena* Ap() { return &ap; }

  BrigEmitter* Brig() { return be.get(); }
  CoreConfig* CoreCfg() { return coreConfig; }

  Context* InitialContext() { return initialContext.get(); }
  Context* ReleaseContext() { return initialContext.release(); }
  hexl::scenario::Scenario* TestScenario() { return scenario.get(); }
  hexl::scenario::Scenario* ReleaseScenario() { return scenario.release(); }

  Variable NewVariable(const std::string& id, BrigSegment segment, BrigType type, Location location = AUTO, BrigAlignment align = BRIG_ALIGNMENT_NONE, uint64_t dim = 0, bool isConst = false, bool output = false);
  Variable NewVariable(const std::string& id, VariableSpec varSpec);
  Variable NewVariable(const std::string& id, VariableSpec varSpec, bool output);
  FBarrier NewFBarrier(const std::string& id, Location location = Location::KERNEL, bool output = false);
  Buffer NewBuffer(const std::string& id, BufferType type, ValueType vtype, size_t count);
  UserModeQueue NewQueue(const std::string& id, UserModeQueueType type);
  Signal NewSignal(const std::string& id, uint64_t initialValue);
  Kernel NewKernel(const std::string& id);
  Function NewFunction(const std::string& id);
  Image NewImage(const std::string& id, ImageSpec spec);
  Sampler NewSampler(const std::string& id, SamplerSpec spec);
};

}

class EmittedTestBase : public TestSpec {
protected:
  std::unique_ptr<hexl::Context> context;
  std::unique_ptr<emitter::TestEmitter> te;

public:
  EmittedTestBase()
    : context(new hexl::Context()),
      te(new hexl::emitter::TestEmitter()) { }

  void InitContext(hexl::Context* context) { this->context->SetParent(context); }
  hexl::Context* GetContext() { return context.get(); }

  Test* Create();

  virtual void Test() = 0;
};

class EmittedTest : public EmittedTestBase {
protected:
  hexl::emitter::CoreConfig* cc;
  emitter::Location codeLocation;
  Grid geometry;
  emitter::Buffer output;
  emitter::Kernel kernel;
  emitter::Function function;
  emitter::Variable functionResult;
  emitter::TypedReg functionResultReg;
    
public:
  EmittedTest(emitter::Location codeLocation_ = emitter::KERNEL, Grid geometry_ = 0);

  std::string CodeLocationString() const;

  virtual void Test();
  virtual void Init();
  virtual void KernelArgumentsInit();
  virtual void FunctionArgumentsInit();
  virtual void GeometryInit();

  virtual void Programs();
  virtual void Program();
  virtual void StartProgram();
  virtual void EndProgram();

  virtual void Modules();
  virtual void Module();
  virtual void StartModule();
  virtual void EndModule();
  virtual void ModuleDirectives();
  virtual void ModuleVariables();

  virtual void Executables();

  virtual void Kernel();
  virtual void StartKernel();
  virtual void EndKernel();
  virtual void KernelArguments();
  virtual void StartKernelBody();
  virtual void KernelDirectives();
  virtual void KernelVariables();
  virtual void KernelInit();
  virtual emitter::TypedReg KernelResult();
  virtual void KernelCode();

  virtual void SetupDispatch(DispatchSetup* dispatch);

  virtual void Function();
  virtual void StartFunction();
  virtual void FunctionFormalOutputArguments();
  virtual void FunctionFormalInputArguments();
  virtual void StartFunctionBody();
  virtual void FunctionDirectives();
  virtual void FunctionVariables();
  virtual void FunctionInit();
  virtual void FunctionCode();
  virtual void EndFunction();

  virtual void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs);

  virtual BrigType ResultType() const { assert(0); return BRIG_TYPE_NONE; }
  virtual uint64_t ResultDim() const { return 0; }
  uint32_t ResultCount() const { assert(ResultDim() < UINT32_MAX); return (std::max)((uint32_t) ResultDim(), (uint32_t) 1); }
  bool IsResultType(BrigType type) const { return ResultType() == type; }
  ValueType ResultValueType() const { return Brig2ValueType(ResultType()); }
  virtual emitter::TypedReg Result() { assert(0); return 0; }
  virtual size_t OutputBufferSize() const;
  virtual Value ExpectedResult() const { assert(0); return Value(MV_UINT64, 0); }
  virtual Value ExpectedResult(uint64_t id, uint64_t pos) const { return ExpectedResult(id); }
  virtual Value ExpectedResult(uint64_t id) const { return ExpectedResult(); }
  virtual Values* ExpectedResults() const;
  virtual void ExpectedResults(Values* result) const;

  virtual void Scenario();
  virtual void ScenarioInit();
  virtual void ScenarioCodes();
  virtual void ScenarioDispatches();
  virtual void ScenarioValidation();
  virtual void ScenarioEnd();
  virtual void Finish();
};

inline std::ostream& operator<<(std::ostream& out, const hexl::emitter::EmitterObject& o) { o.Name(out); return out; }
inline std::ostream& operator<<(std::ostream& out, const hexl::emitter::EmitterObject* o) { o->Name(out); return out; }

}

#endif // HEXL_EMITTER_HPP
