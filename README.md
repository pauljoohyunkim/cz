# cz

A programming language attempting to be as simple as C (and be minimal) but still free from some of common programming mishaps.

## Overview

`cz` is a small, C-like programming language implemented in C, designed for educational purposes. It features:

- A static type system with primitives (`int32`, `bool`, `float`, `void`)
- User-defined types (`struct`, `typedef`, `newtype`)
- Type qualifiers (`const`, references `&`)
- Function definitions with parameter lists and return types
- Control flow (`if/else`, `while`, `for`)
- Arithmetic, bitwise, relational, and equality operators
- Struct member access and initialization
- Cast expressions (`as`)
- Global declarations only (file-scope)

The compiler pipeline follows the classic model: **Lexer → Recursive-Descent Parser → Semantic Analyzer → LLVM IR Code Generator**. The implementation is in C, with Google Test for unit tests.

---

## Features

### Types
| Type     | Description                       |
|----------|-----------------------------------|
| `int32`  | Signed 32-bit integer            |
| `bool`   | Boolean (`true`, `false`)        |
| `float`  | IEEE-754 single-precision float  |
| `void`   | Absence of value (function return only) |

### Type Qualifiers & References
- **`const`** – Read-only qualifier on any type.
- **`&`** – Reference types (must be initialized, non-reseatable).
- Combined: `const int32`, `int32&`, `const int32&`.

### User-Defined Types
| Keyword   | Description                                           |
|-----------|-------------------------------------------------------|
| `struct`  | Structured aggregate of named fields                  |
| `typedef` | Type alias (no new type, used interchangeably)       |
| `newtype` | Distinct, non-interchangeable type wrapper           |

### Keywords
```
func   struct  typedef  newtype  const    if       else     while
for    return  as       =        += -= *= /= %= &= |= ^=
true   false   &        |        ^        == != < > <= >= !
.      ::      ,        ;        (        )        [        ]
{      }       ->       
```

---

## Compiler Pipeline

| Stage                | File                        | Description                                           |
|----------------------|-----------------------------|-------------------------------------------------------|
| **Lexer**            | `src/cz_lexer.c`            | Tokenizes source code (identifiers, operators, literals, etc.) |
| **Parser**           | `src/cz_parser.c`           | Recursive-descent parser; constructs the AST              |
| **Semantic Analyzer**| `src/cz_semantic_analyzer.c`| Type checking, const/reference checks, scope resolution  |
| **Code Generator**   | `src/cz_code_generator.c`   | Translates annotated AST to LLVM IR                      |

---

## Project Structure

```
cz/
├── src/                    # Compiler source code
│   ├── czc.c               # Main driver / entry point
│   ├── cz_lexer.c          # Lexical analysis
│   ├── cz_parser.c         # Parsing (recursive-descent + Pratt-style)
│   ├── cz_semantic_analyzer.c  # Semantic analysis & type checking
│   ├── cz_code_generator.c     # LLVM IR code generation
│   ├── cz_ast.c           # Abstract Syntax Tree implementation
│   ├── cz_type.c          # Type system representation
│   ├── cz_symbol_table.c  # Scope / symbol table management
│   ├── cz_tokens.c        # Token definitions & helpers
│   ├── cz_error.c         # Error reporting
│   └── .gitkeep           # Placeholder for git tracking
├── include/                # Header files
│   ├── cz_ast.h            # AST node types & helper functions
│   ├── cz_lexer.h          # Lexer token definitions
│   ├── cz_parser.h         # Parser API
│   ├── cz_semantic_analyzer.h  # Semantic analysis declarations
│   ├── cz_code_generator.h     # Code generator declarations
│   ├── cz_type.h           # Type system types
│   ├── cz_symbol_table.h   # Symbol table interface
│   ├── cz_tokens.h         # Token type enum/constants
│   ├── cz_error.h          # Error handling utilities
├── tests/                  # Unit & integration tests
│   ├── unittest.cpp        # Test runner
│   ├── lexertest.cpp       # Lexer unit tests (Google Test)
│   ├── parsertest.cpp      # Parser unit tests (Google Test)
│   ├── semanticanalyzertest.cpp  # Semantic analysis tests
│   ├── czc_test_code.cz    # Sample cz source file
│   └── unittest_collection.txt    # Collection of test files/notes
├── bin/                    # Compiled executables (output dir, gitignored)
├── obj/                    # Object files (output dir, gitignored)
├── notes/                  # Design documentation & BNF grammar
│   ├── bnf.txt             # Formal BNF grammar for cz
│   └── referece_const.txt  # Notes on const & reference semantics
├── Makefile                # Build system (clang + LLVM + gtest)
├── CLAUDE.md               # Development guidance / architecture notes
├── language_summary.md     # Comprehensive language reference (detailed spec)
└── README.md               # This file
```

---

## Building & Running

### Building the Compiler
```bash
make czc      # Build the cz compiler
# or simply:
make          # Also builds czc as a default target
```

### Building & Running Unit Tests
```bash
make unittest   # Build and link tests
./bin/unittest  # Run all unit tests
```

### Compiling a `cz` Source File
```bash
# Compile to object file:
./bin/czc tests/czc_test_code.cz output.o

# Output LLVM IR to stdout:
./bin/czc tests/czc_test_code.cz -
```

### Cleaning Build Artifacts
```bash
make clean
```

---

## Example Code

A sample program in `cz` (`tests/czc_test_code.cz`):

```cz
struct Vector {
    x :: float;
    y :: float;
}

typedef int32 int;

n :: int = 1;
k :: float = 0.2;

i :: Vector = Vector {
    .x = 1.0
};
```

---

## Language Highlights

### Global Declarations Only
All declarations (`func`, `struct`, variables, `typedef`, `newtype`) are at **file scope**. Inside function bodies, only statements (including local variable declarations) are permitted.

### References
Reference types (`T&`) must be initialized upon declaration and cannot be rebound. They are compiled as LLVM pointers with appropriate `const` semantics.

```cz
i :: int32 = 10;
ref_i :: int32& = i;   // ref_i refers to i
```

### Constant Expressions
Certain initializers (global variables, struct members) must be **constant expressions** — evaluable at compile-time from literals, `const` variables, and constexpr-compatible operators.

---

## Key Files Quick Reference

| File                                | Purpose                                             |
|-------------------------------------|-----------------------------------------------------|
| `notes/bnf.txt`                     | Formal BNF grammar for the language                |
| `src/cz_parser.c`                   | Recursive-descent + Pratt-style expression parser   |
| `src/cz_semantic_analyzer.c`        | Semantic analysis, type checking                    |
| `include/cz_ast.h`                  | AST node definitions & decorations                 |
| `include/cz_type.h`                 | Type system types                                   |
| `include/cz_symbol_table.h`         | Symbol/scope table interface                       |
| `CLAUDE.md`                         | Detailed development guidance                      |
| `language_summary.md`               | Comprehensive language reference & spec             |

---

## Testing & Debugging

### Debugging the Compiler
```bash
# Print the parsed AST:
valgrind --leak-check=yes ./bin/czc tests/czc_test_code.cz
```

- Each error message includes **line and column numbers**.
- The `cz_ast_root_print()` function prints the full parsed tree for debugging.

### Memory Leak Detection
```bash
valgrind --leak-check=full --show-leak-kinds=all ./bin/czc tests/czc_test_code.cz
```

---

*This README captures the core project overview and build instructions. For a complete language specification, see `language_summary.md`.*
