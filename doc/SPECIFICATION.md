# cz Language Specification v1.0

## 1. Introduction & Design Goals
`cz` is a statically-typed, imperative programming language designed with educational clarity and minimalism in mind. It draws inspiration from C's syntax and semantics but aims to eliminate common pitfalls through:
- **Strict Type Safety**: Compile-time checking of primitive type mismatches and structural alignment.
- **Value Categories**: Explicit distinction between L-values (identifiable memory locations) and R-values (transient expressions), enforced by the semantic analyzer.
- **Const Correctness & Reference Semantics**: `const` qualifies l-value bindings only, while references (`&`) are strictly initialized pointers bound for their entire lifetime. Unary operations never yield `const` types, as they produce transient R-values.
- **Global-Only Declarations**: All top-level definitions reside in file scope; local variable declarations are restricted to block scopes.

## 2. Lexical Structure
The lexer (`src/cz_lexer.c`) processes source code into tokens defined in `include/cz_tokens.h`. Characters like whitespace and line breaks (`\n`) serve only as separators. Line comments start with `//` and terminate at the newline.

### 2.1 Keywords
| Keyword | Meaning                  | Keyword   | Meaning          |
|---------|--------------------------|-----------|------------------|
| `func`  | Function definition      | `if`      | Conditional branch|
| `struct`| Struct definition        | `else`    | Fallback branch  |
| `typedef`| Type alias definition   | `while`   | Loop construct   |
| `newtype`| Distinct type wrapper   | `for`     | Iteration loop   |
| `const` | Read-only qualifier      | `return`  | Return from func |
         |                         | `as`      | Type casting   |

### 2.2 Literals
- **Integer**: Sequence of digits (e.g., `42`). Lexed as numerical literals without delimiters.
- **Floating-point**: Digits with a decimal point, optionally followed by more digits or an exponent (e.g., `3.14`, `-0.5`).
- **Boolean**: `true` and `false`.

*Note: String literals (`STRING_LITERAL`) are lexed but not yet supported beyond lexical processing.*

### 2.3 Identifiers
- Composition: Unicode letters, digits (`0-9`), and underscores (`_`).
- Must not begin with a digit.
- Case-sensitive. Reserved keywords cannot be used as identifiers.
- Interned via a `CZ_StringPool` to enforce canonical symbol representation.

### 2.4 Operators & Punctuation
| Symbol(s)      | Name / Category                | Precedence Level | Associativity |
|----------------|--------------------------------|------------------|---------------|
| `(` `)` `[` `]` | Grouping, indexing             | 15 (Highest)     | Left          |
| `.`            | Structural member access         |                  |               |
| `as`           | Type cast                        | 14               | Right         |
| `-` `!`        | Unary Arithmetic negation / Logical Not | 13        | Right         |
| `*` `/` `%`    | Multiplicative                   | 12               | Left          |
| `+` `-`        | Additive                         | 11               | Left          |
| `&` `|` `^`    | Bitwise                          | 10               | Left          |
| `==` `!=`      | Equality                         | 9                | Left          |
| `<` `>` `<=` `>=` | Relational                   | 8                | Left          |
| `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` | Compound Assignment | 2     | Right         |
| `,`            | Argument / Field separator       |                  |               |
| `;`            | Statement terminator             |                  |               |

## 3. Syntactic Grammar
The parser (`src/cz_parser.c`) implements a recursive-descent algorithm with Pratt-style expression parsing to handle precedence levels natively. The grammar below uses extended BNF notation.

### 3.1 High-Level Structure
```ebnf
<program>          ::= <global_declaration_list> EOF
<global_declaration_list> ::= <global_declaration> (<global_declaration>) | ε
<global_declaration> ::= <function_decl> 
                       | <struct_decl> 
                       | <variable_decl> ";" 
                       | <type_alias_decl> ";" 
                       | <distinct_type_decl> ";"
```

