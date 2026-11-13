#!/usr/bin/env python3
from pathlib import Path
import csv
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "target/esp32_attendance/main.cpp"
PIO = ROOT / "platformio.attendance.ini"
PARTITIONS = ROOT / "target/esp32_attendance/partitions_att.csv"

errors = []

def require(condition, message):
    if not condition:
        errors.append(message)

source = MAIN.read_text(encoding="utf-8")
pio = PIO.read_text(encoding="utf-8")
partition_text = PARTITIONS.read_text(encoding="utf-8")

webserver_patch = (ROOT / "tools/apply_pinned_webserver_patch.py").read_text(encoding="utf-8")
require("extra_scripts = pre:tools/apply_pinned_webserver_patch.py" in pio,
        "attendance target must apply the pinned WebServer patch before build")
for token in (
    "4.30312.261024", "3.3.12",
    "c24f1e65b4ce3fb7ab868f6bca7aab7af5dcdfd1ca81ce9716752a922e52ce78",
    "f3e7a09e3eac2f1588f2e17c0feaddc99b1a0303c1064aa535b0dee5a342e8b4",
    "kWebServerMaxRawBodyLen = 768",
    "contentLengthHeaderCount > 1",
    "transferEncodingHeaderCount > 0",
    "HTTP_MAX_POST_WAIT",
    "client.available()",
    "client.read(_currentRaw->buf, read_len)",
    "RAW_ABORTED",
    "atexit.register(restore_framework_source)",
    "Parsing.cpp.att-original"
):
    require(token in webserver_patch,
            f"bounded WebServer patch guard missing: {token}")
require('sendErrorResponse(client, "405 Method Not Allowed")' in webserver_patch and
        "!_currentHandler->canRaw(*this, _currentUri)" in webserver_patch,
        "body-bearing mutation requests without an explicit raw handler must be rejected before parsing")
require("if (_currentHandler && _currentHandler->canRaw(*this, _currentUri)) {" in webserver_patch and
        "if (!isForm && _currentHandler && _currentHandler->canRaw(*this, _currentUri)) {" in webserver_patch,
        "the patch must replace the multipart-excluding raw condition with unconditional raw-handler dispatch")
require("extra_scripts = pre:tools/apply_pinned_webserver_patch.py" in pio and
        "framework = arduino" in pio,
        "pinned framework patch must be scoped to the attendance PlatformIO environment")

closure_header = (ROOT / "lib/core/src/closure_engine.h").read_text(encoding="utf-8")
closure_source = (ROOT / "lib/core/src/closure_engine.cpp").read_text(encoding="utf-8")
closure_tests = (ROOT / "test/test_coreb2/test_main.cpp").read_text(encoding="utf-8")
require("MAX_CLOSURE_DAYS_PER_STEP = 1" in closure_header and
        "processedThroughDay_" in closure_header and
        "nextDueCheck_" in closure_header,
        "ClosureEngine must declare a one-day step budget and resumable progress state")
require("daysProcessed < MAX_CLOSURE_DAYS_PER_STEP" in closure_source and
        "now < nextDueCheck_" in closure_source and
        "pending() reports remaining backlog" in closure_source,
        "ClosureEngine must bound each successful step and avoid rescanning before a session end")
require("if (!indexOk_ || bad_ || log_.faulted())" in closure_source and
        "if (!closeSession(day, session, now))" in closure_source,
        "ClosureEngine must fail closed on index/health/log faults and stop after uncertain CLOSE append")

log_index_source = (ROOT / "lib/core/src/log_index.cpp").read_text(encoding="utf-8")
log_index_header = (ROOT / "lib/core/src/log_index.h").read_text(encoding="utf-8")
require("closes_[insertAt]" in log_index_source and
        "closes_[i] = closes_[i - 1]" in log_index_source,
        "LogIndex CLOSE references must be inserted in sorted order")
require(log_index_source.count("const size_t middle = first + (last - first) / 2") >= 2 and
        "const size_t insertAt = first" in log_index_source and
        "close.day < day" in log_index_source and
        "close.session < s" in log_index_source,
        "LogIndex insertion position and closed() lookup must use lower-bound binary search")
require("MAX_CLOSES = 800" in log_index_header and
        "CloseRef closes_[MAX_CLOSES]" in log_index_header,
        "LogIndex CLOSE lookup optimization must preserve the existing fixed-capacity array")
index_tests = (ROOT / "test/test_coreb2/test_main.cpp").read_text(encoding="utf-8")
require("RUN_TEST(test_close_index_binary_search_handles_out_of_order_and_duplicate)" in index_tests,
        "LogIndex sorted insertion / binary-search regression must remain registered")
for case in (
    "test_closure_step_is_bounded_to_one_day_and_resumes",
    "test_closure_step_waits_until_session_end_without_repeated_work",
    "test_ambiguous_close_append_is_reconciled_by_replay_before_retry"
):
    require(f"RUN_TEST({case})" in closure_tests,
            f"bounded closure regression missing: {case}")
