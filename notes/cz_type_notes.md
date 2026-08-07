==================================================================
      CZ_TYPE INTERNAL SYNTAX LOGIC (Dev Note)
==================================================================

CORE RULE: Types are constructed from **Base -> Outwards**. 
(Wrappers stack around the base type; `&` is always the outer shell).


INTERNAL TYPE SIGNATURES
-------------------------
Syntax           | Internal CZ_Type Structure                        | Nesting Order
-----------------|---------------------------------------------------|---------------------
`int32`          | Base(Int32)                                       | 1. Base
`const int32`    | Const(Base(Int32))                                | 2a. Immutable value

`int32&`         | Ref(Base(Int32))                                  | 3. Mutable Reference
`const int32&`   | Ref(Const(Base(Int32)))                           | 3. Immutable Reference (copy)

`int32[5]`       | Array(Size=5, Base(Int32))                        | 2b. Mutable elements
`const int32[5]` | Array(Size=5, Const(Base(Int32)))                 | 2b. Immutable elements

`int32[5]&`      | Ref(Array(Size=5, Base(Int32)))                   | 3a. Reference to block
`const int32[5]&`| Ref(Array(Size=5, Const(Base(Int32))))            | 3a. Ref to immutable block


CONSTRUCTION ORDER (Right-to-Left in BNF)
------------------------------------------
1. **Base Type** (`int32`, `bool`) -> Start here.
2. **Modifiers** (`const` or `[N]`) -> Wrap the base immediately.
   *(Note: The BNF forces `[Array]` to be placed strictly before `&**)`.
3. **Reference Wrapper** (`&`) -> Wraps the entire resolved type on top.

==================================================================
