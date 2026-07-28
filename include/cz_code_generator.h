#ifndef CZ_CODE_GENERATOR_H
#define CZ_CODE_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <llvm-c/Core.h>
#include <stdbool.h>
#include "cz_ast.h"
#include "cz_error.h"
#include "cz_semantic_analyzer.h"

typedef struct CZ_Environment_Backend CZ_Environment_Backend;

typedef struct {
    const CZ_Symbol* symbol;

    LLVMValueRef ref;
} CZ_Symbol_To_LLVMValueRef;

typedef struct {
    const CZ_Type* type_name;

    LLVMTypeRef ref;
} CZ_Type_To_LLVMTypeRef;

struct CZ_Environment_Backend {
    CZ_Environment_Backend* parent;

    CZ_Symbol_To_LLVMValueRef* value_map;
    size_t value_count;

    CZ_Type_To_LLVMTypeRef* type_map;
    size_t type_count;
};

typedef struct {
    LLVMContextRef ctx;
    LLVMModuleRef mod;
    LLVMBuilderRef builder;

    CZ_AST_Node* program;
    CZ_Environment* global_env;
    CZ_Environment_Backend* global_env_b;
    CZ_GlobalTypeTable* gtt;
    CZ_StringPool* sp;

    const CZ_Type* current_function_return;

    const char* filename;
    const char* code;
    size_t code_length;


    CZ_ErrorList* error_list;
} CZ_CodeGenerator;

/**
 * @brief Create CZ_Environment_Backend dynamically.
 *
 * @return CZ_Environment_Backend* Pointer to allocated CZ_Environment_Backend struct upon success, or NULL for failure.
 *
 * This is a list of mapping from variable names to LLVMValueRefs.
 */
CZ_Environment_Backend* cz_environment_backend_create(void);

/**
 * @brief Free the allocated CZ_Environment_Backend and its members.
 *
 * @param env_b Pointer to CZ_Environment_Backend.
 */
void cz_environment_backend_free(CZ_Environment_Backend* env_b);

/**
 * @brief Push a mapping (symbol to LLVM reference.)
 *
 * @param env_b Pointer to CZ_Environment_Backend
 * @param symbol Pointer to CZ_Symbol (owned by symbol table)
 * @param llvmval LLVMValueRef
 * @return int 1 for success, 0 for failure.
 */
int cz_environment_backend_push_val_map(CZ_Environment_Backend* env_b, const CZ_Symbol* symbol, LLVMValueRef llvmval);

/**
 * @brief Push a type mapping (type to LLVM type reference.)
 *
 * @param env_b Pointer to CZ_Environment_Backend
 * @param type Pointer to CZ_Type (owned by global type table)
 * @param llvmtype LLVMTypeRef
 * @return int 1 for success, 0 for failure.
 */
int cz_environment_backend_push_type_map(CZ_Environment_Backend* env_b, const CZ_Type* type, LLVMTypeRef llvmtype);

/**
 * @brief Look up LLVMValueRef from backend table.
 *
 * @param env_b Pointer to CZ_Environment_Backend
 * @param symbol Pointer to CZ_Symbol
 * @param cascade Flag for whether or not to look up parent chain.
 * @return const LLVMValueRef LLVMValueRef if found, NULL otherwise.
 */
const LLVMValueRef cz_environment_backend_lookup_val(const CZ_Environment_Backend *env_b, const CZ_Symbol* symbol, bool cascade);

/**
 * @brief Look up LLVMTypeRef from backend table.
 *
 * @param cg Pointer to the CZ_CodeGenerator struct.
 * @param type Pointer to the CZ_Type to look up.
 * @return LLVMTypeRef The LLVM type if found, NULL otherwise.
 */
const LLVMTypeRef cz_environment_backend_lookup_type(const CZ_CodeGenerator* cg, const CZ_Type* type);

/**
 * @brief Create CZ_CodeGenerator struct dynamically
 *
 * @param sa Pointer to CZ_SemanticAnalyzer struct.
 * @return CZ_CodeGenerator* Pointer to allocated CZ_SemanticAnalyzer struct if success. NULL for failure.
 */
CZ_CodeGenerator* cz_code_generator_create(CZ_SemanticAnalyzer* sa);

/**
 * @brief LLVM Code Generation
 *
 * @param cg Pointer to CZ_CodeGenerator struct
 * @return int 1 if successful, 0 if failure.
 */
int cz_code_generator_generate(CZ_CodeGenerator* cg);

/**
 * @brief Emit LLVM module to object file.
 *
 * @param module LLVMModule inside CZ_CodeGenerator struct.
 * @param output_filename Output filename (*.o)
 * @return int 1 if successful, 0 otherwise.
 */
int cz_code_generator_emit_object_file(LLVMModuleRef module, const char* output_filename);

/**
 * @brief Free the memory allocated for CZ_CodeGenerator
 *
 * @param cg Pointer to allocated CZ_CodeGenerator.
 */
void cz_code_generator_free(CZ_CodeGenerator* cg);

#ifdef __cplusplus
}
#endif

#endif  /* CZ_CODE_GENERATOR_H */