require("ClosureEngine" not in source and "closure.step(" not in source,
        "bounded ClosureEngine primitive must remain uncomposed in the read-only boot shell")

payload_start = source.find("const char kNeedsSetupHtml")
payload_end = source.find("struct Application")
require(payload_start >= 0 and payload_end > payload_start,
        "static web payload block is missing")
if payload_start >= 0 and payload_end > payload_start:
    payload = source[payload_start:payload_end].lower()
    for secret_field in ("wifipass", "passphrase", "password", "ssid"):
        require(secret_field not in payload,
                f"web page/status payload must not expose {secret_field}")

require(source.count('app.http.on("') == 2,
        "expected exactly two explicit HTTP routes")
routes = re.findall(r'app\.http\.on\("([^"]+)",\s*HTTP_([A-Z]+)', source)
require(routes == [("/", "GET"), ("/api/status", "GET")],
        f"routes must be GET / and GET /api/status only; found {routes!r}")
require("HTTP_POST" not in source and "HTTP_PUT" not in source and
        "HTTP_DELETE" not in source and "HTTP_PATCH" not in source,
        "mutation HTTP method appears in boot shell")
require('app.http.send(405' in source and 'app.http.send(404' in source,
        "unsupported methods and unknown GET paths must be handled explicitly")

# The unauthenticated status endpoint must expose only its reviewed basic fields.
def object_keys(block):
    return re.findall(r'["\\\\]+([a-z_]+)["\\\\]+:', block)

setup_json_start = source.find("const char kNeedsSetupJson")
setup_json_end = source.find("struct Application", setup_json_start)
setup_json = (source[setup_json_start:setup_json_end]
              if setup_json_start >= 0 and setup_json_end > setup_json_start else "")
expected_setup_keys = [
    "boot", "ap", "storage", "rtc_observation", "clock_trusted",
    "attendance_enabled", "admin_routes_enabled",
    "config_provisioning_enabled", "sf2_export_enabled"
]
require(object_keys(setup_json) == expected_setup_keys,
        "NEEDS_SETUP status JSON keys changed; review the public-field allowlist")

status_start = source.find("void sendConfiguredStatus(Application& app)")
status_end = source.find("void addReadOnlyHeaders", status_start)
status_source = (source[status_start:status_end]
                 if status_start >= 0 and status_end > status_start else "")
expected_status_keys = [
    "boot", "ap", "storage", "log_replay", "log_records",
    "log_baseline_utc", "event_group_audit", "roster_history",
    "attendance_admission", "rtc_observation", "rtc_candidate_utc", "health",
    "clock_trusted", "attendance_enabled", "admin_routes_enabled",
    "config_provisioning_enabled", "sf2_export_enabled"
]
require(object_keys(status_source) == expected_status_keys,
        "READY status JSON keys changed; review the public-field allowlist")

setup_match = re.search(r"void setup\(\)\s*\{(.*?)\n\}", source, re.S)
require(setup_match is not None, "Arduino setup() is missing")
if setup_match:
    setup = setup_match.group(1)
    guard_at = setup.find("Esp32NvsStartupGuard::status()")
    construct_at = setup.find("static Application application;")
    require(guard_at >= 0 and construct_at > guard_at,
            "NVS startup guard must precede Application construction")
    require("if (!nvs.safeForApplication) return;" in setup,
            "unsafe NVS startup must fail closed")

for token in (
    "Esp32NvsConfigBackend", "ConfigBootCoordinator",
    "ApCredentialSetupCoordinator", "Esp32NvsApCredentialStore",
    "Esp32WifiAp", "Esp32I2cBusOwner", "Esp32Oled", "WebServer",
    "Esp32LittleFsMountBackend", "LittleFsStorageOwner", "MonthSet",
    "LogStore", "EventGroupAudit", "RosterStore",
    "LogAttendanceAdmission", "HealthFsm", "updateHealthFromConfigBoot",
    "Esp32PinAuthRuntime", "setupCodeAuthRuntime", "Esp32ClockMonotonicSource",
    "Esp32RamSetupCodeVerifier", "Esp32MbedTlsPinConfigRuntime",
    "AdminSessionBoundary", "AdminSetupCodeBoundary",
    "FirstRunSetupCodeSequence", "FirstRunSetupCodeBoundary", "serviceExpiry()"
):
    require(token in source, f"required boot-shell owner missing: {token}")

require("Esp32MbedTlsAdminPinWriter" not in source,
        "boot shell must not instantiate PIN writer before KDF policy and routes exist")

# Build the Application source slice before checking its members.
app_start = source.find("struct Application")
app_end = source.find("const char* rtcStartupObservationText", app_start)
app_block = source[app_start:app_end] if app_start >= 0 and app_end > app_start else ""

