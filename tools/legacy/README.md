# Legacy version-specific verification scripts

The `verify_v0.2.*.sh` files in this directory were written for specific historical ELF/source snapshots. They are retained as historical evidence tools, not as a current all-purpose verification suite.

Before using one, inspect the script, its expected symbol set and its release/audit record. It may reject a newer ELF or encode an older architecture. Do not run it as a substitute for the current focused tests. Do not flash any image as part of ELF inspection.

Run with `bash tools/legacy/verify_v0.2.xx.sh ...` unless file permissions are preserved by the receiving environment.
