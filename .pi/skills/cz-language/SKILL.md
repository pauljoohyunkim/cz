---
name: cz-language
description: Provides guidance for writing correct CZ language code, including syntax reference, common patterns, and best practices for avoiding semantic analysis errors.
---

# Cz Language Coding Skill

## Usage

This skill helps you write correct CZ code that passes lexer, parser, and semantic analysis stages.

## Basic Syntax Reference

### Variable Declarations
```cz
// Global variables (file scope only)
global_var :: int32 = 42;
global_const :: const int32 = 100;
global_ref :: int32& = global_var;  // Must be initialized
global_const_ref :: const int32& = global_const;

// Arrays (now supported)
// Fixed-size array
global_array :: int32[10];
// Array with initialization
global_array_init :: int32[5] = [1, 2, 3, 4, 5];
// Array with partial initialization (remaining elements zero-initialized)
global_array_partial :: int32[5] = [1, 2, 3];
// Array of structs
struct Point {
    x :: float;
    y :: float;
}
global_points :: Point[3] = [
    Point { .x = 0.0, .y = 0.0 },
    Point { .x = 1.0, .y = 1.0 },
    Point { .x = 2.0, .y = 2.0 }
];
// Local arrays inside functions (as statements)
local_array :: float[4] = [1.0, 2.0, 3.0, 4.0];
```

### Function Declarations
```cz
// Function with parameters and return type
func function_name :: (param1 :: type, param2 :: type&) -> return_type {
    // Function body (statements only, no declarations)
    return expression;
}

// Function returning reference
func get_ref :: () -> int32& {
    return global_var;
}

// Function returning const reference
func get_const_ref :: () -> const int32& {
    return global_const;
}
```

### Types
- **Primitives**: `int32`, `bool`, `float`, `void`
- **Qualifiers**: `const` (lvalues only), `&` (references)
- **User-defined**:
  - `struct` - aggregate with named fields
  - `typedef` - transparent alias
  - `newtype` - distinct wrapper type

### Control Flow
```cz
// If/else
if (condition) {
    // statements
} else {
    // statements
}

// While loop
while (condition) {
    // statements
}

// For loop (3-component)
for (i :: int32 = 0; i < 10; i = i + 1) {
    // statements
}

// For loop (infinite - omit all components)
for (; ;) {
    // statements
    if (break_condition) {
        break;
    }
}
```

### Expressions
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `<<`, `>>`
- Relational: `<`, `>`, `<=`, `>=`
- Equality: `==`, `!=`
- Cast: `unary_expression as type`
- Note: The left-hand side must be a unary expression. To cast an expression with lower precedence (e.g., an additive expression), parentheses are required: `(x + y) as int32`
- Member access: `struct_instance.field`
- Pointer/dereference: implicit with references
- Array indexing: `array[index]` (index must be int32)
- Array literal: `[expression, expression, ...]` (used in initialization)

### Structs
```cz
struct Vector {
    x :: float;
    y :: float;
}

// Initialization with designated fields
v :: Vector = Vector {
    .x = 1.0,
    .y = 2.0
};

// Access
v.x = 3.0;
```

## Common Patterns

### Fibonacci Function
```cz
func fibonacci :: (n :: int32) -> int32 {
    if (n <= 1) {
        return n;
    }
    
    a :: int32 = 0;
    b :: int32 = 1;
    temp :: int32;
    
    for (i :: int32 = 2; i <= n; i = i + 1) {
        temp = a + b;
        a = b;
        b = temp;
    }
    
    return b;
}
```

### ReLU (Rectified Linear Unit) Function
```cz
func relu :: (x :: float) -> float {
    if (x > 0.0) {
        return x;
    }
    return 0.0;
}

// Or using ternary-like pattern (if/else)
func relu_ternary :: (x :: float) -> float {
    result :: float = 0.0;
    if (x > 0.0) {
        result = x;
    }
    return result;
}
```

### Struct Operations
```cz
struct Matrix {
    m00 :: float; m01 :: float; m02 :: float;
    m10 :: float; m11 :: float; m12 :: float;
    m20 :: float; m21 :: float; m22 :: float;
}

func matrix_add :: (a :: const Matrix&, b :: const Matrix&) -> Matrix {
    result :: Matrix;
    result.m00 = a.m00 + b.m00;
    result.m01 = a.m01 + b.m01;
    result.m02 = a.m02 + b.m02;
    result.m10 = a.m10 + b.m10;
    result.m11 = a.m11 + b.m11;
    result.m12 = a.m12 + b.m12;
    result.m20 = a.m20 + b.m20;
    result.m21 = a.m21 + b.m21;
    result.m22 = a.m22 + b.m22;
    return result;
}
```

### Array Operations
```cz
// Fixed-size array declaration
buffer :: int32[256];

// Array initialization with literals
fib :: int32[10] = [0, 1, 1, 2, 3, 5, 8, 13, 21, 34];

// Partial initialization (remaining elements zero-initialized)
partial :: float[5] = [1.0, 2.0, 3.0];  // [1.0, 2.0, 3.0, 0.0, 0.0]

// Array of structs initialization
points :: Point[3] = [
    Point { .x = 0.0, .y = 0.0 },
    Point { .x = 1.0, .y = 1.0 },
    Point { .x = 2.0, .y = 2.0 }
];

// Array indexing (expressions)
func get_third :: (arr :: int32[5]) -> int32 {
    return arr[2];  // Zero-based indexing
}

func set_element :: (arr :: int32[5], index :: int32, value :: int32) -> void {
    arr[index] = value;  // Index must be in bounds
}

// Array loop processing
func array_sum :: (arr :: int32[10]) -> int32 {
    sum :: int32 = 0;
    i :: int32;
    for (i = 0; i < 10; i = i + 1) {
        sum = sum + arr[i];
    }
    return sum;
}
```