# C6/E35: compose the READY-only auth graph from exact runtime/verifier objects.
require("att::kE35AdminPinPbkdf2Iterations" in source and
        "alignas(att::Esp32MbedTlsPinConfigRuntime)" in app_block and
        "alignas(att::AdminSessionBoundary)" in app_block and
        "alignas(att::AdminSetupCodeBoundary)" in app_block,
        "Application must reserve aligned storage for the three long-lived auth owners")
require("new (app.pinConfigRuntimeMemory)" in source and
        "app.configBoot, att::kE35AdminPinPbkdf2Iterations" in source,
        "READY PIN verifier runtime must bind the exact ConfigBoot and E35 iteration count")
require("new (app.adminSessionsMemory)" in source and
        "att::AdminSessionBoundary(*pinAuth, *pinVerifier, app.runtimeRandom)" in source,
        "AdminSessionBoundary must use the exact attsec coordinator, live PIN verifier and AP RNG")
require("new (app.adminSetupCodeBoundaryMemory)" in source and
        "*app.adminSessions, *setupAuth, app.setupCodeVerifier" in source,
        "AdminSetupCodeBoundary must use the exact session owner, attsetup coordinator and RAM verifier")
require("app.setupCodeAuthRuntime.ready()" in source and
        "app.pinConfigRuntime->verifier()" in source,
        "P15 boundary must be created only when READY PIN verifier and attsetup runtime are ready")
require("adminSetupCodeBoundary->authenticateAndRun(" not in source and
        "adminSessions->authorize(" not in source,
        "boot shell must not expose protected operations before route integration")

# E33/E36: separate setup-code attempt runtime; never migrate READY state.
require("att::Esp32PinAuthRuntime setupCodeAuthRuntime;" in app_block and
        'setupCodeAuthRuntime("attsetup", configBoot)' in app_block and
        'pinAuthRuntime("attsec", configBoot)' in app_block,
        "Application must own distinct attsec and attsetup runtimes bound to one ConfigBoot")
app_start_for_p15 = source.find("void startApplication(Application& app)")
app_start_block = source[app_start_for_p15:] if app_start_for_p15 >= 0 else ""
pin_runtime_at = app_start_block.find("app.pinAuthRuntime.start(monotonicSeconds())")
setup_runtime_at = app_start_block.find("app.setupCodeAuthRuntime.start(monotonicSeconds())")
storage_at = app_start_block.find("startStorage(app)")
needs_setup_ap_at = app_start_block.find("app.firstRunSequence->start()")
ready_ap_at = app_start_block.find("startConfiguredAp(app)")
require(pin_runtime_at >= 0 and setup_runtime_at > pin_runtime_at and
        storage_at > setup_runtime_at and needs_setup_ap_at > setup_runtime_at and
        ready_ap_at > setup_runtime_at,
        "both attempt runtimes must start before storage and either AP branch")
require("alignas(att::FirstRunSetupCodeSequence)" in app_block and
        "alignas(att::FirstRunSetupCodeBoundary)" in app_block,
        "Application must reserve aligned first-run sequence and setup-boundary storage")
require("new (app.firstRunSequenceMemory)" in source and
        "att::FirstRunSetupCodeSequence(app.apSetup, app.setupCodeVerifier)" in source,
        "first-run sequence must bind the exact AP setup coordinator and RAM-only issuer")
require("new (app.firstRunSetupBoundaryMemory)" in source and
        "*setupAuth, app.setupCodeVerifier, *app.firstRunSequence" in source and
        "app.initialPinPreparer" in source,
        "first-run boundary must bind the exact attsetup coordinator, verifier, sequence and PIN preparer")
require("att::Esp32MbedTlsInitialPinPreparer initialPinPreparer;" in source and
        "initialPinPreparer(configBoot, runtimeRandom)" in source,
        "Application must bind initial PIN preparer to exact config boot and AP-owned RNG")
require("kE35AdminPinPbkdf2Iterations = 2048" in
        (ROOT / "lib/hal_esp32/src/esp32_mbedtls_pin_writer.cpp").read_text(encoding="utf-8") and
        "att::kE35AdminPinPbkdf2Iterations" in source,
        "initial PIN and READY verifier must share E35's exact 2,048-round constant")
require("app.firstRunSequence->start() != att::FSC_WAITING_FOR_CLIENT" in source,
        "NEEDS_SETUP must start through the combined E28/E33 AP sequence")
require(app_start_block.count("app.setupCodeAuthRuntime.start(") == 1 and
        "(void)app.setupCodeAuthRuntime.start(monotonicSeconds());" in app_start_block,
        "attsetup must have one startup call whose failure is scoped to P15")
require("app.faulted = true;" not in app_start_block[setup_runtime_at:storage_at],
        "attsetup failure must not globally fault the read-only diagnostic shell")
require("setupCodeVerifier.issue(" not in source and
        "setupCodeVerifier.verify(" not in source,
        "boot shell must not expose setup-code actions before auth boundary integration")
