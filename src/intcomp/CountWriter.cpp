// -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
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

// Implements a writer that counts usage patterns within the written values.

#include "intcomp/CountWriter.h"
#include "sexp/Ast.h"

namespace wasm {

namespace intcomp {

using namespace decode;
using namespace filt;

CountWriter::CountWriter(CountNode::RootPtr Root)
    : Writer(true), Root(Root), CountCutoff(1), UpToSize(0) {
}

CountWriter::~CountWriter() {
}

StreamType CountWriter::getStreamType() const {
  return StreamType::Int;
}

void CountWriter::addToUsageMap(IntType Value) {
  CountNode::IntPtr TopNd = lookup(Root, Value);
  if (UpToSize == 1) {
    TopNd->increment();
    return;
  }
  IntFrontier NextFrontier;
  while (!Frontier.empty()) {
    CountNode::IntPtr Nd = Frontier.back();
    Frontier.pop_back();
    if (Nd->getPathLength() >= UpToSize || TopNd->getWeight() < CountCutoff)
      continue;
    Nd = lookup(Nd, Value);
    Nd->increment();
    NextFrontier.push_back(Nd);
  }
  Frontier.swap(NextFrontier);
  if (TopNd->getWeight() >= CountCutoff)
    Frontier.push_back(TopNd);
}

bool CountWriter::writeVaruint64(uint64_t Value) {
  addToUsageMap(Value);
  return true;
}

bool CountWriter::writeBlockEnter() {
  Frontier.clear();
  Root->getBlockEnter()->increment();
  return true;
}

bool CountWriter::writeBlockExit() {
  Frontier.clear();
  Root->getBlockExit()->increment();
  return true;
}

bool CountWriter::writeHeaderValue(decode::IntType Value,
                                   interp::IntTypeFormat Format) {
  return true;
}

}  // end of namespace intcomp

}  // end of namespace wasm
