You are assisting with the cz compiler project, a C-like language compiler implemented in C using LLVM.

Key project information:
- Overview: See README.md
- Development guidance: See CLAUDE.md
- Language specification: See language_summary.md
- Formal grammar: See notes/bnf.txt

When working on this project:
1. Follow the coding style and conventions outlined in CLAUDE.md
2. Ensure changes align with the language specification
3. Run tests with `make unittest` before considering changes complete
4. The compiler entry point is src/czc.c
5. Build the compiler with `make czc` or simply `make`

Common tasks:
- Adding new language features: Update lexer, parser, AST, semantic analysis, and code generator as described in CLAUDE.md
- Debugging: Use the compiler to print AST or LLVM IR as needed
- Memory checking: Use valgrind with `./bin/czc <file>`
