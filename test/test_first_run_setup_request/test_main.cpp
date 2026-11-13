#include <unity.h>
#include <string.h>

#include "first_run_setup_request.h"

using namespace att;

namespace {
const char kValid[] =
    "{\"setup_code\":\"482731\",\"initial_pin\":\"628407\","
    "\"config\":{\"utc_offset_min\":480,\"late_min\":30,"
    "\"scan_lead_min\":30,\"term_start_day\":10,"
    "\"term_end_day\":300,\"auto_sync\":1,\"sound_mode\":0,"
    "\"timetable\":["
    "[[480,750,true],[810,990,true]],"
    "[[480,750,true],[810,990,true]],"
    "[[480,750,true],[810,990,true]],"
    "[[480,750,true],[810,990,true]],"
    "[[480,750,true],[810,990,true]]"
    "]}}";

const char kTimetable[] =
    "\"timetable\":["
    "[[480,750,true],[810,990,true]],"
    "[[480,750,true],[810,990,true]],"
    "[[480,750,true],[810,990,true]],"
    "[[480,750,true],[810,990,true]],"
    "[[480,750,true],[810,990,true]]"
    "]";

FirstRunSetupRequest emptyRequest() {
  FirstRunSetupRequest request;
  memset(&request, 0, sizeof request);
  return request;
}

bool replaceFirst(const char* source, const char* needle,
                  const char* replacement, char* output, size_t capacity) {
  const char* at = strstr(source, needle);
  if (at == nullptr) return false;
  const size_t prefix = static_cast<size_t>(at - source);
  const size_t suffix = strlen(at + strlen(needle));
  const size_t replacementLength = strlen(replacement);
  const size_t total = prefix + replacementLength + suffix;
  if (total + 1 > capacity) return false;
  memcpy(output, source, prefix);
  memcpy(output + prefix, replacement, replacementLength);
  memcpy(output + prefix + replacementLength,
         at + strlen(needle), suffix);
  output[total] = '\0';
  return true;
}
}

void setUp() {}
void tearDown() {}

void test_valid_request_decodes_every_explicit_config_field() {
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_EQUAL_INT(FRSR_OK,
      decodeFirstRunSetupRequest(kValid, strlen(kValid), request));
  TEST_ASSERT_EQUAL_STRING("482731", request.setupCode);
  TEST_ASSERT_EQUAL_STRING("628407", request.initialPin);
  TEST_ASSERT_EQUAL_INT(480, request.config.params.utcOffsetMin);
  TEST_ASSERT_EQUAL_INT(30, request.config.params.lateMin);
  TEST_ASSERT_EQUAL_INT(30, request.config.params.scanLeadMin);
  TEST_ASSERT_EQUAL_INT(10, request.config.termStart);
  TEST_ASSERT_EQUAL_INT(300, request.config.termEnd);
  TEST_ASSERT_EQUAL_INT(1, request.config.autoSync);
  TEST_ASSERT_EQUAL_INT(0, request.config.soundMode);
  TEST_ASSERT_EQUAL_INT(480, request.config.tt.day[0][0].start);
  TEST_ASSERT_EQUAL_INT(750, request.config.tt.day[0][0].end);
  TEST_ASSERT_TRUE(request.config.tt.day[4][1].enabled);
}

void test_caller_cannot_supply_ap_secret_pin_hash_or_pin_salt() {
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_EQUAL_INT(FRSR_OK,
      decodeFirstRunSetupRequest(kValid, strlen(kValid), request));
  TEST_ASSERT_EQUAL_INT(0, request.config.wifiPass[0]);
  TEST_ASSERT_EQUAL_INT(0, request.config.pinSet);
  for (size_t i = 0; i < sizeof request.config.pinHash; ++i)
    TEST_ASSERT_EQUAL_INT(0, request.config.pinHash[i]);
  for (size_t i = 0; i < sizeof request.config.pinSalt; ++i)
    TEST_ASSERT_EQUAL_INT(0, request.config.pinSalt[i]);
}