### Newtype Usage (Distinct Types)
```cz
newtype int32 Meters;
newtype int32 Seconds;

func calculate_speed :: (distance :: Meters, time :: Seconds) -> float {
    // Requires explicit conversion since these are distinct types
    dist_val :: int32 = distance as int32;
    time_val :: int32 = time as int32;
    return (dist_val as float) / (time_val as float);
}
```

## Guidelines to Avoid Semantic Analysis Errors

### 1. Reference Rules
- ✅ **DO**: Initialize references at declaration: `ref :: int32& = var;`
- ❌ **DON'T**: Declare uninitialized references: `ref :: int32&;` (will fail)
- ❌ **DON'T**: Rebind references: `ref = other_var;` (references cannot be rebound)
- ❌ **DON'T**: Return reference to local variable:
  ```cz
  func bad() -> int32& {
      local :: int32 = 5;
      return local;  // ERROR: cannot return reference to local
  }
  ```

### 2. Const Correctness
- ✅ **DO**: Use `const` for read-only parameters: `func read_only(x :: const int32&)`
- ❌ **DON'T**: Assign to const lvalue: `const_var = 10;` (will fail)
- ❌ **DON'T**: Bind mutable reference to const location:
  ```cz
  const_var :: const int32 = 5;
  mut_ref :: int32& = const_var;  // ERROR
  ```
- ❌ **DON'T**: Assign to const function return:
  ```cz
  func get_const() -> const int32 { return 42; }
  get_const() = 10;  // ERROR
  ```

### 3. Type Safety with newtype/typedef
- ✅ **DO**: Use `typedef` for transparent aliases (interchangeable):
  ```cz
  typedef int32 Handle;
  h :: Handle = 5;
  x :: int32 = h;  // OK
  ```
- ❌ **DON'T**: Assume newtype supports operations without explicit handling:
  ```cz
  newtype int32 Meters;
  m1 :: Meters = 5 as Meters;
  m2 :: Meters = 10 as Meters;
  m3 :: Meters = m1 + m2;  // ERROR: newtype doesn't implicitly support arithmetic
  ```
- ✅ **DO**: Cast to/from underlying type when needed:
  ```cz
  m3 :: Meters = (m1 as int32 + m2 as int32) as Meters;
  ```

### 4. Struct Usage
- ✅ **DO**: Initialize structs with designated fields
- ✅ **DO**: Access struct members with `.` operator
- ❌ **DON'T**: Put naked expressions in struct body (only field declarations allowed)

### 5. Global Scope Only
- ✅ **DO**: Put all declarations (functions, structs, variables, typedefs, newtypes) at file scope
- ❌ **DON'T**: Try to declare variables inside function bodies (only statements allowed)
- ✅ **DO**: Inside functions, you can declare and initialize variables as statements:
  ```cz
  func example() -> int32 {
      x :: int32 = 10;  // OK: declaration as statement
      y :: int32 = x * 2;
      return y;
  }
  ```

### 6. Array Usage
- ✅ **DO**: Initialize arrays at declaration: `arr :: int32[5] = [1, 2, 3, 4, 5];`
- ❌ **DON'T**: Access arrays with out-of-bounds indices (compile-time error if detectable, otherwise undefined behavior)
- ❌ **DON'T**: Try to return arrays directly from functions (arrays decay to pointers in return context, which is not allowed)
- ✅ **DO**: Pass arrays by reference when needed: `func process(arr :: int32[10]&) -> void`
- ✅ **DO**: Use loops with explicit bounds for array iteration:
  ```cz
  func print_array :: (arr :: int32[5]) -> void {
      i :: int32;
      for (i = 0; i < 5; i = i + 1) {
          // Process arr[i]
      }
  }
  ```
- ❌ **DON'T**: Assume arrays know their length (unlike slices in some languages, CZ arrays don't carry length information)

## Testing with C Main

To test your CZ code with a C main program:

1. **Compile CZ to object file**:
   ```bash
   ./bin/czc your_file.cz output.o
   ```

2. **Create a C main file** that declares the CZ functions (using correct calling conventions):
   ```c
   // test_main.c
   #include <stdio.h>
   
   // Forward declare CZ functions (they use C calling convention)
   int32_t fibonacci(int32_t n);
   float relu(float x);
   
   int main() {
       printf("fibonacci(10) = %d\n", fibonacci(10));
       printf("relu(-3.5) = %f\n", relu(-3.5f));
       printf("relu(3.5) = %f\n", relu(3.5f));
       return 0;
   }
   ```

3. **Compile and link**:
   ```bash
   clang test_main.c output.o -o test_program
   ./test_program
   ```

## Reference Examples

See these existing test cases for guidance:
- `tests/czc_test_code.cz` - Basic example
- `tests/unittest_collection.txt` - Numerous test cases showing valid/invalid patterns
- Look for "GoodCase" and "BadCase" examples in the semantic analyzer tests

## Debugging Tips

1. **Check AST output**: The czc compiler prints the AST by default to stderr
2. **Check LLVM IR**: Use `./bin/czc file.cz -` to see generated LLVM IR
3. **Memory checking**: Use valgrind to check for leaks:
   ```bash
   valgrind --leak-check=full ./bin/czc file.cz -
   ```
4. **Error messages**: Pay attention to line and column numbers in error messages

## When Code Fails

If your CZ code fails to compile but you believe it should work:
1. Review the error message carefully (lexer, parser, or semantic analysis)
2. Check if you're violating any of the guidelines above
3. Consult the language specification in `language_summary.md`
4. If you still believe it's a language feature that should work, notify me - I'll help determine if it's a feature request or a bug to fix.