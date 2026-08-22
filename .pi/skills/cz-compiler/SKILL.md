---
name: cz-compiler
description: Provides commands for building, testing, and using the cz compiler. Use for compiling .cz files, running tests, and debugging.
---

# Cz Compiler Skill

## Usage

### Build the compiler
```bash
make czc
```
or simply
```bash
make
```

### Build and run unit tests
```bash
make unittest
./bin/unittest
```

### Compile a .cz source file to an object file
```bash
./bin/czc <source_file.cz> <output.o>
```

### Output LLVM IR to stdout
```bash
./bin/czc <source_file.cz> -
```

### Clean build artifacts
```bash
make clean
```

### Debugging with Valgrind (memory leak check)
```bash
valgrind --leak-check=full --show-leak-kinds=all ./bin/czc <source_file.cz> -
```

### Print the parsed AST (default behavior of czc)
```bash
./bin/czc <source_file.cz> <output.o>   # AST printed to stderr
```

## References

- See `README.md` for project overview.
- See `language_summary.md` for complete language specification.
- See `notes/bnf.txt` for formal grammar.
- See `CLAUDE.md` for development guidance.