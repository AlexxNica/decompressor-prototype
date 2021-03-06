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

#include "interp/ByteReader.h"
#include "interp/Interpreter.h"
#include "sexp/Ast.h"
#include "casm/InflateAst.h"
#include "sexp/TextWriter.h"
#include "sexp-parser/Driver.h"
#include "stream/FileReader.h"
#include "stream/ReadBackedQueue.h"

namespace wasm {

using namespace filt;
using namespace interp;
using namespace utils;

namespace decode {

CasmReader::CasmReader()
    : TraceRead(false),
      TraceTree(false),
      TraceLexer(false),
      ErrorsFound(false) {
}

CasmReader::~CasmReader() {
}

void CasmReader::foundErrors() {
  ErrorsFound = true;
  Symtab.reset();
}

void CasmReader::readText(charstring Filename) {
  Symtab = std::make_shared<SymbolTable>();
  Driver Parser(Symtab);
  if (TraceRead)
    Parser.setTraceParsing(true);
  if (TraceLexer)
    Parser.setTraceLexing(true);
  if (!Parser.parse(Filename)) {
    foundErrors();
    return;
  }
  if (TraceTree) {
    TextWriter Writer;
    Writer.write(stderr, Symtab.get());
  }
}

void CasmReader::readBinary(std::shared_ptr<Queue> Binary,
                            std::shared_ptr<SymbolTable> AlgSymtab) {
  auto Inflator = std::make_shared<InflateAst>();
  InterpreterFlags Flags;
  Interpreter MyReader(std::make_shared<ByteReader>(Binary), Inflator, Flags,
                       AlgSymtab);
  if (TraceRead || TraceTree) {
    auto Trace = std::make_shared<TraceClass>("CasmInterpreter");
    Trace->setTraceProgress(true);
    MyReader.setTrace(Trace);
    if (TraceTree)
      Inflator->setTrace(Trace);
  }
  MyReader.algorithmStart();
  MyReader.algorithmReadBackFilled();
  if (MyReader.errorsFound()) {
    foundErrors();
    return;
  }
  Symtab = Inflator->getSymtab();
}

void CasmReader::readBinary(charstring Filename,
                            std::shared_ptr<SymbolTable> AlgSymtab) {
  readBinary(
      std::make_shared<ReadBackedQueue>(std::make_shared<FileReader>(Filename)),
      AlgSymtab);
}

}  // end of namespace filt

}  // end of namespace wasm
