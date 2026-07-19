#include <llvm-c/Core.h>
#include <stdbool.h>
#include <string.h>
#include "cz_code_generator.h"

CZ_Environment_Backend* cz_environment_backend_create(void) {
    CZ_Environment_Backend* env_b = (CZ_Environment_Backend*) calloc(1, sizeof(CZ_Environment_Backend));

    return env_b;
}

void cz_environment_backend_free(CZ_Environment_Backend* env_b) {
    if (env_b != NULL) {
        for (unsigned int i = 0; i < env_b->value_count; i++) {
            free(env_b->value_map[i].var_name);
        }
        free(env_b->value_map);
        for (unsigned int i = 0; i < env_b->type_count; i++) {
            free(env_b->type_map[i].type_name);
        }
        free(env_b->type_map);
    }
    free(env_b);
}

int cz_environment_backend_push_map(CZ_Environment_Backend *env_b, const char *name, struct LLVMOpaqueValue *val) {
    char* name_cpy = NULL;
    CZ_VarName_To_LLVMValueRef* new_map = NULL;

    if (env_b == NULL || name == NULL) return 0;

    new_map = (CZ_VarName_To_LLVMValueRef *)realloc(env_b->value_map, sizeof(CZ_VarName_To_LLVMValueRef) * (env_b->value_count+1));
    if (new_map == NULL) goto error_cleanup;

    name_cpy = (char*)calloc(strlen(name) + 1, sizeof(char));
    if (name_cpy == NULL) goto error_cleanup;
    strcpy(name_cpy, name);

    env_b->value_map = new_map;
    new_map = NULL;
    env_b->value_map[env_b->value_count].var_name = name_cpy;
    env_b->value_map[env_b->value_count].ref = val;
    env_b->value_count++;

    return 1;

error_cleanup:
    free(name_cpy);
    free(new_map);
    return 0;
}

int cz_environment_backend_push_type_map(CZ_Environment_Backend *env_b, const char *name, struct LLVMOpaqueType *type) {
    char* name_cpy = NULL;
    CZ_TypeName_To_LLVMTypeRef* new_map = NULL;

    if (env_b == NULL || name == NULL) return 0;

    new_map = (CZ_TypeName_To_LLVMTypeRef *)realloc(env_b->type_map, sizeof(CZ_TypeName_To_LLVMTypeRef) * (env_b->type_count+1));
    if (new_map == NULL) goto error_cleanup;

    name_cpy = (char*)calloc(strlen(name) + 1, sizeof(char));
    if (name_cpy == NULL) goto error_cleanup;
    strcpy(name_cpy, name);

    env_b->type_map = new_map;
    new_map = NULL;
    env_b->type_map[env_b->type_count].type_name = name_cpy;
    env_b->type_map[env_b->type_count].ref = type;
    env_b->type_count++;

    return 1;

error_cleanup:
    free(name_cpy);
    free(new_map);
    return 0;
}

const LLVMValueRef cz_environment_backend_lookup(const CZ_Environment_Backend *env_b, const char *name, bool cascade) {
    if (env_b == NULL || name == NULL) return NULL;

    // Search the backend's symbol map for a matching entry.
    unsigned int len = strlen(name);
    for (unsigned int i = 0; i < env_b->value_count; ++i) {
        if (env_b->value_map[i].var_name != NULL &&
            strlen(env_b->value_map[i].var_name) == len &&
            strncmp(env_b->value_map[i].var_name, name, len) == 0) {
            return env_b->value_map[i].ref;
        }
    }

    if (env_b->parent != NULL && cascade) {
        return cz_environment_backend_lookup(env_b->parent, name, cascade);
    }

    return NULL;
}

const LLVMTypeRef cz_environment_backend_lookup_type(const CZ_Environment_Backend *env_b, const char *name, bool cascade) {
    if (env_b == NULL || name == NULL) return NULL;

    // Search the backend's symbol map for a matching entry.
    unsigned int len = strlen(name);
    for (unsigned int i = 0; i < env_b->type_count; ++i) {
        if (env_b->type_map[i].type_name != NULL &&
            strlen(env_b->type_map[i].type_name) == len &&
            strncmp(env_b->type_map[i].type_name, name, len) == 0) {
            return env_b->type_map[i].ref;
        }
    }

    if (env_b->parent != NULL && cascade) {
        return cz_environment_backend_lookup_type(env_b->parent, name, cascade);
    }

    return NULL;
}

CZ_CodeGenerator* cz_code_generator_create(CZ_SemanticAnalyzer* sa) {
    CZ_CodeGenerator* cg = NULL;
    LLVMContextRef ctx = NULL;
    LLVMModuleRef mod = NULL;
    LLVMBuilderRef builder = NULL;
    CZ_Environment_Backend* env_b = NULL;
    CZ_ErrorList* error_list = NULL;

    if (sa == NULL) goto error_cleanup;

    cg = (CZ_CodeGenerator*) calloc(1, sizeof(CZ_CodeGenerator));
    if (cg == NULL) goto error_cleanup;

    // Context
    ctx = LLVMContextCreate();
    if (ctx == NULL) goto error_cleanup;
    // Transfer ownership
    cg->ctx = ctx;
    ctx = NULL;

    // Module
    mod = LLVMModuleCreateWithNameInContext(sa->filename, cg->ctx);
    if (mod == NULL) goto error_cleanup;
    // Transfer ownership
    cg->mod = mod;
    mod = NULL;

    // Builder
    builder = LLVMCreateBuilderInContext(cg->ctx);
    if (builder == NULL) goto error_cleanup;
    cg->builder = builder;
    builder = NULL;

    // Backend environment.
    env_b = cz_environment_backend_create();
    if (env_b == NULL) goto error_cleanup;
    cg->global_env_b = env_b;
    env_b = NULL;

    // Error List
    error_list = cz_error_list_create();
    if (error_list == NULL) goto error_cleanup;
    cg->error_list = error_list;
    error_list = NULL;

    // Filename is owned by runtime and not CZ.
    cg->filename = sa->filename;

    // Move the info from sa to cg so that it does not cause double free.
    cg->program = sa->program;
    sa->program = NULL;
    cg->global_env = sa->global_env;
    sa->global_env = NULL;

    return cg;

error_cleanup:
    LLVMContextDispose(ctx);
    LLVMDisposeModule(mod);
    LLVMDisposeBuilder(builder);
    cz_environment_backend_free(env_b);
    cz_error_list_free(error_list);
    cz_code_generator_free(cg);
    return NULL;
}

void cz_code_generator_free(CZ_CodeGenerator* cg) {
    if (cg != NULL) {
        LLVMDisposeBuilder(cg->builder);
        LLVMDisposeModule(cg->mod);
        LLVMContextDispose(cg->ctx);
        cz_ast_root_free(cg->program);
        cz_environment_backend_free(cg->global_env_b);
        cz_environment_free(cg->global_env);
        cz_error_list_free(cg->error_list);
    }
    free(cg);
    LLVMShutdown();
}
