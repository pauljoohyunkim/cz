#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cz_type.h"

#define NULL_POINTER_ERROR_HANDLE(ptr) do { if ((ptr) == NULL) goto error_cleanup; } while (0)

CZ_Type* cz_type_create(CZ_TypeKind type_kind) {
    CZ_Type* type = NULL;
    
    type = (CZ_Type*) calloc(1, sizeof(CZ_Type));
    NULL_POINTER_ERROR_HANDLE(type);

    type->kind = type_kind;

    return type;

error_cleanup:
    free(type);
    return NULL;
}

CZ_StructLayout* cz_struct_layout_create(unsigned int field_count) {
    CZ_StructLayout* struct_layout = NULL;
    CZ_StructField* fields = NULL;

    struct_layout = (CZ_StructLayout*) calloc(1, sizeof(CZ_StructLayout));
    NULL_POINTER_ERROR_HANDLE(struct_layout);

    fields = (CZ_StructField*) calloc(field_count, sizeof(CZ_StructField));
    NULL_POINTER_ERROR_HANDLE(fields);

    struct_layout->fields = fields;
    fields = NULL;
    struct_layout->field_count = field_count;

    return struct_layout;

error_cleanup:
    cz_struct_layout_free(struct_layout);
    free(fields);
    return NULL;
}

void cz_struct_layout_free(CZ_StructLayout* layout) {
    if (layout != NULL) {
        for (unsigned int i = 0; i < layout->field_count; i++) {
            //free((void*)layout->fields[i].name);
            layout->fields[i].name = NULL;

            // Type is owned by type table.
            layout->fields[i].type = NULL;

            layout->fields[i].default_initializer = NULL;
        }
        free(layout->fields);
        layout->fields = NULL;
    }
    free(layout);
}

void cz_type_free(CZ_Type* type) {
    if (type != NULL) {
        switch (type->kind) {
            case CZ_TYPE_KIND_PRIMITIVE:
                // Nothing to do
                break;
            case CZ_TYPE_KIND_STRUCT:
                //free((void*)type->structure.name);
                type->structure.name = NULL;
                cz_struct_layout_free((void*)type->structure.layout);
                type->structure.layout = NULL;
                break;
            case CZ_TYPE_KIND_REFERENCE:
            case CZ_TYPE_KIND_CONST:
                // Nothing to do
                break;
            case CZ_TYPE_KIND_NEWTYPE:
                //free((void*)type->newtype.name);
                type->newtype.name = NULL;
                break;
            case CZ_TYPE_KIND_FUNCTION:
                free(type->function.param_types);
                type->function.param_types = NULL;
                break;
        }
    }
    free(type);
}

CZ_GlobalTypeTable* cz_global_type_table_create(void) {
    CZ_GlobalTypeTable* gtt = NULL;
    CZ_Type** named_types = NULL;
    CZ_Type** all_allocations = NULL;
    CZ_Type* primitive_type = NULL;
    const char** names = NULL;

    gtt = (CZ_GlobalTypeTable*) calloc(1, sizeof(CZ_GlobalTypeTable));
    NULL_POINTER_ERROR_HANDLE(gtt);

    gtt->all_allocations_capacity = 8;
    gtt->named_entry_capacity = 8;

    // Allocate
    named_types = (CZ_Type**) calloc(gtt->named_entry_capacity, sizeof(CZ_Type*));
    NULL_POINTER_ERROR_HANDLE(named_types);
    all_allocations = (CZ_Type**) calloc(gtt->all_allocations_capacity, sizeof(CZ_Type*));
    NULL_POINTER_ERROR_HANDLE(all_allocations);
    names = (const char**) calloc(gtt->named_entry_capacity, sizeof(const char*));

    // Link them
    gtt->named_types = (const CZ_Type**) named_types;
    named_types = NULL;
    gtt->all_allocations = (const CZ_Type**) all_allocations;
    all_allocations = NULL;
    gtt->names = names;
    names = NULL;

    // Create int32
    {
        primitive_type = cz_type_create(CZ_TYPE_KIND_PRIMITIVE);
        primitive_type->primitive = CZ_PRIMITIVE_INT32;
        if (cz_global_type_table_push_type(gtt, "int32", primitive_type) != 1) {
            goto error_cleanup;
        }
        primitive_type = NULL;
    }
    // Create bool
    {
        primitive_type = cz_type_create(CZ_TYPE_KIND_PRIMITIVE);
        primitive_type->primitive = CZ_PRIMITIVE_BOOL;
        if (cz_global_type_table_push_type(gtt, "bool", primitive_type) != 1) {
            goto error_cleanup;
        }
        primitive_type = NULL;
    }
    // Create float
    {
        primitive_type = cz_type_create(CZ_TYPE_KIND_PRIMITIVE);
        primitive_type->primitive = CZ_PRIMITIVE_FLOAT;
        if (cz_global_type_table_push_type(gtt, "float", primitive_type) != 1) {
            goto error_cleanup;
        }
        primitive_type = NULL;
    }


    return gtt;

error_cleanup:
    free(gtt);
    free(named_types);
    free(all_allocations);
    free(names);
    cz_type_free(primitive_type);
    return NULL;
}

