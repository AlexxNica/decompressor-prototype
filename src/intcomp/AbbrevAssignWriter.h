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

// Defines a writer that injects abbreviations into the input stream.

#ifndef DECOMPRESSOR_SRC_INTCOMP_ABBREVASSIGNWRITER_H
#define DECOMPRESSOR_SRC_INTCOMP_ABBREVASSIGNWRITER_H

#include <vector>

#include "intcomp/CompressionFlags.h"
#include "intcomp/CountNode.h"
#include "interp/IntStream.h"
#include "interp/IntWriter.h"
#include "utils/circular-vector.h"

namespace wasm {

namespace intcomp {

class AbbrevAssignValue;

class AbbrevAssignWriter : public interp::Writer {
  AbbrevAssignWriter() = delete;
  AbbrevAssignWriter(const AbbrevAssignWriter&) = delete;
  AbbrevAssignWriter& operator=(const AbbrevAssignWriter&) = delete;

 public:
  AbbrevAssignWriter(CountNode::RootPtr Root,
                     CountNode::PtrSet& Assignments,
                     utils::HuffmanEncoder::NodePtr& EncodingRoot,
                     std::shared_ptr<interp::IntStream> Output,
                     size_t BufSize,
                     bool AssumeByteAlignment,
                     const CompressionFlags& MyFlags);
  ~AbbrevAssignWriter() OVERRIDE;

  decode::StreamType getStreamType() const OVERRIDE;
  bool writeVaruint64(uint64_t Value) OVERRIDE;
  bool writeFreezeEof() OVERRIDE;
  bool writeHeaderValue(decode::IntType Value,
                        interp::IntTypeFormat Format) OVERRIDE;
  bool writeBlockEnter() OVERRIDE;
  bool writeBlockExit() OVERRIDE;

  void setTrace(std::shared_ptr<utils::TraceClass> Trace) OVERRIDE;

 private:
  const CompressionFlags& MyFlags;
  CountNode::RootPtr Root;
  CountNode::PtrSet& Assignments;
  utils::HuffmanEncoder::NodePtr& EncodingRoot;
  interp::IntWriter OutWriter;
  utils::circular_vector<decode::IntType> Buffer;
  std::vector<decode::IntType> DefaultValues;
  // Intermediate structure. Allows us to change encoding of
  // abbreviations once we know the actually usage counts.
  std::vector<AbbrevAssignValue*> Values;
  bool AssumeByteAlignment;
  size_t ProgressCount;

  void bufferValue(decode::IntType Value);
  void forwardAbbrev(CountNode::Ptr Abbrev);
  void forwardAbbrevAfterFlush(CountNode::Ptr Abbev);
  void forwardOtherValue(decode::IntType Value);
  void writeFromBuffer();
  void writeUntilBufferEmpty();
  void popValuesFromBuffer(size_t size);
  void flushDefaultValues();
  void alignIfNecessary();
  bool flushValues();
  void clearValues();
  void reassignAbbreviations();

  const char* getDefaultTraceName() const OVERRIDE;
};

}  // end of namespace intcomp

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_INTCOMP_ABBREVASSIGNWRITER_H
