#include <gtest/gtest.h>
#include "cz_lexer.h"
#include "cz_parser.h"
#include "cz_error.h"
#include "cz_ast.h"

// Helper function to create a parser from source code
static CZ_Parser* create_parser_from_source(const char* source) {
    CZ_Lexer* lexer = cz_lexer_create(source, nullptr);
    if (!lexer) return nullptr;

    if (cz_lexer_analyze(lexer) != 1) {
        cz_lexer_free(lexer);
        return nullptr;
    }

    CZ_Parser* parser = cz_parser_create(lexer);
    cz_lexer_free(lexer); // Parser takes ownership of lexer's resources
    return parser;
}

// Helper function to run parser and get error count and parse result
static void parse_and_get_error_count(CZ_Parser* parser, int& error_count, int& parse_result) {
    if (!parser) {
        error_count = 0;
        parse_result = 0;
        return;
    }

    parse_result = cz_parser_parse(parser);
    error_count = parser->error_list ? parser->error_list->n_errors : 0;

    // Print errors for debugging if any
    if (error_count > 0 && parser->error_list) {
        fprintf(stderr, "Parse failed with %d errors:\n", error_count);
        for (size_t i = 0; i < parser->error_list->n_errors; i++) {
            CZ_Error err = parser->error_list->errors[i];
            fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                    i, err.message, err.line, err.column);
        }
    }

    cz_parser_free(parser);
}