attempt_tests = (ROOT / "test/test_nvs_config/test_main.cpp").read_text(encoding="utf-8")
require("RUN_TEST(test_pin_auth_runtimes_load_and_mutate_only_their_own_namespace)" in attempt_tests and
        "RUN_TEST(test_pin_auth_runtime_ready_missing_setup_counter_fails_without_initialization)" in attempt_tests,
        "separate-domain and READY-missing-attsetup regression cases must remain registered")

# Initial PIN syntax, code authorization, health and credential derivation order.
sequence_core = (ROOT / "lib/core/src/setup_code_sequence.cpp").read_text(encoding="utf-8")
provision_at = sequence_core.find("FirstRunSetupDispatchResult FirstRunSetupCodeBoundary::provision(")
valid_pin_at = sequence_core.find("!validPin(initialPin, initialPinLength)", provision_at)
auth_at = sequence_core.find("setupCodeAuth_.authenticateAndRun(", provision_at)
require(provision_at >= 0 and 0 <= valid_pin_at < auth_at,
        "initial PIN syntax/weak-pattern rejection must precede setup-code consumption")
action_start = sequence_core.find("class FirstRunSetupCodeBoundary::ProvisionAction")
action_end = sequence_core.find("FirstRunSetupCodeBoundary::FirstRunSetupCodeBoundary", action_start)
action_block = sequence_core[action_start:action_end] if action_start >= 0 and action_end > action_start else ""
health_at = action_block.find("authorize(EP_SETUP, health_, false, true)")
pin_prepare_at = action_block.find("initialPinPreparer_.prepare(")
config_provision_at = action_block.find("sequence_.provision(prepared, gate_, bootResult_)")
require(health_at >= 0 and health_at < pin_prepare_at < config_provision_at,
        "initial PIN derivation must follow setup-code success/reset and health, then precede Config provision")
require("validateConfigExceptAdminPin" in
        (ROOT / "lib/core/src/config_store.cpp").read_text(encoding="utf-8"),
        "first-run config preflight must validate non-PIN fields without placeholder hashes")
require("ConfigProvisioner::provision" in
        (ROOT / "lib/core/src/config_provision.cpp").read_text(encoding="utf-8"),
        "first provision must still use the canonical ConfigProvisioner transaction")

# E33 expiry service is owned by Application and advanced only by Arduino loop.
require("att::Esp32ClockMonotonicSource monotonicClock;" in app_block and
        "att::Esp32RamSetupCodeVerifier setupCodeVerifier;" in app_block,
        "Application must own the P15 monotonic source and RAM verifier")
require("setupCodeVerifier(runtimeRandom, monotonicClock, oled)" in app_block,
        "P15 verifier must use the exact AP-owned RF random source, monotonic source and shared OLED")
loop_start = source.find("void loop()")
loop_block = source[loop_start:] if loop_start >= 0 else ""
expiry_positions = [m.start() for m in re.finditer(r"setupCodeVerifier\.serviceExpiry\(\)", loop_block)]
http_at = loop_block.find("gApp->http.handleClient()")
require(len(expiry_positions) == 2 and http_at >= 0 and
        expiry_positions[0] < http_at < expiry_positions[1],
        "P15 expiry must be serviced before and after synchronous HTTP dispatch")
require(loop_block.count("gApp->faulted = true;") >= 2,
        "P15 expiry failure must latch the application fault")

# E33 first-run issuance is gated by DHCP and remains on the serialized owner.
issue_at = loop_block.find("firstRunSequence->issueAfterClientAssigned()")
http_at = loop_block.find("gApp->http.handleClient()")
require(issue_at > expiry_positions[0] and http_at > issue_at,
        "first-run issuance must follow expiry service and precede HTTP dispatch")
require("!gApp->firstRunSetupBoundary->faulted()" in loop_block and
        "!gApp->firstRunSetupBoundary->completed()" in loop_block and
        "!gApp->setupCodeVerifier.faulted()" in loop_block,
        "first-run code reissue must stop after boundary/verifier fault or completion")
require("firstRunSetupBoundary->provision(" not in source and
        "Esp32FirstRunHttpHandler" not in source and "HTTP_POST" not in source,
        "read-only boot shell must not expose a first-run provisioning route before bounded framework request parsing exists")
require("loadExistingForSetup" in source,
        "first-run setup must expose the read-existing-only AP credential path")

# E15 first-run request contract is source-authored, but not reachable from the
# target until the pinned WebServer's multipart pre-parser can be bounded.
request_header = (ROOT / "lib/core/src/first_run_setup_request.h").read_text(encoding="utf-8")
request_core = (ROOT / "lib/core/src/first_run_setup_request.cpp").read_text(encoding="utf-8")
request_tests = (ROOT / "test/test_first_run_setup_request/test_main.cpp").read_text(encoding="utf-8")
request_handler = (ROOT / "lib/hal_esp32/src/esp32_first_run_http_handler.cpp").read_text(encoding="utf-8")
require("FIRST_RUN_SETUP_REQUEST_MAX_BYTES = 768" in request_header and
        "decodeFirstRunSetupRequest" in request_header and
        "clearFirstRunSetupRequest" in request_header,
        "first-run request decoder must expose a fixed body limit and credential wipe boundary")
