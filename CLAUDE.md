# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is "cz" — a simple, C-like programming language compiler implemented in C. It uses LLVM for code generation and Google Test for testing. **Key design goals:**

- Static type system (`int32`, `float`, `bool`, `void`)
- Strict lvalue/rvalue enforcement (value categories)
- Const correctness and reference semantics
- User-defined types (struct, typedef, newtype)
- Global-only declarations at file scope
- Structured initialization with designated fields

## Project Structure

```
cz/
├── src/                  # Compiler source code
│   ├── czc.c             # Main driver / entry point (compilation pipeline orchestration)
│   ├── cz_lexer.c        # Lexical analysis (tokenizer)
│   ├── cz_parser.c       # Recursive-descent parser + Pratt-style expression parsing
│   ├── cz_semantic_analyzer.c  # Type checking, const/reference rules, scope resolution
│   ├── cz_code_generator.c   # LLVM IR code generation (fully implemented)
│   ├── cz_ast.c          # AST tree implementation & printing
│   ├── cz_type.c         # Type system representation (CZ_GlobalTypeTable)
│   ├── cz_symbol_table.c     # Scope / symbol table management
│   └── cz_tokens.c       # Token definitions & helpers
├── include/              # Header files
│   ├── cz_ast.h          # AST node types, decorations, and helper functions
│   ├── cz_lexer.h        # Lexer interface
│   ├── cz_parser.h       # Parser interface
│   ├── cz_semantic_analyzer.h  # Semantic analyzer declarations
│   ├── cz_code_generator.h     # Code generator declarations
│   ├── cz_type.h         # Type system types (CZ_GlobalTypeTable)
│   ├── cz_symbol_table.h     # Symbol/scope table interface
│   ├── cz_tokens.h       # Token type enum/constants
│   └── cz_error.h        # Error reporting utilities
├── tests/                # Unit & integration tests
│   ├── unittest.cpp      # Test runner (Google Test)
│   ├── lexertest.cpp     # Lexer unit tests
│   ├── parsertest.cpp    # Parser unit tests
│   ├── semanticanalyzertest.cpp  # Semantic analysis tests
│   └── czc_test_code.cz  # Sample cz source file for integration testing
├── obj/                  # Object files (gitignored, build output)
├── bin/                  # Executables (gitignored, build output)
├── notes/                # Design documentation
│   ├── bnf.txt           # Formal BNF grammar for cz
│   └── referece_const.txt    # Notes on const & reference semantics
├── doc/
│   └── SPECIFICATION.md  # Comprehensive language specification v1.0
├── Makefile              # Build system (clang + LLVM + gtest)
├── README.md             # Project overview and quick start
└── language_summary.md   # Detailed language reference and semantics
```

## Compiler Architecture

The compiler follows a classic multi-stage pipeline:

### Stage 1 — Lexer (`src/cz_lexer.c`)
- Tokenizes `.cz` source files into tokens (identifiers, literals, operators, keywords)
- Uses `CZ_StringPool` for identifier interning (canonical symbol representation)
- Supports `//` line comments; string literals are lexed but not yet fully supported

### Stage 2 — Parser (`src/cz_parser.c`)
- Recursive-descent parser with Pratt-style expression parsing for precedence
- Constructs an AST covering: functions, structs, variables, typedefs, newtypes
- Global declarations at file scope; local variable declarations only inside blocks

### Stage 3 — Semantic Analyzer (`src/cz_semantic_analyzer.c`)
- Type checking, const/reference rules, and lvalue/rvalue enforcement
- Maintains `CZ_GlobalTypeTable` for type resolution via structural equality (with decay)
- Declares a global environment + per-function function-body scopes
- Enforces: constexpr requirements, reference lifetime safety, cast restrictions

### Stage 4 — Code Generator (`src/cz_code_generator.c`)
- Emits LLVM IR to an `LLVMModuleRef`
- Globals → external/static globals; functions → `LLVMFunctionType`; locals → `alloca`
- References compile to typed pointers with appropriate `const` semantics
- Structs → `LLVMStructType`; control flow → branches (`icmp`, `fcmp`) and terminators

### Entry Point (`src/czc.c`)
Main driver that orchestrates the pipeline:
```c
lexer → parser → semantic analyzer → code generator
        ↓ errors (stop)          ↓ errors (stop)     ↓ errors (warn/skip)
```

## Key Architecture Concepts

### Type System (`include/cz_type.h`)
- **Primitives**: `int32`, `float`, `bool`, `void`
- **Qualifiers**: `const` (on lvalues), `&` (references, compiled as pointers)
- **User-defined**: `struct` (aggregates with named fields), `typedef` (alias), `newtype` (distinct wrapper)
- **Decay rule**: Type comparisons strip outermost `const`/`&` to reach base type

### Lvalue vs Rvalue (`include/cz_ast.h`)
Every expression is decorated with a value category:
- **Lvalues**: Identifiable memory locations (variables, struct members, dereferenced references)
- **Rvalues**: Transient computational results (expressions, literals, unary ops always yield rvalues)

