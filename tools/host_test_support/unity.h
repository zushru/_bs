#ifndef ATT_HOST_UNITY_COMPAT_H
#define ATT_HOST_UNITY_COMPAT_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int att_unity_failures = 0;
static int att_unity_tests = 0;

static inline int UNITY_BEGIN(void) {
  att_unity_failures = 0;
  att_unity_tests = 0;
  return 0;
}

static inline int UNITY_END(void) {
  printf("\nUNITY_SUMMARY tests=%d failures=%d\n",
         att_unity_tests, att_unity_failures);
  return att_unity_failures ? 1 : 0;
}

#define ATT_UNITY_FAIL(message) do { \
  fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, message); \
  ++att_unity_failures; \
  return; \
} while (0)

#define TEST_ASSERT_TRUE(expression) do { \
  if (!(expression)) ATT_UNITY_FAIL("TEST_ASSERT_TRUE(" #expression ")"); \
} while (0)

#define TEST_ASSERT_FALSE(expression) do { \
  if ((expression)) ATT_UNITY_FAIL("TEST_ASSERT_FALSE(" #expression ")"); \
} while (0)

#define TEST_ASSERT_NOT_NULL(expression) do { \
  if ((expression) == NULL) ATT_UNITY_FAIL("TEST_ASSERT_NOT_NULL(" #expression ")"); \
} while (0)

#define TEST_ASSERT_NULL(expression) do { \
  if ((expression) != NULL) ATT_UNITY_FAIL("TEST_ASSERT_NULL(" #expression ")"); \
} while (0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) do { \
  const long long e_ = (long long)(expected); \
  const long long a_ = (long long)(actual); \
  if (e_ != a_) { \
    char message_[160]; \
    snprintf(message_, sizeof(message_), "TEST_ASSERT_EQUAL_INT expected=%lld actual=%lld", e_, a_); \
    ATT_UNITY_FAIL(message_); \
  } \
} while (0)

#define TEST_ASSERT_EQUAL(expected, actual) do { \
  const long long e_ = (long long)(expected); \
  const long long a_ = (long long)(actual); \
  if (e_ != a_) { \
    char message_[160]; \
    snprintf(message_, sizeof(message_), "TEST_ASSERT_EQUAL expected=%lld actual=%lld", e_, a_); \
    ATT_UNITY_FAIL(message_); \
  } \
} while (0)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual) do { \
  const unsigned e_ = (uint8_t)(expected); \
  const unsigned a_ = (uint8_t)(actual); \
  if (e_ != a_) { \
    char message_[160]; \
    snprintf(message_, sizeof(message_), "TEST_ASSERT_EQUAL_UINT8 expected=%u actual=%u", e_, a_); \
    ATT_UNITY_FAIL(message_); \
  } \
} while (0)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual) do { \
  const unsigned long long e_ = (uint16_t)(expected); \
  const unsigned long long a_ = (uint16_t)(actual); \
  if (e_ != a_) { \
    char message_[160]; \
    snprintf(message_, sizeof(message_), "TEST_ASSERT_EQUAL_UINT16 expected=%llu actual=%llu", e_, a_); \
    ATT_UNITY_FAIL(message_); \
  } \
} while (0)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) do { \
  const unsigned long long e_ = (uint32_t)(expected); \
  const unsigned long long a_ = (uint32_t)(actual); \
  if (e_ != a_) { \
    char message_[160]; \
    snprintf(message_, sizeof(message_), "TEST_ASSERT_EQUAL_UINT32 expected=%llu actual=%llu", e_, a_); \
    ATT_UNITY_FAIL(message_); \
  } \
} while (0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) do { \
  const char* e_ = (expected); \
  const char* a_ = (actual); \
  if (!e_ || !a_ || strcmp(e_, a_) != 0) \
    ATT_UNITY_FAIL("TEST_ASSERT_EQUAL_STRING mismatch"); \
} while (0)

#define TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, count) do { \
  if (memcmp((expected), (actual), (count)) != 0) \
    ATT_UNITY_FAIL("TEST_ASSERT_EQUAL_UINT8_ARRAY mismatch"); \
} while (0)

#define RUN_TEST(test_function) do { \
  ++att_unity_tests; \
  const int failures_before_ = att_unity_failures; \
  setUp(); \
  if (att_unity_failures == failures_before_) test_function(); \
  tearDown(); \
  printf("%s %s\n", att_unity_failures == failures_before_ ? "PASS" : "FAIL", #test_function); \
} while (0)

#endif
