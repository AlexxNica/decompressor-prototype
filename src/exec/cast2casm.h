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

// Implements C++ implementations of algorithms. Used by cast2casm-boot{1,2} and
// cast2casm. The former is to be able to generate boot files while the latter
// is the full blown tool.

#include <algorithm>
#include <cctype>
#include <cstdio>

#if WASM_CAST_BOOT > 1
#include "algorithms/casm0x0.h"
#include "casm/CasmReader.h"
#include "casm/CasmWriter.h"
#endif
#include "sexp/Ast.h"
#include "sexp/TextWriter.h"
#include "sexp-parser/Driver.h"
#include "stream/FileWriter.h"
#include "stream/ReadCursor.h"
#include "stream/WriteBackedQueue.h"
#include "utils/ArgsParse.h"

namespace wasm {

using namespace decode;
using namespace filt;
using namespace utils;

namespace {

static charstring LocalName = "Local_";
static charstring FuncName = "Func_";

constexpr size_t WorkBufferSize = 128;
typedef char BufferType[WorkBufferSize];

class CodeGenerator {
  CodeGenerator() = delete;
  CodeGenerator(const CodeGenerator&) = delete;
  CodeGenerator& operator=(const CodeGenerator&) = delete;

 public:
  CodeGenerator(charstring Filename,
                std::shared_ptr<RawStream> Output,
                std::shared_ptr<SymbolTable> Symtab,
                std::vector<charstring>& Namespaces,
                charstring FunctionName)
      : Filename(Filename),
        Output(Output),
        Symtab(Symtab),
        Namespaces(Namespaces),
        FunctionName(FunctionName),
        ErrorsFound(false),
        NextIndex(1) {}
  ~CodeGenerator() {}
  void generateDeclFile();
  void generateImplFile(bool UseArrayImpl);
  bool foundErrors() const { return ErrorsFound; }
  void setStartPos(std::shared_ptr<ReadCursor> StartPos) { ReadPos = StartPos; }

 private:
  charstring Filename;
  std::shared_ptr<RawStream> Output;
  std::shared_ptr<SymbolTable> Symtab;
  std::shared_ptr<ReadCursor> ReadPos;
  std::vector<charstring>& Namespaces;
  std::vector<const LiteralActionDefNode*> ActionDefs;
  charstring FunctionName;
  bool ErrorsFound;
  size_t NextIndex;