for token in (
    '"setup_code"', '"initial_pin"', '"utc_offset_min"', '"late_min"',
    '"scan_lead_min"', '"term_start_day"', '"term_end_day"',
    '"auto_sync"', '"sound_mode"', '"timetable"'
):
    require(token in request_core,
            f"first-run JSON schema field missing: {token}")
require("} else {" in request_core,
        "first-run JSON decoder must reject unrecognized schema members")
require("if (peek_('}')) return false;" in request_core and
        "seen != 0x07" in request_core and "seen != 0x00FF" in request_core,
        "first-run decoder must reject trailing commas and missing/duplicate keys")
require("out = candidate;" in request_core and
        "clearFirstRunSetupRequest(candidate);" in request_core,
        "first-run decoder must leave output unchanged on failure and wipe its temporary copy")
require("char body_[FIRST_RUN_SETUP_REQUEST_MAX_BYTES + 1]" in
        (ROOT / "lib/hal_esp32/src/esp32_first_run_http_handler.h").read_text(encoding="utf-8") and
        "server.clientContentLength()" in request_handler and
        "Transfer-Encoding" in request_handler and
        "rawData.buf == nullptr" in request_handler and
        "RAW_WRITE" in request_handler and
        "Content-Length" in request_handler,
        "candidate HTTP adapter must use a fixed raw-body buffer and reject unsupported transfer framing")
require("boundary_.provision(" in request_handler and
        "request.setupCode" in request_handler and
        "request.initialPin" in request_handler,
        "candidate HTTP adapter must invoke the existing session-free first-run boundary")
require("const char kTimetable[]" in request_tests and
        "kTimetable, badTimetables[i]" in request_tests,
        "timetable shape regressions must retain their exact source fixture")
require("Esp32FirstRunHttpHandler" not in source and
        "app.http.addHandler(" not in source and
        "HTTP_POST" not in source,
        "first-run HTTP handler must remain uncomposed until multipart parsing is bounded")
for case in (
    "test_valid_request_decodes_every_explicit_config_field",
    "test_caller_cannot_supply_ap_secret_pin_hash_or_pin_salt",
    "test_output_remains_unchanged_on_invalid_request",
    "test_duplicate_outer_and_config_keys_are_rejected",
    "test_unknown_outer_and_config_keys_are_rejected",
    "test_missing_required_config_field_is_rejected",
    "test_setup_code_and_pin_require_exactly_six_ascii_digits",
    "test_escaped_credentials_are_rejected",
    "test_numeric_values_must_be_integers_without_exponents_or_leading_zero",
    "test_timezone_schedule_and_sound_ranges_are_bounded",
    "test_timetable_requires_five_days_two_sessions_and_three_slot_values",
    "test_timetable_enabled_flag_requires_json_boolean",
    "test_term_days_are_nonnegative_and_ordered",
    "test_trailing_commas_at_both_object_levels_are_rejected",
    "test_trailing_data_and_empty_document_are_rejected",
    "test_request_body_limit_is_enforced",
    "test_request_credentials_and_config_are_wiped"
):
    require(f"RUN_TEST({case})" in request_tests,
            f"first-run request decoder regression missing: {case}")

require("runningPassphrase_[AP_CREDENTIAL_BUFFER_SIZE]" in source and
        "equalSecret_(passphrase, runningPassphrase_, sizeof runningPassphrase_)" in source,
        "AP-start credential must be retained and compared against later NVS reads")
require("clearRunningPassphrase_()" in source and
        "~ApCredentialSetupCoordinator()" in source,
        "retained AP-start credential must be wiped on fault/completion/destruction")
setup_tests = (ROOT / "test/test_ap_credential_setup/test_main.cpp").read_text(encoding="utf-8")
for case in (
    "test_first_run_code_boundary_provisions_code_only_and_uses_disclosed_ap_credential",
    "test_first_run_wrong_code_is_persisted_before_verify_and_can_retry",
    "test_first_run_malformed_code_is_rejected_before_counter_or_verifier",
    "test_first_run_invalid_config_preflight_does_not_consume_setup_code",
    "test_first_run_health_policy_is_checked_after_code_and_counter_reset",
    "test_first_run_setup_lockout_blocks_sixth_verification_after_five_failures",
    "test_first_run_attempt_persist_failure_prevents_code_verification",
    "test_first_run_counter_reset_failure_prevents_config_provision",
    "test_first_run_ambiguous_config_write_faults_setup_boundary",
    "test_missing_ap_credential_after_start_never_generates_replacement",
    "test_first_run_code_is_reissued_after_expiry_only_while_setup_waits",
    "test_first_run_code_boundary_waits_for_dhcp_before_authentication",
    "test_changed_valid_ap_credential_after_start_faults_without_provision"
):
    require(f"RUN_TEST({case})" in setup_tests,
            f"first-run setup regression missing: {case}")