### 3.2 Type Definitions
```ebnf
<type_expression>  ::= [ "const" ] <base_type> [ "&" ]
<base_type>        ::= "int32" | "float" | "bool" | <identifier> | "(" <type_expression> ")"
<struct_decl>      ::= "struct" <identifier> "{" (<var_decl> ";" )* "}"
<type_alias_decl>  ::= "typedef" <base_type> <identifier>
<distinct_type_decl> ::= "newtype" <base_type> <identifier>
```

### 3.3 Functions & Control Flow
```ebnf
<function_decl>    ::= "func" <identifier> "::" "(" [ <param_list> ] ")" [ "->" <type_expression> ] <block> ";"
<param_list>       ::= <param_decl> ( "," <param_decl> ) | ε
<param_decl>       ::= <identifier> "::" <type_expression>

<block>            ::= "{" (<statement>)* "}"
<statement>        ::= <var_decl> ";" 
                     | <assignment_stmt> ";" 
                     | <return_stmt> ";" 
                     | <if_stmt> 
                     | <for_stmt> 
                     | <while_stmt> 
                     | <block> 
                     | <expression> ";"

<if_stmt>          ::= "if" "(" <expression> ")" <block> [ "else" (<if_stmt> | <block>) ]
<while_stmt>       ::= "while" "(" <expression> ")" <block>
<for_stmt>         ::= "for" "(" [<init>] ";" [<cond>] ";" [<update>] ")" <block>
<return_stmt>      ::= "return" [ <expression> ] ";"

<init>             ::= <var_decl> | <assignment_stmt>
<cond>             ::= <expression>
<update>           ::= <assignment_stmt> | <expression>
```

### 3.4 Expressions
```ebnf
<expression>       ::= <equality_expr>
<cast_expr>        ::= <unary_expr> "as" <type_expression>
<unary_expr>       ::= "-" <postfix_expr> | "!" <postfix_expr> | <postfix_expr>
<postfix_expr>     ::= <primary> ( "(" [ <argument_list> ] ")" | "." <identifier> )*
<primary>          ::= <identifier> 
                     | <literal> 
                     | "(" <expression> ")" 
                     | <struct_init>

<struct_init>      ::= <identifier> "{" [ <member_initializer_list> ] "}"
<member_initializer_list> ::= ( "." <identifier> "=" <expression> ) ( "," ( "." <ident> "=" <expr> )* )? ","?
```

## 4. Type System & Semantics (`include/cz_type.h`)
The compiler maintains a `CZ_GlobalTypeTable` containing all named and anonymous types. Types are internally classified by `CZ_TypeKind`.

### 4.1 Primitives
| Primitive | Internal Enum               | Size (LLVM Target Dependent) |
|-----------|-----------------------------|------------------------------|
| `int32`   | `CZ_PRIMITIVE_INT32`        | 32-bit signed integer        |
| `float`   | `CZ_PRIMITIVE_FLOAT`        | IEEE-754 single-precision    |
| `bool`    | `CZ_PRIMITIVE_BOOL`         | Boolean (`true`/`false`)     |
| `void`    | `CZ_PRIMITIVE_VOID`         | No returned data             |

### 4.2 Type Representation & Structural Equality
Types are modeled as a tree structure:
- **Primitives**: Leaf nodes identifying the raw primitive kind.
- **Structures**: Nodes mapping names to `CZ_StructLayout` (list of typed fields and default initializers).
- **Newtypes**: Nodes wrapping an underlying type (`newtype`), ensuring they are distinct from their base for implicit conversion checks.
- **Qualifiers**: Nodes that wrap a parent type:
  - `CZ_TYPE_KIND_CONST`: Marks its inner type as immutable.
  - `CZ_TYPE_KIND_REFERENCE`: Pointer semantics compiled to LLVM pointers; must be initialized at declaration.

**Decay Rule**: Type comparisons and arithmetic validity checks utilize *type decay*, which strips outermost `const` and `&` wrappers until a base primitive, struct, or newtype is reached.
**Equality Check**: `cz_type_equals()` verifies structural match of two types *after* decaying them both. For instance, `int32` matches `const int32`, but `int32` does not structurally match a different `newtype(int32)`.