  void puts(charstring Str) { Output->puts(Str); }
  void putc(char Ch) { Output->putc(Ch); }
  void putSymbol(charstring Name, bool Capitalize = true);
  char symbolize(char Ch, bool Capitalize);
#if WASM_CAST_BOOT > 1
  void generateArrayImplFile();
#endif
  void generateFunctionImplFile();
  void generateHeader();
  void generateEnterNamespaces();
  void generateExitNamespaces();
  void collectActionDefs();
  void generatePredefinedEnum();
  void generatePredefinedEnumNames();
  void generatePredefinedNameFcn();
  void generateAlgorithmHeader();
  size_t generateNode(const Node* Nd);
  size_t generateSymbol(const SymbolNode* Sym);
  size_t generateIntegerNode(charstring NodeType, const IntegerNode* Nd);
  size_t generateNullaryNode(charstring NodeType, const Node* Nd);
  size_t generateUnaryNode(charstring NodeType, const Node* Nd);
  size_t generateBinaryNode(charstring NodeType, const Node* Nd);
  size_t generateTernaryNode(charstring NodeType, const Node* Nd);
  size_t generateNaryNode(charstring NodeName, const Node* Nd);
  void generateInt(IntType Value);
  void generateFormat(ValueFormat Format);
  void generateLocal(size_t Index);
  void generateLocalVar(std::string NodeType, size_t Index);
  void generateFunctionName(size_t Index);
  void generateFunctionCall(size_t Index);
  void generateFunctionHeader(std::string NodeType, size_t Index);
  void generateCloseFunctionFooter();
  void generateFunctionFooter();
  void generateCreate(charstring NodeType);
  void generateReturnCreate(charstring NodeType);
  size_t generateBadLocal(const Node* Nd);
  void generateArrayName() {
    puts(FunctionName);
    puts("Array");
  }
};

void CodeGenerator::generateInt(IntType Value) {
  BufferType Buffer;
  sprintf(Buffer, "%" PRIuMAX "", uintmax_t(Value));
  puts(Buffer);
}

void CodeGenerator::generateFormat(ValueFormat Format) {
  switch (Format) {
    case ValueFormat::Decimal:
      puts("ValueFormat::Decimal");
      break;
    case ValueFormat::SignedDecimal:
      puts("ValueFormat::SignedDecimal");
      break;
    case ValueFormat::Hexidecimal:
      puts("ValueFormat::Hexidecimal");
      break;
  }
}

void CodeGenerator::generateHeader() {
  puts(
      "// -*- C++ -*- \n"
      "\n"
      "// *** AUTOMATICALLY GENERATED FILE (DO NOT EDIT)! ***\n"
      "\n"
      "// Copyright 2016 WebAssembly Community Group participants\n"
      "//\n"
      "// Licensed under the Apache License, Version 2.0 (the \"License\");\n"
      "// you may not use this file except in compliance with the License.\n"
      "// You may obtain a copy of the License at\n"
      "//\n"
      "//     http://www.apache.org/licenses/LICENSE-2.0\n"
      "//\n"
      "// Unless required by applicable law or agreed to in writing, software\n"
      "// distributed under the License is distributed on an \"AS IS\" BASIS,\n"
      "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or "
      "implied.\n"
      "// See the License for the specific language governing permissions and\n"
      "// limitations under the License.\n"
      "\n"
      "// Generated from: \"");
  puts(Filename);
  puts(
      "\"\n"
      "\n"
      "#include \"sexp/Ast.h\"\n"
      "\n"
      "#include <memory>\n"
      "\n");
}

void CodeGenerator::generateEnterNamespaces() {
  for (charstring Name : Namespaces) {
    puts("namespace ");
    puts(Name);
    puts(
        " {\n"
        "\n");
  }
}

void CodeGenerator::generateExitNamespaces() {
  for (std::vector<charstring>::reverse_iterator Iter = Namespaces.rbegin(),
                                                 IterEnd = Namespaces.rend();
       Iter != IterEnd; ++Iter) {
    puts("}  // end of namespace ");
    puts(*Iter);
    puts(
        "\n"
        "\n");
  }
}

void CodeGenerator::putSymbol(charstring Name, bool Capitalize) {
  size_t Len = strlen(Name);
  for (size_t i = 0; i < Len; ++i) {
    char Ch = Name[i];
    switch (Ch) {
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
      case 'g':
      case 'h':
      case 'i':
      case 'j':
      case 'k':
      case 'l':
      case 'm':
      case 'n':
      case 'o':
      case 'p':
      case 'q':
      case 'r':
      case 's':
      case 't':
      case 'u':
      case 'v':
      case 'w':
      case 'x':
      case 'y':
      case 'z':
        putc(i > 0 ? Ch : ((Ch - 'a') + 'A'));
        break;
      case 'A':
      case 'B':
      case 'C':
      case 'D':
      case 'E':
      case 'F':
      case 'G':
      case 'H':
      case 'I':
      case 'J':
      case 'K':
      case 'L':
      case 'M':
      case 'N':
      case 'O':
      case 'P':
      case 'Q':
      case 'R':
      case 'S':
      case 'T':
      case 'U':
      case 'V':
      case 'W':
      case 'X':
      case 'Y':
      case 'Z':
        putc(Ch);
        break;
      case '_':
        puts("__");
        break;
      case '.':
        putc('_');
        break;
      default: {
        BufferType Buffer;
        sprintf(Buffer, "_x%X_", Ch);
        puts(Buffer);
        break;
      }
    }
  }
}

namespace {

IntType getActionDefValue(const LiteralActionDefNode* Nd) {
  if (const IntegerNode* Num = dyn_cast<IntegerNode>(Nd->getKid(1)))
    return Num->getValue();
  return 0;
}

std::string getActionDefName(const LiteralActionDefNode* Nd) {
  if (const SymbolNode* Sym = dyn_cast<SymbolNode>(Nd->getKid(0)))
    return Sym->getName();
  return std::string("???");
}

bool compareLtActionDefs(const LiteralActionDefNode* N1,
                         const LiteralActionDefNode* N2) {
  IntType V1 = getActionDefValue(N1);
  IntType V2 = getActionDefValue(N2);
  if (V1 < V2)
    return true;
  if (V1 > V2)
    return false;
  return getActionDefName(N1) < getActionDefName(N2);
}

}  // end of anonymous namespace

void CodeGenerator::collectActionDefs() {
  if (!ActionDefs.empty())
    return;
  SymbolTable::ActionDefSet DefSet;
  Symtab->collectActionDefs(DefSet);
  for (const LiteralActionDefNode* Def : DefSet)
    ActionDefs.push_back(Def);
  std::sort(ActionDefs.begin(), ActionDefs.end(), compareLtActionDefs);
}

void CodeGenerator::generatePredefinedEnum() {
  collectActionDefs();
  puts("enum class Predefined");
  puts(FunctionName);
  puts(" : uint32_t {\n");
  bool IsFirst = true;
  for (const LiteralActionDefNode* Def : ActionDefs) {
    if (IsFirst)
      IsFirst = false;
    else
      puts(",\n");
    puts("  ");
    putSymbol(getActionDefName(Def).c_str());
    BufferType Buffer;
    sprintf(Buffer, " = %" PRIuMAX "", uintmax_t(getActionDefValue(Def)));
    puts(Buffer);
  }
  puts(
      "\n"
      "};\n"
      "\n"
      "charstring getName(Predefined");
  puts(FunctionName);
  puts(
      " Value);\n"
      "\n");
}

void CodeGenerator::generatePredefinedEnumNames() {
  collectActionDefs();
  puts(
      "struct {\n"
      "  Predefined");
  puts(FunctionName);
  puts(
      " Value;\n"
      "  charstring Name;\n"
      "} PredefinedNames[] {\n");
  bool IsFirst = true;
  for (const LiteralActionDefNode* Def : ActionDefs) {
    if (IsFirst)
      IsFirst = false;
    else
      puts(",\n");
    puts("  {Predefined");
    puts(FunctionName);
    puts("::");
    putSymbol(getActionDefName(Def).c_str());
    puts(", \"");
    puts(getActionDefName(Def).c_str());
    puts("\"}");
  }
  puts(
      "\n"
      "};\n"
      "\n");
}

void CodeGenerator::generatePredefinedNameFcn() {
  puts("charstring getName(Predefined");
  puts(FunctionName);
  puts(
      " Value) {\n"
      "  for (size_t i = 0; i < size(PredefinedNames); ++i) {\n"
      "    if (PredefinedNames[i].Value == Value) \n"
      "      return PredefinedNames[i].Name;\n"
      "  }\n"
      "  return getName(PredefinedSymbol::Unknown);\n"
      "}\n"
      "\n");
}

void CodeGenerator::generateAlgorithmHeader() {
  puts("std::shared_ptr<filt::SymbolTable> get");
  puts(FunctionName);
  puts("Symtab()");
}

size_t CodeGenerator::generateBadLocal(const Node* Nd) {
  TextWriter Writer;
  fprintf(stderr, "Unrecognized: ");
  Writer.writeAbbrev(stderr, Nd);
  size_t Index = NextIndex++;
  ErrorsFound = true;
  generateLocalVar("Node", Index);
  puts("nullptr;\n");
  return Index;
}

void CodeGenerator::generateLocal(size_t Index) {
  puts(LocalName);
  generateInt(Index);
}

void CodeGenerator::generateLocalVar(std::string NodeType, size_t Index) {
  puts("  ");
  puts(NodeType.c_str());
  puts("* ");
  generateLocal(Index);
  puts(" = ");
}

void CodeGenerator::generateFunctionName(size_t Index) {
  puts(FuncName);
  generateInt(Index);
}

void CodeGenerator::generateFunctionCall(size_t Index) {
  generateFunctionName(Index);
  puts("(Symtab)");
}

void CodeGenerator::generateFunctionHeader(std::string NodeType, size_t Index) {
  puts(NodeType.c_str());
  puts("* ");
  generateFunctionName(Index);
  puts("(SymbolTable* Symtab) {\n");
}

void CodeGenerator::generateFunctionFooter() {
  puts(
      "}\n"
      "\n");
}

void CodeGenerator::generateCloseFunctionFooter() {
  puts(");\n");
  generateFunctionFooter();
}

void CodeGenerator::generateCreate(charstring NodeType) {
  puts("Symtab->create<");
  puts(NodeType);
  puts(">(");
}

void CodeGenerator::generateReturnCreate(charstring NodeType) {
  puts("  return ");
  generateCreate(NodeType);
}

size_t CodeGenerator::generateSymbol(const SymbolNode* Sym) {
  size_t Index = NextIndex++;
  generateFunctionHeader("SymbolNode", Index);
  puts("  return Symtab->getSymbolDefinition(\"");
  for (auto& Ch : Sym->getName())
    putc(Ch);
  putc('"');
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateIntegerNode(charstring NodeName,
                                          const IntegerNode* Nd) {
  size_t Index = NextIndex++;
  std::string NodeType(NodeName);
  NodeType.append("Node");
  generateFunctionHeader(NodeType, Index);
  puts("  return Symtab->get");
  puts(NodeName);
  puts("Definition(");
  generateInt(Nd->getValue());
  puts(", ");
  generateFormat(Nd->getFormat());
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateNullaryNode(charstring NodeType, const Node* Nd) {
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateUnaryNode(charstring NodeType, const Node* Nd) {
  size_t Index = NextIndex++;
  assert(Nd->getNumKids() == 1);
  size_t Kid1 = generateNode(Nd->getKid(0));
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  generateFunctionCall(Kid1);
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateBinaryNode(charstring NodeType, const Node* Nd) {
  assert(Nd->getNumKids() == 2);
  size_t Kid1 = generateNode(Nd->getKid(0));
  size_t Kid2 = generateNode(Nd->getKid(1));
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  generateFunctionCall(Kid1);
  puts(", ");
  generateFunctionCall(Kid2);
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateTernaryNode(charstring NodeType, const Node* Nd) {
  assert(Nd->getNumKids() == 3);
  size_t Kid1 = generateNode(Nd->getKid(0));
  size_t Kid2 = generateNode(Nd->getKid(1));
  size_t Kid3 = generateNode(Nd->getKid(2));
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateReturnCreate(NodeType);
  generateFunctionCall(Kid1);
  puts(", ");
  generateFunctionCall(Kid2);
  puts(", ");
  generateFunctionCall(Kid3);
  generateCloseFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateNaryNode(charstring NodeType, const Node* Nd) {
  std::vector<size_t> Kids;
  for (int i = 0; i < Nd->getNumKids(); ++i)
    Kids.push_back(generateNode(Nd->getKid(i)));
  size_t Index = NextIndex++;
  generateFunctionHeader(NodeType, Index);
  generateLocalVar(NodeType, Index);
  generateCreate(NodeType);
  puts(");\n");
  for (size_t KidIndex : Kids) {
    puts("  ");
    generateLocal(Index);
    puts("->append(");
    generateFunctionCall(KidIndex);
    puts(");\n");
  }
  puts("  return ");
  generateLocal(Index);
  puts(";\n");
  generateFunctionFooter();
  return Index;
}

size_t CodeGenerator::generateNode(const Node* Nd) {
  if (Nd == nullptr)
    return generateBadLocal(Nd);

  switch (Nd->getType()) {
    default:
      return generateBadLocal(Nd);
    case OpAnd:
      return generateBinaryNode("AndNode", Nd);
    case OpBit:
      return generateNullaryNode("BitNode", Nd);
    case OpBitwiseAnd:
      return generateBinaryNode("BitwiseAndNode", Nd);
    case OpBitwiseNegate:
      return generateUnaryNode("BitwiseNegateNode", Nd);
    case OpBitwiseOr:
      return generateBinaryNode("BitwiseOrNode", Nd);
    case OpBitwiseXor:
      return generateBinaryNode("BitwiseXorNode", Nd);
    case OpBlock:
      return generateUnaryNode("BlockNode", Nd);
    case OpCallback:
      return generateUnaryNode("CallbackNode", Nd);
    case OpCase:
      return generateBinaryNode("CaseNode", Nd);
    case OpDefine:
      return generateNaryNode("DefineNode", Nd);
    case OpError:
      return generateNullaryNode("ErrorNode", Nd);
    case OpEval:
      return generateNaryNode("EvalNode", Nd);
    case OpFile:
      return generateTernaryNode("FileNode", Nd);
    case OpFileHeader:
      return generateNaryNode("FileHeaderNode", Nd);
    case OpIfThen:
      return generateBinaryNode("IfThenNode", Nd);
    case OpIfThenElse:
      return generateTernaryNode("IfThenElseNode", Nd);
    case OpI32Const:
      return generateIntegerNode("I32Const", cast<I32ConstNode>(Nd));
    case OpI64Const:
      return generateIntegerNode("I64Const", cast<I64ConstNode>(Nd));
    case OpLastRead:
      return generateNullaryNode("LastReadNode", Nd);
    case OpLastSymbolIs:
      return generateUnaryNode("LastSymbolIsNode", Nd);
    case OpLiteralActionDef:
      return generateBinaryNode("LiteralActionDefNode", Nd);
    case OpLiteralActionUse:
      return generateUnaryNode("LiteralActionUseNode", Nd);
    case OpLiteralDef:
      return generateBinaryNode("LiteralDefNode", Nd);
    case OpLiteralUse:
      return generateUnaryNode("LiteralUseNode", Nd);
    case OpLocal:
      return generateIntegerNode("Local", cast<LocalNode>(Nd));
    case OpLocals:
      return generateIntegerNode("Locals", cast<LocalNode>(Nd));
    case OpLoop:
      return generateBinaryNode("LoopNode", Nd);
    case OpLoopUnbounded:
      return generateUnaryNode("LoopUnboundedNode", Nd);
    case OpMap:
      return generateNaryNode("MapNode", Nd);
    case OpNot:
      return generateUnaryNode("NotNode", Nd);
    case OpOpcode:
      return generateNaryNode("OpcodeNode", Nd);
    case OpOr:
      return generateBinaryNode("OrNode", Nd);
    case OpParam:
      return generateIntegerNode("Param", cast<ParamsNode>(Nd));
    case OpParams:
      return generateIntegerNode("Params", cast<ParamsNode>(Nd));
    case OpPeek:
      return generateUnaryNode("PeekNode", Nd);
    case OpRead:
      return generateUnaryNode("ReadNode", Nd);
    case OpRename:
      return generateBinaryNode("RenameNode", Nd);
    case OpSection:
      return generateNaryNode("SectionNode", Nd);
    case OpSequence:
      return generateNaryNode("SequenceNode", Nd);
    case OpSet:
      return generateBinaryNode("SetNode", Nd);
    case OpSymbol:
      return generateSymbol(cast<SymbolNode>(Nd));
    case OpSwitch:
      return generateNaryNode("SwitchNode", Nd);
    case OpUint8:
      return generateNullaryNode("Uint8Node", Nd);
    case OpUint32:
      return generateNullaryNode("Uint32Node", Nd);
    case OpUint64:
      return generateNullaryNode("Uint64Node", Nd);
    case OpUndefine:
      return generateUnaryNode("UndefineNode", Nd);
    case OpU8Const:
      return generateIntegerNode("U8Const", cast<U8ConstNode>(Nd));
    case OpU32Const:
      return generateIntegerNode("U32Const", cast<U32ConstNode>(Nd));
    case OpU64Const:
      return generateIntegerNode("U64Const", cast<U64ConstNode>(Nd));
    case OpVarint32:
      return generateNullaryNode("Varint32Node", Nd);
    case OpVarint64:
      return generateNullaryNode("Varint64Node", Nd);
    case OpVaruint32:
      return generateNullaryNode("Varuint32Node", Nd);
    case OpVaruint64:
      return generateNullaryNode("Varuint64Node", Nd);
    case OpVoid:
      return generateNullaryNode("VoidNode", Nd);
    case OpWrite:
      return generateNaryNode("WriteNode", Nd);
  }
  WASM_RETURN_UNREACHABLE(0);
}

void CodeGenerator::generateDeclFile() {
  generateHeader();
  generateEnterNamespaces();
  generatePredefinedEnum();
  generateAlgorithmHeader();
  puts(";\n\n");
  generateExitNamespaces();
}

#if WASM_CAST_BOOT > 1
void CodeGenerator::generateArrayImplFile() {
  BufferType Buffer;
  constexpr size_t BytesPerLine = 15;
  puts("static const uint8_t ");
  generateArrayName();
  puts("[] = {\n");
  while (!ReadPos->atEof()) {
    uint8_t Byte = ReadPos->readByte();
    size_t Address = ReadPos->getCurAddress();
    if (Address > 0 && Address % BytesPerLine == 0)
      putc('\n');
    bool IsPrintable = std::isalnum(Byte);
    switch (Byte) {
      case ' ':
      case '!':
      case '@':
      case '#':
      case '$':
      case '%':
      case '^':
      case '&':
      case '*':
      case '(':
      case ')':
      case '_':
      case '-':
      case '+':
      case '=':
      case '{':
      case '[':
      case '}':
      case ']':
      case '|':
      case ':':
      case ';':
      case '<':
      case ',':
      case '>':
      case '.':
      case '?':
      case '/':
        IsPrintable = true;
      default:
        break;
    }
    if (IsPrintable)
      sprintf(Buffer, " '%c'", Byte);
    else
      sprintf(Buffer, " %u", Byte);
    puts(Buffer);
    if (!ReadPos->atEof())
      putc(',');
  }
  puts(
      "};\n"
      "\n"
      "}  // end of anonymous namespace\n"
      "\n");
  generateAlgorithmHeader();
  puts(
      " {\n"
      "  static std::shared_ptr<SymbolTable> Symtable;\n"
      "  if (Symtable)\n"
      "    return Symtable;\n"
      "  auto ArrayInput = std::make_shared<ArrayReader>(\n"
      "    ");
  generateArrayName();
  puts(", size(");
  generateArrayName();
  puts(
      "));\n"
      "  auto Input = std::make_shared<ReadBackedQueue>(ArrayInput);\n"
      "  CasmReader Reader;\n"
      "  Reader.readBinary(Input);\n"
      "  assert(!Reader.hasErrors());\n"
      "  Symtable = Reader.getReadSymtab();\n"
      "  return Symtable;\n");
  generateFunctionFooter();
}
#endif

void CodeGenerator::generateFunctionImplFile() {
  size_t Index = generateNode(Symtab->getInstalledRoot());
  puts(
      "}  // end of anonymous namespace\n"
      "\n");
  generateAlgorithmHeader();
  puts(
      " {\n"
      "  static std::shared_ptr<SymbolTable> Symtable;\n"
      "  if (Symtable)\n"
      "    return Symtable;\n"
      "  Symtable = std::make_shared<SymbolTable>();\n"
      "  SymbolTable* Symtab = Symtable.get();\n"
      "  Symtab->install(");
  generateFunctionCall(Index);
  puts(
      ");\n"
      "  return Symtable;\n");
  generateFunctionFooter();
}

void CodeGenerator::generateImplFile(bool UseArrayImpl) {
  generateHeader();
  if (UseArrayImpl)
    puts(
        "#include \"casm/CasmReader.h\"\n"
        "#include \"stream/ArrayReader.h\"\n"
        "#include \"stream/ReadBackedQueue.h\"\n"
        "\n"
        "#include <cassert>\n"
        "\n");
  generateEnterNamespaces();
  // Note: We don't know the include path for the enum, so just repeat
  // generating it.
  generatePredefinedEnum();
  puts(
      "using namespace wasm::filt;\n"
      "\n"
      "namespace {\n"
      "\n");
  generatePredefinedEnumNames();
#if WASM_CAST_BOOT > 1
  if (UseArrayImpl)
    generateArrayImplFile();
  else
#endif
    generateFunctionImplFile();
  generatePredefinedNameFcn();
  generateExitNamespaces();
}

std::shared_ptr<SymbolTable> readCasmFile(const char* Filename,
                                          bool TraceLexer,
                                          bool TraceParser) {
  bool HasErrors = false;
  std::shared_ptr<SymbolTable> Symtab;
#if WASM_CAST_BOOT == 1
  Symtab = std::make_shared<SymbolTable>();
  Driver Parser(Symtab);
  Parser.setTraceLexing(TraceLexer);
  Parser.setTraceParsing(TraceParser);
  HasErrors = !Parser.parse(Filename);
#else
  CasmReader Reader;
  Reader.setTraceRead(TraceParser).setTraceLexer(TraceLexer).readText(Filename);
  Symtab = Reader.getReadSymtab();
  HasErrors = Reader.hasErrors();
#endif
  if (HasErrors)
    Symtab.reset();
  return Symtab;
}

}  // end of anonymous namespace

}  // end of namespace wasm;

using namespace wasm;
using namespace wasm::decode;
using namespace wasm::filt;
using namespace wasm::utils;

int main(int Argc, charstring Argv[]) {
  charstring AlgorithmFilename = nullptr;
  charstring FunctionName = nullptr;
  charstring OutputFilename = "-";
  charstring InputFilename = "-";
  std::set<std::string> KeepActions;
  bool ShowSavedCast = false;
  bool StripActions = false;
  bool StripAll = false;
  bool StripLiterals = false;
  bool StripLiteralDefs = false;
  bool StripLiteralUses = false;
  bool TraceAlgorithm = false;
  bool TraceInputTree = false;
  bool TraceLexer = false;
  bool TraceParser = false;
  bool Verbose = false;
  bool HeaderFile = false;

#if WASM_CAST_BOOT > 1
  bool BitCompress = true;
  bool MinimizeBlockSize = false;
  bool TraceFlatten = false;
  bool TraceWrite = false;
  bool TraceTree = false;
  bool UseArrayImpl = false;
#endif

  {
    ArgsParser Args("Converts compression algorithm from text to binary");

    ArgsParser::Optional<charstring> AlgorithmFlag(AlgorithmFilename);
    Args.add(AlgorithmFlag.setShortName('a')
                 .setLongName("algorithm")
                 .setOptionName("ALGORITHM")
                 .setDescription(
                     "Use algorithm in ALGORITHM file "
                     "to parse text file"));

    ArgsParser::Optional<bool> ExpectFailFlag(ExpectExitFail);
    Args.add(ExpectFailFlag.setDefault(false)

                 .setLongName("expect-fail")
                 .setDescription("Succeed on failure/fail on success"));

    ArgsParser::Optional<charstring> FunctionNameFlag(FunctionName);
    Args.add(FunctionNameFlag.setShortName('f')
                 .setLongName("function")
                 .setOptionName("NAME")
                 .setDescription(
                     "Generate c++ source code to implement a function "
                     "'void NAME(std::shared_ptr<SymbolTable>) to install "
                     "the INPUT cast algorithm"));

    ArgsParser::Optional<bool> HeaderFileFlag(HeaderFile);
    Args.add(HeaderFileFlag.setLongName("header").setDescription(
        "Generate header version of c++ source instead "
        "of implementatoin file (only applies when "
        "'--function Name' is specified)"));

    ArgsParser::Required<charstring> InputFlag(InputFilename);
    Args.add(InputFlag.setOptionName("INPUT")
                 .setDescription("Text file to convert to binary"));

    ArgsParser::RepeatableSet<std::string> KeepActionsFlag(KeepActions);
    Args.add(
        KeepActionsFlag.setLongName("keep")
            .setOptionName("ACTION")
            .setDescription("Don't strip callbacks on ACTION from the input"));

    ArgsParser::Optional<charstring> OutputFlag(OutputFilename);
    Args.add(OutputFlag.setShortName('o')
                 .setLongName("output")
                 .setOptionName("OUTPUT")
                 .setDescription("Generated binary file"));

    ArgsParser::Optional<bool> ShowSavedCastFlag(ShowSavedCast);
    Args.add(ShowSavedCastFlag.setLongName("cast")
                 .setDescription("Show cast text being written"));

    ArgsParser::Optional<bool> StripActionsFlag(StripActions);
    Args.add(StripActionsFlag.setLongName("strip-actions")
                 .setDescription("Remove callback actions from input"));

    ArgsParser::Optional<bool> StripAllFlag(StripAll);
    Args.add(StripAllFlag.setShortName('s').setLongName("strip").setDescription(
        "Apply all strip actions to input"));

    ArgsParser::Optional<bool> StripLiteralsFlag(StripLiterals);
    Args.add(StripLiteralsFlag.setLongName("strip-literals")
                 .setDescription(
                     "Replace literal uses with their definition, then "
                     "remove unreferenced literal definitions from the input"));

    ArgsParser::Optional<bool> StripLiteralDefsFlag(StripLiteralDefs);
    Args.add(StripLiteralDefsFlag.setLongName("strip-literal-defs")
                 .setDescription(
                     "Remove unreferenced literal definitions from "
                     "the input"));

    ArgsParser::Optional<bool> StripLiteralUsesFlag(StripLiteralUses);
    Args.add(StripLiteralUsesFlag.setLongName("strip-literal-uses")
                 .setDescription("Replace literal uses with their defintion"));

    ArgsParser::Optional<bool> TraceInputTreeFlag(TraceInputTree);
    Args.add(TraceInputTreeFlag.setLongName("verbose=input")
                 .setDescription("Show generated AST from reading input"));

    ArgsParser::Optional<bool> TraceLexerFlag(TraceLexer);
    Args.add(
        TraceLexerFlag.setLongName("verbose=lexer")
            .setDescription("Show lexing of algorithm (defined by option -a)"));

    ArgsParser::Optional<bool> TraceParserFlag(TraceParser);
    Args.add(TraceParserFlag.setLongName("verbose=parser")
                 .setDescription(
                     "Show parsing of algorithm (defined by option -a)"));

    ArgsParser::Toggle VerboseFlag(Verbose);
    Args.add(
        VerboseFlag.setShortName('v').setLongName("verbose").setDescription(
            "Show progress and tree written to binary file"));

#if WASM_CAST_BOOT > 1

    ArgsParser::Optional<bool> BitCompressFlag(BitCompress);
    Args.add(BitCompressFlag.setLongName("bit-compress")
                 .setDescription(
                     "Perform bit compresssion on binary opcode expressions"));

    ArgsParser::Toggle MinimizeBlockFlag(MinimizeBlockSize);
    Args.add(MinimizeBlockFlag.setDefault(true)
                 .setShortName('m')
                 .setLongName("minimize")
                 .setDescription(
                     "Minimize size in binary file "
                     "(note: runs slower)"));

    ArgsParser::Optional<bool> TraceAlgorithmFlag(TraceAlgorithm);
    Args.add(
        TraceAlgorithmFlag.setLongName("verbose=algorithm")
            .setDescription("Show algorithm used to generate compressed file"));

    ArgsParser::Optional<bool> TraceFlattenFlag(TraceFlatten);
    Args.add(TraceFlattenFlag.setLongName("verbose=flatten")
                 .setDescription("Show how algorithms are flattened"));

    ArgsParser::Optional<bool> TraceWriteFlag(TraceWrite);
    Args.add(TraceWriteFlag.setLongName("verbose=write")
                 .setDescription("Show how binary file is encoded"));

    ArgsParser::Optional<bool> TraceTreeFlag(TraceTree);
    Args.add(TraceTreeFlag.setLongName("verbose=tree")
                 .setDescription(
                     "Show tree being written while writing "
                     "(implies --verbose=write)"));

    ArgsParser::Optional<bool> UseArrayImplFlag(UseArrayImpl);
    Args.add(UseArrayImplFlag.setLongName("array").setDescription(
        "Internally implement function NAME() using an "
        "array implementation, rather than the default that "
        "uses direct code"));

#endif

    switch (Args.parse(Argc, Argv)) {
      case ArgsParser::State::Good:
        break;
      case ArgsParser::State::Usage:
        return exit_status(EXIT_SUCCESS);
      default:
        fprintf(stderr, "Unable to parse command line arguments!\n");
        return exit_status(EXIT_FAILURE);
    }

    if (StripAll) {
      StripActions = true;
      StripLiterals = true;
    }

#if WASM_CAST_BOOT > 1
    // Be sure to update implications!
    if (TraceTree)
      TraceWrite = true;
    // TODO(karlschimpf) Extend ArgsParser to be able to return option
    // name so that we don't have hard-coded dependency.
    if (UseArrayImpl && FunctionName == nullptr) {
      fprintf(stderr, "Option --array can't be used without option -f\n");
      return exit_status(EXIT_FAILURE);
    }

    if (UseArrayImpl && HeaderFile) {
      fprintf(stderr, "Opition --array can't be used with option --header\n");
      return exit_status(EXIT_FAILURE);
    }
#else
    assert(AlgorithmFilename != nullptr);
#endif
  }

  if (Verbose)
    fprintf(stderr, "Reading input: %s\n", InputFilename);
  std::shared_ptr<SymbolTable> InputSymtab =
      readCasmFile(InputFilename, TraceLexer, TraceParser);
  if (!InputSymtab) {
    fprintf(stderr, "Unable to parse: %s\n", InputFilename);
    return exit_status(EXIT_FAILURE);
  }
  if (StripActions)
    InputSymtab->stripCallbacksExcept(KeepActions);
  // Note: Must run after StripActions to guarantee that literal defintions
  // associated with stripped actions will also be removed.
  if (StripLiteralUses)
    InputSymtab->stripLiteralUses();
  if (StripLiteralDefs)
    InputSymtab->stripLiteralDefs();
  if (StripLiterals)
    InputSymtab->stripLiterals();
  if (TraceInputTree) {
    TextWriter Writer;
    Writer.write(stderr, InputSymtab.get());
  }
  if (Verbose) {
    if (AlgorithmFilename)
      fprintf(stderr, "Reading algorithms file: %s\n", AlgorithmFilename);
    else
      fprintf(stderr, "Using prebuilt casm algorithm\n");
  }
  std::shared_ptr<SymbolTable> AlgSymtab;
  if (AlgorithmFilename) {
    AlgSymtab = readCasmFile(AlgorithmFilename, TraceLexer, TraceParser);
    if (!AlgSymtab) {
      fprintf(stderr, "Problems reading file: %s\n", InputFilename);
      return exit_status(EXIT_FAILURE);
    }
#if WASM_CAST_BOOT > 1
  } else {
    AlgSymtab = getAlgcasm0x0Symtab();
#endif
  }

  if (TraceAlgorithm) {
    TextWriter Writer;
    Writer.write(stderr, AlgSymtab.get());
  }

  if (ShowSavedCast)
    InputSymtab->describe(stderr);

  if (Verbose && strcmp(OutputFilename, "-") != 0)
    fprintf(stderr, "Opening file: %s\n", OutputFilename);
  auto Output = std::make_shared<FileWriter>(OutputFilename);
  if (Output->hasErrors()) {
    fprintf(stderr, "Problems opening output file: %s", OutputFilename);
    return exit_status(EXIT_FAILURE);
  }

  std::shared_ptr<Queue> OutputStream;
  std::shared_ptr<ReadCursor> OutputStartPos;
  if (FunctionName != nullptr) {
#if WASM_CAST_BOOT > 1
    if (UseArrayImpl) {
      OutputStream = std::make_shared<Queue>();
      OutputStartPos =
          std::make_shared<ReadCursor>(StreamType::Byte, OutputStream);
    }
#endif
  } else {
    OutputStream = std::make_shared<WriteBackedQueue>(Output);
  }

#if WASM_CAST_BOOT > 1
  if (OutputStream) {
    // Generate binary stream
    CasmWriter Writer;
    Writer.setTraceWriter(TraceWrite)
        .setTraceFlatten(TraceFlatten)
        .setTraceTree(TraceTree)
        .setMinimizeBlockSize(MinimizeBlockSize)
        .setBitCompress(BitCompress);
    Writer.writeBinary(InputSymtab, OutputStream, AlgSymtab);
    if (Writer.hasErrors()) {
      fprintf(stderr, "Problems writing: %s\n", OutputFilename);
      return exit_status(EXIT_FAILURE);
    }
  }
#endif

  if (FunctionName == nullptr)
    return exit_status(EXIT_SUCCESS);

  // Generate C++ code.
  std::vector<charstring> Namespaces;
  Namespaces.push_back("wasm");
  Namespaces.push_back("decode");
  CodeGenerator Generator(InputFilename, Output, InputSymtab, Namespaces,
                          FunctionName);
  if (HeaderFile)
    Generator.generateDeclFile();
  else {
#if WASM_CAST_BOOT > 1
    if (UseArrayImpl)
      Generator.setStartPos(OutputStartPos);
#else
    bool UseArrayImpl = false;
#endif
    Generator.generateImplFile(UseArrayImpl);
  }
  if (Generator.foundErrors()) {
    fprintf(stderr, "Unable to generate valid C++ source!\n");
    return exit_status(EXIT_FAILURE);
  }
  return exit_status(EXIT_SUCCESS);
}