require("test_first_run_weak_pin_is_rejected_before_setup_code_attempt" in setup_tests and
        "test_initial_pin_preparation_failure_faults_after_code_consumption" in setup_tests,
        "first-run PIN syntax and post-auth preparation-failure cases must remain registered")
sequence_header = (ROOT / "lib/core/src/setup_code_sequence.h").read_text(encoding="utf-8")
require("bool restartReady() const" in sequence_header and
        "sequence_.provisioningCleanupComplete()" in sequence_header and
        "apSetup_.state() == ACSS_PROVISIONED" in sequence_header,
        "first-run restart must require completed provision and successful OLED blank request")
require("class FirstRunProvisioningRestartGate" in sequence_header and
        "if (requested_ || !boundary.restartReady()) return false;" in sequence_core and
        "requested_ = true;" in sequence_core,
        "first-run platform restart gate must be one-shot and completion-gated")
require("#include <esp_system.h>" in source and "void restartAfterProvisioning() override { esp_restart(); }" in source,
        "ESP32 restart adapter must use the platform reset primitive")
restart_at = loop_block.find("firstRunRestart.service(")
clear_retry_at = loop_block.find("firstRunSequence->retryBlankFrame()")
require(http_at >= 0 and clear_retry_at > http_at and restart_at > clear_retry_at,
        "first-run clear retry and restart must run only after the synchronous HTTP handler returns")
require("nextProvisionClearRetryUs = nowUs + 1000000LL;" in loop_block,
        "OLED clear retry must be throttled to a bounded one-second cadence")
require("RUN_TEST(test_first_run_provisioning_restart_waits_for_commit_and_is_one_shot)" in setup_tests,
        "first-run restart ordering/pending-clear regression must remain registered")
pin_tests = (ROOT / "test/test_esp32_mbedtls_pin_verifier/test_main.cpp").read_text(encoding="utf-8")
for case in (
    "test_initial_pin_preparer_uses_rf_rng_and_exact_e35_work_factor_without_persisting",
    "test_initial_pin_preparer_refuses_ready_boot_before_random_or_kdf",
    "test_initial_pin_preparer_rejects_malformed_and_weak_pins_before_entropy",
    "test_initial_pin_preparer_refuses_unavailable_rf_random_without_mutation",
    "test_initial_pin_preparer_rejects_zero_salt_before_pbkdf",
    "test_initial_pin_preparer_kdf_error_and_zero_hash_never_mutate_config",
    "test_initial_pin_preparer_rejects_invalid_non_pin_config_after_derivation"
):
    require(f"RUN_TEST({case})" in pin_tests,
            f"initial PIN preparer regression missing: {case}")
config_tests = (ROOT / "test/test_config/test_main.cpp").read_text(encoding="utf-8")
for case in (
    "test_initial_pin_preflight_validates_non_pin_fields_only",
    "test_initial_pin_preflight_still_rejects_bad_wifi_and_term"
):
    require(f"RUN_TEST({case})" in config_tests,
            f"initial PIN Config preflight regression missing: {case}")
require("setupCodeVerifier.issue(" not in source and
        "setupCodeVerifier.verify(" not in source,
        "read-only boot shell must not issue/verify setup codes or enable protected routes")

for token in ("Esp32Ds3231RegisterIo", "Ds3231Clock", "readUtcEpoch(0, reading)",
              "EPOCH_2020", "rtc_observation"):
    require(token in source,
            f"signed UTC RTC diagnostic contract missing: {token}")
rtc_probe_start = source.find("void observeRtcStartup(Application& app)")
rtc_probe_end = source.find("void sendConfiguredStatus", rtc_probe_start)
rtc_probe = source[rtc_probe_start:rtc_probe_end] if rtc_probe_start >= 0 and rtc_probe_end > rtc_probe_start else ""
require("app.bootState != att::CFG_BOOT_READY" in rtc_probe,
        "RTC startup probe must skip NEEDS_SETUP")
require("readUtcEpoch(0, reading)" in rtc_probe and
        "clearOscillatorStopFlag" not in rtc_probe and
        "writeUtcEpoch" not in rtc_probe,
        "RTC probe must read UTC at offset zero without modifying RTC state")
require("reading.oscillatorStopped" in rtc_probe and
        "reading.epochUtc < att::EPOCH_2020" in rtc_probe and
        "RTC_STARTUP_CANDIDATE_UNTRUSTED" in rtc_probe,
        "RTC probe must classify OSF, the minimum epoch and an untrusted candidate")
rtc_call = source.find("observeRtcStartup(app);")
bus_start = source.find("app.i2cBus.begin(kSdaPin, kSclPin, kI2cClockHz)")
auth_start = source.find("app.pinAuthRuntime.start(monotonicSeconds())")
require(rtc_call > bus_start and rtc_call < auth_start,
        "READY-only RTC observation must follow shared I2C initialization and precede later startup owners")
