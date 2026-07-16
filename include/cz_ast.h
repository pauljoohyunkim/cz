#ifndef CZ_AST_H
#define CZ_AST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "cz_tokens.h"
#include "cz_type.h"
#include "cz_symbol_table.h"

typedef struct CZ_AST_Node CZ_AST_Node;

typedef enum {
    CZ_VALUE_CATEGORY_LVALUE,
    CZ_VALUE_CATEGORY_RVALUE
} CZ_ValueCategory;

typedef struct {
    const CZ_Type* resolved_type;
    CZ_ValueCategory value_category;
} CZ_AST_Decoration;

typedef enum {
    CZ_AST_ProgramNodeType,
    CZ_AST_StructDeclarationNodeType,
    CZ_AST_StructMemberNodeType,
    CZ_AST_FunctionDeclarationNodeType,
    CZ_AST_ParameterListNodeType,

    CZ_AST_BlockStatementNodeType,
    CZ_AST_VariableDeclarationNodeType,
    CZ_AST_TypedefDeclarationNodeType,
    CZ_AST_NewtypeDeclarationNodeType,
    CZ_AST_AssignmentStatementNodeType,
    CZ_AST_ReturnStatementNodeType,
    CZ_AST_IfStatementNodeType,
    CZ_AST_ForStatementNodeType,
    CZ_AST_WhileStatementNodeType,

    CZ_AST_TypeNodeType,

    CZ_AST_BinaryExpressionNodeType,
    CZ_AST_UnaryExpressionNodeType,
    CZ_AST_FunctionCallNodeType,
    CZ_AST_LiteralNodeType,
    CZ_AST_IdentifierNodeType,
    CZ_AST_CastExpressionNodeType,
    CZ_AST_StructMemberAccessNodeType
} CZ_AST_NodeType;

typedef struct {
    /** Parameter list node (CZ_AST_ParameterListNodeType) */
    CZ_AST_Node* parameter_list;
    /** Return type node (CZ_AST_TypeNodeType) */
    CZ_AST_Node* return_type;
} CZ_AST_Function_Encapsulation;

struct CZ_AST_Node {
    CZ_AST_NodeType node_type;
    unsigned int line;
    unsigned int col;
    CZ_AST_Decoration* decoration;

    union {
        /** Used for node_type == CZ_AST_ProgramNodeType */
        struct {
            /** Array of CZ_AST_Node* (each being a global declaration) */
            CZ_AST_Node** global_declaration_list;
            unsigned int declaration_count;
            unsigned int capacity;
        } program;

        /** Used for node_type == CZ_AST_StructDeclarationNodeType */
        struct {
            /** Identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* identifier;
            /** Array of CZ_AST_Node* (each being a struct member, CZ_AST_StructMemberNodeType) */
            CZ_AST_Node** members;
            unsigned int member_count;
        } struct_declaration;

        /** Used for node_type == CZ_AST_StructMemberNodeType */
        struct {
            /** Identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* identifier;
            /** Type node (CZ_AST_TypeNodeType) */
            CZ_AST_Node* type;
        } struct_member;

        /** Used for node_type == CZ_AST_FunctionDeclarationNodeType */
        struct {
            /** Identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* function_identifier;
            /** Function encapsulation: parameter list and return type */
            CZ_AST_Function_Encapsulation function;
            /** Body: Block statement node (CZ_AST_BlockStatementNodeType) */
            CZ_AST_Node* body;
        } function_declaration;

        /** Used for node_type == CZ_AST_ParameterListNodeType */
        struct {
            /** Array of CZ_AST_Node* (each being a parameter, likely VariableDeclaration node) */
            CZ_AST_Node** params;
            unsigned int param_count;
        } parameter_list;

        /** Used for node_type == CZ_AST_BlockStatementNodeType */
        struct {
            /** Array of CZ_AST_Node* (each being a statement) */
            CZ_AST_Node** statements;
            unsigned int statement_count;
            /** Scope for this block */
            CZ_Environment* scope;
        } statement_list;

        /** Used for node_type == CZ_AST_VariableDeclarationNodeType */
        struct {
            /** Identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* identifier;
            /** Type node (CZ_AST_TypeNodeType) */
            CZ_AST_Node* type;
            /** Initializer expression node (can be any expression) */
            CZ_AST_Node* expression;
        } variable_declaration;

        /** Used for node_type == CZ_AST_TypedefDeclarationNodeType or node_type == CZ_AST_NewtypeDeclarationNodeType */
        struct {
            /** Existing type node (CZ_AST_TypeNodeType) */
            CZ_AST_Node* type;
            /** New type name: Identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* new_type;
        } typedef_declaration;

        /** Used for node_type == CZ_AST_ReturnStatementNodeType */
        struct {
            /** Whether this is a reference return */
            bool is_ref;
            /** Expression node (can be any expression) */
            CZ_AST_Node* expression;
        } return_statement;