void cz_global_type_table_free(CZ_GlobalTypeTable* gtt) {
    if (gtt != NULL) {
        for (unsigned int i = 0; i < gtt->named_entry_count; i++) {
            //free((void*)gtt->names[i]);
            gtt->names[i] = NULL;
            // Do not free types inside named_types.
        }
        free(gtt->names);
        free(gtt->named_types);
        for (unsigned int i = 0; i < gtt->all_entry_count; i++) {
            cz_type_free((CZ_Type*)gtt->all_allocations[i]);
            gtt->all_allocations[i] = NULL;
        }
        free(gtt->all_allocations);
        gtt->all_allocations = NULL;
    }
    free(gtt);
}

int cz_global_type_table_push_type(CZ_GlobalTypeTable* gtt, const char* name, const CZ_Type* type) {
    const char** new_names = NULL;
    const CZ_Type** new_named_types = NULL;
    const CZ_Type** new_all_allocations = NULL;
    NULL_POINTER_ERROR_HANDLE(gtt);
    NULL_POINTER_ERROR_HANDLE(type);

    // Check if type already exists.
    // If not, add.
    // Otherwise, skip.
    const CZ_Type* lookup_type = cz_global_type_table_find_type(gtt, type);
    if (lookup_type == NULL) {
        // All allocation
        if (gtt->all_allocations_capacity == gtt->all_entry_count) {
            new_all_allocations = realloc(gtt->all_allocations, sizeof(const CZ_Type*) * (gtt->all_allocations_capacity) * 2);
            NULL_POINTER_ERROR_HANDLE(new_all_allocations);

            gtt->all_allocations = (const CZ_Type**) new_all_allocations;
            new_all_allocations = NULL;
            gtt->all_allocations_capacity *= 2;
        }
        gtt->all_allocations[gtt->all_entry_count] = (CZ_Type*) type;
        gtt->all_entry_count++;

        // Share the pointer so that it can be used for named type adding.
        lookup_type = type;
    }

    // TODO: Fail when another push with the same name.
    if (name != NULL) {
        if (gtt->named_entry_count == gtt->named_entry_capacity) {
            new_names = (const char**) realloc(gtt->names, sizeof(const char*) * (gtt->named_entry_capacity) * 2);
            NULL_POINTER_ERROR_HANDLE(new_names);

            new_named_types = (const CZ_Type**) realloc(gtt->named_types, sizeof(const CZ_Type*) * (gtt->named_entry_capacity) * 2);
            NULL_POINTER_ERROR_HANDLE(new_named_types);

            gtt->names = new_names;
            new_names = NULL;
            gtt->named_types = (const CZ_Type**) new_named_types;
            new_named_types = NULL;
            gtt->named_entry_capacity *= 2;
        }

        gtt->named_types[gtt->named_entry_count] = (CZ_Type*) lookup_type;
        gtt->names[gtt->named_entry_count] = name;
        gtt->named_entry_count++;
    }

    return 1;
    
error_cleanup:
    free(new_names);
    free(new_named_types);
    free(new_all_allocations);
    return 0;
}

