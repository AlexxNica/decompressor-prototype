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
//

// Implements a binary generator for filter s-expressions.

#include "binary/BinaryWriter.h"
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "interp/ByteWriteStream.h"

namespace wasm {

using namespace decode;
using namespace interp;
using namespace utils;

namespace filt {

namespace {}  // end of anonymous namespace

BinaryWriter::BinaryWriter(std::shared_ptr<decode::Queue> Output,
                           std::shared_ptr<SymbolTable> Symtab)
    : WritePos(decode::StreamType::Byte, Output),
      Writer(std::make_shared<ByteWriteStream>()),
      SectionSymtab(Symtab),
      MinimizeBlockSize(false) {
}

void BinaryWriter::setTrace(std::shared_ptr<TraceClassSexp> NewTrace) {
  Trace = NewTrace;
  if (!Trace)
    return;
  Trace->addContext(WritePos.getTraceContext());
}

TraceClassSexp& BinaryWriter::getTrace() {
  if (!Trace)
    setTrace(std::make_shared<TraceClassSexp>("BinaryWriter"));
  return *Trace;
}

void BinaryWriter::writePreamble() {
  Writer->writeUint32(CasmBinaryMagic, WritePos);
}

void BinaryWriter::writeFile(const FileNode* File) {
  TRACE_METHOD("writeFile");
  TRACE_SEXP(nullptr, File);
  writeNode(File);
}

void BinaryWriter::writeSection(const SectionNode* Section) {
  TRACE_METHOD("writeSection");
  TRACE_SEXP(nullptr, Section);
  writeNode(Section);
}

void BinaryWriter::writeNode(const Node* Nd) {
  TRACE_METHOD("writeNode");
  TRACE_SEXP(nullptr, Nd);
  switch (NodeType Opcode = Nd->getType()) {
    case NO_SUCH_NODETYPE:
#define X(tag, format, defval, mergable, NODE_DECLS) case Op##tag:
      AST_VERSION_INTEGERNODE_TABLE
#undef X
    case OpUnknownSection: {
      // TODO(kschimpf) Fix this list.
      fprintf(stderr, "Misplaced s-expression: %s\n", getNodeTypeName(Opcode));
      fatal("Unable to write filter s-expression");
      break;
    }
#define X(tag, format, defval, mergable, NODE_DECLS)           \
  case Op##tag: {                                              \
    Writer->writeUint8(Opcode, WritePos);                      \
    auto* Int = cast<tag##Node>(Nd);                           \
    if (Int->isDefaultValue()) {                               \
      Writer->writeUint8(0, WritePos);                         \
    } else {                                                   \
      Writer->writeUint8(int(Int->getFormat()) + 1, WritePos); \
      Writer->write##format(Int->getValue(), WritePos);        \
    }                                                          \
    break;                                                     \
  }
      AST_OTHER_INTEGERNODE_TABLE
#undef X
    case OpAnd:
    case OpBlock:
    case OpBitwiseAnd:
    case OpBitwiseNegate:
    case OpBitwiseOr:
    case OpBitwiseXor:
    case OpCallback:
    case OpCase:
    case OpConvert:
    case OpOr:
    case OpNot:
    case OpError:
    case OpIfThen:
    case OpIfThenElse:
    case OpLastSymbolIs:
    case OpLoop:
    case OpLoopUnbounded:
    case OpPeek:
    case OpRead:
    case OpUndefine:
    case OpLastRead:
    case OpRename:
    case OpSet:
    case OpLiteralDef:
    case OpLiteralUse:
    case OpVoid: {
      // Operations that are written out in postorder, with a fixed number of
      // arguments.
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      Writer->writeUint8(Opcode, WritePos);
      break;
    }
    case OpFile: {
      int NumKids = Nd->getNumKids();
      if (NumKids < 2)
        fatal("File must begin with casm and wasm versions");
      for (int i = 0, NumKids = Nd->getNumKids(); i < NumKids; ++i) {
        const Node* Kid = Nd->getKid(i);
        switch (i) {
          case 0:
            if (auto* Version = dyn_cast<WasmVersionNode>(Kid)) {
              uint32_t Value = Version->getValue();
              TRACE(hex_uint32_t, "Wasm version", Value);
              Writer->writeUint32(Value, WritePos);
            } else {
              fatal("Wasm version not specified");
            }
            break;
          case 1:
            if (auto* Version = dyn_cast<CasmVersionNode>(Kid)) {
              uint32_t Value = Version->getValue();
              TRACE(hex_uint32_t, "casm version", Value);
              Writer->writeUint32(Value, WritePos);
            } else {
              fatal("Casm version not specified");
            }
            break;
          default:
            writeNode(Kid);
            break;
        }
      }
      break;
    }
    case OpStream: {
      const auto* Stream = cast<StreamNode>(Nd);
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeUint8(Stream->getEncoding(), WritePos);
      break;
    }
    case OpSection: {
      writeBlock([&]() {
        const auto* Section = cast<SectionNode>(Nd);
        SectionSymtab.installSection(Section);
        const SectionSymbolTable::IndexLookupType& Vector =
            SectionSymtab.getVector();
        Writer->writeVaruint32(Vector.size(), WritePos);
        for (const SymbolNode* Symbol : Vector)
          writeSymbol(Symbol);
        for (int i = 0, len = Nd->getNumKids(); i < len; ++i)
          writeNode(Nd->getKid(i));
      });
      SectionSymtab.clear();
      break;
    }
    case OpDefine:
    case OpEval:
    case OpFilter:
    case OpOpcode:
    case OpMap:
    case OpSwitch:
    case OpSequence:
    case OpWrite: {
      // Operations that are written out in postorder, and have a variable
      // number of arguments.
      for (const auto* Kid : *Nd)
        writeNode(Kid);
      Writer->writeUint8(Opcode, WritePos);
      Writer->writeVaruint32(Nd->getNumKids(), WritePos);
      break;
    }
    case OpSymbol: {
      Writer->writeUint8(Opcode, WritePos);
      SymbolNode* Sym = cast<SymbolNode>(const_cast<Node*>(Nd));
      Writer->writeVaruint32(SectionSymtab.getSymbolIndex(Sym), WritePos);
      break;
    }
  }
}

void BinaryWriter::writeBlock(std::function<void()> ApplyFn) {
  WriteCursor BlockStart(WritePos);
  Writer->writeFixedBlockSize(WritePos, 0);
  size_t SizeAfterSizeWrite = Writer->getStreamAddress(WritePos);
  ApplyFn();
  const size_t NewSize = Writer->getBlockSize(BlockStart, WritePos);
  if (!MinimizeBlockSize) {
    Writer->writeFixedBlockSize(BlockStart, NewSize);
  } else {
    Writer->writeVarintBlockSize(BlockStart, NewSize);
    size_t SizeAfterBackPatch = Writer->getStreamAddress(BlockStart);
    size_t Diff = SizeAfterSizeWrite - SizeAfterBackPatch;
    if (Diff) {
      size_t CurAddress = Writer->getStreamAddress(WritePos);
      Writer->moveBlock(BlockStart, SizeAfterSizeWrite,
                        (CurAddress - Diff) - SizeAfterBackPatch);
      WritePos.swap(BlockStart);
    }
  }
}

void BinaryWriter::writeSymbol(const Node* Symbol) {
  TRACE_METHOD("writeSymbol");
  TRACE_SEXP(nullptr, Symbol);
  assert(isa<SymbolNode>(Symbol) &&
         "BinaryWriter::writeSymbol called on non-symbol");
  const auto* Sym = cast<SymbolNode>(Symbol);
  const std::string& SymName = Sym->getName();
  Writer->writeVaruint32(SymName.size(), WritePos);
  for (size_t i = 0, len = SymName.size(); i < len; ++i)
    Writer->writeUint8(SymName[i], WritePos);
}

}  // end of namespace filt

}  // end of namespace wasm
