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
    CZ_PRIMITIVE_BOOL,
    CZ_PRIMITIVE_FLOAT
} CZ_PrimitiveType;

typedef enum {
    CZ_TYPE_KIND_PRIMITIVE,
    CZ_TYPE_KIND_STRUCT,
    CZ_TYPE_KIND_CONST,
    CZ_TYPE_KIND_REFERENCE,
    CZ_TYPE_KIND_NEWTYPE,
    CZ_TYPE_KIND_FUNCTION,
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

    union {
        CZ_PrimitiveType primitive;

        struct {
            const char* name;
            const CZ_StructLayout* layout;
        } structure;

        const CZ_Type* const_of;

        const CZ_Type* reference_to;

        struct {
            const char* name;
            const CZ_Type* underlying;
        } newtype;

        struct {
            const CZ_Type* return_type;
            const CZ_Type** param_types;
            unsigned int param_count;
        } function;
    };
};

typedef struct {
    const char** names;
    const CZ_Type** named_types;  // Shallow pointer! points to somewhere in all_allocations
    unsigned int named_entry_count;
    unsigned int named_entry_capacity;

    const CZ_Type** all_allocations;      // Owner of all types!
    unsigned int all_entry_count;
    unsigned int all_allocations_capacity;
} CZ_GlobalTypeTable;

/**
 * @brief Create CZ_Type
 * 
 * @param typekind CZ_Type kind
 * @return CZ_Type* Allocated CZ_Type on success, NULL on failure.
 */
CZ_Type* cz_type_create(CZ_TypeKind typekind);

/**
 * @brief Create CZ_StructLayout and the fields inside.
 * 
 * @param field_count Number of fields
 * @return CZ_StructLayout* Pointer to CZ_StructLayout on success, NULL on failure.
 */
CZ_StructLayout* cz_struct_layout_create(unsigned int field_count);

/**
 * @brief Free CZ_StructLayout
 * 
 * @param layout Pointer to CZ_StructLayout
 */
void cz_struct_layout_free(CZ_StructLayout* layout);

/**
 * @brief Free CZ_Type
 * 
 * @param type Pointer to CZ_Type on success, NULL on failure.
 * 
 * This does not free any internal CZ_Type, so that it can be used with global type table. (Shallow free)
 * Freeing global type table will automatically free all the types.
 */
void cz_type_free(CZ_Type* type);

/**
 * @brief Create global type table
 * 
 * @return CZ_GlobalTypeTable* Pointer to CZ_GlobalTypeTable on success, NULL on failure.
 */
CZ_GlobalTypeTable* cz_global_type_table_create(void);

/**
 * @brief Free CZ_GlobalTypeTable
 * 
 * @param gtt Pointer to CZ_GlobalTypeTable
 */
void cz_global_type_table_free(CZ_GlobalTypeTable* gtt);

/**
 * @brief Push a type to global type table.
 * 
 * @param gtt Pointer to CZ_GlobalTypeTable
 * @param name Name (if NULL, it will be an unnamed type). Will copy internally.
 * @param type Type to push. (Do not free after pushing, as it is just the ownership transfer)
 * 
 * @return int 1 on success, 0 on failure.
 * 
 * Note that you have to check if type exists before pushing.
 */
int cz_global_type_table_push_type(CZ_GlobalTypeTable* gtt, const char* name, const CZ_Type* type);

/**
 * @brief Find an existing identical type in the global type table.
 * 
 * @param gtt Pointer to CZ_GlobalTypeTable
 * @param query The type template to search for (e.g., a temporary ref type)
 * @return const CZ_Type* Pointer to the existing canonical type, or NULL if not found.
 */
const CZ_Type* cz_global_type_table_find_type(const CZ_GlobalTypeTable* gtt, const CZ_Type* query);

/**
 * @brief Find an existing identical type in the global type table.
 * 
 * @param gtt Pointer to CZ_GlobalTypeTable
 * @param query The name to search for.
 * @return const CZ_Type* Pointer to the existing canonical type, or NULL if not found.
 */
const CZ_Type* cz_global_type_table_find_type_by_name(const CZ_GlobalTypeTable* gtt, const char* query);

#ifdef __cplusplus
}
#endif

#endif  /* CZ_TYPE_H */
