# v0.2.35 release note

This snapshot fixes two C++ test compile errors in `test_nvs_config`: the non-copyability assertions now refer to `att::Esp32NvsPinAttemptStore` and `att::Esp32PinAuthRuntime`. No production code or test count changed. The correction has not been compiled or run; use `RELEASE.md` for exact evidence and verification commands.
