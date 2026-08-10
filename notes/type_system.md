# cz Type System & Semantic Analyzer — Reference for AI Coding Agents

This document explains **how the `cz` compiler represents and reasons about types**, focusing on the data structures in `include/cz_type.h` (and their backing implementation in `src/cz_type.c`) plus how the **semantic analyzer** (`src/cz_semantic_analyzer.c`) consumes and augments that information. It is meant to be read by AI agents that need a fast, high-confidence mental model before editing type-related code.

> See also: `notes/bnf.txt` (grammar), `notes/cz_type_notes.md` (intended type signatures), `notes/l-r-value-node-types.md` (value categories).

---

## 1. Two-Layer View of "Type Information"

There are two distinct things people often conflate when working on this compiler:

| Layer | Lives in | Purpose |
|---|---|---|
| **AST type nodes** | `CZ_AST_Node` with `node_type == CZ_AST_TypeNodeType` (in `include/cz_ast.h`) | The *syntactic* description of a type as written in source (e.g. `const int32&`). Only the parser produces these. |
| **Semantic `CZ_Type` objects** | `include/cz_type.h`, owned by `CZ_GlobalTypeTable` | The *canonical, interned* semantic type used after type resolution. All type checks compare these. |

The semantic analyzer's job is to convert AST type nodes into `CZ_Type*` (via `cz_type_from_type_node`) and to attach metadata to AST expression nodes via `CZ_AST_Decoration` (see §6).

---

## 2. `CZ_Type` — The Canonical Type Object

Defined in `include/cz_type.h`. A `CZ_Type` is a tagged union (`CZ_TypeKind`) with a single inline `union` of payloads:

```c
struct CZ_Type {
    CZ_TypeKind kind;
    union {
        CZ_PrimitiveType primitive;                 // VOID / INT32 / UINT32 / BOOL / FLOAT
        struct { name; layout; } structure;         // struct
        struct { element_type; size; } array_info;  // fixed-size array
        const CZ_Type* list_of;                     // dynamic list
        const CZ_Type* const_of;                    // const wrapper
        const CZ_Type* reference_to;                // reference (&) wrapper
        struct { name; underlying; } newtype;       // newtype wrapper
        struct { return_type; param_types; param_count; } function; // function type
    };
};
```

### 2.1 `CZ_TypeKind` — what kinds exist

| Kind | Notes |
|---|---|
| `CZ_TYPE_KIND_PRIMITIVE` | `int32`, `uint32`, `bool`, `float`, `void` (see `CZ_PrimitiveType`). |
| `CZ_TYPE_KIND_STRUCT` | Nominal: `cz_type_equals` only checks `structure.name`. Layout is a `CZ_StructLayout*` (allocated lazily in pass 2 — see §5.2). |
| `CZ_TYPE_KIND_ARRAY` | Fixed-size. `size` is currently a `unsigned int`; not all code paths populate it yet. |
| `CZ_TYPE_KIND_LIST` | Dynamic list. The semantic analyzer does not yet construct array/list types (`cz_type_from_type_node` has TODO comments for this). |
| `CZ_TYPE_KIND_CONST` | Wrapper. `const_of` points at the wrapped type. |
| `CZ_TYPE_KIND_REFERENCE` | Wrapper. `reference_to` points at the wrapped type. Implemented as a pointer at the LLVM level. |
| `CZ_TYPE_KIND_NEWTYPE` | Distinct, named wrapper over an underlying type. Equality is by name. |
| `CZ_TYPE_KIND_FUNCTION` | First-class function type: `return_type`, `param_types[]`, `param_count`. Used for both function declarations and function-typed parameters. |

### 2.2 `CZ_StructLayout` and `CZ_StructField`

```c
typedef struct {
    const char* name;
    const CZ_Type* type;            // owned by the global type table
    unsigned int idx;
    const CZ_AST_Node* default_initializer;  // borrowed pointer; only set if a constexpr default exists
} CZ_StructField;

typedef struct {
    CZ_StructField* fields;
    unsigned int field_count;
} CZ_StructLayout;
```

`default_initializer` is **only meaningful** for fields that are `const` or `&` (it lets later code skip requiring an explicit initializer at use sites). For all other fields it stays `NULL`.

### 2.3 Memory ownership

