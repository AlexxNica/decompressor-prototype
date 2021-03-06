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

// Implements the compressor of WASM files base on integer usage.

#include "intcomp/IntCompress.h"

#include "intcomp/AbbrevAssignWriter.h"
#include "intcomp/AbbreviationCodegen.h"
#include "intcomp/AbbreviationsCollector.h"
#include "intcomp/CountWriter.h"
#include "intcomp/RemoveNodesVisitor.h"
#include "interp/ByteReader.h"
#include "interp/ByteWriter.h"
#include "interp/Interpreter.h"
#include "interp/IntInterpreter.h"
#include "interp/IntReader.h"
#include "casm/CasmWriter.h"
#include "sexp/TextWriter.h"
#include "utils/ArgsParse.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace interp;
using namespace utils;
using namespace wasm;

namespace intcomp {

IntCompressor::IntCompressor(std::shared_ptr<decode::Queue> Input,
                             std::shared_ptr<decode::Queue> Output,
                             std::shared_ptr<filt::SymbolTable> Symtab,
                             const CompressionFlags& MyFlags)
    : Input(Input),
      Output(Output),
      MyFlags(MyFlags),
      Symtab(Symtab),
      ErrorsFound(false) {
  if (MyFlags.TraceCompression)
    setTraceProgress(true);
}

void IntCompressor::setTrace(std::shared_ptr<TraceClass> NewTrace) {
  Trace = NewTrace;
}

std::shared_ptr<TraceClass> IntCompressor::getTracePtr() {
  if (!Trace)
    setTrace(std::make_shared<TraceClass>("IntCompress"));
  return Trace;
}

CountNode::RootPtr IntCompressor::getRoot() {
  if (!Root)
    Root = std::make_shared<RootCountNode>();
  return Root;
}

void IntCompressor::readInput() {
  Contents = std::make_shared<IntStream>();
  auto MyWriter = std::make_shared<IntWriter>(Contents);
  Interpreter MyReader(std::make_shared<ByteReader>(Input), MyWriter,
                       MyFlags.MyInterpFlags, Symtab);
  if (MyFlags.TraceReadingInput)
    MyReader.getTrace().setTraceProgress(true);
  MyReader.algorithmRead();
  bool Successful = MyReader.isFinished() && MyReader.isSuccessful();
  if (!Successful)
    ErrorsFound = true;
  Input.reset();
  return;
}

const BitWriteCursor IntCompressor::writeCodeOutput(
    std::shared_ptr<SymbolTable> Symtab) {
  TRACE_METHOD("writeCodeOutput");
  CasmWriter Writer;
  return Writer.setTraceWriter(MyFlags.TraceWritingCodeOutput)
      .setTraceTree(MyFlags.TraceWritingCodeOutput)
      .setMinimizeBlockSize(MyFlags.MinimizeCodeSize)
      .setFreezeEofAtExit(false)
      .setBitCompress(MyFlags.BitCompressOpcodes)
      .writeBinary(Symtab, Output);
}

void IntCompressor::writeDataOutput(const BitWriteCursor& StartPos,
                                    std::shared_ptr<SymbolTable> Symtab) {
  TRACE_METHOD("writeDataOutput");
  auto Writer = std::make_shared<ByteWriter>(Output);
  Writer->setPos(StartPos);
  Interpreter MyReader(std::make_shared<IntReader>(IntOutput), Writer,
                       MyFlags.MyInterpFlags, Symtab);
  if (MyFlags.TraceWritingDataOutput)
    MyReader.getTrace().setTraceProgress(true);
  MyReader.useFileHeader(Symtab->getTargetHeader());
  MyReader.algorithmStart();
  MyReader.algorithmReadBackFilled();
  bool Successful = MyReader.isFinished() && MyReader.isSuccessful();
  if (!Successful)
    ErrorsFound = true;
  return;
}

IntCompressor::~IntCompressor() {
}

bool IntCompressor::compressUpToSize(size_t Size) {
  TRACE_BLOCK({
    if (Size == 1)
      TRACE_MESSAGE("Collecting integer sequences of length: 1");
    else
      TRACE_MESSAGE("Collecting integer sequences of (up to) length: " +
                    std::to_string(Size));
  });
  auto Writer = std::make_shared<CountWriter>(getRoot());
  Writer->setCountCutoff(MyFlags.CountCutoff);
  Writer->setUpToSize(Size);

  IntInterpreter Reader(std::make_shared<IntReader>(Contents), Writer,
                        MyFlags.MyInterpFlags, Symtab);
  if (MyFlags.TraceReadingIntStream)
    Reader.getTrace().setTraceProgress(true);
  Reader.structuralRead();
  return !Reader.errorsFound();
}

void IntCompressor::removeSmallUsageCounts(bool KeepSingletonsUsingCount,
                                           bool ZeroOutSmallNodes) {
  // NOTE: The main purpose of this method is to shrink the size of
  // the trie to (a) recover memory and (b) make remaining analysis
  // faster.  It does this by removing int count nodes that are not
  // not useful (See case RemoveFrame::State::Exit for details).
  RemoveNodesVisitor Visitor(Root, MyFlags, KeepSingletonsUsingCount,
                             ZeroOutSmallNodes);
  Visitor.walk();
}

void IntCompressor::compress() {
  TRACE_METHOD("compress");
  TRACE_MESSAGE("Reading input");
  readInput();
  if (errorsFound()) {
    fprintf(stderr, "Unable to decompress, input malformed");
    return;
  }
  TRACE(size_t, "Number of integers in input", Contents->getNumIntegers());
  if (MyFlags.TraceInputIntStream)
    Contents->describe(stderr, "Input int stream");
  // Start by collecting number of occurrences of each integer, so
  // that we can use as a filter on integer sequence inclusion into the
  // trie.
  if (!compressUpToSize(1))
    return;
  removeSmallSingletonUsageCounts();
  if (MyFlags.TraceIntCounts)
    describeCutoff(stderr, MyFlags.CountCutoff,
                   makeFlags(CollectionFlag::TopLevel),
                   MyFlags.TraceIntCountsCollection);
  if (MyFlags.PatternLengthLimit > 1) {
    if (!compressUpToSize(MyFlags.PatternLengthLimit))
      return;
    removeAllSmallUsageCounts();
    if (MyFlags.TraceSequenceCounts)
      describeCutoff(stderr, MyFlags.WeightCutoff,
                     makeFlags(CollectionFlag::IntPaths),
                     MyFlags.TraceSequenceCountsCollection);
  }
  TRACE_MESSAGE("Assigning (initial) abbreviations to integer sequences");
  // SInce we don't actually know the number of times default patterns will
  // be used, assume a large number.
  Root->getDefaultSingle()->setCount(100);
  Root->getDefaultMultiple()->setCount(100);
  if (MyFlags.UseHuffmanEncoding)
    // Assume an alignment added at end of file.
    Root->getAlign()->setCount(1);
  CountNode::PtrSet AbbrevAssignments;
  assignInitialAbbreviations(AbbrevAssignments);
  zeroSmallUsageCounts();
  if (MyFlags.TraceInitialAbbreviationAssignments)
    describeAbbreviations(stderr,
                          MyFlags.TraceAbbreviationAssignmentsCollection);
  IntOutput = std::make_shared<IntStream>();
  TRACE_MESSAGE("Generating compressed integer stream");
  if (!generateIntOutput(AbbrevAssignments))
    return;
  TRACE(size_t, "Number of integers in compressed output",
        IntOutput->getNumIntegers());
  if (MyFlags.TraceCompressedIntOutput)
    IntOutput->describe(stderr, "Output int stream");
  TRACE_MESSAGE("Appending compression algorithm to output");
  const BitWriteCursor Pos =
      writeCodeOutput(generateCodeForReading(AbbrevAssignments));
  if (errorsFound()) {
    fprintf(stderr, "Unable to compress, output malformed\n");
    return;
  }
  TRACE(size_t, "Pos after code", Pos.getAddress());
  TRACE_MESSAGE("Appending compressed WASM file to output");
  writeDataOutput(Pos, generateCodeForWriting(AbbrevAssignments));
  if (errorsFound()) {
    fprintf(stderr, "Unable to compress, output malformed\n");
    return;
  }
}

void IntCompressor::assignInitialAbbreviations(CountNode::PtrSet& Assignments) {
  AbbreviationsCollector Collector(getRoot(), Assignments, MyFlags);
  if (MyFlags.TraceAssigningAbbreviations && hasTrace())
    Collector.setTrace(getTracePtr());
  EncodingRoot = Collector.assignAbbreviations();
}

bool IntCompressor::generateIntOutput(CountNode::PtrSet& Assignments) {
  auto Writer = std::make_shared<AbbrevAssignWriter>(
      Root, Assignments, EncodingRoot, IntOutput,
      MyFlags.PatternLengthLimit * MyFlags.PatternLengthMultiplier,
      !MyFlags.UseHuffmanEncoding, MyFlags);
  IntInterpreter Interp(std::make_shared<IntReader>(Contents), Writer,
                        MyFlags.MyInterpFlags, Symtab);
  if (MyFlags.TraceIntStreamGeneration)
    Interp.setTraceProgress(true);
  Interp.structuralRead();
  assert(IntOutput->isFrozen());
  return !Interp.errorsFound();
}

std::shared_ptr<SymbolTable> IntCompressor::generateCode(
    CountNode::PtrSet& Assignments,
    bool ToRead,
    bool Trace) {
  TRACE_METHOD("generateCode");
  AbbreviationCodegen Codegen(Root, EncodingRoot, MyFlags.AbbrevFormat,
                              Assignments);
  std::shared_ptr<SymbolTable> Symtab = Codegen.getCodeSymtab(ToRead);
  if (Trace) {
    TextWriter Writer;
    Writer.write(stderr, Symtab->getInstalledRoot());
  }
  return Symtab;
}

void IntCompressor::describeCutoff(FILE* Out,
                                   uint64_t CountCutoff,
                                   uint64_t WeightCutoff,
                                   CollectionFlags Flags,
                                   bool Trace) {
  CountNodeCollector Collector(getRoot());
  if (Trace)
    Collector.setTrace(getTracePtr());
  Collector.collectUsingCutoffs(CountCutoff, WeightCutoff, Flags);
  TRACE_BLOCK({ Collector.describe(getTrace().getFile()); });
}

void IntCompressor::describeAbbreviations(FILE* Out, bool Trace) {
  CountNodeCollector Collector(getRoot());
  if (Trace)
    Collector.setTrace(getTracePtr());
  Collector.collectAbbreviations();
  TRACE_BLOCK({ Collector.describe(getTrace().getFile()); });
}

}  // end of namespace intcomp

}  // end of namespace wasm
