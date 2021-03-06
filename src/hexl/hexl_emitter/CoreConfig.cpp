/*
   Copyright 2014-2015 Heterogeneous System Architecture (HSA) Foundation

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

#include "CoreConfig.hpp"
#include "Emitter.hpp"
#include "BrigEmitter.hpp"
#include "RuntimeContext.hpp"

namespace hexl {

namespace emitter {
const char *CoreConfig::CONTEXT_KEY = "hsail_conformance.coreConfig";

CoreConfig::CoreConfig(
  BrigVersion32_t majorVersion_, BrigVersion32_t minorVersion_,
  BrigMachineModel8_t model_, BrigProfile8_t profile_,
  uint32_t wavesize_, uint8_t wavesPerGroup_)
  : ap(new Arena()),
    majorVersion(majorVersion_), minorVersion(minorVersion_),
    model(model_), profile(profile_),
    wavesize(wavesize_),
    wavesPerGroup(wavesPerGroup_),
    isDetectSupported(true),
    isBreakSupported(true),
    endianness(ENDIANNESS_LITTLE),
    grids(this),
    segments(this),
    types(this),
    variables(this),
    queues(this),
    memory(this),
    directives(this),
    controlFlow(this),
    functions(this),
    images(this),
    samplers(this)
{
  assert(PlatformEndianness() == ENDIANNESS_LITTLE);
}

CoreConfig* CoreConfig::CreateAndInitialize(Context *context) {
  runtime::RuntimeContext* runtimeContext = context->Runtime();
  BrigProfile8_t profile = runtimeContext->ModuleProfile();
  uint32_t wavesize = runtimeContext->Wavesize();
  uint8_t wavesPerGroup = static_cast<uint8_t>(runtimeContext->WavesPerGroup());
  return new CoreConfig(
      BRIG_VERSION_HSAIL_MAJOR,
      BRIG_VERSION_HSAIL_MINOR,
      sizeof(void *) == 8 ? BRIG_MACHINE_LARGE : BRIG_MACHINE_SMALL,
      profile,
      wavesize,
      wavesPerGroup);
}

CoreConfig::GridsConfig::GridsConfig(CoreConfig* cc)
  : ConfigBase(cc),
    dimensions(NEWA VectorSequence<uint32_t>(ap)),
    defaultGeometry(3, 35, 5, 3, 9, 4, 2),
    trivialGeometry(1, 1, 1, 1, 1, 1, 1),
    allWavesIdGeometry(3, 40, 32, 32, 8, 8, 4),
    defaultGeometrySet(NEWA OneValueSequence<Grid>(&defaultGeometry)),
    trivialGeometrySet(NEWA OneValueSequence<Grid>(&trivialGeometry)),
    allWavesIdSet(NEWA OneValueSequence<Grid>(&allWavesIdGeometry)),
    simple(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    degenerate(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    dimension(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    boundary24(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    boundary32(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    severalwaves(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    workgroup256(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    limitGrids(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    singleGroup(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    atomic(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    mmodel(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    emodel(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    barrier(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    fbarrier(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    fbarrierEven(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    images(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    memfence(NEWA hexl::VectorSequence<hexl::Grid>(ap)),
    partial(NEWA hexl::VectorSequence<hexl::Grid>(ap))
{
  dimensions->Add(0);
  dimensions->Add(1);
  dimensions->Add(2);
  // Representative set of grid geometries for each dimensions:
  // * Standard geometry when grid size and workgroup size is power of two.
  // * Geometry with grid size not multiple of workgroup size.
  // * Non-power-of-two workgroup sizes.
  simple->Add(NEWA GridGeometry(1,  256,  1,   1,  64,  1,   1));
  simple->Add(NEWA GridGeometry(1,  200,  1,   1,  32,  1,   1));
  simple->Add(NEWA GridGeometry(1,  42,   1,   1,  11,  1,   1));
  simple->Add(NEWA GridGeometry(2,  64,   8,   1,  16,  4,   1));
  simple->Add(NEWA GridGeometry(2,  30,   7,   1,  8,   4,   1));
  simple->Add(NEWA GridGeometry(2,  10,   4,   1,  4,   3,   1));
  simple->Add(NEWA GridGeometry(3,  4,    8,  16,  4,   2,   8));
  simple->Add(NEWA GridGeometry(3,  3,    5,  11,  4,   2,   8));
  simple->Add(NEWA GridGeometry(3,  5,    7,  12,  3,   5,   7));
  degenerate->Add(NEWA GridGeometry(1,  1,  1,   1,  64,  1,   1));
  degenerate->Add(NEWA GridGeometry(2,  200,  1,   1,  64,  1,   1));
  degenerate->Add(NEWA GridGeometry(3,  30,  7,   1,  8,  4,   1));
  degenerate->Add(NEWA GridGeometry(3,  200,  1,   1,  64,  1,   1));
  dimension->Add(NEWA GridGeometry(1,  200,  1,   1,  64,  1,   1));
  dimension->Add(NEWA GridGeometry(2,  30,   7,   1,  8,   4,   1));
  dimension->Add(NEWA GridGeometry(3,  3,    5,  11,  4,   2,   8));
  boundary24->Add(NEWA GridGeometry(1,  0x1000040,          1,          1, 256,   1,   1));
  boundary24->Add(NEWA GridGeometry(2,   0x800020,          2,          1, 256,   1,   1));
  boundary24->Add(NEWA GridGeometry(2,          2,   0x800020,          1,   1, 256,   1));
  boundary24->Add(NEWA GridGeometry(3,   0x400020,          2,          2, 256,   1,   1));
  boundary24->Add(NEWA GridGeometry(3,          2,   0x400020,          2,   1, 256,   1));
  boundary24->Add(NEWA GridGeometry(3,          2,          2,   0x400020,   1,   1, 256));
  boundary32->Add(NEWA GridGeometry(2, 0x80000040,          2,          1, 256,   1,   1));
  boundary32->Add(NEWA GridGeometry(2,          2, 0x80000040,          1,   1, 256,   1));
  boundary32->Add(NEWA GridGeometry(3, 0x40000020,          2,          2, 256,   1,   1));
  boundary32->Add(NEWA GridGeometry(3,          2, 0x40000020,          2,   1, 256,   1));
  boundary32->Add(NEWA GridGeometry(3,          2,          2, 0x40000020,   1,   1, 256));
  severalwaves->Add(NEWA GridGeometry(1,  cc->Wavesize()*4,  1,   1,  cc->Wavesize(),  1,   1));
  workgroup256->Add(NEWA GridGeometry(1, 256, 1, 1, 256, 1, 1));
  workgroup256->Add(NEWA GridGeometry(2, 16, 16, 1, 16, 16, 1));
  workgroup256->Add(NEWA GridGeometry(2, 64, 4, 1, 64, 4, 1));
  workgroup256->Add(NEWA GridGeometry(3, 8, 8, 4, 8, 8, 4));
  workgroup256->Add(NEWA GridGeometry(3, 2, 32, 4, 2, 32, 4));
  limitGrids->Add(NEWA GridGeometry(1, 0xffffffff, 1, 1, 256, 1, 1));
  limitGrids->Add(NEWA GridGeometry(2, 1, 0xffffffff, 1, 1, 256, 1));
  limitGrids->Add(NEWA GridGeometry(3, 1, 1, 0xffffffff, 1, 1, 256));
  limitGrids->Add(NEWA GridGeometry(3, 65537, 257, 255, 8, 8, 4));
  limitGrids->Add(NEWA GridGeometry(3, 257, 65537, 255, 8, 8, 4));
  limitGrids->Add(NEWA GridGeometry(3, 255, 257, 65537, 4, 8, 8));
  singleGroup->Add(NEWA GridGeometry(1, 64, 1, 1, 64, 1, 1));
  singleGroup->Add(NEWA GridGeometry(1, 256, 1, 1, 256, 1, 1));
  singleGroup->Add(NEWA GridGeometry(2, 16, 16, 1, 16, 16, 1));
  singleGroup->Add(NEWA GridGeometry(3, 8, 8, 4, 8, 8, 4));
  atomic->Add(NEWA GridGeometry(1,  cc->Wavesize(),     1,   1,  cc->Wavesize(),      1,   1));
  atomic->Add(NEWA GridGeometry(1,  cc->Wavesize() * 4, 1,   1,  cc->Wavesize(),      1,   1));
  atomic->Add(NEWA GridGeometry(1,  cc->Wavesize() * 8, 1,   1,  cc->Wavesize() * 4,  1,   1));
  atomic->Add(NEWA GridGeometry(1,  cc->Wavesize() * 8, 1,   1,  cc->Wavesize() * 8,  1,   1));
  atomic->Add(NEWA GridGeometry(1,  32,                 1,   1,  32,                  1,   1));
  atomic->Add(NEWA GridGeometry(1,  32,                 1,   1,  16,                  1,   1));
  atomic->Add(NEWA GridGeometry(1,  64,                 1,   1,  64,                  1,   1));
  atomic->Add(NEWA GridGeometry(1,  64,                 1,   1,  32,                  1,   1));
  mmodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 8,   1,   1,  cc->Wavesize(),      1,   1));
  mmodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 32,  1,   1,  cc->Wavesize() * 4,  1,   1));
  mmodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 64,  1,   1,  cc->Wavesize() * 8,  1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 8,   1,   1,  cc->Wavesize(),      1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 64,  1,   1,  cc->Wavesize(),      1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 256, 1,   1,  cc->Wavesize(),      1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 8,   1,   1,  cc->Wavesize() * 4,  1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 64,  1,   1,  cc->Wavesize() * 4,  1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 256, 1,   1,  cc->Wavesize() * 4,  1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 8,   1,   1,  cc->Wavesize() * 8,  1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 64,  1,   1,  cc->Wavesize() * 8,  1,   1));
  emodel->Add(NEWA GridGeometry(1,  cc->Wavesize() * 256, 1,   1,  cc->Wavesize() * 8,  1,   1));
  barrier->Add(NEWA GridGeometry(1,  cc->Wavesize() * 4,  1,   1,  cc->Wavesize() * 4,  1,   1));
  barrier->Add(NEWA GridGeometry(1,  cc->Wavesize() * 8,  1,   1,  cc->Wavesize() * 8,  1,   1));
  barrier->Add(NEWA GridGeometry(1,  cc->Wavesize() * 16, 1,   1,  cc->Wavesize() * 16,  1,   1));
  //barrier->Add(NEWA GridGeometry(1,  cc->Wavesize()*8,  1,   1,  cc->Wavesize()*2,  1,   1));
  //barrier->Add(NEWA GridGeometry(1,  cc->Wavesize()*16,  1,   1,  cc->Wavesize()*4,  1,   1));
  fbarrier->Add(NEWA GridGeometry(1, cc->Wavesize(), 1, 1, cc->Wavesize(), 1, 1));
  fbarrier->Add(NEWA GridGeometry(1, cc->Wavesize()*16, 1, 1, cc->Wavesize()*4, 1, 1));
  fbarrier->Add(NEWA GridGeometry(1, cc->Wavesize()*4, 1, 1, cc->Wavesize()*4, 1, 1));
  fbarrier->Add(NEWA GridGeometry(2, 16, 16, 1, 16, 16, 1));
  fbarrier->Add(NEWA GridGeometry(2, cc->Wavesize(), 4, 1, cc->Wavesize(), 4, 1));
  fbarrier->Add(NEWA GridGeometry(3, 6, 14, 5, 4, 13, 4));
  fbarrier->Add(NEWA GridGeometry(3, 8, 8, 4, 8, 8, 4));
  fbarrier->Add(NEWA GridGeometry(3, 2, 32, 4, 2, 32, 4));
  fbarrier->Add(NEWA GridGeometry(3, 5, 7, 12, 3, 5, 7));
  fbarrier->Add(NEWA GridGeometry(3, 3, 9, 13, 2, 7, 11));
  if (cc->Wavesize() > 1 && cc->WavesPerGroup() > 1) {
    fbarrierEven->Add(NEWA GridGeometry(1, cc->Wavesize()*4, 1, 1, cc->Wavesize()*2, 1, 1));
    fbarrierEven->Add(NEWA GridGeometry(2, 4, cc->Wavesize(), 1, 2, cc->Wavesize(), 1));
    if (cc->WavesPerGroup() >= 4) {
      fbarrierEven->Add(NEWA GridGeometry(1, cc->Wavesize()*8, 1, 1, cc->Wavesize()*4, 1, 1));
      fbarrierEven->Add(NEWA GridGeometry(3, 4, 1, cc->Wavesize(), 2, 1, cc->Wavesize()));
    }
  }
  images->Add(NEWA GridGeometry(1, 1, 1, 1, 1, 1, 1));
  images->Add(NEWA GridGeometry(1, 100, 1, 1, 100, 1, 1));
  images->Add(NEWA GridGeometry(2, 100, 10, 1, 100, 1, 1));
  images->Add(NEWA GridGeometry(3, 10, 10, 10, 10, 1, 1));
  memfence->Add(NEWA GridGeometry(1,  cc->Wavesize()*4,  1,   1,  cc->Wavesize(),  1,   1));
  memfence->Add(NEWA GridGeometry(1,  cc->Wavesize()*4,  1,   1,  cc->Wavesize()*4,  1,   1));
  memfence->Add(NEWA GridGeometry(1,  cc->Wavesize()*16,  1,   1,  cc->Wavesize()*4,  1,   1));
  memfence->Add(NEWA GridGeometry(1,  cc->Wavesize()*64,  1,   1,  cc->Wavesize()*2,  1,   1));
  partial->Add(NEWA GridGeometry(1, 64, 1, 1, 198, 1, 1));
  partial->Add(NEWA GridGeometry(1, 256, 1, 1, 198, 1, 1));
  partial->Add(NEWA GridGeometry(2, 8, 7, 1, 9, 12, 1));
  partial->Add(NEWA GridGeometry(2, 32, 15, 1, 9, 12, 1));
  partial->Add(NEWA GridGeometry(3, 3, 5, 7, 8, 8, 4));
  partial->Add(NEWA GridGeometry(3, 5, 7, 12, 3, 5, 7));
}


static const BrigImageChannelOrder allChannelOrder[] = {
    BRIG_CHANNEL_ORDER_A,
    BRIG_CHANNEL_ORDER_R,
    BRIG_CHANNEL_ORDER_RX,
    BRIG_CHANNEL_ORDER_RG,
    BRIG_CHANNEL_ORDER_RGX,
    BRIG_CHANNEL_ORDER_RA,
    BRIG_CHANNEL_ORDER_RGB,
    BRIG_CHANNEL_ORDER_RGBX,
    BRIG_CHANNEL_ORDER_RGBA,
    BRIG_CHANNEL_ORDER_BGRA,
    BRIG_CHANNEL_ORDER_ARGB,
    BRIG_CHANNEL_ORDER_ABGR,
    BRIG_CHANNEL_ORDER_SRGB,
    BRIG_CHANNEL_ORDER_SRGBX,
    BRIG_CHANNEL_ORDER_SRGBA,
    BRIG_CHANNEL_ORDER_SBGRA,
    BRIG_CHANNEL_ORDER_INTENSITY,
    BRIG_CHANNEL_ORDER_LUMINANCE,
    BRIG_CHANNEL_ORDER_DEPTH,
    BRIG_CHANNEL_ORDER_DEPTH_STENCIL,
};

static const  BrigImageChannelType allChannelType[] = {
    BRIG_CHANNEL_TYPE_SNORM_INT8,
    BRIG_CHANNEL_TYPE_SNORM_INT16,
    BRIG_CHANNEL_TYPE_UNORM_INT8,
    BRIG_CHANNEL_TYPE_UNORM_INT16,
    BRIG_CHANNEL_TYPE_UNORM_INT24,
    BRIG_CHANNEL_TYPE_UNORM_SHORT_555,
    BRIG_CHANNEL_TYPE_UNORM_SHORT_565,
    BRIG_CHANNEL_TYPE_UNORM_INT_101010,
    BRIG_CHANNEL_TYPE_SIGNED_INT8,
    BRIG_CHANNEL_TYPE_SIGNED_INT16,
    BRIG_CHANNEL_TYPE_SIGNED_INT32,
    BRIG_CHANNEL_TYPE_UNSIGNED_INT8,
    BRIG_CHANNEL_TYPE_UNSIGNED_INT16,
    BRIG_CHANNEL_TYPE_UNSIGNED_INT32,
    BRIG_CHANNEL_TYPE_HALF_FLOAT,
    BRIG_CHANNEL_TYPE_FLOAT,
};

static const BrigImageGeometry allGeometry[] = {
    BRIG_GEOMETRY_1D,
    BRIG_GEOMETRY_2D,
    BRIG_GEOMETRY_3D,
    BRIG_GEOMETRY_1DA,
    BRIG_GEOMETRY_2DA,
    BRIG_GEOMETRY_1DB,
    BRIG_GEOMETRY_2DDEPTH,
    BRIG_GEOMETRY_2DADEPTH,
};

static const BrigType rdCoordTypeArray[] = {
    BRIG_TYPE_S32,
    BRIG_TYPE_F32,
};

static const BrigType allAccess[] = {
    BRIG_TYPE_ROIMG,
    BRIG_TYPE_WOIMG,
    BRIG_TYPE_RWIMG,
};

static const BrigImageQuery allImgQueries[] = {
    BRIG_IMAGE_QUERY_WIDTH,
    BRIG_IMAGE_QUERY_HEIGHT,
    BRIG_IMAGE_QUERY_DEPTH,
    BRIG_IMAGE_QUERY_ARRAY,
    BRIG_IMAGE_QUERY_CHANNELORDER,
    BRIG_IMAGE_QUERY_CHANNELTYPE,
};

static const unsigned arrayGeometry[] = { 1, 2, 10, };

static const uint32_t numberRwArray[] = { 17, 32, 47 };

CoreConfig::ImageConfig::ImageConfig(CoreConfig* cc)
  : ConfigBase(cc),
    defaultImageGeometry(NEWA hexl::VectorSequence<hexl::ImageGeometry*>(ap)),
    imageGeometryProps(NEWA ArraySequence<BrigImageGeometry>(allGeometry, NELEM(allGeometry))),
    imageChannelOrders(NEWA ArraySequence<BrigImageChannelOrder>(allChannelOrder, NELEM(allChannelOrder))),
    imageChannelTypes(NEWA ArraySequence<BrigImageChannelType>(allChannelType, NELEM(allChannelType))),
    imageQueryTypes(NEWA ArraySequence<BrigImageQuery>(allImgQueries, NELEM(allImgQueries))),
    imageAccessTypes(NEWA ArraySequence<BrigType>(allAccess, NELEM(allAccess))),
    imageArray(NEWA ArraySequence<unsigned>(arrayGeometry, NELEM(arrayGeometry))),
    numberRW(NEWA ArraySequence<unsigned>(numberRwArray, NELEM(numberRwArray))),
    rdCoordTypes(NEWA ArraySequence<BrigType>(rdCoordTypeArray, NELEM(rdCoordTypeArray)))
{
   defaultImageGeometry->Add(NEWA ImageGeometry(1000));
   defaultImageGeometry->Add(NEWA ImageGeometry(100, 10));
   defaultImageGeometry->Add(NEWA ImageGeometry(10, 10, 10));
   defaultImageGeometry->Add(NEWA ImageGeometry(100, 1, 1, 10));
}

static const BrigSamplerAddressing allAddressing[] = {
    BRIG_ADDRESSING_UNDEFINED,
    BRIG_ADDRESSING_CLAMP_TO_EDGE,
    BRIG_ADDRESSING_CLAMP_TO_BORDER,
    BRIG_ADDRESSING_REPEAT,
    BRIG_ADDRESSING_MIRRORED_REPEAT,
};

static const BrigSamplerCoordNormalization allCoords[] = {
    BRIG_COORD_UNNORMALIZED,
    BRIG_COORD_NORMALIZED,
};

static const BrigSamplerFilter allFilters[] = {
    BRIG_FILTER_NEAREST,
    BRIG_FILTER_LINEAR,
};

static const BrigSamplerQuery allSmpQueries[] = {
  BRIG_SAMPLER_QUERY_ADDRESSING,
  BRIG_SAMPLER_QUERY_COORD,
  BRIG_SAMPLER_QUERY_FILTER,
};

CoreConfig::SamplerConfig::SamplerConfig(CoreConfig* cc)
  : ConfigBase(cc),
    samplerCoords(NEWA ArraySequence<BrigSamplerCoordNormalization>(allCoords, NELEM(allCoords))),
    samplerFilters(NEWA ArraySequence<BrigSamplerFilter>(allFilters, NELEM(allFilters))),
    samplerAddressings(NEWA ArraySequence<BrigSamplerAddressing>(allAddressing, NELEM(allAddressing))),
    allSamplers(SequenceMap<SamplerParams>(ap, SequenceProduct(ap, samplerCoords, samplerFilters, samplerAddressings))),
    samplerQueryTypes(NEWA ArraySequence<BrigSamplerQuery>(allSmpQueries, NELEM(allSmpQueries)))
{

}


static const BrigSegment allSegments[] = {
  BRIG_SEGMENT_FLAT,
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_READONLY,
  BRIG_SEGMENT_KERNARG,
  BRIG_SEGMENT_GROUP,
  BRIG_SEGMENT_PRIVATE,
  BRIG_SEGMENT_SPILL,
  BRIG_SEGMENT_ARG,
};

static const BrigSegment variableSegments[] = {
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_READONLY,
  BRIG_SEGMENT_KERNARG,
  BRIG_SEGMENT_GROUP,
  BRIG_SEGMENT_PRIVATE,
  BRIG_SEGMENT_SPILL,
  BRIG_SEGMENT_ARG,
};

static const BrigSegment atomicSegments[] = {
  BRIG_SEGMENT_FLAT,
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_GROUP,
};

static const BrigSegment initializableSegments[] = {
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_READONLY,
};

static const BrigSegment moduleScopeArray[] = {
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_GROUP,
  BRIG_SEGMENT_PRIVATE,
  BRIG_SEGMENT_READONLY,
};

static const BrigSegment functionScopeArray[] = {
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_GROUP,
  BRIG_SEGMENT_PRIVATE,
  BRIG_SEGMENT_SPILL,
  BRIG_SEGMENT_READONLY
};

static const uint32_t staticGroupSizeArray[] = {
  0,
  7,
  10,
  1024
};


CoreConfig::SegmentsConfig::SegmentsConfig(CoreConfig* cc)
  : ConfigBase(cc),
    all(NEWA hexl::ArraySequence<BrigSegment>(allSegments, NELEM(allSegments))),
    variable(NEWA hexl::ArraySequence<BrigSegment>(variableSegments, NELEM(variableSegments))),
    atomic(NEWA hexl::ArraySequence<BrigSegment>(atomicSegments, NELEM(atomicSegments))),
    initializable(NEWA hexl::ArraySequence<BrigSegment>(initializableSegments, NELEM(initializableSegments))),
    moduleScope(NEWA hexl::ArraySequence<BrigSegment>(moduleScopeArray, NELEM(moduleScopeArray))),
    functionScope(NEWA hexl::ArraySequence<BrigSegment>(functionScopeArray, NELEM(functionScopeArray))),
    staticGroupSize(NEWA hexl::ArraySequence<uint32_t>(staticGroupSizeArray, NELEM(staticGroupSizeArray)))
{
  for (unsigned segment = BRIG_SEGMENT_NONE; segment != BRIG_SEGMENT_MAX; ++segment) {
    singleList[segment] = new (ap) hexl::OneValueSequence<BrigSegment>((BrigSegment) segment);
  }

}

bool CoreConfig::SegmentsConfig::CanStore(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_KERNARG:
    return false;
  case BRIG_SEGMENT_FLAT:
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
  case BRIG_SEGMENT_SPILL:
  case BRIG_SEGMENT_ARG:
    return true;
  default:
    assert(false); return true;
  }
}

bool CoreConfig::SegmentsConfig::HasAddress(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_ARG:
  case BRIG_SEGMENT_SPILL:
    return false;
  case BRIG_SEGMENT_KERNARG:
  case BRIG_SEGMENT_FLAT:
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
    return true;
  default:
    assert(false); return true;
  }
}

bool CoreConfig::SegmentsConfig::HasNullptr(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_ARG:
  case BRIG_SEGMENT_SPILL:
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_KERNARG:
    return false;
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
  case BRIG_SEGMENT_FLAT:
    return true;
  default:
    assert(false); return true;
  }
}

bool CoreConfig::SegmentsConfig::HasFlatAddress(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_ARG:
  case BRIG_SEGMENT_SPILL:
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_KERNARG:
    return false;
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
    return true;
  case BRIG_SEGMENT_FLAT:
    assert(false); return true;
  default:
    assert(false); return true;
  }
}

bool CoreConfig::SegmentsConfig::CanPassAddressToKernel(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_KERNARG:
  case BRIG_SEGMENT_ARG:
  case BRIG_SEGMENT_SPILL:
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
    return false;
  case BRIG_SEGMENT_FLAT:
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_READONLY:
    return true;
  default:
    assert(false); return true;
  }
}

hexl::Sequence<BrigSegment>*
CoreConfig::SegmentsConfig::Single(BrigSegment segment)
{
  return singleList[segment];
}

static const BrigType compoundTypes[] = {
  BRIG_TYPE_U8,
  BRIG_TYPE_U16,
  BRIG_TYPE_U32,
  BRIG_TYPE_U64,
  BRIG_TYPE_S8,
  BRIG_TYPE_S16,
  BRIG_TYPE_S32,
  BRIG_TYPE_S64,
//  BRIG_TYPE_F16,
  BRIG_TYPE_F32,
  BRIG_TYPE_F64,
};

static const BrigType compoundIntegralTypes[] = {
  BRIG_TYPE_U8,
  BRIG_TYPE_U16,
  BRIG_TYPE_U32,
  BRIG_TYPE_U64,
  BRIG_TYPE_S8,
  BRIG_TYPE_S16,
  BRIG_TYPE_S32,
  BRIG_TYPE_S64
};

static const BrigType compoundFloatingTypes[] = {
//  BRIG_TYPE_F16,
  BRIG_TYPE_F32,
  BRIG_TYPE_F64
};

static const BrigType packedTypes[] = {
  BRIG_TYPE_U8X4,
  BRIG_TYPE_U8X8,
  BRIG_TYPE_S8X4,
  BRIG_TYPE_S8X8,
  BRIG_TYPE_U16X2,
  BRIG_TYPE_U16X4,
  BRIG_TYPE_S16X2,
  BRIG_TYPE_S16X4,
  BRIG_TYPE_U32X2,
  BRIG_TYPE_S32X2,
  BRIG_TYPE_F32X2
};

static const BrigType packed128BitTypes[] = {
  BRIG_TYPE_U8X16,
  BRIG_TYPE_U16X8,
  BRIG_TYPE_U32X4,
  BRIG_TYPE_U64X2,
  BRIG_TYPE_S8X16,
  BRIG_TYPE_S16X8,
  BRIG_TYPE_S32X4,
  BRIG_TYPE_S64X2,
  BRIG_TYPE_F32X4,
  BRIG_TYPE_F64X2
};

static const BrigType atomicTypes[] = {
  BRIG_TYPE_U32,
  BRIG_TYPE_U64,
  BRIG_TYPE_S32,
  BRIG_TYPE_S64,
  BRIG_TYPE_B32,
  BRIG_TYPE_B64
};

static const BrigType memModelTypes[] = {
  BRIG_TYPE_U32,
  BRIG_TYPE_S64,
  BRIG_TYPE_B64
};

static const BrigType memfenceTypes[] = {
  BRIG_TYPE_U16,
  BRIG_TYPE_U32,
  BRIG_TYPE_U64,
  BRIG_TYPE_S16,
  BRIG_TYPE_S32,
  BRIG_TYPE_S64,
  BRIG_TYPE_F16,
  BRIG_TYPE_F32,
  BRIG_TYPE_F64
};

static const size_t registerSizesArr[] = {
  32, 64, 128
};

CoreConfig::TypesConfig::TypesConfig(CoreConfig* cc)
  : ConfigBase(cc),
    compound(NEWA ArraySequence<BrigType>(compoundTypes, NELEM(compoundTypes))),
    compoundIntegral(NEWA ArraySequence<BrigType>(compoundIntegralTypes, NELEM(compoundIntegralTypes))),
    compoundFloating(NEWA ArraySequence<BrigType>(compoundFloatingTypes, NELEM(compoundFloatingTypes))),
    packed(NEWA ArraySequence<BrigType>(packedTypes, NELEM(packedTypes))),
    packed128(NEWA ArraySequence<BrigType>(packed128BitTypes, NELEM(packed128BitTypes))),
    atomic(NEWA ArraySequence<BrigType>(atomicTypes, NELEM(atomicTypes))),
    memModel(NEWA ArraySequence<BrigType>(memModelTypes, NELEM(memModelTypes))),
    memfence(NEWA ArraySequence<BrigType>(memfenceTypes, NELEM(memfenceTypes))),
    registerSizes(NEWA ArraySequence<size_t>(registerSizesArr, NELEM(registerSizesArr)))
{
}

static const uint64_t smallDimensions[] = { 0, 1, 2, 3, 4, 8, };
static const uint64_t initializerDimensions[] = {0, 1, 2, 64};
static const Location initializerLocationsArray[] = {
  Location::MODULE,
  Location::KERNEL,
  Location::FUNCTION
};

static const BrigLinkage moduleScopeLinkageArray[] = {
  BRIG_LINKAGE_MODULE,
  BRIG_LINKAGE_PROGRAM
};

CoreConfig::VariablesConfig::VariablesConfig(CoreConfig* cc)
  : ConfigBase(cc),
  bySegmentType(SequenceMap<EVariableSpec>(ap, SequenceProduct(ap, cc->Segments().Variable(), cc->Types().Compound()))),
  dim0(NEWA OneValueSequence<uint64_t>(0)),
  dims(NEWA ArraySequence<uint64_t>(smallDimensions, NELEM(smallDimensions))),
  initializerDims(NEWA ArraySequence<uint64_t>(initializerDimensions, NELEM(initializerDimensions))),
  autoLocation(NEWA OneValueSequence<Location>(AUTO)),
  initializerLocations(NEWA ArraySequence<Location>(initializerLocationsArray, NELEM(initializerLocationsArray))),
  moduleScopeLinkage(NEWA ArraySequence<BrigLinkage>(moduleScopeLinkageArray, NELEM(moduleScopeLinkageArray))),
  allAlignment(NEWA VectorSequence<BrigAlignment>(ap)),
  annotationLocations(NEWA EnumSequence<AnnotationLocation>(ap, AnnotationLocation::ANNOTATION_LOCATION_BEGIN, AnnotationLocation::ANNOTATION_LOCATION_END))
{
  for (unsigned a = BRIG_ALIGNMENT_1; a != BRIG_ALIGNMENT_LAST; a++) {
    allAlignment->Add((BrigAlignment) a);
  }
  for (unsigned segment = BRIG_SEGMENT_NONE; segment != BRIG_SEGMENT_MAX; segment++) {
    auto product = SequenceProduct(ap,
      cc->Segments().Single((BrigSegment) segment),
      cc->Types().Compound(),
      cc->Variables().AutoLocation(),
      cc->Variables().AllAlignment());
    byTypeAlign[segment] = SequenceMap<EVariableSpec>(ap, product);
    byTypeDimensionAlign[segment] = SequenceMap<EVariableSpec>(ap,
      SequenceProduct(ap,
        cc->Segments().Single((BrigSegment) segment),
        cc->Types().Compound(),
        cc->Variables().AutoLocation(),
        cc->Variables().AllAlignment(),
        cc->Variables().Dims()
      ));
  }
}

static const BrigSegment queueSegments[] = { BRIG_SEGMENT_GLOBAL, BRIG_SEGMENT_FLAT };
static const BrigOpcode ldOpcodesValues[] = { BRIG_OPCODE_LDQUEUEREADINDEX, BRIG_OPCODE_LDQUEUEWRITEINDEX };
static const BrigOpcode addCasOpcodesValues[] = { BRIG_OPCODE_ADDQUEUEWRITEINDEX, BRIG_OPCODE_CASQUEUEWRITEINDEX };
static const BrigOpcode stOpcodesValues[] = { BRIG_OPCODE_STQUEUEREADINDEX, BRIG_OPCODE_STQUEUEWRITEINDEX };
static const BrigMemoryOrder ldMemoryOrdersValues[] = { BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_SC_ACQUIRE };
static const BrigMemoryOrder addCasMemoryOrdersValues[] = { BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE };
static const BrigMemoryOrder stMemoryOrdersValues[] = { BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_SC_RELEASE };

CoreConfig::QueuesConfig::QueuesConfig(CoreConfig* cc)
  : ConfigBase(cc),
    types(NEWA EnumSequence<UserModeQueueType>(ap, SOURCE_START, SOURCE_END)),
    segments(NEWA ArraySequence<BrigSegment>(queueSegments, NELEM(queueSegments))),
    ldOpcodes(NEWA ArraySequence<BrigOpcode>(ldOpcodesValues, NELEM(ldOpcodesValues))),
    addCasOpcodes(NEWA ArraySequence<BrigOpcode>(addCasOpcodesValues, NELEM(addCasOpcodesValues))),
    stOpcodes(NEWA ArraySequence<BrigOpcode>(stOpcodesValues, NELEM(stOpcodesValues))),
    ldMemoryOrders(NEWA ArraySequence<BrigMemoryOrder>(ldMemoryOrdersValues, NELEM(ldMemoryOrdersValues))),
    addCasMemoryOrders(NEWA ArraySequence<BrigMemoryOrder>(addCasMemoryOrdersValues, NELEM(addCasMemoryOrdersValues))),
    stMemoryOrders(NEWA ArraySequence<BrigMemoryOrder>(stMemoryOrdersValues, NELEM(stMemoryOrdersValues)))
{
}

static const BrigAtomicOperation allAtomicsValues[] = {
// TODO: initial & expected values for the rest atomics in barrier tests
  BRIG_ATOMIC_ADD,
  BRIG_ATOMIC_AND,
  BRIG_ATOMIC_CAS,
  BRIG_ATOMIC_EXCH,
  BRIG_ATOMIC_LD,
  BRIG_ATOMIC_MAX,
  BRIG_ATOMIC_MIN,
  BRIG_ATOMIC_OR,
  BRIG_ATOMIC_ST,
  BRIG_ATOMIC_SUB,
  BRIG_ATOMIC_WRAPDEC,
  BRIG_ATOMIC_WRAPINC,
  BRIG_ATOMIC_XOR
};

static const BrigAtomicOperation limitedAtomicsValues[] = {
  BRIG_ATOMIC_ADD,
  BRIG_ATOMIC_AND,
  BRIG_ATOMIC_CAS,
  BRIG_ATOMIC_EXCH,
  BRIG_ATOMIC_MAX,
  BRIG_ATOMIC_ST,
  BRIG_ATOMIC_WRAPINC
};

static const BrigAtomicOperation signalSendAtomicsValues[] = {
  BRIG_ATOMIC_ST,
  BRIG_ATOMIC_ADD,
  BRIG_ATOMIC_AND,
  BRIG_ATOMIC_CAS,
  BRIG_ATOMIC_EXCH,
  BRIG_ATOMIC_OR,
  BRIG_ATOMIC_SUB,
  BRIG_ATOMIC_XOR
};

static const BrigAtomicOperation signalWaitAtomicsValues[] = {
  BRIG_ATOMIC_LD,
  BRIG_ATOMIC_WAIT_EQ,
  BRIG_ATOMIC_WAIT_NE,
  BRIG_ATOMIC_WAIT_LT,
  BRIG_ATOMIC_WAIT_GTE,
  BRIG_ATOMIC_WAITTIMEOUT_EQ,
  BRIG_ATOMIC_WAITTIMEOUT_NE,
  BRIG_ATOMIC_WAITTIMEOUT_LT,
  BRIG_ATOMIC_WAITTIMEOUT_GTE
};

static const BrigSegment memfenceSegmentsValues[] = {
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_GROUP
};

static const BrigOpcode ldStOpcodesValues[] = {
  BRIG_OPCODE_LD,
  BRIG_OPCODE_ST
};

static const BrigOpcode atomicOpcodesValues[] = {
  BRIG_OPCODE_ATOMIC,
  BRIG_OPCODE_ATOMICNORET
};

static const BrigAtomicOperation atomicOperationsValues[] = {
  BRIG_ATOMIC_ADD,
  BRIG_ATOMIC_AND,
  BRIG_ATOMIC_CAS,
  BRIG_ATOMIC_EXCH,
  BRIG_ATOMIC_LD,
  BRIG_ATOMIC_MAX,
  BRIG_ATOMIC_MIN,
  BRIG_ATOMIC_OR,
  BRIG_ATOMIC_ST,
  BRIG_ATOMIC_SUB,
  BRIG_ATOMIC_WRAPDEC,
  BRIG_ATOMIC_WRAPINC,
  BRIG_ATOMIC_XOR
};

CoreConfig::MemoryConfig::MemoryConfig(CoreConfig* cc)
  : ConfigBase(cc),
    allMemoryOrders(NEWA EnumSequence<BrigMemoryOrder>(ap, BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_LAST)),
    signalSendMemoryOrders(NEWA EnumSequence<BrigMemoryOrder>(ap, BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_LAST)),
    signalWaitMemoryOrders(NEWA EnumSequence<BrigMemoryOrder>(ap, BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_SC_RELEASE)),
    memfenceMemoryOrders(NEWA EnumSequence<BrigMemoryOrder>(ap, BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_ORDER_LAST)),
    allMemoryScopes(NEWA EnumSequence<BrigMemoryScope>(ap, BRIG_MEMORY_SCOPE_WORKITEM, BRIG_MEMORY_SCOPE_LAST)),
    memfenceMemoryScopes(NEWA EnumSequence<BrigMemoryScope>(ap, BRIG_MEMORY_SCOPE_WAVEFRONT, BRIG_MEMORY_SCOPE_LAST)),
    allAtomics(NEWA ArraySequence<BrigAtomicOperation>(allAtomicsValues, NELEM(allAtomicsValues))),
    limitedAtomics(NEWA ArraySequence<BrigAtomicOperation>(limitedAtomicsValues, NELEM(limitedAtomicsValues))),
    atomicOperations(NEWA ArraySequence<BrigAtomicOperation>(atomicOperationsValues, NELEM(atomicOperationsValues))),
    signalSendAtomics(NEWA ArraySequence<BrigAtomicOperation>(signalSendAtomicsValues, NELEM(signalSendAtomicsValues))),
    signalWaitAtomics(NEWA ArraySequence<BrigAtomicOperation>(signalWaitAtomicsValues, NELEM(signalWaitAtomicsValues))),
    memfenceSegments(new (ap) hexl::ArraySequence<BrigSegment>(memfenceSegmentsValues, NELEM(memfenceSegmentsValues))),
    ldStOpcodes(NEWA ArraySequence<BrigOpcode>(ldStOpcodesValues, NELEM(ldStOpcodesValues))),
    atomicOpcodes(NEWA ArraySequence<BrigOpcode>(atomicOpcodesValues, NELEM(atomicOpcodesValues)))
{
}

static const BrigControlDirective gridGroupRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
  BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS,
};

static const BrigControlDirective gridSizeRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
};

static const BrigControlDirective workitemIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
};

static const BrigControlDirective workitemAbsIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
};

static const BrigControlDirective workitemFlatIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
  BRIG_CONTROL_MAXFLATWORKGROUPSIZE,
};

static const BrigControlDirective workitemFlatAbsIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
};

static const BrigControlDirective degenerateRelatedValues[] = {
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
};

static const BrigControlDirective boundary24WorkitemAbsIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
};

static const BrigControlDirective boundary24WorkitemFlatAbsIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
};

static const BrigControlDirective boundary24WorkitemFlatIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
  BRIG_CONTROL_MAXFLATWORKGROUPSIZE,
};

static const BrigKind pragmaOperandTypesValues[] = {
  BRIG_KIND_OPERAND_CONSTANT_BYTES,
  BRIG_KIND_OPERAND_STRING,
  BRIG_KIND_OPERAND_CODE_REF
};

static const uint32_t validExceptionNumbersValues[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

static const BrigControlDirective exceptionDirectivesValues[] = {
  BRIG_CONTROL_ENABLEBREAKEXCEPTIONS,
  BRIG_CONTROL_ENABLEDETECTEXCEPTIONS
};

static const BrigControlDirective geometryDirectivesValues[] = {
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_MAXFLATWORKGROUPSIZE,
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
  BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS
};

static const std::string validExtensionsNames[] = {
  "IMAGE",
  "CORE",
  ""
};

CoreConfig::ControlDirectivesConfig::ControlDirectivesConfig(CoreConfig* cc)
  : ConfigBase(cc),
    none(NEWA EControlDirectives(NEWA EmptySequence<BrigControlDirective>())),
    dimensionRelated(NEWA EControlDirectives(NEWA OneValueSequence<BrigControlDirective>(BRIG_CONTROL_REQUIREDDIM))),
    gridGroupRelated(Array(ap, gridGroupRelatedValues, NELEM(gridGroupRelatedValues))),
    gridSizeRelated(Array(ap, gridSizeRelatedValues, NELEM(gridSizeRelatedValues))),
    workitemIdRelated(Array(ap, workitemIdRelatedValues, NELEM(workitemIdRelatedValues))),
    workitemAbsIdRelated(Array(ap, workitemAbsIdRelatedValues, NELEM(workitemAbsIdRelatedValues))),
    workitemFlatIdRelated(Array(ap, workitemFlatIdRelatedValues, NELEM(workitemFlatIdRelatedValues))),
    workitemFlatAbsIdRelated(Array(ap, workitemFlatAbsIdRelatedValues, NELEM(workitemFlatAbsIdRelatedValues))),
    degenerateRelated(Array(ap, degenerateRelatedValues, NELEM(degenerateRelatedValues))),
    boundary24WorkitemAbsIdRelated(Array(ap, boundary24WorkitemAbsIdRelatedValues, NELEM(boundary24WorkitemAbsIdRelatedValues))),
    boundary24WorkitemFlatAbsIdRelated(Array(ap, boundary24WorkitemFlatAbsIdRelatedValues, NELEM(boundary24WorkitemFlatAbsIdRelatedValues))),
    boundary24WorkitemFlatIdRelated(Array(ap, boundary24WorkitemFlatIdRelatedValues, NELEM(boundary24WorkitemFlatIdRelatedValues))),
    noneSets(DSubsets(ap, none)),
    dimensionRelatedSets(DSubsets(ap, dimensionRelated)),
    gridGroupRelatedSets(DSubsets(ap, gridGroupRelated)),
    gridSizeRelatedSets(DSubsets(ap, gridSizeRelated)),
    workitemIdRelatedSets(DSubsets(ap, workitemIdRelated)),
    workitemAbsIdRelatedSets(DSubsets(ap, workitemAbsIdRelated)),
    workitemFlatIdRelatedSets(DSubsets(ap, workitemFlatIdRelated)),
    workitemFlatAbsIdRelatedSets(DSubsets(ap, workitemFlatAbsIdRelated)),
    degenerateRelatedSets(DSubsets(ap, degenerateRelated)),
    boundary24WorkitemAbsIdRelatedSets(DSubsets(ap, boundary24WorkitemAbsIdRelated)),
    boundary24WorkitemFlatAbsIdRelatedSets(DSubsets(ap, boundary24WorkitemFlatAbsIdRelated)),
    boundary24WorkitemFlatIdRelatedSets(DSubsets(ap, boundary24WorkitemFlatIdRelated)),
    pragmaOperandTypes(NEWA ArraySequence<BrigKind>(pragmaOperandTypesValues, NELEM(pragmaOperandTypesValues))),
    validExceptionNumbers(NEWA ArraySequence<uint32_t>(validExceptionNumbersValues, NELEM(validExceptionNumbersValues))),
    exceptionDirectives(NEWA ArraySequence<BrigControlDirective>(exceptionDirectivesValues, NELEM(exceptionDirectivesValues))),
    geometryDirectives(NEWA ArraySequence<BrigControlDirective>(geometryDirectivesValues, NELEM(geometryDirectivesValues))),
    validExtensions(NEWA ArraySequence<std::string>(validExtensionsNames, NELEM(validExtensionsNames)))
{
}

hexl::Sequence<ControlDirectives>* CoreConfig::ControlDirectivesConfig::DSubsets(Arena *ap, const ControlDirectives& set)
{
  return SequenceMap<EControlDirectives>(ap, Subsets(ap, set->Spec()));
}

ControlDirectives CoreConfig::ControlDirectivesConfig::Array(Arena* ap, const BrigControlDirective *values, size_t count)
{
  return NEWA EControlDirectives(NEWA ArraySequence<BrigControlDirective>(values, (unsigned) count));
}

CoreConfig::ControlFlowConfig::ControlFlowConfig(CoreConfig* cc)
  : ConfigBase(cc),
    allWidths(NEWA EnumSequence<BrigWidth>(ap, BRIG_WIDTH_NONE, BRIG_WIDTH_LAST)),
    workgroupWidths(NEWA VectorSequence<BrigWidth>(ap)),
    cornerWidths(NEWA VectorSequence<BrigWidth>(ap)),
    conditionInputs(NEWA VectorSequence<ConditionInput>(ap)),
    binaryConditions(SequenceMap<ECondition>(ap, SequenceProduct(ap, NEWA OneValueSequence<ConditionType>(COND_BINARY), ConditionInputs(), WorkgroupWidths()))),
    nestedConditions(SequenceMap<ECondition>(ap, SequenceProduct(ap, NEWA OneValueSequence<ConditionType>(COND_BINARY), ConditionInputs(), CornerWidths()))),
    sbrTypes(NEWA EnumSequence<BrigType>(ap, BRIG_TYPE_U32, BRIG_TYPE_S8)),
    switchConditions(SequenceMap<ECondition>(ap, SequenceProduct(ap, NEWA OneValueSequence<ConditionType>(COND_SWITCH), ConditionInputs(), SbrTypes(), WorkgroupWidths()))),
    nestedSwitchConditions(SequenceMap<ECondition>(ap, SequenceProduct(ap, NEWA OneValueSequence<ConditionType>(COND_SWITCH), ConditionInputs(), SbrTypes(), CornerWidths())))
{
  for (unsigned w = BRIG_WIDTH_1; w <= BRIG_WIDTH_256; ++w) {
    workgroupWidths->Add((BrigWidth) w);
  }
  workgroupWidths->Add(BRIG_WIDTH_WAVESIZE);
  workgroupWidths->Add(BRIG_WIDTH_ALL);
  cornerWidths->Add(BRIG_WIDTH_1);
  cornerWidths->Add(BRIG_WIDTH_WAVESIZE);
  cornerWidths->Add(BRIG_WIDTH_ALL);
  conditionInputs->Add(COND_HOST_INPUT);
  conditionInputs->Add(COND_IMM_PATH0);
  conditionInputs->Add(COND_IMM_PATH1);
  conditionInputs->Add(COND_WAVESIZE);
}

static const unsigned scallFunctionsNumberArray[] = {
  1,
  3,
  16
};

CoreConfig::FunctionsConfig::FunctionsConfig(CoreConfig* cc)
  : ConfigBase(cc),
    scallFunctionsNumber(NEWA VectorSequence<unsigned>(ap)),
    scallIndexValue(NEWA VectorSequence<unsigned>(ap)),
    scallNumberRepeating(NEWA VectorSequence<unsigned>(ap)),
    scallIndexType(NEWA VectorSequence<BrigType>(ap))
{
  scallFunctionsNumber->Add(1);
  scallFunctionsNumber->Add(3);
  scallFunctionsNumber->Add(16);
  scallIndexValue->Add(0);
  scallIndexValue->Add(1);
  scallIndexValue->Add(3);
  scallIndexValue->Add(8);
  scallIndexValue->Add(16);
  scallNumberRepeating->Add(1);
  scallNumberRepeating->Add(4);
  scallNumberRepeating->Add(15);
  scallIndexType->Add(BRIG_TYPE_U32);
  scallIndexType->Add(BRIG_TYPE_U64);
}

}
}
