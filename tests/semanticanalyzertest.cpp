#include <gtest/gtest.h>
#include <memory>
#include <cstdio>
#include "cz_lexer.h"
#include "cz_parser.h"
#include "cz_error.h"
#include "cz_semantic_analyzer.h"

// Helper function to create a lexer from source code and analyze it.
static std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)> create_lexer(const char* source, int& lexer_error_count) {
    CZ_Lexer* lexer = cz_lexer_create(source, nullptr);
    if (!lexer) {
        lexer_error_count = 0;
        return std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)>(nullptr, cz_lexer_free);
    }
    if (cz_lexer_analyze(lexer) != 1) {
        lexer_error_count = lexer->error_list ? lexer->error_list->n_errors : 0;
        cz_lexer_free(lexer);
        return std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)>(nullptr, cz_lexer_free);
    }
    lexer_error_count = 0;  // No lexer errors if analysis succeeded
    return std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)>(lexer, cz_lexer_free);
}

// Helper function to create a parser from a lexer (parser takes ownership of lexer resources).
static std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)> create_parser(std::unique_ptr<CZ_Lexer, decltype(&cz_lexer_free)>& lexer_ptr, int& parser_error_count) {
    CZ_Lexer* lexer = lexer_ptr.release();  // Transfer ownership to parser
    CZ_Parser* parser = cz_parser_create(lexer);
    if (!parser) {
        cz_lexer_free(lexer);  // If parser creation fails, we must free lexer
        return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(nullptr, cz_parser_free);
    }
    if (cz_parser_parse(parser) != 1) {
        parser_error_count = parser->error_list ? parser->error_list->n_errors : 0;
        cz_parser_free(parser);
        return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(nullptr, cz_parser_free);
    }
    // Parser now owns lexer's resources, so we don't free lexer separately
    parser_error_count = 0;  // No parser errors if parsing succeeded
    return std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>(parser, cz_parser_free);
}

// Helper function to create a semantic analyzer from a parser (semantic analyzer takes ownership of parser resources).
static std::unique_ptr<CZ_SemanticAnalyzer, decltype(&cz_semantic_analyzer_free)> create_semantic_analyzer(std::unique_ptr<CZ_Parser, decltype(&cz_parser_free)>& parser_ptr) {
    CZ_Parser* parser = parser_ptr.release();  // Transfer ownership to semantic analyzer
    CZ_SemanticAnalyzer* sa = cz_semantic_analyzer_create(parser);
    if (!sa) {
        cz_parser_free(parser);  // If semantic analyzer creation fails, we must free parser
        return std::unique_ptr<CZ_SemanticAnalyzer, decltype(&cz_semantic_analyzer_free)>(nullptr, cz_semantic_analyzer_free);
    }
    // Semantic analyzer now owns parser's resources, so we don't free parser separately
    return std::unique_ptr<CZ_SemanticAnalyzer, decltype(&cz_semantic_analyzer_free)>(sa, cz_semantic_analyzer_free);
}

// Test Case 28: SemanticAnalyzerTest.GoodCase1_GlobalCounter
TEST(SemanticAnalyzerTest, GoodCase1_GlobalCounter) {
    const char* source =
        "global_counter :: int32 = 42;\n\n"
        "func get_counter :: () -> int32 & {\n"
        "    return global_counter;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        // Note: We don't easily have access to lexer/parser errors anymore due to ownership transfer
        // But we can still print semantic errors if needed
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}

