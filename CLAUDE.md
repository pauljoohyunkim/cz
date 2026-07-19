# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is "cz" - a simple C-like programming language compiler implemented in C. The compiler uses LLVM for code generation and Google Test for unit testing.

## Project Structure

- `src/` - Contains the compiler source code:
  - `cz_lexer.c` - Lexical analyzer
  - `cz_parser.c` - Parser
  - `cz_semantic_analyzer.c` - Semantic analyzer
  - `cz_ast.c` - Abstract Syntax Tree implementation
  - `czc.c` - Main compiler driver
  - Other supporting modules (tokens, types, symbols, error handling)

- `include/` - Header files corresponding to the source files

- `tests/` - Unit tests using Google Test framework:
  - `unittest.cpp` - Test runner
  - `lexertest.cpp` - Lexer tests
  - `parsertest.cpp` - Parser tests
  - `czc_test_code.cz` - Sample cz language test file

- `obj/` - Object files generated during build (gitignored)
- `bin/` - Executables generated during build (gitignored)

## Development Commands

### Building the Compiler
```bash
# Build the compiler executable
make czc
# or simply
make

# Build and run unit tests
make unittest
```

### Running Tests
```bash
# Run unit tests after building
./bin/unittest

# Run the compiler on a test file
./bin/czc tests/czc_test_code.cz output.o
```

### Cleaning Build Artifacts
```bash
make clean
```

### Code Generation
The compiler uses LLVM for code generation. To see the LLVM IR output:
```bash
./bin/czc tests/czc_test_code.cz -  # Outputs LLVM IR to stdout
```

To generate an object file:
```bash
./bin/czc tests/czc_test_code.cz output.o
```

## Key Architecture Points

1. **Compiler Pipeline**: The compiler follows a traditional pipeline:
   - Lexer (`cz_lexer.c`) → Parser (`cz_parser.c`) → Semantic Analyzer (`cz_semantic_analyzer.c`) → Code Generator (commented out in `czc.c`)

2. **Data Structures**:
   - Abstract Syntax Tree (AST) nodes defined in `cz_ast.h`
   - Symbol table in `cz_symbol_table.h`
   - Type system in `cz_type.h`

3. **Error Handling**: Error reporting is handled through error lists in each component (`cz_error.h`)

4. **Testing Approach**:
   - Unit tests use Google Test framework
   - Lexer and parser have dedicated test files
   - Integration tests can be done by compiling sample `.cz` files

## Common Development Tasks

### Adding New Language Features
1. Update lexer tokens in `cz_lexer.h` and `cz_lexer.c` if needed
2. Update grammar rules in `cz_parser.y` (implicit in parser code)
3. Update AST node types in `cz_ast.h` if needed
4. Add semantic analysis rules in `cz_semantic_analyzer.c`
5. Extend code generation in `cz_code_generator.c` (when implemented)
6. Add tests in appropriate test files

### Debugging
- The compiler uses `-g` flag for debug information in Makefile
- Error messages include line and column numbers
- AST can be printed using `cz_ast_root_print()` function (called in `czc.c`)

### Memory Leak Detection
To check for memory leaks when running the compiler:
```bash
valgrind --leak-check=yes ./bin/czc tests/czc_test_code.cz
```

### Testing Best Practices
- Unit tests should cover lexer, parser, and semantic analysis individually
- Integration tests should use sample `.cz` files in the tests directory
- When fixing bugs, add a test case that reproduces the issue first

## Code Style
- Follows typical C99 style with clear function separation
- Error handling uses goto cleanup pattern
- Memory management uses explicit malloc/free pairs
- Header files use include guards