- The `CZ_GlobalTypeTable` is the **sole owner** of every `CZ_Type` and every `CZ_StructLayout`. Freeing a `CZ_Type` with `cz_type_free` is a **shallow** free (it frees the type itself and, for `STRUCT`, frees the layout's field array; it does not free pointee types or string names).
- When pushing a new type via `cz_global_type_table_push_type`, **ownership transfers** to the table. Do not call `cz_type_free` on something you have pushed.
- `cz_type_free` *does* free `function.param_types` (the param type array, not the pointee types), because that array is freshly allocated per function-type construction.

---

## 3. `CZ_GlobalTypeTable` — The Type Interner

A single instance lives on the `CZ_SemanticAnalyzer` (`sa->gtt`). It serves three purposes:

1. **Name registry** — mapping user-facing names (`int32`, `Vector`, `typedef X Y`) to `CZ_Type*`.
2. **Interner** — ensuring structural type identity: `const int32` constructed at two different source sites yields the *same* `CZ_Type*` pointer. This is what makes `cz_type_equals` work via pointer fast-path (`a == b`).
3. **Ownership** — single point of free for all types.

### 3.1 Internal layout

```c
typedef struct {
    const char** names;             // parallel to named_types
    const CZ_Type** named_types;    // "shallow pointer" into all_allocations
    unsigned int named_entry_count;
    unsigned int named_entry_capacity;

    const CZ_Type** all_allocations;     // owns every CZ_Type*; freed in cz_global_type_table_free
    unsigned int all_entry_count;
    unsigned int all_allocations_capacity;
} CZ_GlobalTypeTable;
```

- The four primitive types (`int32`, `uint32`, `bool`, `float`) are created on `cz_global_type_table_create` and given names. `void` is **not** pre-registered; it is created on first use (lazily) inside `cz_type_from_type_node`.
- `cz_global_type_table_push_type(gtt, name, type)` does two things in order:
  1. Add `type` to `all_allocations` **only if no structurally-equal type already exists** (deduplication via `cz_global_type_table_find_type`).
  2. If `name != NULL`, also index it by name in the named table. (The same type may end up with multiple names — e.g. a `typedef` re-uses the underlying type's `CZ_Type*` while adding a new name. See `register_typedef` in `cz_semantic_analyzer.c`.)
- `cz_global_type_table_find_type` uses structural equality; `cz_global_type_table_find_type_by_name` uses `strcmp`. Names are interned by the parser's `CZ_StringPool` (see `sa->sp`), so identical names are often pointer-equal too.

### 3.2 What "interning" buys you

- `cz_type_equals(a, b)` short-circuits on `a == b`. Because the interning step reuses canonical instances, most type comparisons never recurse.
- Const/reference wrappers are interned too. `const int32` and `const int32` from two distinct source sites resolve to the same `CZ_Type*` (see `cz_type_table_get_or_create_const` and the `is_const` branch of `cz_type_from_type_node`).
- This is the design that lets `cz_type_is_const` and `cz_type_decay_type` be safe to call on shared instances.

---

## 4. Equality, Decay, Const Propagation

These three helpers in `include/cz_type.h` (implemented in `src/cz_type.c`) are the workhorses of type checking.

### 4.1 `cz_type_equals(a, b)`
- Pointer fast-path: `a == b` → true.
- Same `kind` required.
- **Structural cases:**
  - `PRIMITIVE` → compare `primitive` enum.
  - `REFERENCE` → recurse on `reference_to`.
  - `CONST` → recurse on `const_of`.
  - `NEWTYPE` → `strcmp` on `newtype.name` (nominal).
  - `STRUCT` → `strcmp` on `structure.name` (nominal — no layout comparison).
  - `FUNCTION` → arity match, then recurse on `return_type` and each `param_types[i]`.
- `ARRAY`, `LIST` are not yet handled by this function (returns false unless pointer-equal).

### 4.2 `cz_type_decay_type(type)`
Strips at most one layer each of `REFERENCE` and `CONST`:
```c
if (REF)     type = type->reference_to;
if (CONST)   type = type->const_of;
return type;
```
Used everywhere the analyzer wants to compare "the underlying thing" — e.g. checking that an initializer's type matches the declared type. Note it does **not** unwrap arbitrary nesting; in practice the table only ever wraps const/reference around a non-const/reference, so one peel is enough.

### 4.3 `cz_type_is_const(type)`
Returns true if the outermost layer is `CONST`, or if it is `REFERENCE` to a `CONST`. Used for const-correctness checks (assignment to const, binding non-const ref to const, etc.). **Not** aware of `const`-propagated-through-struct-access — that propagation happens explicitly in `cz_semantic_analyzer_check_struct_access` via `cz_type_table_get_or_create_const`.

---

## 5. How the Semantic Analyzer Uses the Type Table

`src/cz_semantic_analyzer.c` runs in two passes over the global declarations, then walks function bodies.

### 5.1 Pass 1 — `cz_semantic_analyzer_build_global_table`

Walks `sa->program->program.global_declaration_list`. For each top-level declaration it calls one of:

| Declaration kind | Handler | What it does to `gtt` |
|---|---|---|
| `CZ_AST_FunctionDeclarationNodeType` | `register_function_decl` | Resolves return + param types via `cz_type_from_type_node`, builds a `CZ_TYPE_KIND_FUNCTION` (allocating a new `param_types[]`), interns it (dedup via `find_type`), adds a `CZ_SYMBOL_KIND_VALUE` symbol whose `data.value.type` is the canonical function type. |
| `CZ_AST_StructDeclarationNodeType` | `register_struct_decl` | Allocates a `CZ_TYPE_KIND_STRUCT` with `name = struct_name` and `layout = NULL`. Pushes it with that name. Layout is filled in pass 2. |
| `CZ_AST_VariableDeclarationNodeType` (global) | `register_variable_decl` | Resolves declared type; adds a value symbol. References are rejected for global vars at this stage. |
| `CZ_AST_TypedefDeclarationNodeType` | `register_typedef` | Resolves base type, then re-pushes the **same** `CZ_Type*` under the alias name. (Two names, one canonical type.) |
| `CZ_AST_NewtypeDeclarationNodeType` | `register_newtypedef` | Allocates a fresh `CZ_TYPE_KIND_NEWTYPE { name, underlying }` and pushes it. Cycle detection via `cz_semantic_analyzer_newtypedef_detect_cycle` walks wrappers. |

`cz_type_from_type_node` is the workhorse for resolving type expressions into canonical `CZ_Type*`. It runs in three phases:
1. **Base type** — look up primitive by enum, or look up named type by `cz_global_type_table_find_type_by_name`. Lazily creates `void` if missing.
2. **Const wrap** — if `type_node->type_expression.is_const`, build a `CZ_TYPE_KIND_CONST` and dedup-insert.
3. **Reference wrap** — same idea with `CZ_TYPE_KIND_REFERENCE`.

Because each wrapper is interned, the same source spelling always yields the same canonical `CZ_Type*`, even if it appears in many places.

### 5.2 Pass 2 — `cz_semantic_analyzer_full_analyze`

Calls, per declaration:
- `cz_semantic_analyzer_check_function_body` — type-checks the body, including parameter list, return-on-all-paths, return statement, etc. Sets `parameter_list->parameter_list.scope` and `body->statement_list.scope`.
- `cz_semantic_analyzer_check_struct_fields` — **this is where the `CZ_StructLayout` is finally allocated and attached** to the struct's `CZ_TYPE_KIND_STRUCT` node from pass 1. It also checks field initializer expressions (must be `constexpr`, must decay-match the field type, reference fields need an l-value initializer, etc.) and stores any default initializer in `field.default_initializer`.
- `cz_semantic_analyzer_check_global_var_init` — checks the initializer expression: must be `constexpr`, must decay-match, references must bind to l-values and must not bind to const, structs with const/reference fields without defaults require an initializer.
- After all declarations, a separate loop runs `cz_semantic_analyzer_struct_cycle_detect` to reject cyclic struct layouts (a struct that directly or transitively contains itself as a field).

### 5.3 Statement and expression checking (function bodies)

The analyzer walks statements via `cz_semantic_analyzer_check_statement`, dispatching on `node_type`. Key behaviors:

- **Variable declaration** (`check_variable_declaration_statement`): resolves type, requires an initializer if `const` or `&`, requires initializer to be an l-value if `&`, forbids non-const reference bound to const initializer, requires decay-equal type, and for struct types ensures any field lacking a default and being const/`&` is provided in the initializer. Creates a `CZ_Symbol` with `is_escapable_ref` set to `type->kind == CZ_TYPE_KIND_REFERENCE && init->decoration->is_escapable_ref`.
- **Assignment** (`check_assignment_statement`): LHS must be l-value and non-const. Both sides decayed must be primitive. Compound assignments (`+=`, `-=`, …) are restricted to (int32, uint32, float) per-operator; `%= &= |= ^=` are int-only.
- **Return** (`check_return_statement`): enforces void vs. non-void. For non-void reference returns, expression must be an l-value, must have `is_escapable_ref == true` (lifetime check), and must not silently strip const.
- **If / while / for**: condition must decay to `bool`. `for` introduces a child environment for its init/condition/step.
- **Block statement** introduces a child environment with `scope_level = parent + 1` and attaches it to `stmt->statement_list.scope`.
- **Function call** (`check_function_call_expression`): callee must be a function; arg count must match; each argument must decay-match its parameter, and reference parameters require an l-value argument and forbid passing a const into a non-const ref param. The call's decoration is `LVALUE` iff the return type is `&`, else `RVALUE`. `is_escapable_ref` propagates from the return type.
- **Binary / unary expressions**: operands are decayed and then required to be primitives. Result type is one of the four primitives (or `bool` for comparisons). The result is always an `RVALUE`. `is_constexpr` is the AND of the operands' `is_constexpr`. `scope_level` is the max of the two operands (used for reference-return safety).
- **Identifier** (`check_identifier_expression`): looks up the symbol. If the symbol's type is `FUNCTION`, the identifier is an r-value (you can't take the address of a function value in this language). Otherwise it is an l-value. `is_escapable_ref` is copied from the symbol.
- **Struct member access** (`check_struct_access`): base must decay to a struct; member name must exist in layout. Inherits the base's l-value category. **Crucially, if the base is const but the member type is non-const, it calls `cz_type_table_get_or_create_const` to obtain a const variant of the member type from the GTT** — this is how const-correctness propagates through field access. `is_escapable_ref` propagates from the base.
- **Struct initializer** (`check_struct_init`): checks that any const or reference field lacking a `default_initializer` is provided, that initializers decay-match, that reference fields get l-value non-const initializers, and that there are no duplicates. Produces an r-value decoration whose `is_constexpr` is the AND of all initializers' `is_constexpr`.
- **Cast** (`cast_expression`): the only allowed casts are between primitives and newtypes-of-primitives, plus same-primitive identity casts. Casting to a reference is forbidden (it would let you synthesize an l-value out of an r-value). Casts are not allowed to drop const.

---

## 6. `CZ_AST_Decoration` — Per-Node Type Info

While the `CZ_GlobalTypeTable` answers *"what is the canonical type T?"*, every expression and initializer in the AST gets a per-node decoration that records the result of type-checking that specific occurrence:

```c
typedef struct {
    const CZ_Type* resolved_type;       // borrowed from CZ_GlobalTypeTable
    CZ_ValueCategory value_category;    // LVALUE or RVALUE
    bool is_constexpr;                  // all operands were constexpr, etc.
    bool is_reference_source;           // was the original form a reference (T&)?
    unsigned int scope_level;           // scope at which this expr was produced
    bool is_escapable_ref;              // can this reference safely escape (e.g. be returned)?
} CZ_AST_Decoration;
```

Allocated by `cz_ast_decoration_create`, freed by `cz_ast_decoration_free`, and attached to `node->decoration`. Important invariants:

- `resolved_type` is **always** a pointer into the GTT — never an independent allocation, never freed by the decoration.
- `value_category` is set per expression kind (see §5.3 for the per-case rules). Reference-typed results of function calls are l-values; arithmetic results are r-values.
- `is_constexpr` propagates from operands (AND-combined) and is also tracked on `CZ_Symbol` for variables.
- `scope_level` mirrors the lexical scope in which the value was produced. The semantic analyzer historically used this for reference-lifetime checks (e.g. blocks at level ≥2 cannot be returned by reference) — there is commented-out code in `check_return_statement` showing the older rules. Currently the live rule is the `is_escapable_ref` check.
- `is_escapable_ref` is the modern lifetime/escape flag. It is set on:
  - **Symbols** during declaration: true for globals, false for locals declared as `T&` whose initializer's `is_escapable_ref` is false (see `register_variable_decl` and `check_variable_declaration_statement`).
  - **Expressions**: copied from the source for identifiers, struct member access, and function calls. Function calls' `is_escapable_ref` is true iff the return type is `&` (since the function returned a reference to something whose escape the callee already validated).

`return` statements and reference variable declarations use these flags to enforce "cannot return a reference to local stack memory" and similar safety properties.

---

## 7. Scope Levels — Quick Reference

| `scope_level` | Meaning |
|---|---|
| `0` | Global scope. |
| `1` | Function parameter scope. Parameters cannot be shadowed by inner declarations (see `check_variable_declaration_statement`). |
| `2` | Function body scope. |
| `≥3` | Nested block / for-loop / while-loop / if-body scopes. |

This is also what the `CZ_Environment` tree is built from. Each block statement creates a child environment with `scope_level = parent + 1`, and the semantic analyzer attaches the environment to the statement's `statement_list.scope` / `for_statement.scope` so the code generator can find symbols later.

---

## 8. End-to-End Example: `const Vector& v = ...`

1. **Parser** produces an `IdentifierNodeType` ("v") and a `TypeNodeType` with `is_const = true, is_reference = true, primitive.kind = IDENTIFIER, primitive.name = "Vector"`.
2. **Pass 1** resolves the type expression in `cz_type_from_type_node`:
   - Base: `cz_global_type_table_find_type_by_name("Vector")` → the `CZ_TYPE_KIND_STRUCT` registered in `register_struct_decl`.
   - Const wrap: builds `Const(Base(Vector))`, dedups/inserts in GTT.
   - Reference wrap: builds `Ref(Const(Base(Vector)))`, dedups/inserts in GTT.
3. **Pass 2** type-checks the initializer: must be an l-value (because `&` is outermost), must not be const unless the ref is `const&`, etc. The decoration on the initializer is propagated to the variable's symbol (`is_escapable_ref`).
4. Every later use of `v` as an identifier produces a decoration with `resolved_type` pointing at the *same* canonical `Ref(Const(Base(Vector)))` from the GTT, and `value_category = LVALUE` (since it's not a function). The `const` flows through any subsequent member access via `cz_type_table_get_or_create_const`.

---

## 9. Practical Cheat-Sheet for Modifications

When you change anything type-related, remember these invariants:

1. **Always go through the GTT.** Don't `cz_type_create` a wrapper and pass it around without pushing it; you will lose interning and break pointer-fast-path equality.
2. **Never `cz_type_free` something you pushed.** Ownership has transferred; the table will free it.
3. **Function types own their `param_types` array** at construction; the GTT later shallow-frees the `CZ_TYPE_KIND_FUNCTION` struct including that array. Don't double-free.
4. **`CZ_StructLayout` ownership**: the layout's `fields` array is freed by `cz_struct_layout_free`. Field `name` and `type` are **borrowed** and not freed. The layout itself is owned by the struct `CZ_Type` and freed via `cz_type_free`.
5. **`resolved_type` in decorations is borrowed** from the GTT. Don't free it from the decoration.
6. **Const propagation through struct access** must go through `cz_type_table_get_or_create_const` — there is no automatic wrapper.
7. **Decay before comparing types** in most places (`cz_type_decay_type` then `cz_type_equals`). Direct `cz_type_equals` will reject `int32` vs `const int32` and `int32` vs `int32&`.
8. **Newtypes are nominal** (compared by name). Two `newtype X int32;` in different scopes would not be equal — name is the identifier.
9. **Structs are nominal** (compared by name). Layout changes do not affect `cz_type_equals`.
10. **Function types compare structurally**: arity + return + each param. Use this to deduplicate function-typed values.

---

## 10. File Map

| Concern | File |
|---|---|
| `CZ_Type` / `CZ_GlobalTypeTable` API | `include/cz_type.h` |
| `CZ_Type` / `CZ_GlobalTypeTable` impl | `src/cz_type.c` |
| `CZ_AST_Decoration`, `CZ_ValueCategory` | `include/cz_ast.h` (top) |
| `CZ_Symbol`, `CZ_Environment` | `include/cz_symbol_table.h` |
| Semantic analyzer entry points | `src/cz_semantic_analyzer.c` (`cz_semantic_analyzer_create`, `_analyze`, `_free`) |
| Pass 1 (declaration registration) | `register_function_decl`, `register_struct_decl`, `register_variable_decl`, `register_typedef`, `register_newtypedef` |
| Pass 2 (body / field / init checks) | `check_function_body`, `check_struct_fields`, `check_global_var_init`, `check_statement*` |
| Expression type checking | `check_expression`, `check_function_call_expression`, `check_binary_expression`, `check_unary_expression`, `check_literal_expression`, `check_identifier_expression`, `check_struct_access`, `check_struct_init`, `cast_expression` |
| Type-from-AST-node resolver | `cz_type_from_type_node` |
| Const helper used during struct access | `cz_type_table_get_or_create_const` (file-local in `cz_semantic_analyzer.c`) |
| Cycle detection | `cz_semantic_analyzer_struct_cycle_detect`, `cz_semantic_analyzer_newtypedef_detect_cycle` |
| Return-on-all-paths | `cz_ast_node_returns_on_all_paths` |
