#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
printf 'Legacy command name: forwarding to the current v0.2.50 verifier.\n'
exec "$ROOT/tools/verify_v0.2.50.sh" "${1:-targeted}"
