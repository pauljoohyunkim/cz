# cz Language Reference

## Overview
`cz` is a small, C-like programming language designed for educational purposes. It features a static type system with primitives (`int32`, `bool`, `float`, `void`), structs, type aliases (`typedef`), distinct newtypes (`newtype`), const qualifiers, and references. The compiler follows a classic pipeline: lexer → recursive‑descent parser with Pratt‑style expression parsing → semantic analyzer (type checking, const/reference checks) → LLVM‑IR code generator. The implementation is in C, with Google Test for unit tests.

---

## Lexical Structure

### Keywords
```
func struct typedef newtype const if else while for return
```

### Literals
- **Integer literals** – e.g., `42`, `-7`
- **Floating‑point literals** – e.g., `3.14`, `-0.5`
- **Boolean literals** – `true`, `false`
- **String literals** are lexed but not yet supported in semantic analysis.

### Identifiers
- Letters, digits, underscore; must not start with a digit.
- Case‑sensitive.

### Operators & Punctuation
```
+ - * / %          (arithmetic)
+= -= *= /= %=     (compound assignment)
& | ^              (bitwise)
&= |= ^=           (compound bitwise)
=                  (simple assignment)
== !=              (equality)
< > <= >=          (relational)
!                  (logical negation)
-                  (unary minus)
.                  (struct member access)
as                 (type cast)
::                 (type annotation in declarations)
, ;                (separators)
() [] {}           (grouping)
```

### Comments
- Line comments: `// ...` (as seen in examples).

---

## Types and Type System

### Primitive Types
| Keyword | Kind        | Description |
|---------|-------------|-------------|
| `int32` | signed 32‑bit integer |
| `bool`  | boolean (`true`/`false`) |
| `float` | IEEE‑754 single‑precision floating point |
| `void`  | absence of value (function return only) |

### Type Qualifiers
- **`const`** – read‑only qualifier; can be applied to any type.
- **`&` (reference)** – creates a reference type, similar to C++ reference; must be initialized to an existing object and cannot be reseated.

Qualifiers appear after the base type:
```
type ::= [const] base_type [&]?
```
Examples:
- `int32`            – mutable integer
- `const int32`      – read‑only integer
- `int32&`           // reference to mutable integer
- `const int32&`     // reference to const integer

### User‑Defined Types
- **`struct`** – aggregates named fields, each with its own type. Fields may have default initializers (must be `constexpr`).
- **`typedef`** – creates a type alias (no new type introduced).
- **`newtype`** – creates a distinct, distinct type that is not interchangeable with its underlying type except via explicit cast.

### Function Types
A function type is defined by its parameter types and return type. Functions are first‑class only insofar as they can be called; they cannot be stored in variables (no function‑type variables).

### Type Equality
Two types are equal after **decaying** (stripping `const` and `&` wrappers) if they are the same primitive, struct, or newtype name. For function types, parameter and return types must match exactly after decay.

---

## Declarations

All declarations are **global** (file‑scope). Inside functions only variable declarations and statements are allowed.

### General Form
```
decl ::= func_decl
       | struct_decl
       | var_decl          (with optional initializer)
       | typedef_decl
       | newtype_decl
```

### Variable Declaration
```
var_decl ::= identifier "::" type [ "=" expression ] ";"
```
- The type may include `const` and `&`.
- If the declared type is `const` or a reference, an initializer **must** be provided.
- Global variables with an initializer must have a **constant‑expression** initializer (evaluable at compile time).

### Function Declaration
```
func_decl ::= "func" identifier "::" "(" param_list ")" [ "->" type ] block ";"
param_list ::= param_decl ( "," param_decl )* | ""
param_decl ::= identifier "::" type
```
- Return type is optional; omitted → `void`.
- Parameters are passed **by value** unless their type includes a reference (`&`) or `const` qualifier (the latter does not affect pass‑by‑value semantics; only references affect aliasing).

### Struct Declaration
```
struct_decl ::= "struct" identifier "{" struct_member_list "}"
struct_member_list ::= struct_member ( ";" struct_member )* | ""
struct_member ::= var_decl ";"
```
- Members may have default initializers (must be `constexpr`).
- A struct containing a `const` field or a reference field **without** a default initializer requires every instance to provide an initializer for that field.

### Typedef / Newtype
```
typedef_decl ::= "typedef" base_type identifier ";"
newtype_decl ::= "newtype" base_type identifier ";"
```
- `typedef` creates an alias; the new name can be used interchangeably with the base type.
- `newtype` creates a distinct type; implicit conversions are disallowed. Explicit casts (`expr as NewType`) are allowed when the underlying representation is compatible (primitive‑to‑primitive or newtype‑to‑newtype with same underlying primitive).

