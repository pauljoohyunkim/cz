# cz Project Progress Log

## Project Initiation
- Initial commit (b68c37a): Set up repository, basic directory structure.

## Early Development (July 2026)
- Implemented core type system: Global Type Table (GTT), type representation, lookup.
- Built semantic analyzer passes: declaration processing, expression checking, symbol table management.
- Added string pool for identifier interning.
- AST node improvements and memory fixes.
- Implemented basic declarations: variables, functions, structs, typedefs, newtypes.
- Added control flow statement checking (if, while, for).
- Implemented expression checking: binary/unary operations, assignments, cast expressions.
- Added reference and const correctness rules (escapability, initialization).
- Struct layout and member access validation.
- Function call checking (lvalue/rvalue, constexpr).
- Return statement checks (non‑void functions must return).

## Mid Development (July‑August 2026)
- Code generation prototypes: function declarations, global variables, struct initialization.
- Assignment statement generation (including compound assignments).
- Binary and unary operation code generation.
- Struct member access code generation.
- Function call code generation (argument passing, return value handling).
- Return statement generation.
- Constant expression handling and compile‑time evaluation.
- Type decay and reference handling in code generation.
- Added uint32 type support (semantic analysis, code generation).
- Array and list type parsing and AST node fixes.
- Work on const/reference array creation and type node ordering.

## Recent Work (August 2026)
- Reordered type node → cz_type creation to gracefully create const/reference arrays.
- Added STOP_AT_* ifdefs to czc.c and VS Code configurations for debugging.
- Improved internal type parsing (cz_type_from_type_node).
- Documentation updates: renamed reference_const.txt to cz_type_notes.md, updated CZ_TYPE syntax rules.
- Multiple merge requests: uint32 feature, bugfixes (return paths), dev branch updates, code generation improvements.

## Branching Strategy
- `main`: stable releases
- `dev`: active development branch
- Feature branches: `feature/*` (e.g., feature/uint32)
- Bugfix branches: `bugfix/*` (e.g., bugfix/return-paths)
- Other prefixes: `cg-*` (code generation), `sa-*` (semantic analyzer), etc.

## Next Steps
- Complete array and list type support with references.
- Finalize code generation for all language features.
- Run comprehensive unit and integration tests.
- Prepare for a stable release on `main`.