// (Good) Test 14: Function test_postfix_chains with chained member access and high-order calls
TEST(ParserTest, GoodExample10_FunctionPostfixChains) {
    const char* source =
        "func test_postfix_chains :: () -> int {\n"
        "    // 1. Chained member access\n"
        "    current_zip :: int32 = company.subsidiary.address.zip;\n\n"
        "    // 2. High-order call: a function returning a function being invoked immediately\n"
        "    result :: int32 = get_calculator()(10, 20);\n\n"
        //"    // 3. Mixed chain: accessing a method pointer on a struct field and calling it\n"
        //"    status :: bool = cluster.nodes[0].get_status();\n\n"
        "    return current_zip;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}


// (Good) Test 15: Function test_loops with various loop constructs
TEST(ParserTest, GoodExample11_FunctionLoops) {
    const char* source =
        "func test_loops :: () -> int32 {\n"
        "    sum :: int32 = 0;\n\n"
        "    // Standard 3-component loop\n"
        "    for (i :: int32 = 0; i < 10; i = i + 1) {\n"
        "        sum = sum + i;\n"
        "    }\n\n"
        "    // Loop with completely omitted components (Infinite loop variant)\n"
        "    for (; ;) {\n"
        "        if (sum > 1000) {\n"
        "            return sum;\n"
        "        }\n"
        "        sum = sum + 1;\n"
        "    }\n\n"
        "    return sum;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}


// (Good) Test 16: Function process_matrix with if-else and while loop
TEST(ParserTest, GoodExample12_FunctionProcessMatrix) {
    const char* source =
        "func process_matrix :: (threshold :: int32) -> void {\n"
        "    if (system_is_ready()) {\n"
        "        while (count < threshold) {\n"
        "            count = count + 1;\n"
        "            if (count == 50) {\n"
        "                log_checkpoint();\n"
        "            }\n"
        "        }\n"
        "    } else {\n"
        "        trigger_fallback();\n"
        "    }\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}


// (Good) Test 17: Typedef, struct, and function with struct parameter
TEST(ParserTest, GoodExample13_TypedefStructFunction) {
    const char* source =
        "typedef int32 Handle;\n\n"
        "struct Vector3 {\n"
        "    x :: float;\n"
        "    y :: float;\n"
        "    z :: float;\n"
        "}\n\n"
        "func optimize_position :: (h :: Handle, pos :: Vector3) -> void {\n"
        "    multiplier :: float = h as float;\n\n"
        "    while (pos.x < 100.0) {\n"
        "        pos.x = pos.x + (1.5 as float) * multiplier;\n"
        "    }\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}


// (Bad) Test 18: Error A - Malformed for-loop header (missing second semicolon)
TEST(ParserTest, BadExample13_ErrorA_MalformedForLoop) {
    const char* source =
        "// Error A: Malformed for-loop header (missing second semicolon)\n"
        "func bad_for :: () -> void {\n"
        "    for (i :: int32 = 0; i < 10) {\n"
        "        sum = sum + 1;\n"
        "    }\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}


// (Bad) Test 19: Error B - Naked expression inside struct declaration block
TEST(ParserTest, BadExample14_ErrorB_NakedExpressionInStruct) {
    const char* source =
        "// Error B: Naked expression inside struct declaration block\n"
        "struct BadStruct {\n"
        "    x :: int32;\n"
        "    5 + 3; // Should trigger error and sync to next declaration boundary\n"
        "    y :: float;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}


// (Good) Test 20: Verification Anchor - Function that should parse correctly after error recovery
TEST(ParserTest, GoodExample14_VerificationAnchor) {
    const char* source =
        "// Verification Anchor: The parser MUST recover from the errors above \n"
        "// and cleanly parse this final function node!\n"
        "func recovery_anchor :: () -> int32 {\n"
        "    return 1;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}
// Test the exact examples from the original parsertest.cpp comments

// (Good) Test 1: Function main with variable declarations and assignment
TEST(ParserTest, GoodExample1_FunctionMain) {
    const char* source =
        "func main :: () -> int32 {\n"
        "    x :: const int32 = 10;\n"
        "    y :: int32 = 5;\n"
        "    \n"
        "    // Pratt check: should parse as (y parse as (y * 2) + (x /  y * 2) + (x / 5) / 2, left-to-right division\n"
        "    y = y * 2 + x / 5 / 2; \n"
        "    \n"
        "    return y;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}

// (Good) Test 2: Function get_ptr with reference parameter and return
TEST(ParserTest, GoodExample2_FunctionGetPtr) {
    const char* source =
        "func get_ptr :: (val :: const int32&) -> int32& {\n"
        "    backup :: const int32 = val;\n"
        "    return val;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}

// (Good) Test 3: Global constant and function using it
TEST(ParserTest, GoodExample3_GlobalConstant) {
    const char* source =
        "GLOBALMASK :: const int32 = 255;\n"
        "\n"
        "func apply_mask :: (val :: int32) -> int32 {\n"
        "    return val & GLOBAL_MASK;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}

// (Bad) Test 4: Error A - Missing semicolon in global scope
TEST(ParserTest, BadExample4_MissingSemicolonGlobal) {
    const char* source =
        "// Error A: Missing semicolon in global scope\n"
        "wrong :: const int32 = 5";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}

// (Bad) Test 5: Error B - Expression in parameter declaration
TEST(ParserTest, BadExample5_ExpressionInParameter) {
    const char* source =
        "// Error B: Expression in parameter declaration\n"
        "func bad_param :: (x :: int32 = 12) -> int32 { \n"
        "    return x;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}

// (Bad) Test 6: Error C - Operator precedence lockup / malformed statement
TEST(ParserTest, BadExample6_MalformedExpression) {
    const char* source =
        "// Error C: Operator precedence lockup / malformed statement\n"
        "func bad_math :: () -> int32 {\n"
        "    x :: int32 = + * 5; \n"
        "    return x;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}

// (Good) Test 7: Function check_value with if-else statement
TEST(ParserTest, GoodExample7_FunctionCheckValue) {
    const char* source =
        "func check_value :: (val :: int32) -> int32 {\n"
        "    result :: int32 = 0;\n"
        "\n"
        "    if (val > 100) {\n"
        "        result = 1;\n"
        "    } else if (val == 100) {\n"
        "        result = 2;\n"
        "    } else {\n"
        "        result = 3;\n"
        "    }\n"
        "\n"
        "    return result;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}

// (Good) Test 8: Function compute_ratio with casting operations
TEST(ParserTest, GoodExample8_FunctionComputeRatio) {
    const char* source =
        "func compute_ratio :: (alpha :: int32, beta :: bool) -> int32 {\n"
        "    // Verifies your Pratt parse loop handles complex parenthesis-wrapped casting flows\n"
        "    intermediate :: const float = ((alpha as float) + (4.5 as float));\n"
        "    \n"
        "    // Verifies sequential left-associative casting: bool -> int32 -> float\n"
        "    complex_cast :: const float = beta as int32 as float;\n"
        "    \n"
        "    return intermediate as int32;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}

// (Good) Test 9: Function process_system with typedef usage
TEST(ParserTest, GoodExample9_FunctionProcessSystem) {
    const char* source =
        "typedef int32 custom_status;\n"
        "typedef bool toggle_flag;\n"
        "\n"
        "func process_system :: (status :: custom_status) -> toggle_flag {\n"
        "    is_active :: toggle_flag = false;\n"
        "    \n"
        "    if (status == (1 as custom_status)) {\n"
        "        is_active = true;\n"
        "    }\n"
        "    \n"
        "    return is_active;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(error_count, 0);
}

// (Bad) Test 10: Error A - Naked conditional statement (Missing mandatory curly braces)
TEST(ParserTest, BadExample10_ErrorA_NakedConditional) {
    const char* source =
        "// Error A: Naked conditional statement (Missing mandatory curly braces)\n"
        "func bad_if :: (x :: int32) -> int32 {\n"
        "    if (x == 5) return 10; \n"
        "    return 0;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}

// (Bad) Test 11: Error B - Casting a type into an expression (Right side of 'as' must be a TypeNode)
TEST(ParserTest, BadExample11_ErrorB_CastingExpression) {
    const char* source =
        "// Error B: Casting a type into an expression (Right side of 'as' must be a TypeNode)\n"
        "func bad_cast_target :: (x :: int32) -> int32 {\n"
        "    y :: const int32 = x as (5 + 3); \n"
        "    return y;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}

// (Bad) Test 12: Error C - Malformed typedef formatting
TEST(ParserTest, BadExample12_ErrorC_MalformedTypedef) {
    const char* source =
        "// Error C: Malformed typedef formatting\n"
        "typedef my_bad_alias; \n";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}

// (Bad) Test 13: Error D - Statement separator leakage inside conditional check block headers
TEST(ParserTest, BadExample13_ErrorD_StatementSeparator) {
    const char* source =
        "// Error D: Statement separator leakage inside conditional check block headers\n"
        "func bad_header :: () -> int32 {\n"
        "    if (true;) { \n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}";

    int error_count = 0;
    int parse_result = 0;
    CZ_Parser* parser = create_parser_from_source(source);
    parse_and_get_error_count(parser, error_count, parse_result);
    EXPECT_EQ(parse_result, 1);
    EXPECT_GT(error_count, 0);
}
