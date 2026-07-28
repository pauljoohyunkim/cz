# L-Value & R-Value Expression Node Classification
**Source File:** `src/cz_semantic_analyzer.c`

## Value Category Reference Table

| AST Node Type | Value Category | Condition / Logic | Related Check Function |
| :--- | :--- | :--- | :--- |
| `CZ_AST_IdentifierNodeType` | ✅ Conditional (L ↔ R) | **L-Value:** Resolves to a variable symbol (`const`, `constexpr`, or mutable).<br>**R-Value:** Resolves to a function type (`CZ_TYPE_KIND_FUNCTION`). | `cz_semantic_analyzer_check_identifier_expression` |
| `CZ_AST_StructMemberAccessNodeType` | ✅ Conditional (L ↔ R) | Directly inherits from the base expression's `value_category`. L-value base → L-value member. R-value base → R-value member. | `cz_semantic_analyzer_check_struct_access` |
| `CZ_AST_FunctionCallNodeType` | ✅ Conditional (L ↔ R) | **L-Value:** Return type is a reference (`CZ_TYPE_KIND_REFERENCE`).<br>**R-Value:** Normal functions return copies/temporaries. | `cz_semantic_analyzer_check_function_call_expression` |
| `CZ_AST_BinaryExpressionNodeType` | 🔒 R-Value (Always) | Computes a temporary result (`+`, `-`, `*`, `/`, comparisons, bitwise ops). Never assignable. | `cz_semantic_analyzer_check_binary_expression` |
| `CZ_AST_UnaryExpressionNodeType` | 🔒 R-Value (Always) | Computes a temporary result (`-`, `!`). Never assignable. | `cz_semantic_analyzer_check_unary_expression` |
| `CZ_AST_LiteralNodeType` | 🔒 R-Value (Always) | Compiles directly to constant values (`int32`, `float`, `true/false`). Treated as temporaries. | `cz_semantic_analyzer_check_literal_expression` |
| `CZ_AST_StructInitNodeType` | 🔒 R-Value (Always) | Creates a temporary struct instance (`{field: val}`). Hardcoded to R-value. | `cz_semantic_analyzer_check_struct_init` |
| `CZ_AST_CastExpressionNodeType` | 🔒 R-Value (Always) | Produces an implicit/temporary converted value. Casting to references is explicitly disabled in the analyzer. | `cz_semantic_analyzer_cast_expression` |

## How the Compiler Enforces These Categories

The semantic analyzer strictly checks `expr->decoration->value_category` during downstream pass to maintain C-style memory safety:

1. **Assignment LHS Requirement** (~line 1442)
   - Only L-values may appear on the left side of assignment (`L = R`). Attempting to assign to an R-value (e.g., `(a + b) = 5;`) triggers a semantic error.

2. **Reference Parameter Binding** (~lines 1964-1970)
   - Arguments bound to `T&` parameters must be L-values. Passing R-values (literals, computed expressions, or temporaries) to mutable references causes a compilation failure.

3. **Struct Field Reference/Const Initialization** (~line 2554)
   - When a struct field is declared as `ref T` or `const T`, the initializer expression must be an L-value. R-value initializers violate binding rules for such fields.

4. **Return Statement Semantics** (~line 1645 & ~1966)
   - Returning a reference type (`T&`) requires the function call to evaluate to an L-value at runtime. Standard variable/return expressions are treated as R-values/copy contexts.

## Key Takeaways for Implementation / Parsing
- **L-Values** = Named, addressable memory locations or references that can safely occupy the left side of assignments or be bound by non-const references.
- **R-Values** = Transient, computed, literal, or temporized data without stable addresses. They cannot be assigned to, but can be passed by value or const reference.
- The `value_category` field on `CZ_AST_Decoration` is either inherited from child nodes, derived from symbol lookup metadata (type/function vs variable), or explicitly set (`CZ_VALUE_CATEGORY_RVALUE`) for computed/expression nodes during declaration evaluation passes.
