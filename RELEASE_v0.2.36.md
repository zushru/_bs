# v0.2.36 release note

This snapshot corrects a false-negative in the v0.2.35 `test_nvs_config` runtime error-path fixture. The fake had left the `attsec` NVS namespace absent, so the store failed at `nvs_open(NVS_READONLY)` with `ESP_ERR_NVS_NOT_FOUND`; the injected `getResult = ESP_FAIL` never reached `nvs_get_u16()`. The runtime then correctly attempted first-setup initialization and returned `EPAR_COUNTER_INITIALIZE_FAILED`, contradicting the test's intended assertion for a read I/O failure.

The fixture now models an existing namespace with an absent `pin_fails` key, so the injected error is exercised by `nvs_get_u16()`. It asserts exact open/get/close call counts, the runtime fault latch, no exposed coordinator, and that no set or commit occurs. Only the test fixture and documentation changed; production code and registration counts are unchanged. The correction has not been run under PlatformIO.
