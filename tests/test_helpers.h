/*-------------------------------------------------------------------------
 *
 * test_helpers.h
 *    Common test utilities and macros for PostFGA tests
 *
 *-------------------------------------------------------------------------
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Test Tracking
 * -------------------------------------------------------------------------
 */

typedef struct {
    int total;
    int passed;
    int failed;
    const char *current_test;
} TestContext;

static TestContext test_ctx = {0, 0, 0, NULL};

/* -------------------------------------------------------------------------
 * Test Macros
 * -------------------------------------------------------------------------
 */

/* Start a test suite */
#define TEST_SUITE_START(name) \
    do { \
        printf("\n"); \
        printf("========================================\n"); \
        printf("Test Suite: %s\n", name); \
        printf("========================================\n"); \
        test_ctx.total = 0; \
        test_ctx.passed = 0; \
        test_ctx.failed = 0; \
    } while(0)

/* Start an individual test */
#define TEST_START(name) \
    do { \
        test_ctx.total++; \
        test_ctx.current_test = name; \
        printf("\n[TEST %d] %s\n", test_ctx.total, name); \
    } while(0)

/* Basic assertion */
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ PASS: %s\n", message); \
            test_ctx.passed++; \
        } else { \
            printf("  ✗ FAIL: %s\n", message); \
            test_ctx.failed++; \
        } \
    } while(0)

/* String equality assertion */
#define TEST_ASSERT_STR_EQ(actual, expected, message) \
    do { \
        if (strcmp((actual), (expected)) == 0) { \
            printf("  ✓ PASS: %s\n", message); \
            printf("         Got: '%s'\n", actual); \
            test_ctx.passed++; \
        } else { \
            printf("  ✗ FAIL: %s\n", message); \
            printf("         Expected: '%s'\n", expected); \
            printf("         Got:      '%s'\n", actual); \
            test_ctx.failed++; \
        } \
    } while(0)

/* Integer equality assertion */
#define TEST_ASSERT_INT_EQ(actual, expected, message) \
    do { \
        if ((actual) == (expected)) { \
            printf("  ✓ PASS: %s\n", message); \
            printf("         Got: %d\n", actual); \
            test_ctx.passed++; \
        } else { \
            printf("  ✗ FAIL: %s\n", message); \
            printf("         Expected: %d\n", expected); \
            printf("         Got:      %d\n", actual); \
            test_ctx.failed++; \
        } \
    } while(0)

/* Boolean equality assertion */
#define TEST_ASSERT_BOOL_EQ(actual, expected, message) \
    do { \
        if ((actual) == (expected)) { \
            printf("  ✓ PASS: %s\n", message); \
            printf("         Got: %s\n", (actual) ? "true" : "false"); \
            test_ctx.passed++; \
        } else { \
            printf("  ✗ FAIL: %s\n", message); \
            printf("         Expected: %s\n", (expected) ? "true" : "false"); \
            printf("         Got:      %s\n", (actual) ? "true" : "false"); \
            test_ctx.failed++; \
        } \
    } while(0)

/* Pointer not NULL assertion */
#define TEST_ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) != NULL) { \
            printf("  ✓ PASS: %s\n", message); \
            test_ctx.passed++; \
        } else { \
            printf("  ✗ FAIL: %s (got NULL)\n", message); \
            test_ctx.failed++; \
        } \
    } while(0)

/* Pointer is NULL assertion */
#define TEST_ASSERT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            printf("  ✓ PASS: %s\n", message); \
            test_ctx.passed++; \
        } else { \
            printf("  ✗ FAIL: %s (expected NULL)\n", message); \
            test_ctx.failed++; \
        } \
    } while(0)

/* Floating point equality assertion (with epsilon) */
#define TEST_ASSERT_FLOAT_EQ(actual, expected, epsilon, message) \
    do { \
        double _diff = (actual) - (expected); \
        if (_diff < 0) _diff = -_diff; \
        if (_diff <= (epsilon)) { \
            printf("  ✓ PASS: %s\n", message); \
            printf("         Got: %f\n", (double)(actual)); \
            test_ctx.passed++; \
        } else { \
            printf("  ✗ FAIL: %s\n", message); \
            printf("         Expected: %f\n", (double)(expected)); \
            printf("         Got:      %f\n", (double)(actual)); \
            printf("         Diff:     %f (epsilon: %f)\n", _diff, (double)(epsilon)); \
            test_ctx.failed++; \
        } \
    } while(0)

/* Print test summary */
#define TEST_SUITE_END() \
    do { \
        printf("\n"); \
        printf("========================================\n"); \
        printf("Test Summary\n"); \
        printf("========================================\n"); \
        printf("Total Tests:  %d\n", test_ctx.total); \
        printf("Assertions:   %d\n", test_ctx.passed + test_ctx.failed); \
        printf("  Passed:     %d\n", test_ctx.passed); \
        printf("  Failed:     %d\n", test_ctx.failed); \
        printf("========================================\n"); \
        if (test_ctx.failed == 0) { \
            printf("✓ All tests passed!\n"); \
        } else { \
            printf("✗ Some tests failed!\n"); \
        } \
        printf("\n"); \
    } while(0)

/* Get test result (for return code) */
#define TEST_RESULT() (test_ctx.failed == 0 ? 0 : 1)

/* -------------------------------------------------------------------------
 * Test Utilities
 * -------------------------------------------------------------------------
 */

/* Skip a test */
#define TEST_SKIP(message) \
    do { \
        printf("  ⊘ SKIP: %s\n", message); \
    } while(0)

/* Print informational message */
#define TEST_INFO(format, ...) \
    do { \
        printf("  ℹ INFO: " format "\n", ##__VA_ARGS__); \
    } while(0)

/* Print a section header */
#define TEST_SECTION(name) \
    do { \
        printf("\n--- %s ---\n", name); \
    } while(0)

#endif /* TEST_HELPERS_H */
