# Module specification index

The specifications define module contracts; they do not prove implementation or integration. Paths and test-suite names below are navigation aids derived from the current source layout. Before changing a module, inspect the test registration bodies and callers to confirm exact coverage. Tests may overlap and are not additive proof.

| Specification | Contract focus | Implementation starting point | Test starting point |
|---|---|---|---|
| [`ap_credential_bootstrap.md`](ap_credential_bootstrap.md) | AP credential bootstrap, secure randomness and NVS credential persistence | `lib/core/src/ap_credential_bootstrap.*; lib/hal_esp32/src/nvs_ap_credential_store.*` | `test_ap_credential; test_nvs_ap_credential; test_esp32_bootstrap_random` |
| [`ap_credential_setup.md`](ap_credential_setup.md) | First-provision setup coordinator and AP readiness sequence | `lib/core/src/ap_credential_setup.*; lib/hal_esp32/src/esp32_wifi_ap.*` | `test_ap_credential_setup; test_esp32_wifi_ap` |
| [`first_run_setup_request.md`](first_run_setup_request.md) | Proposed strict first-run JSON schema and bounded HTTP adapter; target route intentionally not composed | `lib/core/src/first_run_setup_request.*; lib/hal_esp32/src/esp32_first_run_http_handler.*` | `test_first_run_setup_request` |
| [`api_logic.md`](api_logic.md) | Hardware-independent API validation, request logic and token behavior | `lib/core/src/api_logic.*; pin/auth support` | `test_api; related auth/session suites` |
| [`as608_adapter.md`](as608_adapter.md) | AS608 transport, sensor ownership, startup and adapter contract | `lib/as608/src/*; lib/hal_esp32/src/as608_*; stream_as608_link.*` | `test_as608; test_as608_gate; test_as608_pipeline; test_enroll` |
| [`as608_attendance_pipeline.md`](as608_attendance_pipeline.md) | Scan-to-attendance pipeline and admission policy | `lib/as608/src/as608_attendance_pipeline.*; as608_attendance_client.*; log_attendance_admission.cpp` | `test_as608_pipeline; test_attendance_admission` |
| [`backup_format.md`](backup_format.md) | Backup container/format validation contract | `lib/core/src/backup_format.*; restore_replay.*` | `test_backup; test_restore_replay` |
| [`clock_policy.md`](clock_policy.md) | Clock set policy, ordering and GAP semantics | `lib/core/src/clock_policy.*; time_rules.*; DS3231 adapter callers` | `test_core; test_coreb2; test_config_apply; test_ds3231` |
| [`config_apply.md`](config_apply.md) | A/B configuration commit/apply coordinator | `lib/core/src/config_apply.*; config_runtime_guard.*` | `test_config_apply; test_config_guard` |
| [`config_boot.md`](config_boot.md) | Config boot, startup guard and first provisioning | `lib/core/src/config_boot.*; config_provision.*; ESP32 NVS startup adapters` | `test_config_boot; test_config_provision; test_nvs_config; test_nvs_startup_guard_policy` |
| [`config_runtime_guard.md`](config_runtime_guard.md) | Runtime configuration change preconditions | `lib/core/src/config_runtime_guard.*; config_apply.*` | `test_config_guard; test_config_apply` |
| [`config_store.md`](config_store.md) | Persistent config encoding, validation and A/B slot semantics | `lib/core/src/config_store.*` | `test_config; test_nvs_config` |
| [`core_fixes.md`](core_fixes.md) | Historical Core B fixes and their code/test provenance | `lib/core/src/ event/log/closure/roster modules` | `test_coreb; test_coreb2; test_hardening` |
| [`diag_sketch.md`](diag_sketch.md) | Legacy diagnostic sketch constraints | `sketch/sketch_compact_minimal_fixed.ino` | `test_hardware_probe; source/manual review` |
| [`ds3231_clock.md`](ds3231_clock.md) | DS3231 clock and ESP32 register-I/O adapter | `lib/hal_esp32/src/ds3231_clock.*; esp32_ds3231_register_io.*` | `test_ds3231; test_i2c_bus_owner` |
| [`enroll_fsm.md`](enroll_fsm.md) | Enrollment finite-state machine contract | `lib/core/src/enroll_fsm.*; lib/as608/src/as608_enroll_sensor.*` | `test_enroll; test_as608` |
| [`enrolled_roster.md`](enrolled_roster.md) | Enrollment/roster relationship and validation | `lib/core/src/enrolled_roster.*; roster snapshots/history` | `test_roster; test_roster_snapshot; test_roster_history` |
| [`esp32_oled.md`](esp32_oled.md) | SSD1306 OLED hardware adapter | `lib/hal_esp32/src/esp32_oled.*` | `test_esp32_oled` |
| [`esp32_wifi_ap.md`](esp32_wifi_ap.md) | ESP32 SoftAP configuration and startup behavior | `lib/hal_esp32/src/esp32_wifi_ap.*; AP credential stores` | `test_esp32_wifi_ap; test_ap_credential; test_ap_credential_setup` |
| [`littlefs_recovery_protocol.md`](littlefs_recovery_protocol.md) | LittleFS recovery proposal and constraints | `lib/littlefs_file_set/src/*; lib/core/src/month_set.*; log_store.*` | `test_littlefs_fileset; test_littlefs_mount_owner; test_littlefs_serialized_fileset; test_monthset; test_restore_replay` |
| [`month_set.md`](month_set.md) | File-set/month-set/log-store storage interface and month semantics | `lib/core/src/fileset.h; month_set.*; log_store.*` | `test_monthset; test_event_group_audit; test_restore_replay` |
| [`roster_store.md`](roster_store.md) | Roster events, lifecycle, names and history | `lib/core/src/roster_store.*; roster_name_history.*; roster_snapshot.*` | `test_roster; test_roster_history; test_roster_snapshot; test_month_roster` |

## Status cautions

- The specifications have different histories and authority classifications. Read the relevant `DECISIONS.md` entries before implementing any delegated/open rule.
- `littlefs_recovery_protocol.md` is a design proposal; it does not authorize destructive recovery or establish target LittleFS durability.
- `diag_sketch.md` describes legacy diagnostic material, not the production path.
- An owning test suite is a starting point for inspection, not proof that every requirement in the specification is covered.
