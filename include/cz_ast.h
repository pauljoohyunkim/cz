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
    bool is_constexpr;
    bool is_reference_source;
    unsigned int scope_level;
    bool is_escapable_ref;      // Whether or not if it is a reference to a location outside of a local function scope.
} CZ_AST_Decoration;

typedef enum {
    CZ_AST_ProgramNodeType,
    CZ_AST_StructDeclarationNodeType,
    CZ_AST_StructInitNodeType,
    CZ_AST_StructInitMemberNodeType,
    CZ_AST_ArrayInitNodeType,
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
        /** Used for node_type == CZ_AST_ProgramNodeType (top-level/program node)
         *  Contains a list of globally-scoped declarations:
         *   - CZ_AST_FunctionDeclarationNode
         *   - CZ_AST_StructDeclarationNode  
         *   - CZ_AST_VariableDeclarationNode (global variables only)
         *   - CZ_AST_TypedefDeclarationNode / CZ_AST_NewtypeDeclarationNode */
        struct {
            /** Array of global declaration nodes (each being one of the above types) */
            CZ_AST_Node** global_declaration_list;
            unsigned int declaration_count;
            unsigned int capacity;
        } program;

        /** Used for node_type == CZ_AST_StructDeclarationNodeType or CZ_AST_StructInitNodeType */
        struct {
            /** Identifier node (CZ_AST_IdentifierNodeType)
             *  For StructDecl: the name of the struct being declared
             *  For StructInit: the type name being initialized, or NULL for anonymous init */
            CZ_AST_Node* identifier;
            /** Array of CZ_AST_Node*
             *  For StructDecl: each item is a variable_declaration (field type + field name)
             *  For StructInit: each item is a struct_init_member (.field = value_expr) */
            CZ_AST_Node** members;
            unsigned int member_count;
        } struct_declaration;

        /** Used for node_type == CZ_AST_ArrayInitNodeType */
        struct {
            CZ_AST_Node** elements;
            unsigned int element_count;
        } array_init;

        /** Used for node_type == CZ_AST_StructInitMemberNodeType
         * Represents a named field assignment in a struct initializer, e.g. `.field = value`
         * Accessed as: `node->struct_init_member.identifier` and `node->struct_init_member.expression` */
        struct {
            /** Field name: Identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* identifier;
            /** Field initializer: expression node */
            CZ_AST_Node* expression;
        } struct_init_member;

        /** Used for node_type == CZ_AST_FunctionDeclarationNodeType */
        struct {
            /** Function name: Identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* function_identifier;
            /** Function encapsulation info
             *  parameter_list -> CZ_AST_ParameterListNodeType
             *  return_type   -> CZ_AST_TypeNodeType */
            CZ_AST_Function_Encapsulation function;
            /** Body: Block statement node (CZ_AST_BlockStatementNodeType) */
            CZ_AST_Node* body;
        } function_declaration;

        /** Used for node_type == CZ_AST_ParameterListNodeType */
        struct {
            /** Array of CZ_AST_Node* - each item is a parameter (CZ_AST_VariableDeclarationNodeType) */
            CZ_AST_Node** params;
            unsigned int param_count;
            /** Semantic scope for this parameter list */
            CZ_Environment* scope;
        } parameter_list;

        /** Used for node_type == CZ_AST_BlockStatementNodeType */
        struct {
            /** Array of CZ_AST_Node* - each item is a statement:
             *  VariableDecl, AssignmentStmt, ReturnStmt, IfStmt, ForStmt, WhileStmt, BlockStmt, or Expression */
            CZ_AST_Node** statements;
            unsigned int statement_count;
            /** Semantic scope for this block */
            CZ_Environment* scope;
        } statement_list;

        /** Used for node_type == CZ_AST_VariableDeclarationNodeType */
        struct {
            /** Variable name: Identifier node (CZ_AST_IdentifierNodeType) */
            CZ_AST_Node* identifier;
            /** Type annotation: type node (CZ_AST_TypeNodeType) specifying the declared type */
            CZ_AST_Node* type;
            /** Optional initializer expression (can be any expr node, or NULL if no initializer) */
            CZ_AST_Node* expression;
        } variable_declaration;

        /** Used for node_type == CZ_AST_TypedefDeclarationNodeType or node_type == CZ_AST_NewtypeDeclarationNodeType */
        struct {
            /** Source type: type node (CZ_AST_TypeNodeType) - the existing/type being aliased/wrapped */
            CZ_AST_Node* type;
            /** New type name: Identifier node (CZ_AST_IdentifierNodeType) - alias or wrapper name */
            CZ_AST_Node* new_type;
        } typedef_declaration;

        /** Used for node_type == CZ_AST_ReturnStatementNodeType */
        struct {
            /** Return value expression (any expression node, or NULL if returning void) */
            CZ_AST_Node* expression;
        } return_statement;

        /** Used for node_type == CZ_AST_IfStatementNodeType */
        struct {
            /** Condition: boolean-type expression node */
            CZ_AST_Node* condition;
            /** "then" branch: Block statement node (CZ_AST_BlockStatementNodeType) */
            CZ_AST_Node* if_branch;
            /** Optional else branch: can be BlockStatementNode, IfStatementNode (for else-if chain), or NULL */
            CZ_AST_Node* else_branch;
        } if_statement;

        /** Used for node_type == CZ_AST_ForStatementNodeType */
        struct {
            /** Loop initialization: VariableDeclarationNode or AssignmentStatementNode, or NULL */
            CZ_AST_Node* initialization;
            /** Loop condition: expression node (or NULL if none) */
            CZ_AST_Node* condition;
            /** Loop iteration step: typically AssignmentStatementNode or any expr node, or NULL */
            CZ_AST_Node* iteration_step;
            /** Loop body: Block statement node (CZ_AST_BlockStatementNodeType) */
            CZ_AST_Node* body;

            /** Semantic scope for variables declared within this for-loop */
            CZ_Environment* scope;
        } for_statement;

        /** Used for node_type == CZ_AST_WhileStatementNodeType */
        struct {
            /** Loop condition: expression node */
            CZ_AST_Node* condition;
            /** Loop body: Block statement node (CZ_AST_BlockStatementNodeType) */
            CZ_AST_Node* body;
        } while_statement;

        /** Used for node_type == CZ_AST_TypeNodeType */
        struct {
            /* The following boolean flags describe type qualifiers */
            /** Whether this is a const-declared lvalue (e.g. "const int") */
            bool is_const;
            /** Whether this is a reference type (e.g. "int&", compiled as pointer) */
            bool is_reference;
            /** For function types: whether union member holds `function_signature` vs `primitive` */
            bool is_function_type;
            /** For compile-time (fixed-size) arrays (e.g. int32[10])*/
            bool is_array;
            /** For dynamic runtime lists (e.g. int32[]) */
            bool is_list;

            union {
                /* -- For primitive/base types: VOID, INT32, BOOL, FLOAT, or user-defined identifier -- */
                struct {
                    /** Type name literal (stored from token lexeme, NOT an AST node). NULL for void type. */
                    const char* name;

                    /** Type category/kind */
                    enum {
                        CZ_AST_TYPE_KIND_VOID,
                        CZ_AST_TYPE_KIND_INT32,
                        CZ_AST_TYPE_KIND_UINT32,
                        CZ_AST_TYPE_KIND_BOOL,
                        CZ_AST_TYPE_KIND_FLOAT,
                        /** Named type (struct, newtype, typedef identifier) -- resolved later during semantic analysis */
                        CZ_AST_TYPE_KIND_IDENTIFIER
                    } kind;
                } primitive;

                /* -- For function types only: -- */
                /** Function signature (parameter list + return type)
                 *  Used when this node represents a function's type in the parameter or return type context. */
                CZ_AST_Function_Encapsulation function_signature;

                struct {
                    /** Element type node */
                    CZ_AST_Node* element_type;

                    /** Compile-time evaluated expression or constant size for arrays.
                     * Arrays: Const size (e.g. 10)
                     * Lists: NULL
                     */
                    CZ_AST_Node* size_expr;
                } array;
            };
        } type_expression;

        /** Used for node_type == CZ_AST_BinaryExpressionNodeType
         *     and node_type == CZ_AST_AssignmentStatementNodeType (shared structure).
         *  The op field distinguishes them:
         *    - For BinaryExpr: +, -, *, /, %, ==, !=, <, >, <=, >=, |, &, ^
         *    - For AssignmentStmt: =, +=, -=, etc. (cz_token_type_is_assignment(op) returns true) */
        struct {
            /** Operator token type */
            CZ_TokenType op;
            /** Left-hand operand: any expression node */
            CZ_AST_Node* left;
            /** Right-hand operand: any expression node */
            CZ_AST_Node* right;
        } binary_expression;

        /** Used for node_type == CZ_AST_UnaryExpressionNodeType
         *  Created by parsing unary operators: -, ! (from parser_create_unary) */
        struct {
            /** Operator token type (- or !) */
            CZ_TokenType op;
            /** Operand expression node (any expression) */
            CZ_AST_Node* operand;
        } unary_expression;

        /** Used for node_type == CZ_AST_FunctionCallNodeType
         *  Represents a function call: `callee(args...)`
         *  The callee can be:
         *   - An IdentifierNode (named function) 
         *   - Or another expression yielding a callable */
        struct {
            /** Callee expression node (Identifier or other expression yielding a function) */
            CZ_AST_Node* callee;
            /** Array of argument expressions (all rvalues typically)
             *  Argument types depend on the declared parameter types */
            CZ_AST_Node** arguments;
            unsigned int arg_count;
        } function_call;

        /** Used for node_type == CZ_AST_LiteralNodeType
         *  Literal value created during primary expression parsing. Types:
         *   - CZ_TT_NUMERICAL_LITERAL (integers)
         *   - CZ_TT_STRING_LITERAL (strings, TODO: not fully supported)
         *   - CZ_TT_TRUE / CZ_TT_FALSE (bool literals) */
        struct {
            /** Literal kind token type */
            CZ_TokenType literal_type;
            /** Literal's lexeme text (from token, NOT a separate AST node). 
             *  E.g. "42", "hello", "true" */
            const char* lexeme;
        } literal;

        /** Used for node_type == CZ_AST_IdentifierNodeType
         *  Represents a bare identifier. Created for:
         *   - Variable/function names in declarations
         *   - References to variables/functions in expressions
         *   - Type names (struct, typedef, newtype keywords)
         *   - Struct member names after . operator */
        struct {
            /** Identifier name string (from token lexeme, NOT a sibling AST node) */
            const char* name;
        } identifier;

        /** Used for node_type == CZ_AST_CastExpressionNodeType
         *  Represents explicit type casting using the `as` operator.
         *  E.g. `(expr as target_type)` - parsed at CAST precedence level */
        struct {
            /** Expression being cast (any expression node type) */
            CZ_AST_Node* expression;
            /** Target type to cast to: TypeNode (primitive or named type) */
            CZ_AST_Node* type;
        } cast_expression;

        /** Used for node_type == CZ_AST_StructMemberAccessNodeType
         *  Represents dot-access: `object.member`
         *  The object can be any expression; the member MUST be an IdentifierNode. */
        struct {
            /** Object being accessed (any expr yielding a struct or reference) */
            CZ_AST_Node* object;
            /** Member field name: IdentifierNode -- which field to access */
            CZ_AST_Node* member;
        } struct_member_access;
    };
};

/**
 * @brief Creates decoration for AST node (for expressions)
 * 
 * @param type CZ_Type from global type table.
 * @param val_category Whether or not expression is l-value or r-value.
 * @param is_constexpr Whether or not expression is constexpr.
 * @param is_reference_source Whether or not expression is source of reference.
 * @param scope_level 0 means global, 1 means function parameter, 2 for local variables and subsequent levels mean inner blocks.
 * @return CZ_AST_Decoration* Pointer to CZ_AST_Decoration allocated on success, NULL on failure.
 */
CZ_AST_Decoration* cz_ast_decoration_create(const CZ_Type* type, CZ_ValueCategory val_category, bool is_constexpr, bool is_reference_source, unsigned int scope_level);

/**
 * @brief Frees AST node decoration.
 * 
 * @param decor Pointer to CZ_AST_Decoration
 */
void cz_ast_decoration_free(CZ_AST_Decoration* decor);


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