void test_output_remains_unchanged_on_invalid_request() {
  FirstRunSetupRequest request;
  memset(&request, 0x5A, sizeof request);
  FirstRunSetupRequest before = request;
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest("{}", 2, request));
  TEST_ASSERT_EQUAL_MEMORY(&before, &request, sizeof request);
}

void test_duplicate_outer_and_config_keys_are_rejected() {
  char json[sizeof kValid + 64];
  FirstRunSetupRequest request = emptyRequest();

  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"initial_pin\"", "\"setup_code\":\"482731\",\"initial_pin\"",
      json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));

  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"late_min\":30,",
      "\"late_min\":30,\"late_min\":30,", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
}

void test_unknown_outer_and_config_keys_are_rejected() {
  char json[sizeof kValid + 64];
  FirstRunSetupRequest request = emptyRequest();

  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"initial_pin\"",
      "\"ignored\":1,\"initial_pin\"", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));

  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"sound_mode\":0,",
      "\"sound_mode\":0,\"wifi_pass\":\"not-allowed\",",
      json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
}

void test_missing_required_config_field_is_rejected() {
  char json[sizeof kValid];
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"sound_mode\":0,", "", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
}

void test_setup_code_and_pin_require_exactly_six_ascii_digits() {
  const char* badValues[] = {
      "\"48273\"", "\"4827310\"", "\"48a731\"", "\"123 56\""
  };
  char json[sizeof kValid + 16];
  FirstRunSetupRequest request = emptyRequest();
  for (size_t i = 0; i < sizeof badValues / sizeof badValues[0]; ++i) {
    TEST_ASSERT_TRUE(replaceFirst(
        kValid, "\"482731\"", badValues[i], json, sizeof json));
    TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
        decodeFirstRunSetupRequest(json, strlen(json), request));
  }
  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"628407\"", "\"62840\"", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
}

void test_escaped_credentials_are_rejected() {
  char json[sizeof kValid + 16];
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"482731\"", "\"482\\u003731\"", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
}

void test_numeric_values_must_be_integers_without_exponents_or_leading_zero() {
  const char* badValues[] = {"480.0", "4.8e2", "0480"};
  char json[sizeof kValid + 16];
  FirstRunSetupRequest request = emptyRequest();
  for (size_t i = 0; i < sizeof badValues / sizeof badValues[0]; ++i) {
    TEST_ASSERT_TRUE(replaceFirst(
        kValid, "\"utc_offset_min\":480", badValues[i], json, sizeof json));
    TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
        decodeFirstRunSetupRequest(json, strlen(json), request));
  }
}

void test_timezone_schedule_and_sound_ranges_are_bounded() {
  const char* needles[] = {
      "\"utc_offset_min\":480",
      "\"utc_offset_min\":480",
      "\"late_min\":30",
      "\"scan_lead_min\":30",
      "\"sound_mode\":0"
  };
  const char* replacements[] = {"841", "-721", "241", "61", "3"};
  char json[sizeof kValid + 16];
  FirstRunSetupRequest request = emptyRequest();
  for (size_t i = 0; i < sizeof needles / sizeof needles[0]; ++i) {
    TEST_ASSERT_TRUE(replaceFirst(
        kValid, needles[i], replacements[i], json, sizeof json));
    TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
        decodeFirstRunSetupRequest(json, strlen(json), request));
  }
}

void test_timetable_requires_five_days_two_sessions_and_three_slot_values() {
  const char* badTimetables[] = {
      "\"timetable\":[[[480,750,true],[810,990,true]]]",
      "\"timetable\":[[[480,750,true]],[[480,750,true]],"
      "[[480,750,true]],[[480,750,true]],[[480,750,true]]]",
      "\"timetable\":[[[480,750],[810,990,true]],"
      "[[480,750,true],[810,990,true]],[[480,750,true],[810,990,true]],"
      "[[480,750,true],[810,990,true]],[[480,750,true],[810,990,true]]]"
  };
  char json[sizeof kValid + 256];
  FirstRunSetupRequest request = emptyRequest();
  for (size_t i = 0; i < sizeof badTimetables / sizeof badTimetables[0]; ++i) {
    TEST_ASSERT_TRUE(replaceFirst(
        kValid, kTimetable, badTimetables[i], json, sizeof json));
    TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
        decodeFirstRunSetupRequest(json, strlen(json), request));
  }
}

