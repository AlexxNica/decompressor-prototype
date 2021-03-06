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

// Defines a pointer to a byte stream for reading.

#ifndef DECOMPRESSOR_SRC_STREAM_READCURSOR_H
#define DECOMPRESSOR_SRC_STREAM_READCURSOR_H

#include "stream/Cursor.h"

namespace wasm {

namespace utils {
class TraceClass;
}  // end of namespace utils

namespace decode {

class ReadCursor : public Cursor {
 public:
  // Note: The nullary read cursor should not be used until it has been assigned
  // a valid value.
  ReadCursor();
  ReadCursor(std::shared_ptr<Queue> Que);
  ReadCursor(StreamType Type, std::shared_ptr<Queue> Que);
  explicit ReadCursor(const Cursor& C);
  ReadCursor(const Cursor& C, size_t StartAddress);
  ~ReadCursor() OVERRIDE;

  ReadCursor& operator=(const ReadCursor& C) {
    assign(C);
    return *this;
  }
  virtual bool atEob();
  void pushEobAddress(AddressType NewValue);
  void popEobAddress();

  // Reads next byte. Returns zero if at end of file.
  virtual ByteType readByte();
  virtual ByteType readBit();

  // Try to advance Distance bytes. Returns actual number of bytes advanced.  If
  // zero is returned (and Distance > 0), no more bytes are available to advance
  // on.
  size_t advance(size_t Distance);

 protected:
  uint8_t readOneByte();
  uint8_t readByteAfterReadFill();
};

}  // end of namespace decode

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRC_STREAM_READCURSOR_H
