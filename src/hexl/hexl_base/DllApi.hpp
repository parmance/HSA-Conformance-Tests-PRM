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

#ifndef HEXL_DLL_API_HPP
#define HEXL_DLL_API_HPP

#ifdef _WIN32
#include <Windows.h>
#define snprintf _snprintf
#else
#include "dlfcn.h"
#endif
#include "HexlTest.hpp"

namespace hexl {

class Options;

template <typename ApiTable>
class DllApi {
private:
#ifdef _WIN32
  typedef HMODULE DllHandle;
#else
  typedef void *DllHandle;
#endif
  DllHandle dllHandle;
  const ApiTable* apiTable;
  const char* libName;

protected:
  Context* context;
  const Options* options;

  bool InitDll() {
    char buf[0x100];
#ifdef _WIN32
    _snprintf_s(buf, sizeof(buf), "%s.dll", libName);
    dllHandle = LoadLibrary(buf);
    if (!dllHandle) {
      context->Error() << "LoadLibrary(" << buf << ") failed: GetLastError=" << GetLastError() << std::endl;
      return false;
    }
#else
    snprintf(buf, sizeof(buf), "lib%s.so", libName);
    dllHandle = dlopen(buf, RTLD_NOW|RTLD_LOCAL);
    if (!dllHandle) {
      context->Error() << "dlopen(" << buf << ") failed: " << dlerror() << std::endl;
      return false;
    }
#endif
    return true;
  }

  template <typename F>
  F GetFunction(const char *functionName, F f0 = 0) {
#ifdef _WIN32
    F f = (F) GetProcAddress(dllHandle, functionName);
    if (!f) {
      context->Error() << "GetProcAddress(" << functionName << ") failed: GetLastError=" << GetLastError() << std::endl;
      return 0;
    }
#else
    F f = (F) dlsym(dllHandle, functionName);
    if (!f) {
      context->Error() << "dlsym(" << functionName << ") failed: " << dlerror() << std::endl;
      return 0;
    }
#endif
    return f;
  }

public:
  DllApi(Context* context_, const Options* options_, const char* libName_)
    : dllHandle(0), apiTable(0),
      libName(libName_), context(context_), options(options_) { }

  ~DllApi() {
    if (apiTable) { delete apiTable; }
    if (dllHandle) {
#ifdef _WIN32
      FreeLibrary(dllHandle);
#else
      dlclose(dllHandle);
#endif
    }
  }

  const ApiTable* operator->() const { return apiTable; }

  virtual const ApiTable* InitApiTable() = 0;

  bool Init() {
    if (!InitDll()) { return false; }
    apiTable = InitApiTable();
    if (!apiTable) { return false; }
    return true;
  }
};

}

#endif // HEXL_DLL_API_HPP