### Const Correctness
- `const` qualifies lvalues only; unary operators/arithmetics always produce non-const rvalues
- Casting away `const` via `as` is prohibited
- Non-const references require non-const, existing lvalue bindings

## Development Commands

### Building the Compiler
```bash
# Build the compiler executable
make czc

# Or simply (czc is the default target)
make
```

### Building and Running Tests
```bash
# Build and run all tests
make unittest
./bin/unittest
```

### Compiling a `.cz` Source File
```bash
# Compile to an object file
./bin/czc tests/czc_test_code.cz output.o

# Output LLVM IR to stdout
./bin/czc tests/czc_test_code.cz -
```

### Cleaning Build Artifacts
```bash
make clean
```

## Common Development Tasks

### Adding New Language Features
1. **Lexer**: Update token types in `include/cz_tokens.h` and lexing rules in `src/cz_lexer.c`
2. **Parser**: Add grammar rules to `src/cz_parser.c` (recursive-descent / Pratt)
3. **AST Nodes**: Extend AST node types in `include/cz_ast.h` if new node types are needed
4. **Type System**: Update `include/cz_type.h` / `src/cz_type.c` for new type kinds
5. **Semantic Analysis**: Add rules to `src/cz_semantic_analyzer.c` for type checking & validation
6. **Code Generation**: Extend `src/cz_code_generator.c` to emit LLVM IR for the new feature
7. **Tests**: Update corresponding test file(s) in `tests/`

### Debugging the Compiler
```bash
# Print parsed AST to stdout (already done in czc.c by default)
./bin/czc src.c output.o

# Print the semantic environment / symbol table (already printed in czc.c)
./bin/czc tests/czc_test_code.cz -            # + LLVM IR dump via LLVMDumpModule()
```

Error messages include **line and column numbers** throughout all stages.

### Memory Leak Detection
```bash
valgrind --leak-check=full --show-leak-kinds=all ./bin/czc tests/czc_test_code.cz
```

## Testing Guidelines

- **Unit tests** in `tests/*.cpp` use Google Test framework (lexer, parser, semantic analyzer)
- **Integration testing**: Compile sample `.cz` files and inspect LLVM IR output
- When fixing bugs or adding features, add a test case that reproduces the issue first
- The existing `tests/czc_test_code.cz` provides a running example of the language

## Code Style & Conventions

- C99 with clear function separation; errors use goto cleanup pattern in driver
- Explicit malloc/free memory management throughout (`cz_*_free()` functions)
- Header include guards on all `.h` files
- Symbol table / environment uses `CZ_Environment` struct for scopes
- Type equality via `cz_type_equals()` which performs structural comparison after decay
- All source objects tracked as `.o` in `obj/`; dependencies auto-generated via `-MMD -MP`

## Configuration & Build Notes

### Compiler Flags (Makefile)
- `clang` / `clang++` for compilation
- `-g -Wall -O0` for debugging; `-I$(INCLUDE)` for headers
- LLVM linked via `llvm-config --cflags` / `llvm-config --libs`

### Dependencies
- **LLVM** — Required for code generation (linked libraries)
- **Google Test** — Requirement for unit tests (`pkg-config gtest`)
- **clang/clang++** — Compiler toolchain

## Important Files Quick Reference

| File | Purpose |
|------|---------|
| `src/cz_lexer.c` + `include/cz_lexer.h` / `cz_tokens.h` | Tokenizer & token definitions |
| `src/cz_parser.c` + `include/cz_parser.h` | Parser grammar & interface |
| `include/cz_ast.h` | AST node types, decorations (value category, scope level) |
| `src/cz_semantic_analyzer.c` + `include/cz_semantic_analyzer.h` | Type checking, const/reference enforcement |
| `include/cz_type.h` | Type system (`CZ_GlobalTypeTable`, type kinds, decay rules) |
| `include/cz_symbol_table.h` | Symbol and environment table interface |
| `src/cz_code_generator.c` + `include/cz_code_generator.h` | LLVM IR code emission |
| `doc/SPECIFICATION.md` | Complete language specification (v1.0) |
| `language_summary.md` | Detailed language reference & semantics |
| `notes/bnf.txt` | Formal BNF grammar |

## References for Learning the Codebase

- **Start here**: `src/czc.c` to understand pipeline flow, then `include/cz_ast.h` for AST types
- **Lexer**: `src/cz_lexer.c` → token stream production, `CZ_StringPool` for identifiers
- **Parser**: `src/cz_parser.c` → recursive-descent with Pratt-style precedence
- **Semantics**: `src/cz_semantic_analyzer.c` → type checking, scope lookup in `cz_global_type_table.h` (via `include/cz_type.h`)
- **Code gen**: `src/cz_code_generator.c` → LLVM IR emission (functions, structs, allocas)

---
*This CLAUDE.md reflects the current state of the repository as of the latest updates.*