bool cz_type_equals(const CZ_Type* a, const CZ_Type* b) {
    if (a == b) return true; // Fast-path: identical pointers
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;

    switch (a->kind) {
        case CZ_TYPE_KIND_PRIMITIVE:
            return a->primitive == b->primitive;

        case CZ_TYPE_KIND_REFERENCE:
            // Recursively compare the pointed-to types
            return cz_type_equals(a->reference_to, b->reference_to);

        case CZ_TYPE_KIND_CONST:
            return cz_type_equals(a->const_of, b->const_of);

        case CZ_TYPE_KIND_NEWTYPE:
            // Newtypes are unique by name
            return strcmp(a->newtype.name, b->newtype.name) == 0;

        case CZ_TYPE_KIND_STRUCT:
            // Structs are nominal in C-like languages; unique by name
            return strcmp(a->structure.name, b->structure.name) == 0;

        case CZ_TYPE_KIND_FUNCTION: {
            if (a->function.param_count != b->function.param_count) return false;
            if (!cz_type_equals(a->function.return_type, b->function.return_type)) return false;
            
            for (unsigned int i = 0; i < a->function.param_count; i++) {
                if (!cz_type_equals(a->function.param_types[i], b->function.param_types[i])) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

const CZ_Type* cz_global_type_table_find_type(const CZ_GlobalTypeTable* gtt, const CZ_Type* query) {
    if (!gtt || !query) return NULL;

    for (unsigned int i = 0; i < gtt->all_entry_count; i++) {
        if (cz_type_equals(gtt->all_allocations[i], query)) {
            return gtt->all_allocations[i]; // Return the existing canonical instance
        }
    }
    return NULL;
}

const CZ_Type* cz_global_type_table_find_type_by_name(const CZ_GlobalTypeTable* gtt, const char* query) {
    if (!gtt || !query) return NULL;

    for (unsigned int i = 0; i < gtt->named_entry_count; i++) {
        if (strcmp(query, gtt->names[i]) == 0) {
            return gtt->named_types[i];
        }
    }

    return NULL;
}

void cz_global_type_table_print(const CZ_GlobalTypeTable* gtt) {
    if (gtt == NULL) {
        printf("NULL global type table\n");
        return;
    }

    printf("Global Type Table:\n");
    printf("  Named entries (%u/%u):\n", gtt->named_entry_count, gtt->named_entry_capacity);
    for (unsigned int i = 0; i < gtt->named_entry_count; i++) {
        printf("    [%u] Name: %s, Type: %p\n", i, gtt->names[i], (void*)gtt->named_types[i]);
    }
    printf("  All allocations (%u/%u):\n", gtt->all_entry_count, gtt->all_allocations_capacity);
    for (unsigned int i = 0; i < gtt->all_entry_count; i++) {
        printf("    [%u] Type: %p\n", i, (void*)gtt->all_allocations[i]);
    }
}

bool cz_type_is_const(const CZ_Type* type) {
    if (type == NULL) return false;
    if (type->kind == CZ_TYPE_KIND_CONST) return true;
    
    if (type->kind == CZ_TYPE_KIND_REFERENCE) {
        return cz_type_is_const(type->reference_to);
    }
    // TODO: Const array type->array_of here too
    
    return false;
}

bool cz_primitive_type_is_numerical(CZ_PrimitiveType primitive_type) {
    switch (primitive_type) {
        case CZ_PRIMITIVE_FLOAT:
        case CZ_PRIMITIVE_INT32:
            return true;
        default:
            return false;
    }
    return false;
}

const CZ_Type* cz_type_decay_type(const CZ_Type* type) {
    if (type->kind == CZ_TYPE_KIND_REFERENCE) {
        type = type->reference_to;
    }
    if (type->kind == CZ_TYPE_KIND_CONST) {
        type = type->const_of;
    }
    return type;
}
