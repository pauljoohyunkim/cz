#ifndef CZ_TYPE_H
#define CZ_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct CZ_Type CZ_Type;

typedef enum {
    CZ_PRIMITIVE_VOID,
    CZ_PRIMITIVE_INT32,
    CZ_PRIMITIVE_FLOAT
} CZ_PrimitiveType;

typedef enum {
    CZ_TYPE_KIND_PRIMITIVE,
    CZ_TYPE_KIND_STRUCT,
    CZ_TYPE_KIND_REFERENCE,
    CZ_TYPE_KIND_NEWTYPE,
    CZ_TYPE_KIND_FUNCTION
} CZ_TypeKind;

typedef struct {
    const char* name;
    const CZ_Type* type;
    unsigned int idx;
} CZ_StructField;

typedef struct {
    CZ_StructField* fields;
    unsigned int field_count;
} CZ_StructLayout;

struct CZ_Type {
    CZ_TypeKind kind;
    bool is_const;

    union {
        CZ_PrimitiveType primitive;

        struct {
            const char* name;
            const CZ_StructLayout* layout;
        } structure;

        CZ_Type* reference_to;

        struct {
            const char* name;
            CZ_Type* underlying;
        } newtype;

        struct {
            CZ_Type* return_type;
            CZ_Type** param_types;
            unsigned int param_count;
        } function;
    };
};

/**
 * @brief Create CZ_Type
 * 
 * @param primitive Primitive type
 * @param is_const Constness of type
 * @return CZ_Type* Allocated CZ_Type on success, NULL on failure.
 */
CZ_Type* cz_type_create(CZ_PrimitiveType primitive, bool is_const);

/**
 * @brief Free CZ_Type
 * 
 * @param type Pointer to CZ_Type
 * 
 * This does not free any internal CZ_Type, so that it can be used with global type table. (Shallow free)
 * Freeing global type table will automatically free all the types.
 */
void cz_type_free(CZ_Type* type);

#ifdef __cplusplus
}
#endif

#endif  /* CZ_TYPE_H */