void test_timetable_enabled_flag_requires_json_boolean() {
  char json[sizeof kValid + 16];
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "480,750,true", "480,750,1", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
}

void test_term_days_are_nonnegative_and_ordered() {
  char json[sizeof kValid + 16];
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"term_start_day\":10", "\"term_start_day\":-1",
      json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));

  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "\"term_end_day\":300", "\"term_end_day\":9",
      json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
}

void test_term_days_allow_one_inclusive_day_but_reject_epoch_horizon_overflow() {
  char json[sizeof kValid + 32];
  char json2[sizeof kValid + 32];
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_TRUE(replaceFirst(kValid, "\"term_end_day\":300",
      "\"term_end_day\":10", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_OK, decodeFirstRunSetupRequest(json, strlen(json), request));
  TEST_ASSERT_EQUAL_INT(10, request.config.termStart);
  TEST_ASSERT_EQUAL_INT(10, request.config.termEnd);
  TEST_ASSERT_TRUE(replaceFirst(kValid, "\"term_end_day\":300",
      "\"term_end_day\":31446", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_OK, decodeFirstRunSetupRequest(json, strlen(json), request));
  TEST_ASSERT_EQUAL_INT(MAX_TERM_DAY, request.config.termEnd);
  TEST_ASSERT_TRUE(replaceFirst(json, "\"term_start_day\":10",
      "\"term_start_day\":31446", json2, sizeof json2));
  TEST_ASSERT_EQUAL_INT(FRSR_OK, decodeFirstRunSetupRequest(json2, strlen(json2), request));
  TEST_ASSERT_EQUAL_INT(MAX_TERM_DAY, request.config.termStart);
  TEST_ASSERT_EQUAL_INT(MAX_TERM_DAY, request.config.termEnd);
  TEST_ASSERT_TRUE(replaceFirst(kValid, "\"term_end_day\":300",
      "\"term_end_day\":31447", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST, decodeFirstRunSetupRequest(json, strlen(json), request));
  TEST_ASSERT_TRUE(replaceFirst(kValid, "\"term_end_day\":300",
      "\"term_end_day\":31447", json, sizeof json));
  TEST_ASSERT_TRUE(replaceFirst(json, "\"term_start_day\":10",
      "\"term_start_day\":31447", json2, sizeof json2));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST, decodeFirstRunSetupRequest(json2, strlen(json2), request));
}
void test_trailing_commas_at_both_object_levels_are_rejected() {
  char json[sizeof kValid + 16];
  FirstRunSetupRequest request = emptyRequest();

  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "}}", "},}", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));

  TEST_ASSERT_TRUE(replaceFirst(
      kValid, "]]}}", "]],}}", json, sizeof json));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
}

void test_trailing_data_and_empty_document_are_rejected() {
  char json[sizeof kValid + 16];
  FirstRunSetupRequest request = emptyRequest();
  strcpy(json, kValid);
  strcat(json, "junk");
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(json, strlen(json), request));
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest("", 0, request));
}

void test_request_body_limit_is_enforced() {
  char body[FIRST_RUN_SETUP_REQUEST_MAX_BYTES + 1];
  memset(body, ' ', sizeof body);
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_EQUAL_INT(FRSR_INVALID_REQUEST,
      decodeFirstRunSetupRequest(body, sizeof body, request));
}

void test_request_credentials_and_config_are_wiped() {
  FirstRunSetupRequest request = emptyRequest();
  TEST_ASSERT_EQUAL_INT(FRSR_OK,
      decodeFirstRunSetupRequest(kValid, strlen(kValid), request));
  clearFirstRunSetupRequest(request);
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&request);
  for (size_t i = 0; i < sizeof request; ++i)
    TEST_ASSERT_EQUAL_INT(0, bytes[i]);
}