---

## Statements

### Expression Statement
```
expression_stmt ::= expression ";"
```
Any expression (including a function call) may be a statement; its result is discarded.

### Variable Declaration Statement
Same syntax as a global variable declaration but appears inside a block.

### Assignment Statement
```
assignment_stmt ::= left_hand_side assign_op expression ";"
assign_op ::= "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^="
```
- The left‑hand side must be an **lvalue** (modifiable memory location).
- The left‑hand side must **not** be `const`.
- Compound operators require the underlying (decayed) type to be a primitive:
  - `+=`, `-=`, `*=`, `/=`, `%=` – only for `int32` or `float`.
  - `&=`, `|=`, `^=` – only for `int32`.

### Control Flow
```
if_stmt     ::= "if" "(" expression ")" block [ "else" ( if_stmt | block ) ]
while_stmt  ::= "while" "(" expression ")" block
for_stmt    ::= "for" "(" [ init ] ";" [ condition ] ";" [ update ] ")" block
```
- `init` may be a variable declaration or an assignment statement.
- `condition` (if present) must be of type `bool`.
- `update` may be an expression statement or assignment statement.
- Braces `{}` are required for all blocks.

### Return Statement
```
return_stmt ::= "return" [ expression ] ";"
```
- If the function returns `void`, no expression may be present.
- Otherwise, an expression must be present and its type (after decay) must match the return type (after decay).
- **Reference return rules**:
  - The expression must be an **lvalue**.
  - The expression must not refer to a local (stack‑allocated) variable; only globals, function parameters, or struct members are allowed.
  - If the function returns a **non‑const reference**, the expression must not be `const`.

### Block
```
block ::= "{" statement_list "}"
statement_list ::= statement* 
```
Each block introduces a new lexical scope.

---

## Expressions

### Literals
- Numeric literals: `int32` if no decimal point, `float` otherwise.
- Boolean literals: `true`, `false`.

### Identifiers
- Resolve to a variable, function, or type name (in type expressions).
- Produce an **lvalue** for variables (unless the type is a function, which yields an rvalue).

### Member Access
```
expr "." identifier
```
- The left expression must be a struct type (after decaying references/const).
- The identifier must name a member of that struct.
- Result is an **lvalue** if the base expression is an lvalue; otherwise, an rvalue.
- Accessing a member of a `const` struct yields a `const`‑qualified member type.

### Function Call
```
expr "(" [ argument_list ] ")"
```
- The expression before `(` must be of function type.
- Arguments are matched by position to parameters; each argument’s type must match the parameter type after decay.
- For reference parameters, the argument must be an **lvalue** and must not discard `const` (i.e., you cannot bind a `const` argument to a non‑const reference parameter).
- The call yields an **lvalue** if the function returns a reference; otherwise, an rvalue.

### Struct Initialization
```
identifier "{" [ member_init_list ] "}"
member_init_list ::= ( "." identifier "=" expression ) ( "," [ "." identifier "=" expression ] )* [ "," ]?
```
- Only members with explicit initializers are set; others take their default initializer (if any) or are zero‑initialized.
- All provided initializers must be `constexpr` if the struct definition requires it (e.g., the struct has a `const` or reference member without a default).

### Cast Expression
```
expression "as" type
```
- Allowed casts: between primitives, between a newtype and its underlying primitive, or between two newtypes that share the same underlying primitive.
- Casting away `const` via `as` is **not allowed**; constness is part of the type and cannot be removed by a cast.
- The result is an rvalue.

### Unary Operators
- `- expr` – arithmetic negation (valid on `int32` and `float`).
- `! expr` – logical negation (valid on `bool`).

### Binary Operators (Precedence & Associativity)

| Level | Operator(s)                     | Associativity |
|-------|---------------------------------|---------------|
| 15    | `.` `(` `)`                     | Left          |
| 14    | `as`                            | Right         |
| 13    | unary `-` `!`                   | Right         |
| 12    | `*` `/` `%`                     | Left          |
| 11    | `+` `-`                         | Left          |
| 10    | `&` `|` `^`                     | Left          |
| 9     | `==` `!=`                       | Left          |
| 8     | `<` `>` `<=` `>=`               | Left          |
| 2     | `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` | Right (assignment) |

- Logical operators `&&` and `||` are **not** part of the language (only equality and relational operators, which return `bool`).

