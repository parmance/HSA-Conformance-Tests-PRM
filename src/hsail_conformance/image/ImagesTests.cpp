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

#include "ImagesTests.hpp"
#include "ImageRdTests.hpp"
#include "ImageQueryTests.hpp"
#include "ImageLdTests.hpp"
#include "ImageStTests.hpp"
#include "ImageInitializerTests.hpp"
#include "ImageLimitsTests.hpp"
#include "CoreConfig.hpp"

using namespace hexl;

namespace hsail_conformance { 

DECLARE_TESTSET_UNION(PrmImagesTests);

PrmImagesTests::PrmImagesTests()
  : TestSetUnion("image")
{
  Add(new ImageRdTestSet());
  Add(new ImageQueryTestSet());
  Add(new ImageLdTestSet());
	Add(new ImageStTestSet());
  Add(new ImageInitializerTestSet());
  Add(new ImageLimitsTestSet());
}

hexl::TestSet* NewPrmImagesTests()
{
  return new PrmImagesTests();
}

}
