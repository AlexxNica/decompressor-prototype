// -*- C++ -*- */
//
// Copyright 2017 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Implement functions to read in a CASM (binary compressed) algorithm file.

#include "casm/CasmReader.h"

#include "algorithms/casm0x0.h"

namespace wasm {

using namespace filt;
using namespace interp;
using namespace utils;

namespace decode {

void CasmReader::readBinary(std::shared_ptr<Queue> Binary) {
  readBinary(Binary, getAlgcasm0x0Symtab());
}

void CasmReader::readBinary(charstring Filename) {
  readBinary(Filename, getAlgcasm0x0Symtab());
}

}  // end of namespace filt

}  // end of namespace wasm