        /** Used for node_type == CZ_AST_IfStatementNodeType */
        struct {
            /** Condition expression node */
            CZ_AST_Node* condition;
            /** If branch statement node */
            CZ_AST_Node* if_branch;
            /** Else branch statement node (can be NULL) */
            CZ_AST_Node* else_branch;
        } if_statement;

        /** Used for node_type == CZ_AST_ForStatementNodeType */
        struct {
            /** Initialization node (expression or variable declaration) */
            CZ_AST_Node* initialization;
            /** Condition expression node */
            CZ_AST_Node* condition;
            /** Iteration step expression node */
            CZ_AST_Node* iteration_step;
            /** Body statement node */
            CZ_AST_Node* body;
        } for_statement;

        /** Used for node_type == CZ_AST_WhileStatementNodeType */
        struct {
            /** Condition expression node */
            CZ_AST_Node* condition;
            /** Body statement node */
            CZ_AST_Node* body;
        } while_statement;

        /** Used for node_type == CZ_AST_TypeNodeType */
        struct {
            /** Whether this is a const declaration */
            bool is_const;
            /** Whether this is a reference type */
            bool is_reference;
            /** Whether this is a function type */
            bool is_function_type;

            union {

                struct {
                    // for is_function_type == false
                    /** Type name (not a node) */
                    const char* name;

                    enum {
                        CZ_AST_TYPE_KIND_INT32,
                        CZ_AST_TYPE_KIND_BOOL,
                        CZ_AST_TYPE_KIND_FLOAT,
                        CZ_AST_TYPE_KIND_IDENTIFIER
                    } kind;
                } primitive;

                /** Function signature: parameter list and return type */
                CZ_AST_Function_Encapsulation function_signature;
            };
        } type_expression;

        /** Used for node_type == CZ_AST_BinaryExpressionNodeType and CZ_AST_AssignmentStatementNodeType */
        struct {
            /** Operator token type */
            CZ_TokenType op;
            /** Left expression node */
            CZ_AST_Node* left;
            /** Right expression node */
            CZ_AST_Node* right;
        } binary_expression;

        /** Used for node_type == CZ_AST_UnaryExpressionNodeType */
        struct {
            /** Operator token type */
            CZ_TokenType op;
            /** Operand expression node */
            CZ_AST_Node* operand;
        } unary_expression;

        /** Used for node_type == CZ_AST_FunctionCallNodeType */
        struct {
            /** Callee expression node (Identifier or FunctionCall) */
            CZ_AST_Node* callee;
            /** Array of expression nodes (arguments) */
            CZ_AST_Node** arguments;
            unsigned int arg_count;
        } function_call;

        /** Used for node_type == CZ_AST_LiteralNodeType */
        struct {
            /** Literal type token */
            CZ_TokenType literal_type;
            /** Lexeme text (not a node) */
            char* lexeme;
        } literal;

        /** Used for node_type == CZ_AST_IdentifierNodeType */
        struct {
            /** Identifier name (not a node) */
            char* name;
        } identifier;

        /** Used for node_type == CZ_AST_CastExpressionNodeType */
        struct {
            /** Expression node to cast (can be any expression node type) */
            CZ_AST_Node* expression;
            /** Target type node (CZ_AST_TypeNodeType) */
            CZ_AST_Node* type;
        } cast_expression;

        /** Used for node_type == CZ_AST_StructMemberAccessNodeType */
        struct {
            /** Object expression node (struct instance, can be any expression node type) */
            CZ_AST_Node* object;
            /** Member identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* member;
        } struct_member_access;
    };
};

/**
 * @brief Creates AST node from type.
 * 
 * @param node_type Type of AST Node.
 * @param line Line number of the token for the AST node
 * @param col Column number of the token for the AST node
 * @return CZ_AST_Node* Pointer to allocated CZ_AST_Node struct. NULL if unsuccessful.
 */
CZ_AST_Node* cz_ast_node_create(CZ_AST_NodeType node_type, unsigned int line, unsigned int col);

/**
 * @brief Frees AST node once passed a root.
 * 
 * @param node The root node.
 * 
 * This will recursively free the child nodes as well.
 */
void cz_ast_root_free(CZ_AST_Node* node);

/**
 * @brief Prints structure of AST tree from the root node.
 * 
 * @param node The root node.
 * @param depth Current depth. Set to zero.
 */
void cz_ast_root_print(const CZ_AST_Node* node, unsigned int depth);

#ifdef __cplusplus
}
#endif

#endif  /* CZ_AST_H */