clock_disabled = chr(92) + chr(34) + "clock_trusted" + chr(92) + chr(34) + ":false"
attendance_disabled = chr(92) + chr(34) + "attendance_enabled" + chr(92) + chr(34) + ":false"
require(clock_disabled in source and attendance_disabled in source,
        "RTC diagnostic must never mark clock or attendance enabled")

for forbidden in (
    "nvs_flash_erase", "esp_partition_erase_range", "As608", "AS608",
    "ClosureEngine", "EV_SCAN", "HTTP_POST", "HTTP_PUT",
    "HTTP_DELETE", "HTTP_PATCH", "format_if_mount_failed = true",
    "truncateFile(", "quarantineCopy(", "quarantineFrom("
):
    require(forbidden not in source,
            f"boot shell must not include/use {forbidden}")

for name in (
    "attendance_enabled", "admin_routes_enabled",
    "config_provisioning_enabled", "sf2_export_enabled"
):
    flag = chr(92) + chr(34) + name + chr(92) + chr(34) + ":false"
    require(flag in source, f"status must explicitly keep {name} disabled")

for header in (
    "Cache-Control", "no-store", "X-Content-Type-Options",
    "nosniff", "Referrer-Policy", "no-referrer",
    "Content-Security-Policy", "frame-ancestors 'none'"
):
    require(header in source, f"required read-only response header missing: {header}")

require("espressif32@8.0.0" in pio, "PlatformIO platform version is not pinned")
require("board = esp32dev" in pio, "ESP32 Dev Module board is not selected")
require("board_build.flash_size = 4MB" in pio, "4 MB flash size is not explicit")
mount_backend = (ROOT / "lib/hal_esp32/src/esp32_littlefs_mount.cpp").read_text(encoding="utf-8")
require("isSignedLittleFsPartitionGeometry(partition->address" in mount_backend,
        "LittleFS mount must reject a partition outside signed geometry")
mount_header = (ROOT / "lib/hal_esp32/src/esp32_littlefs_mount.h").read_text(encoding="utf-8")
require("kSignedLittleFsPartitionAddress = 0x190000UL" in mount_header and
        "kSignedLittleFsPartitionSize = 0x270000UL" in mount_header and
        'kSignedLittleFsPartitionLabel[] = "spiffs"' in mount_header,
        "HAL partition identity must match signed address, size and label")
require("config.format_if_mount_failed = false" in mount_backend,
        "LittleFS mount must never format after mount failure")
require("config.read_only = true" in mount_backend,
        "boot-shell LittleFS mount must be read-only")
boot_start = source.find("void startApplication(Application& app)")
ap_start = source.find("bool startConfiguredAp(Application& app)")
storage_start = source.find("bool startStorage(Application& app)")
require(storage_start >= 0 and boot_start > storage_start and ap_start > storage_start,
        "storage owner must be defined before READY boot/AP composition")
require("app.bootState == att::CFG_BOOT_READY && !startStorage(app)" in source,
        "only READY boot may mount storage, and failure must block AP startup")
require(source.count("startStorage(app)") == 1,
        "READY-only storage startup must be the only call site")
require(source.count("storageOwner->begin()") == 1,
        "the single production storage owner must have one startup call")
require("if (app.oledReady) (void)app.oled.showStorageFault();" in source,
        "READY storage failure must be visible on an available OLED")
auth_start = source.find("if (!app.pinAuthRuntime.start(monotonicSeconds()))")
storage_gate = source.find("if (app.bootState == att::CFG_BOOT_READY && !startStorage(app))")
setup_gate = source.find("if (app.bootState == att::CFG_BOOT_NEEDS_SETUP)", storage_gate)
require(auth_start > boot_start and auth_start < storage_gate and
        auth_start < setup_gate,
        "PIN-attempt runtime must start after config boot and before storage/AP branches")
require(source.count("pinAuthRuntime.start(") == 1,
        "PIN-attempt runtime must have exactly one startup call")
require("if (app.oledReady) (void)app.oled.showAuthFault();" in source,
        "PIN-attempt startup failure must be visible on an available OLED")
timer_start = source.find("uint32_t monotonicSeconds()")
timer_end = source.find("void addReadOnlyHeaders", timer_start)
timer_source = source[timer_start:timer_end] if timer_start >= 0 and timer_end > timer_start else ""
require("#include <esp_timer.h>" in source and "esp_timer_get_time()" in timer_source,
        "auth time must use ESP-IDF monotonic timer")
require("millis()" not in timer_source,
        "auth time must not use the wrapping millisecond counter")
require("layout_validated" in source and "not_started" in source,
        "status must distinguish READY storage validation from NEEDS_SETUP")
replay_start = source.find("bool startStorage(Application& app)")
replay_end = source.find("void updateRuntimeHealth(Application& app)", replay_start)
replay_source = source[replay_start:replay_end] if replay_start >= 0 and replay_end > replay_start else ""
require("new (app.monthSetMemory) att::MonthSet(*files)" in replay_source and
        "new (app.logStoreMemory) att::LogStore(*app.months)" in replay_source and
        "app.log->open()" in replay_source and
        "opened.baselineEpoch" in replay_source and
        "opened.ok" in replay_source,
        "READY storage startup must replay the complete MonthSet/LogStore stream before publishing baseline")