// Test Case 29: SemanticAnalyzerTest.GoodCase2_ChooseLeft
TEST(SemanticAnalyzerTest, GoodCase2_ChooseLeft) {
    const char* source =
        "func choose_left :: (left :: int32&, right :: int32&) -> int32 & {"
        "    return left;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}

// Test Case 30: SemanticAnalyzerTest.BadCase3_InvalidLocalReferenceReturn
TEST(SemanticAnalyzerTest, BadCase3_InvalidLocalReferenceReturn) {
    const char* source =
        "func bad_leak :: () -> int32 & {\n"
        "    local_val :: int32 = 100;\n"
        "    return local_val;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}

// Test Case 31: SemanticAnalyzerTest.BadCase4_InvalidDeeplyNestedLocalReferenceReturn
TEST(SemanticAnalyzerTest, BadCase4_InvalidDeeplyNestedLocalReferenceReturn) {
    const char* source =
        "func complex_leak :: (condition :: bool) -> int32 & {\n"
        "    if (condition) {\n"
        "        nested_val :: int32 = 5;\n"
        "        return nested_val;\n"
        "    }\n"
        "    global_backup :: int32 = 0;\n"
        "    return global_backup;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 32: SemanticAnalyzerTest.TestCase1_GetMagicNumber_ReturnConstInt
TEST(SemanticAnalyzerTest, TestCase1_GetMagicNumber_ReturnConstInt) {
    const char* source =
        "func get_magic_number :: () -> const int32 {\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "func main :: () {\n"
        "    // Should compile perfectly\n"
        "    val :: int32 = get_magic_number();\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}


// Test Case 33: SemanticAnalyzerTest.TestCase2_AssignToConstFunctionReturn
TEST(SemanticAnalyzerTest, TestCase2_AssignToConstFunctionReturn) {
    const char* source =
        "func get_magic_number :: () -> const int32 {\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "func main :: () {\n"
        "    get_magic_number() = 100;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 34: SemanticAnalyzerTest.TestCase3_AssignToConstRefFunctionReturn
TEST(SemanticAnalyzerTest, TestCase3_AssignToConstRefFunctionReturn) {
    const char* source =
        "global_weight :: int32 = 80;\n"
        "\n"
        "func get_weight_limit :: () -> const int32& {\n"
        "    return global_weight;\n"
        "}\n"
        "\n"
        "func main :: () {\n"
        "    // Should FAIL semantic analysis:\n"
        "    // LHS is an L-value, but it's marked CONST via the return type!\n"
        "    get_weight_limit() = 100;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 35: SemanticAnalyzerTest.TestCase4_BindMutableRefToConstLocation
TEST(SemanticAnalyzerTest, TestCase4_BindMutableRefToConstLocation) {
    const char* source =
        "func main :: () {\n"
        "    static_score :: const int32 = 100;\n"
        "\n"
        "    // Should FAIL semantic analysis:\n"
        "    // Cannot bind a mutable reference (int32&) to a constant location (const int32)\n"
        "    alias :: int32& = static_score;\n"
        "\n"
        "    alias = 200; // This would illegally mutate static_score if allowed!\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 36: SemanticAnalyzerTest.TestCase5_PassConstRefToConstRefParam
TEST(SemanticAnalyzerTest, TestCase5_PassConstRefToConstRefParam) {
    const char* source =
        "func print_val :: (val :: const int32&) {\n"
        "    // Read-only is fine\n"
        "    x :: int32 = val;\n"
        "}\n"
        "\n"
        "func main :: () {\n"
        "    data :: int32 = 999;\n"
        "\n"
        "    // Should PASS seamlessly:\n"
        "    // Parameter expects const int32&, and data is int32 (L-value). Safe.\n"
        "    print_val(data);\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}


// Test Case 37: SemanticAnalyzerTest.TestCase6_ReturnLocalConstRef
TEST(SemanticAnalyzerTest, TestCase6_ReturnLocalConstRef) {
    const char* source =
        "func leaky_constant :: () -> const int32& {\n"
        "    temporary :: const int32 = 7;\n"
        "\n"
        "    // Should FAIL semantic analysis:\n"
        "    // Even though it's const, 'temporary' is local and will decay on the stack!\n"
        "    return temporary;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 38: SemanticAnalyzerTest.TestCase7_ConstDroppingFail
TEST(SemanticAnalyzerTest, TestCase7_ConstDroppingFail) {
    const char* source =
        "static_value :: const float = 10.3;\n"
        "func get_static_value :: () -> const float& { return static_value; }\n"
        "func main :: () { val :: float& = get_static_value(); }";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 39: SemanticAnalyzerTest.TestCase8_ConstDroppingSuccess
TEST(SemanticAnalyzerTest, TestCase8_ConstDroppingSuccess) {
    const char* source =
        "static_value :: const float = 10.3;\n"
        "func get_static_value :: () -> const float& { return static_value; }\n"
        "func main :: () { val :: float = get_static_value(); }";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}


// Test Case 40: SemanticAnalyzerTest.BadCase_Metres_ProcessDistance_ArithmeticNotSupported
TEST(SemanticAnalyzerTest, BadCase_Metres_ProcessDistance_ArithmeticNotSupported) {
    const char* source =
        "newtype float Metres;\n"
        "\n"
        "func process_distance :: (current :: Metres&, delta :: Metres) -> Metres {\n"
        "    // 1. current is a Metres&, delta is Metres.\n"
        "    // 2. resolve_core strips reference layer from 'current', leaving Metres.\n"
        "    // 3. nominal guard sees Metres == Metres.\n"
        "    // 4. structural layout resolves both to float.\n"
        "    // 5. However, newtype does not implicitly support arithmetic operations.\n"
        "    //    The '+' operator requires explicit conversion or operator overloading.\n"
        "    result :: Metres = current + delta;\n"
        "    return result;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 41: SemanticAnalyzerTest.GoodCase_Handle_UserHandle_UID_BootstrapId
TEST(SemanticAnalyzerTest, GoodCase_Handle_UserHandle_UID_BootstrapId) {
    const char* source =
        "typedef int32 Handle;\n"
        "typedef Handle UserHandle;\n"
        "newtype int32 UID;\n"
        "\n"
        "func bootstrap_id :: () {\n"
        "    raw_id :: UserHandle = 1024;\n"
        "\n"
        "    // 1. LHS is UID. RHS is explicit cast to UID.\n"
        "    // 2. Cast unboxes UserHandle -> Handle -> int32.\n"
        "    // 3. Cast unboxes UID -> int32.\n"
        "    // 4. Memory layouts match (int32 == int32). Valid!\n"
        "    user_id :: UID = raw_id as UID;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}


// Test Case 42: SemanticAnalyzerTest.BadCase_USD_GBP_TypeMismatch
TEST(SemanticAnalyzerTest, BadCase_USD_GBP_TypeMismatch) {
    const char* source =
        "newtype int32 USD;\n"
        "newtype int32 GBP;\n"
        "\n"
        "func trade :: () {\n"
        "    wallet_a :: USD = 100 as USD;\n"
        "\n"
        "    // ERROR: Type mismatch!\n"
        "    // Both unbox to 'int32' at a machine level, but they are nominally distinct.\n"
        "    // Pass 2 must fail here because no explicit cast was provided.\n"
        "    wallet_b :: GBP = wallet_a;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 43: SemanticAnalyzerTest.BadCase_Age_TemporaryReference
TEST(SemanticAnalyzerTest, BadCase_Age_TemporaryReference) {
    const char* source =
        "newtype int32 Age;\n"
        "\n"
        "func update_age :: (target :: Age&) {\n"
        "    // ...\n"
        "}\n"
        "\n"
        "func test :: () {\n"
        "    // ERROR: Cannot bind an L-value reference (Age&) to a raw temporary R-value.\n"
        "    // Even though 25 is cast to 'Age', the result of an 'as' cast is a temporary value,\n"
        "    // not a memory location that can be safely referenced.\n"
        "    update_age(25 as Age);\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 44: SemanticAnalyzerTest.GoodCase_Handle_TypeTransparency
TEST(SemanticAnalyzerTest, GoodCase_Handle_TypeTransparency) {
    const char* source =
        "typedef int32 Handle;\n"
        "\n"
        "func increment_raw :: (value :: int32&) {\n"
        "    // ...\n"
        "}\n"
        "\n"
        "func test :: () {\n"
        "    active_handle :: Handle = 42;\n"
        "\n"
        "    // 1. active_handle is an L-value of type Handle.\n"
        "    // 2. resolve_core unwraps Handle directly to 'int32'.\n"
        "    // 3. The function expects an 'int32&'.\n"
        "    // 4. Since Handle is a transparent alias, names match structurally. Valid!\n"
        "    increment_raw(active_handle);\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}


// Test Case 45: SemanticAnalyzerTest.BadCase_ItemCount_TypeMismatch
TEST(SemanticAnalyzerTest, BadCase_ItemCount_TypeMismatch) {
    const char* source =
        "newtype int32 ItemCount;\n"
        "\n"
        "func corrupt_count :: (raw_ptr :: int32&) {\n"
        "    // If allowed, this would bypass the nominal constraints of ItemCount!\n"
        "    raw_ptr = -500;\n"
        "}\n"
        "\n"
        "func test :: () {\n"
        "    stock :: ItemCount = 10 as ItemCount;\n"
        "\n"
        "    // ERROR: Type mismatch!\n"
        "    // resolve_core on 'stock' leaves it as 'ItemCount&' -> 'ItemCount'.\n"
        "    // resolve_core on the parameter is 'int32&' -> 'int32'.\n"
        "    // cz_type_is_equal_nominal flags that ItemCount != int32.\n"
        "    // You cannot implicitly pass a strong type reference to a raw reference.\n"
        "    corrupt_count(stock);\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 46: SemanticAnalyzerTest.CzcTestCode_ConstReferenceAssignment
TEST(SemanticAnalyzerTest, CzcTestCode_ConstReferenceAssignment) {
    const char* source =
        "newtype int32 SecureID;\n"
        "\n"
        "func test :: (a :: const int32&, b :: int32) {\n"
        "    // ERROR: 'a' points to a read-only location.\n"
        "    // If your assignment checker only checks if 'a' is a reference\n"
        "    // but forgets to check if its inner_type is marked 'is_const',\n"
        "    // it will illegally allow you to mutate a read-only variable!\n"
        "    a = b;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}

// Test Case 47: SemanticAnalyzerTest.NewType_SecondsFrames_Success
TEST(SemanticAnalyzerTest, NewType_SecondsFrames_Success) {
    const char* source =
        "newtype int32 Seconds;\n"
        "newtype int32 Frames;\n\n"
        "func main :: () {\n"
        "    s :: Seconds = 60 as Seconds;\n"
        "    f :: Frames = s as Frames;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}


// Test Case 48: SemanticAnalyzerTest.StructVector_GetDefaultX_Success
TEST(SemanticAnalyzerTest, StructVector_GetDefaultX_Success) {
    const char* source =
        "struct Vector {\n"
        "    x :: int32;\n"
        "    y :: int32;\n"
        "}\n\n"
        "func get_default_x :: () -> int32 {\n"
        "    val :: int32 = Vector{1, 2}.x;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_EQ(error_count, 0);
}


// Test Case 49: SemanticAnalyzerTest.RefHolder_UninitializedReference_Failure
TEST(SemanticAnalyzerTest, RefHolder_UninitializedReference_Failure) {
    const char* source =
        "struct RefHolder {\n"
        "    data :: int32&;\n"
        "}\n\n"
        "func break_references :: () {\n"
        "    rh :: RefHolder;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}


// Test Case 50: SemanticAnalyzerTest.ConstReference_AssignmentFailure
TEST(SemanticAnalyzerTest, ConstReference_AssignmentFailure) {
    const char* source =
        "func process :: (x :: const int32&) {\n"
        "    y :: int32& = x;\n"
        "}";

    int lexer_error_count = 0;
    auto lexer_ptr = create_lexer(source, lexer_error_count);
    ASSERT_TRUE(lexer_ptr != nullptr) << "Failed to create lexer";

    int parser_error_count = 0;
    auto parser_ptr = create_parser(lexer_ptr, parser_error_count);
    ASSERT_TRUE(parser_ptr != nullptr) << "Failed to create parser";

    auto sa_ptr = create_semantic_analyzer(parser_ptr);
    ASSERT_TRUE(sa_ptr != nullptr) << "Failed to create semantic analyzer";

    int analyze_result = cz_semantic_analyzer_analyze(sa_ptr.get());
    int semantic_error_count = sa_ptr->error_list ? sa_ptr->error_list->n_errors : 0;
    int error_count = lexer_error_count + parser_error_count + semantic_error_count;

    if (error_count > 0) {
        fprintf(stderr, "Analysis failed with %d errors (%d lexer, %d parser, %d semantic):\n",
                error_count, lexer_error_count, parser_error_count, semantic_error_count);
        if (semantic_error_count > 0) {
            for (size_t i = 0; i < sa_ptr->error_list->n_errors; i++) {
                CZ_Error err = sa_ptr->error_list->errors[i];
                fprintf(stderr, "  Semantic Error %zu: %s at line %d, column %d\n",
                        i, err.message, err.line, err.column);
            }
        }
    }

    EXPECT_EQ(analyze_result, 1);
    EXPECT_GT(error_count, 0);
}