### Constant Expressions
Certain contexts require **compile‑time constant expressions** (`constexpr`):
- Initializers of global variables that have an initializer.
- Initializers of struct members that lack a default initializer.
- (Future) array bounds, etc.
A `constexpr` expression may contain literals, `const` variables, `constexpr` functions (not yet implemented), and operators whose operands are `constexpr`.

---

## Semantic Analysis Rules (Selected)

- **Type Checking**: Every expression is assigned a type; mismatches are errors.
- **Const Correctness**:
  - A `const` object cannot be assigned to.
  - A reference to `const` can bind to a `const` or non‑const lvalue.
  - A non‑const reference can bind only to a non‑const lvalue.
- **Reference Safety**:
  - A reference must be initialized.
  - Returning a reference from a function is allowed only if the referred object has static or parameter lifetime (i.e., not a local stack variable).
- **Struct Initialization**:
  - If a struct contains a `const` field or a reference field without a default initializer, every instance must provide an initializer for that field.
  - Member initializers must be `constexpr` if the struct definition requires it.
- **Function Calls**:
  - Argument types must match parameter types after decay.
  - For reference parameters, the argument must be an lvalue and must not discard `const`.
- **Return Statements**:
  - Return type must match the function’s return type after decay.
  - Returning a reference follows the same rules as initializing a reference.

---

## Code Generation (LLVM IR)

The compiler uses LLVM as its backend:
- Each function becomes an LLVM function.
- Structs are represented as LLVM struct types.
- References are compiled as LLVM pointers (`i8*` or typed pointers) with appropriate `const` semantics.
- Global constants are emitted as `constant` globals.
- The compiler focuses on correctness; no optimizations are performed.

---

## Example Program (tests/czc_test_code.cz)

```cz
struct Vector {
    x :: int32;
    y :: int32;
}

func get_default_x :: () -> int32 {
    // Vector{1, 2} is an R-value.
    // Accessing .x on an R-value struct yields an R-value copy.
    // This should safely initialize a plain copy variable.
    val :: int32 = Vector{.x = 1, .y = 2}.x;
}
```

**Explanation**
- Defines a simple `struct Vector` with two integer fields.
- Function `get_default_x` takes no parameters and returns an `int32`.
- Inside the function, a temporary `Vector` is constructed with `{.x = 1, .y = 2}`.
- The `.x` field is accessed (yielding an `int32` rvalue) and used to initialize the local variable `val`.
- The function returns `val` (an explicit `return` statement would be required in the actual language; the snippet illustrates struct construction and member access).

---

## Notable Conditions (L‑Value / R‑Value Rules)

| Construct               | Produces L‑Value? | Conditions / Restrictions |
|-------------------------|-------------------|---------------------------|
| Variable identifier     | Yes               | Unless its type is a function (then rvalue). |
| Struct member access (`e.f`) | Yes if `e` is lvalue, otherwise rvalue | `e` must be a struct type (after decaying). |
| Function call (`f()`) | Yes if function returns a reference type, otherwise rvalue | Reference return must obey lifetime rules. |
| Assignment LHS (`lhs = rhs`) | Must be lvalue | Cannot be `const`; for compound operators underlying decayed type must be primitive (`int32`/`float` for arithmetic, `int32` for bitwise). |
| Reference parameter binding | Argument must be lvalue | Additionally, if parameter is non‑const reference, argument must not be `const`. |
| Returning a reference   | Expression must be lvalue | Expression must not refer to a local stack variable; must be global, parameter, or struct member. If returning non‑const reference, expression must not be `const`. |
| Struct initialization   | Result is rvalue  | Members with explicit initializers must be `constexpr` if struct requires it (e.g., const/ref members without default). |
| Cast expression         | Result is rvalue  | Casting away `const` via `as` is prohibited. |
| Unary `- expr`          | rvalue            | Operand must be `int32` or `float`. |
| Unary `! expr`          | rvalue            | Operand must be `bool`. |
| Binary arithmetic (`+ - * / %`) | rvalue | Operands must be same primitive type (`int32`/`float`) after decay. |
| Binary bitwise (`& | ^`) | rvalue | Operands must be `int32` after decay. |
| Equality/relational (`== != < > <= >=`) | rvalue | Operands must be same primitive type (`int32`/`float`/`bool` for equality) after decay. |

---

## References
- Source files: `notes/bnf.txt`, `src/cz_parser.c`, `src/cz_semantic_analyzer.c`
- Compiler entry point: `src/czc.c`
- Build system: `Makefile` (`make czc` or `make` to build the compiler; `make unittest` to run tests).

--- 

*This document captures the core language features as of the current implementation. Details may evolve as the language develops.*