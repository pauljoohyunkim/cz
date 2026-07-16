#include <gtest/gtest.h>
#include "cz_lexer.h"
#include "cz_parser.h"
#include "cz_semantic_analyzer.h"
#include "cz_error.h"
#include "cz_ast.h"
#include <cstdio>
#include <memory>

// Helper function to parse source and return parser (with parse results via output params)
// Returns a unique_ptr to the parser (caller owns it) if lexing and parsing succeeded, otherwise returns empty unique_ptr.
// Note: cz_parser_create takes ownership of the lexer, so we release lexer ownership after parser creation.
static std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)> parse_source(const char* source, int& parse_result, int& error_count) {
    // Create lexer with smart pointer for automatic cleanup
    auto lexer { cz_lexer_create(source, nullptr) };
    if (!lexer) {
        parse_result = 0;
        error_count = 0;
        return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(nullptr, cz_parser_free);
    }

    std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> lexer_ptr { lexer, cz_lexer_free };

    if (cz_lexer_analyze(lexer) != 1) {
        parse_result = 0;
        error_count = lexer->error_list ? lexer->error_list->n_errors : 0;
        // Print lexer errors for debugging if any
        if (error_count > 0 && lexer->error_list) {
            fprintf(stderr, "Lexer failed with %d errors:\n", error_count);
            for (size_t i = 0; i < lexer->error_list->n_errors; i++) {
                CZ_Error err = lexer->error_list->errors[i];
                fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
        return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(nullptr, cz_parser_free);
    }

    // Create parser - parser takes ownership of lexer's resources
    CZ_Parser* parser = cz_parser_create(lexer);
    // Release lexer ownership since parser now owns it
    lexer_ptr.release();

    if (!parser) {
        parse_result = 0;
        error_count = 0;
        return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(nullptr, cz_parser_free);
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

    return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(parser, cz_parser_free);
}

// Helper function to run semantic analyzer and get error count
// Does NOT take ownership of parser (parser remains owned by caller)
static void analyze_and_get_error_count(std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>& parser, int& error_count) {
    if (!parser) {
        error_count = 0;
        return;
    }

    CZ_SemanticAnalyzer* sa = cz_semantic_analyzer_create(parser.get());
    if (!sa) {
        error_count = 0;
        return;
    }

    // Semantic analyzer does NOT take ownership of parser, so we keep the parser unique_ptr
    std::unique_ptr<CZ_SemanticAnalyzer, decltype(&cz_semantic_analyzer_free)> sa_ptr { sa, cz_semantic_analyzer_free };

    int ret = cz_semantic_analyzer_build_global_symbol_table(sa);
    // Note: ret != 1 does not mean there are errors; we still need to check error list
    if (ret != 1) {
        // Failure in pass I
        error_count = sa->error_list ? sa->error_list->n_errors : 0;
        // Print errors
        if (error_count > 0 && sa->error_list) {
            fprintf(stderr, "Semantic analysis pass I failed with %d errors:\n", error_count);
            for (size_t i = 0; i < sa->error_list->n_errors; i++) {
                CZ_Error err = sa->error_list->errors[i];
                fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
        return;
    }

    ret = cz_semantic_analyzer_full_analyze(sa);
    if (ret != 1) {
        // Failure in pass II
        error_count = sa->error_list ? sa->error_list->n_errors : 0;
        // Print errors
        if (error_count > 0 && sa->error_list) {
            fprintf(stderr, "Semantic analysis pass II failed with %d errors:\n", error_count);
            for (size_t i = 0; i < sa->error_list->n_errors; i++) {
                CZ_Error err = sa->error_list->errors[i];
                fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
        return;
    }

    error_count = sa->error_list ? sa->error_list->n_errors : 0;
    // Print errors if any
    if (error_count > 0 && sa->error_list) {
        fprintf(stderr, "Semantic analysis completed with %d errors:\n", error_count);
        for (size_t i = 0; i < sa->error_list->n_errors; i++) {
            CZ_Error err = sa->error_list->errors[i];
            fprintf(stderr, "  Error %zu: %s at line %d, column %d\n",
                    i, err.message, err.line, err.column);
        }
    } else {
        fprintf(stderr, "Semantic analysis completed with 0 errors.\n");
    }
}

// Test cases from semantictest.cpp

// (Case 1: Good)
TEST(SemanticAnalyzerTest, GoodCase1_GlobalCounter) {
    const char* source =
        "global_counter :: int32 = 42;\n\n"
        "func get_counter :: () -> int32& {\n"
        "    return global_counter; \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    // Now run semantic analysis
    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_EQ(sem_error_count, 0);
}

// (Case 2: Good)
TEST(SemanticAnalyzerTest, GoodCase2_ChooseLeft) {
    const char* source =
        "func choose_left :: (left :: int32&, right :: int32&) -> int32& {\n"
        "    return left; \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0);

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_EQ(sem_error_count, 0);
}

// (Case 3: bad: Invalid Local Reference Return)
TEST(SemanticAnalyzerTest, BadCase3_InvalidLocalReferenceReturn) {
    const char* source =
        "func bad_leak :: () -> int32& {\n"
        "    local_val :: int32 = 100;\n"
        "    return local_val; \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Parsing should succeed

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    // Print the error count for debugging
    fprintf(stderr, "BadCase3 sem_error_count = %d\n", sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error
}

// (Case 4: Invalid Deeply Nested Local Reference Return)
TEST(SemanticAnalyzerTest, BadCase4_InvalidDeeplyNestedLocalReferenceReturn) {
    const char* source =
        "func complex_leak :: (condition :: bool) -> int32& {\n"
        "    if (condition) {\n"
        "        nested_val :: int32 = 5;\n"
        "        return nested_val; \n"
        "    }\n"
        "    global_backup :: int32 = 0;\n"
        "    return global_backup;\n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0);

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    fprintf(stderr, "BadCase4 sem_error_count = %d\n", sem_error_count);
    EXPECT_GT(sem_error_count, 0);
}

// Test case 1: Should pass
TEST(SemanticAnalyzerTest, TestCase1_GetMagicNumber_ReturnConstInt) {
    const char* source =
        "func get_magic_number :: () -> const int32 {\n"
        "    return 42;\n"
        "}\n\n"
        "func main :: () {\n"
        "    // Should compile perfectly\n"
        "    val :: int32 = get_magic_number(); \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0);

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_EQ(sem_error_count, 0);
}

// Test case 2: Should fail
TEST(SemanticAnalyzerTest, TestCase2_AssignToConstFunctionReturn) {
    const char* source =
        "func get_magic_number :: () -> const int32 {\n"
        "    return 42;\n"
        "}\n\n"
        "func main :: () {\n"
        "    get_magic_number() = 100; \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0);

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (cannot assign to const function return)
}

// Test case 3: Should fail
TEST(SemanticAnalyzerTest, TestCase3_AssignToConstRefFunctionReturn) {
    const char* source =
        "global_weight :: int32 = 80;\n\n"
        "func get_weight_limit :: () -> const int32& {\n"
        "    return global_weight;\n"
        "}\n\n"
        "func main :: () {\n"
        "    // Should FAIL semantic analysis: \n"
        "    // LHS is an L-value, but it's marked CONST via the return type!\n"
        "    get_weight_limit() = 100; \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0);

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (cannot assign to const reference return)
}

// Test case 4: Fail
TEST(SemanticAnalyzerTest, TestCase4_BindMutableRefToConstLocation) {
    const char* source =
        "func main :: () {\n"
        "    static_score :: const int32 = 100;\n"
        "\n"
        "    // Should FAIL semantic analysis: \n"
        "    // Cannot bind a mutable reference (int32&) to a constant location (const int32)\n"
        "    alias :: int32& = static_score; \n"
        "\n"
        "    alias = 200; // This would illegally mutate static_score if allowed!\n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0);

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (cannot bind int& to const int)
}

// Test case 5: Pass
TEST(SemanticAnalyzerTest, TestCase5_PassConstRefToConstRefParam) {
    const char* source =
        "func print_val :: (val :: const int32&) {\n"
        "    // Read-only is fine\n"
        "    x :: int32 = val;\n"
        "}\n\n"
        "func main :: () {\n"
        "    data :: const int32 = 999;\n"
        "\n"
        "    // Should PASS seamlessly: \n"
        "    // Parameter expects const int32&, and data is const int32 (L-value). Safe.\n"
        "    print_val(data); \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0);

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_EQ(sem_error_count, 0);
}

// Test case 6: Fail
TEST(SemanticAnalyzerTest, TestCase6_ReturnLocalConstRef) {
    const char* source =
        "func leaky_constant :: () -> const int32& {\n"
        "    temporary :: const int32 = 7;\n"
        "\n"
        "    // Should FAIL semantic analysis: \n"
        "    // Even though it's const, 'temporary' is local and will decay on the stack!\n"
        "    return temporary; \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0);

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (returning reference to local variable)
}


// Test case 7: Should fail - Const dropping
TEST(SemanticAnalyzerTest, TestCase7_ConstDroppingFail) {
    const char* source =
        "static_value :: const float = 10.3;\n"
        "func get_static_value :: () -> const float& { return static_value; }\n"
        "func main :: () { val :: float& = get_static_value(); }\n";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    // Now run semantic analysis
    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (cannot bind float& to const float& return)
}

// Test case 8: Should pass - Const preserved
TEST(SemanticAnalyzerTest, TestCase8_ConstDroppingSuccess) {
    const char* source =
        "static_value :: const float = 10.3;\n"
        "func get_static_value :: () -> const float& { return static_value; }\n"
        "func main :: () { val :: float = get_static_value(); }\n";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    // Now run semantic analysis
    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_EQ(sem_error_count, 0); // Should have no semantic error
}

// Test case: Good case - Metres
TEST(SemanticAnalyzerTest, GoodCase_Metres_ProcessDistance) {
    const char* source =
        "newtype float Metres;\n\n"
        "func process_distance :: (current :: Metres&, delta :: Metres) -> Metres {\n"
        "    // 1. current is a Metres&, delta is Metres.\n"
        "    // 2. resolve_core strips reference layer from 'current', leaving Metres.\n"
        "    // 3. nominal guard sees Metres == Metres.\n"
        "    // 4. structural layout resolves both to float.\n"
        "    // 5. Returns a cloned Metres wrapper value.\n"
        "    result :: Metres = current + delta;\n"
        "    return result;\n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_EQ(sem_error_count, 0); // Should have no semantic error
}


// Test case: Good case - Handle/UserHandle/UID
TEST(SemanticAnalyzerTest, GoodCase_Handle_UserHandle_UID_BootstrapId) {
    const char* source =
        "typedef int32 Handle;\n"
        "typedef Handle UserHandle;\n"
        "newtype int32 UID;\n\n"
        "func bootstrap_id :: () {\n"
        "    raw_id :: UserHandle = 1024;\n"
        "    \n"
        "    // 1. LHS is UID. RHS is explicit cast to UID.\n"
        "    // 2. Cast unboxes UserHandle -> Handle -> int32.\n"
        "    // 3. Cast unboxes UID -> int32.\n"
        "    // 4. Memory layouts match (int32 == int32). Valid!\n"
        "    user_id :: UID = raw_id as UID;\n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_EQ(sem_error_count, 0); // Should have no semantic error
}

// Test case: Bad case - USD/GBP type mismatch
TEST(SemanticAnalyzerTest, BadCase_USD_GBP_TypeMismatch) {
    const char* source =
        "newtype int32 USD;\n"
        "newtype int32 GBP;\n\n"
        "func trade :: () {\n"
        "    wallet_a :: USD = 100 as USD;\n"
        "    \n"
        "    // ERROR: Type mismatch!\n"
        "    // Both unbox to 'int32' at a machine level, but they are nominally distinct.\n"
        "    // Pass 2 must fail here because no explicit cast was provided.\n"
        "    wallet_b :: GBP = wallet_a; \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (type mismatch)
}

// Test case: Bad case - Age with temporary reference
TEST(SemanticAnalyzerTest, BadCase_Age_TemporaryReference) {
    const char* source =
        "newtype int32 Age;\n\n"
        "func update_age :: (target :: Age&) {\n"
        "    // ...\n"
        "}\n\n"
        "func test :: () {\n"
        "    // ERROR: Cannot bind an L-value reference (Age&) to a raw temporary R-value.\n"
        "    // Even though 25 is cast to 'Age', the result of an 'as' cast is a temporary value,\n"
        "    // not a memory location that can be safely referenced.\n"
        "    update_age(25 as Age); \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (cannot bind reference to temporary)
}

// Test case: Good case - Handle type transparency
TEST(SemanticAnalyzerTest, GoodCase_Handle_TypeTransparency) {
    const char* source =
        "typedef int32 Handle;\n\n"
        "func increment_raw :: (value :: int32&) {\n"
        "    // ...\n"
        "}\n\n"
        "func test :: () {\n"
        "    active_handle :: Handle = 42;\n"
        "    \n"
        "    // 1. active_handle is an L-value of type Handle.\n"
        "    // 2. resolve_core unwraps Handle directly to 'int32'.\n"
        "    // 3. The function expects an 'int32&'.\n"
        "    // 4. Since Handle is a transparent alias, names match structurally. Valid!\n"
        "    increment_raw(active_handle); \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_EQ(sem_error_count, 0); // Should have no semantic error
}

// Test case: Bad case - ItemCount type mismatch
TEST(SemanticAnalyzerTest, BadCase_ItemCount_TypeMismatch) {
    const char* source =
        "newtype int32 ItemCount;\n\n"
        "func corrupt_count :: (raw_ptr :: int32&) {\n"
        "    // If allowed, this would bypass the nominal constraints of ItemCount!\n"
        "    raw_ptr = -500; \n"
        "}\n\n"
        "func test :: () {\n"
        "    stock :: ItemCount = 10 as ItemCount;\n"
        "    \n"
        "    // ERROR: Type mismatch!\n"
        "    // resolve_core on 'stock' leaves it as 'ItemCount&' -> 'ItemCount'.\n"
        "    // resolve_core on the parameter is 'int32&' -> 'int32'.\n"
        "    // cz_type_is_equal_nominal flags that ItemCount != int32.\n"
        "    // You cannot implicitly pass a strong type reference to a raw reference.\n"
        "    corrupt_count(stock); \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (type mismatch)
}


// Test case: Czc test code - Const reference assignment error
TEST(SemanticAnalyzerTest, CzcTestCode_ConstReferenceAssignment) {
    const char* source =
        "newtype int32 SecureID;\n\n"
        "func test :: (a :: const SecureID&, b :: SecureID) {\n"
        "    // ERROR: 'a' points to a read-only location.\n"
        "    // If your assignment checker only checks if 'a' is a reference \n"
        "    // but forgets to check if its inner_type is marked 'is_const', \n"
        "    // it will illegally allow you to mutate a read-only variable!\n"
        "    a = b;a = b; \n"
        "}";

    int parse_result = 0;
    int parse_error_count = 0;
    auto parser = parse_source(source, parse_result, parse_error_count);
    EXPECT_EQ(parse_result, 1);
    EXPECT_EQ(parse_error_count, 0); // Lexing and parsing should succeed

    int sem_error_count = 0;
    analyze_and_get_error_count(parser, sem_error_count);
    EXPECT_GT(sem_error_count, 0); // Should have semantic error (cannot assign to const reference parameter)
}