void test_body_buffer_rejects_invalid_lengths_and_unstarted_operations() {
  FirstRunSetupBodyBuffer body;
  const uint8_t bytes[] = { 'a' };

  TEST_ASSERT_FALSE(body.ready());
  TEST_ASSERT_TRUE(body.data() == nullptr);
  TEST_ASSERT_EQUAL_UINT32(0, body.length());
  TEST_ASSERT_EQUAL_INT(FRSB_INVALID_LENGTH, body.begin(0));
  TEST_ASSERT_EQUAL_INT(FRSB_TOO_LARGE,
                       body.begin(FIRST_RUN_SETUP_REQUEST_MAX_BYTES + 1));
  TEST_ASSERT_EQUAL_INT(FRSB_BAD_STATE, body.append(bytes, 1, 1));
  TEST_ASSERT_EQUAL_INT(FRSB_BAD_STATE, body.finish());
  TEST_ASSERT_FALSE(body.ready());
}

void test_body_buffer_accumulates_split_chunks_only_after_finish() {
  FirstRunSetupBodyBuffer body;
  const uint8_t bytes[] = { 'a', 'b', 'c', 'd', 'e', 'f' };

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(sizeof bytes));
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.append(bytes, 2, 2));
  TEST_ASSERT_FALSE(body.ready());
  TEST_ASSERT_TRUE(body.data() == nullptr);
  TEST_ASSERT_EQUAL_UINT32(0, body.length());

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.append(bytes + 2, 4, 6));
  TEST_ASSERT_FALSE(body.ready());
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.finish());
  TEST_ASSERT_TRUE(body.ready());
  TEST_ASSERT_EQUAL_UINT32(sizeof bytes, body.length());
  TEST_ASSERT_EQUAL_MEMORY(bytes, body.data(), sizeof bytes);
  TEST_ASSERT_EQUAL_STRING("abcdef", body.data());
}

void test_body_buffer_rejects_inconsistent_cumulative_lengths() {
  FirstRunSetupBodyBuffer body;
  const uint8_t bytes[] = { 'a', 'b', 'c', 'd' };

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(4));
  TEST_ASSERT_EQUAL_INT(FRSB_INVALID_CHUNK, body.append(bytes, 2, 3));
  TEST_ASSERT_FALSE(body.ready());
  TEST_ASSERT_TRUE(body.data() == nullptr);
  TEST_ASSERT_EQUAL_UINT32(0, body.length());

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(4));
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.append(bytes, 2, 2));
  TEST_ASSERT_EQUAL_INT(FRSB_INVALID_CHUNK, body.append(bytes + 2, 2, 5));
  TEST_ASSERT_FALSE(body.ready());
}

void test_body_buffer_rejects_null_and_empty_chunks() {
  FirstRunSetupBodyBuffer body;
  const uint8_t bytes[] = { 'a', 'b' };

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(2));
  TEST_ASSERT_EQUAL_INT(FRSB_INVALID_CHUNK, body.append(nullptr, 1, 1));
  TEST_ASSERT_FALSE(body.ready());

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(2));
  TEST_ASSERT_EQUAL_INT(FRSB_INVALID_CHUNK, body.append(bytes, 0, 0));
  TEST_ASSERT_FALSE(body.ready());
}

void test_body_buffer_rejects_capacity_and_declared_length_overflow() {
  FirstRunSetupBodyBuffer body;
  uint8_t tooLarge[FIRST_RUN_SETUP_REQUEST_MAX_BYTES + 1];
  memset(tooLarge, 'x', sizeof tooLarge);

  TEST_ASSERT_EQUAL_INT(FRSB_OK,
                       body.begin(FIRST_RUN_SETUP_REQUEST_MAX_BYTES));
  TEST_ASSERT_EQUAL_INT(FRSB_TOO_LARGE,
                       body.append(tooLarge, sizeof tooLarge, sizeof tooLarge));
  TEST_ASSERT_FALSE(body.ready());

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(3));
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.append(tooLarge, 2, 2));
  TEST_ASSERT_EQUAL_INT(FRSB_TOO_LARGE, body.append(tooLarge + 2, 2, 4));
  TEST_ASSERT_FALSE(body.ready());

  uint8_t exactMaximum[FIRST_RUN_SETUP_REQUEST_MAX_BYTES];
  memset(exactMaximum, 'y', sizeof exactMaximum);
  TEST_ASSERT_EQUAL_INT(FRSB_OK,
                       body.begin(FIRST_RUN_SETUP_REQUEST_MAX_BYTES));
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.append(
      exactMaximum, sizeof exactMaximum, sizeof exactMaximum));
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.finish());
  TEST_ASSERT_EQUAL_UINT32(FIRST_RUN_SETUP_REQUEST_MAX_BYTES, body.length());
  TEST_ASSERT_EQUAL_MEMORY(exactMaximum, body.data(), sizeof exactMaximum);
}