require("eventGroupAudit.attach(*app.log)" in replay_source and
        "eventGroupAudit.readyFor(*app.log, opened)" in replay_source and
        "hasObserver(&app.eventGroupAudit)" in replay_source,
        "READY startup must attach and validate event commit-group structure")
require("new (app.rosterMemory) att::RosterStore(config->params.utcOffsetMin)" in replay_source and
        "app.log->attach(app.roster)" in replay_source and
        "app.roster->historyComplete()" in replay_source,
        "READY startup must replay a complete operational roster from the same log")
require("new (app.attendanceAdmissionMemory)" in replay_source and
        "att::LogAttendanceAdmission(*app.log, config->params)" in replay_source and
        "app.attendanceAdmission->attached()" in replay_source and
        "app.attendanceAdmission->usesParams(config->params)" in replay_source,
        "READY startup must attach the exact-config log-backed admission projection")
require(re.search(r"\bRosterTransitionAudit\s+\w+\s*(?:;|=|\()", source) is None and
        "replayRosterPrefix(" not in source,
        "E27 reporting-only transition audit must not be constructed or called by operational boot")
require("app.log->append(" not in source and
        "app.log->truncate(" not in source and
        "app.months->setMonth(" not in source and
        "app.months->repair(" not in source and
        "appendFile(" not in source and "addFile(" not in source and
        "truncateFile(" not in source,
        "boot shell log/MonthSet path must remain strictly read-only")
health_header = (ROOT / "lib/core/src/health_fsm.h").read_text(encoding="utf-8")
require("bool clockTrusted" in health_header,
        "HealthFsm must require an explicit trusted-clock input")
health_source = (ROOT / "lib/core/src/health_fsm.cpp").read_text(encoding="utf-8")
require("in.provisioned && (!in.clockTrusted" in health_source and
        "else if (!in.sensorOk)" in health_source,
        "HealthFsm must require clock/sensor readiness only after provisioning")
health_start = source.find("void updateRuntimeHealth(Application& app)")
health_end = source.find("bool startConfiguredAp", health_start)
health_block = source[health_start:health_end] if health_start >= 0 and health_end > health_start else ""
require("app.eventGroupAuditReady && app.rosterHistoryReady &&" in health_block and
        "app.attendanceAdmissionAttached" in health_block,
        "runtime health must fail closed unless event, roster and admission projections are ready")
require("app.logBaselineEpoch != 0" in health_block and
        "app.rtcEpochUtc < app.logBaselineEpoch" in health_block and
        "false};" in health_block,
        "runtime health must compare UTC candidate with replayed baseline and keep clock untrusted")
for dependency in (
    "Adafruit SSD1306@2.5.17",
    "Adafruit GFX Library@1.12.6",
    "Adafruit BusIO@1.17.4"
):
    require(dependency in pio, f"dependency not pinned: {dependency}")

rows = []
for row in csv.reader(line for line in partition_text.splitlines()
                       if line.strip() and not line.lstrip().startswith("#")):
    require(len(row) >= 5, f"invalid partition row: {row!r}")
    if len(row) >= 5:
        rows.append((row[0], int(row[3], 0), int(row[4], 0)))
expected = [
    ("nvs", 0x9000, 0x5000),
    ("otadata", 0xE000, 0x2000),
    ("factory", 0x10000, 0x180000),
    ("spiffs", 0x190000, 0x270000),
]
require(rows == expected, f"partition layout mismatch: {rows!r}")
require(max((offset + size for _, offset, size in rows), default=0) == 0x400000,
        "partition map does not end at 4 MB")

if errors:
    for error in errors:
        print(f"FAIL: {error}", file=sys.stderr)
    raise SystemExit(1)

print("PASS: read-only boot-shell and READY-only storage-owner contract")
print("PASS: startup guard precedes NVS-backed owner construction")
print("PASS: persistent PIN-attempt runtime starts before AP/HTTP with monotonic time")
print("PASS: exactly GET / and GET /api/status; mutations are disabled")
print("PASS: NEEDS_SETUP and READY status JSON use the reviewed public-field allowlists")
print("PASS: response security headers and disabled feature flags present")
print("PASS: READY-only read-only cross-month MonthSet/LogStore replay")
print("PASS: explicit clock trust required only after provisioning")
print("PASS: separate E33 attsetup runtime starts before storage/AP; READY migration remains disabled")
print("PASS: E33 P15 expiry service is owned and ticked around synchronous HTTP; issuance/routes remain disabled")
print("PASS: strict first-run JSON decoder and 768-byte raw-body handler candidate; POST route remains disabled")
print("PASS: hash-pinned WebServer raw-only body guard, 768-byte/5-second bounds and duplicate-framing rejection")
print("PASS: pinned dependencies and signed 4 MB partition table")
