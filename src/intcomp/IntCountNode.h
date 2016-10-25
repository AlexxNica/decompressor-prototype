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

// Defines nodes to count usages of integers in a WASM module.

#ifndef DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H
#define DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H

#include "utils/Defs.h"

#include <map>
#include <memory>

namespace wasm {

namespace intcomp {

class IntCountNode;

typedef std::map<decode::IntType,
                 std::shared_ptr<IntCountNode>> IntCountUsageMap;

// Implements a notion of a trie on value usage counts.
class IntCountNode : public std::enable_shared_from_this<IntCountNode> {
  IntCountNode(const IntCountNode&) = delete;
  IntCountNode& operator=(const IntCountNode&) = delete;
 public:
  explicit IntCountNode(decode::IntType Value);
  ~IntCountNode();
  void increment() { ++Count; }
 private:
  size_t Count;
  decode::IntType Value;
  std::unique_ptr<IntCountUsageMap> NextUsageMap;
  std::weak_ptr<IntCountNode> parent;
};

void addUsage(IntCountUsageMap& UsageMap, decode::IntType Value);

} // end of namespace intcomp

} // end of namespace wasm


#endif // DEcOMPRESSOR_SRC_INTCOMP_INTCOUNTNODE_H