void test_body_buffer_incomplete_and_abort_paths_allow_clean_reuse() {
  FirstRunSetupBodyBuffer body;
  const uint8_t partial[] = { 'a', 'b' };
  const uint8_t complete[] = { 'X', 'Y' };

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(4));
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.append(partial, sizeof partial, 2));
  TEST_ASSERT_EQUAL_INT(FRSB_INCOMPLETE, body.finish());
  TEST_ASSERT_FALSE(body.ready());
  TEST_ASSERT_TRUE(body.data() == nullptr);

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(4));
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.append(partial, sizeof partial, 2));
  body.abort();
  TEST_ASSERT_FALSE(body.ready());
  TEST_ASSERT_EQUAL_UINT32(0, body.length());

  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.begin(sizeof complete));
  TEST_ASSERT_EQUAL_INT(FRSB_OK,
                       body.append(complete, sizeof complete, sizeof complete));
  TEST_ASSERT_EQUAL_INT(FRSB_OK, body.finish());
  TEST_ASSERT_EQUAL_STRING("XY", body.data());
  body.clear();
  TEST_ASSERT_FALSE(body.ready());
  TEST_ASSERT_TRUE(body.data() == nullptr);
  TEST_ASSERT_EQUAL_UINT32(0, body.length());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_request_decodes_every_explicit_config_field);
  RUN_TEST(test_caller_cannot_supply_ap_secret_pin_hash_or_pin_salt);
  RUN_TEST(test_output_remains_unchanged_on_invalid_request);
  RUN_TEST(test_duplicate_outer_and_config_keys_are_rejected);
  RUN_TEST(test_unknown_outer_and_config_keys_are_rejected);
  RUN_TEST(test_missing_required_config_field_is_rejected);
  RUN_TEST(test_setup_code_and_pin_require_exactly_six_ascii_digits);
  RUN_TEST(test_escaped_credentials_are_rejected);
  RUN_TEST(test_numeric_values_must_be_integers_without_exponents_or_leading_zero);
  RUN_TEST(test_timezone_schedule_and_sound_ranges_are_bounded);
  RUN_TEST(test_timetable_requires_five_days_two_sessions_and_three_slot_values);
  RUN_TEST(test_timetable_enabled_flag_requires_json_boolean);
  RUN_TEST(test_term_days_are_nonnegative_and_ordered);
  RUN_TEST(test_term_days_allow_one_inclusive_day_but_reject_epoch_horizon_overflow);
  RUN_TEST(test_trailing_commas_at_both_object_levels_are_rejected);
  RUN_TEST(test_trailing_data_and_empty_document_are_rejected);
  RUN_TEST(test_request_body_limit_is_enforced);
  RUN_TEST(test_request_credentials_and_config_are_wiped);
  RUN_TEST(test_body_buffer_rejects_invalid_lengths_and_unstarted_operations);
  RUN_TEST(test_body_buffer_accumulates_split_chunks_only_after_finish);
  RUN_TEST(test_body_buffer_rejects_inconsistent_cumulative_lengths);
  RUN_TEST(test_body_buffer_rejects_null_and_empty_chunks);
  RUN_TEST(test_body_buffer_rejects_capacity_and_declared_length_overflow);
  RUN_TEST(test_body_buffer_incomplete_and_abort_paths_allow_clean_reuse);
  return UNITY_END();
}
