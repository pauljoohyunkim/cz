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
    char* var_name;

    LLVMValueRef ref;
} CZ_VarName_To_LLVMValueRef;

typedef struct {
    char* type_name;

    LLVMTypeRef ref;
} CZ_TypeName_To_LLVMTypeRef;

struct CZ_Environment_Backend {
    CZ_Environment_Backend* parent;

    CZ_VarName_To_LLVMValueRef* value_map;
    size_t value_count;

    CZ_TypeName_To_LLVMTypeRef* type_map;
    size_t type_count;
};

typedef struct {
    LLVMContextRef ctx;
    LLVMModuleRef mod;
    LLVMBuilderRef builder;

    CZ_AST_Node* program;
    CZ_Environment* global_env;
    CZ_Environment_Backend* global_env_b;

    const char* filename;

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
 * @brief Push a mapping (null terminated var name to LLVM reference.)
 *
 * @param env_b Pointer to CZ_Environment_Backend
 * @param name Null-terminated variable name
 * @param val LLVMValueRef (or LLVMTypeRef if needed)
 * @return int 1 for success, 0 for failure.
 */
int cz_environment_backend_push_map(CZ_Environment_Backend* env_b, const char* name, struct LLVMOpaqueValue* val);

/**
 * @brief Push a type mapping (null terminated type name to LLVM type reference.)
 *
 * @param env_b Pointer to CZ_Environment_Backend
 * @param name Null-terminated type name
 * @param type LLVMTypeRef
 * @return int 1 for success, 0 for failure.
 */
int cz_environment_backend_push_type_map(CZ_Environment_Backend* env_b, const char* name, struct LLVMOpaqueType* type);

/**
 * @brief Look up LLVMValueRef from backend table.
 *
 * @param env_b Pointer to CZ_Environment_Backend
 * @param name Null-terminated variable name
 * @param cascade Flag for whether or not to look up parent chain.
 * @return const LLVMValueRef LLVMValueRef if found, NULL otherwise.
 */
const LLVMValueRef cz_environment_backend_lookup(const CZ_Environment_Backend *env_b, const char *name, bool cascade);

/**
 * @brief Look up LLVMTypeRef from backend table.
 *
 * @param env_b Pointer to CZ_Environment_Backend
 * @param name Null-terminated variable name
 * @param cascade Flag for whether or not to look up parent chain.
 * @return const LLVMTypeRef LLVMTypeRef if found, NULL otherwise.
 */
const LLVMTypeRef cz_environment_backend_lookup_type(const CZ_Environment_Backend *env_b, const char *name, bool cascade);

/**
 * @brief Create CZ_CodeGenerator struct dynamically
 *
 * @param sa Pointer to CZ_SemanticAnalyzer struct.
 * @return CZ_CodeGenerator* Pointer to allocated CZ_SemanticAnalyzer struct if success. NULL for failure.
 */
CZ_CodeGenerator* cz_code_generator_create(CZ_SemanticAnalyzer* sa);

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
