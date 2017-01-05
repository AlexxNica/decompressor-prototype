// -*- C++ -*-
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

// Defines a converter of an Ast algorithm, to the corresponding
// (integer) CASM stream.

#ifndef DECOMPRESSOR_SRC_SEXP_FLATTENAST_H
#define DECOMPRESSOR_SRC_SEXP_FLATTENAST_H

#include "binary/SectionSymbolTable.h"
#include "interp/IntWriter.h"

namespace wasm {

namespace filt {

class FlattenAst {
  FlattenAst() = delete;
  FlattenAst(const FlattenAst&) = delete;
  FlattenAst& operator=(const FlattenAst&) = delete;

 public:
  FlattenAst(std::shared_ptr<interp::IntStream> Output,
             std::shared_ptr<SymbolTable> Symtab);

  ~FlattenAst();

  bool flatten();

  void setFreezeEofOnDestruct(bool Value) { FreezeEofOnDestruct = Value; }

  void setTrace(std::shared_ptr<utils::TraceClass> Trace);
  void setTraceProgress(bool NewValue);
  std::shared_ptr<utils::TraceClass> getTracePtr();
  utils::TraceClass& getTrace() { return *getTracePtr(); }

 private:
  std::shared_ptr<interp::IntWriter> Writer;
  std::shared_ptr<SymbolTable> Symtab;
  SectionSymbolTable SectionSymtab;
  bool FreezeEofOnDestruct;
  bool HasErrors;
  bool WrotePrimaryHeader;
  std::shared_ptr<utils::TraceClass> Trace;

  void flattenNode(const Node* Nd);
  void reportError(const char* Message);
  void reportError(const char* Message, const Node* Nd);
  void freezeOutput();
};

}  // end of namespace filt

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_SEXP_FLATTENAST_H