## 5. Scoping & Storage
- **Global Declarations**: Functions, structs, typedefs, newtypes, and externally-linkable variables exist in file scope, accessible across translation units if compiled separately (LLVM linkage dependent).
- **Local Variables**: Declared inside `<block>` statements. Their `CZ_AST_Decoration.scope_level` tracks nesting depth. Lifetime is strictly bound to the enclosing block scope.
- **Lifetimes & References**:
  - Returning a reference requires the source object to have static, global, or parameter lifetime (e.g., global variable, parameter, or struct field). Binding a local stack variable's reference constitutes a fatal semantic error.

## 6. Value Categories & Const Semantics (`cz_ast.h` + `referece_const.txt`)
Every expression is decorated at compile time with `CZ_ValueCategory`:
- **L-values**: Denote identifiable memory locations (variables, struct fields, dereferenced references). Can appear on the LHS of assignments.
- **R-values**: Transient computational results. Cannot be assigned to.

**Const Rules (`const` qualifiers)**:
1. `const` is exclusively an l-value qualifier in `cz`. A transient R-value produced by unary operators (e.g., `-x`, `!x`) or arithmetic always has a non-const type.
2. `const int32& x` means "a reference to a location whose contents are read-only." Binding a non-const lvalue to this is valid; binding a const lvalue is also valid. But binding a non-const lvalue to a *non-const* reference requires the source not to be const.
3. A declared `const` variable cannot appear on the LHS of an assignment or compound operator.

## 7. Constant Expressions (`constexpr`)
Certain contexts strictly require compile-time constant expressions:
- Initializers for global variables.
- Members in structures lacking default initializers.
A valid `constexpr` may comprise literals, `const` variables, nested struct init, and operators whose operands are themselves constexpr.

## 8. Semantic Analysis Rules (`cz_semantic_analyzer.c`)
The semantic analyzer enforces the following critical constraints before LLVM IR generation:
- **Assignment Constraints**: LHS must be an l-value and non-const. Compound operators on `&=, |=, ^=` require underlying decayed type to be strictly `int32`. Arithmetic operators (`+, -, *, /, %`) operate only if both operands share the same primal type post-decay.
- **Function Calls**: Arguments must match parameter types exactly after decay. For reference parameters, the argument must be an l-value and must not discard const. R-values cannot bind to non-const reference parameters.
- **Cast Expressions (`expr as Type`)**: Allowed if:
  1. Primitives share a numerical underlying category.
  2. A `newtype` is cast to/from its exact underlying primitive, or between two compatible `newtype`s sharing the same base.
  3. Casting away `const` via `as` is universally prohibited; constness is strictly part of the type boundary.

## 9. Compilation Architecture (& LLVM Integration)
`cz` compiles to native binaries using LLVM:
1. **Module Creation**: A single LLVM module represents the program unit.
2. **Global Symbol Emission**: Structs are lowered to `LLVMStructType`. Global variables emit as external linkage or static internal declarations depending on compilation target.
3. **Function Generation**: Each `func` becomes an `LLVMFunctionType`. Local variable declarations compile to `alloca` instructions within the entry basic block.
4. **Control Flow Graph**: `if`, `while`, and `return` statements translate to conditional branches (` icmp`, `fcmp`) and `br` / `ret` terminators.

## 10. Example Program (`tests/czc_test_code.cz`)
```cz
// Defining a struct with floating point fields.
struct Vector {
    x :: float;
    y :: float;
}

// Aliasing int32 as 'int'. Note: typedef creates an alias, interchangeable with base.
typedef int32 int;

// Global variables must be initialized with constexpr values.
n :: int = 1;
k :: float = 0.2;

// Struct instance created via designated initialization.
i :: Vector = Vector {
    .x = 1.0
};
```

---
*This specification documents the core language features, semantic boundaries, and architectural pipeline as implemented in the `cz` compiler base.*
