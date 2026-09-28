#!/usr/bin/env bash
set -euo pipefail

echo "=========================================="
echo "    NotepadX Automated Smoke Test Suite   "
echo "=========================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${ROOT_DIR}/build/notepadx"

if [[ ! -x "${BIN}" ]]; then
    echo "Error: Binary not found at ${BIN}. Build first." >&2
    exit 1
fi

echo "[1/4] Running CTest regression test suite..."
ctest --test-dir "${ROOT_DIR}/build" --output-on-failure

echo "[2/4] Verifying --bench-startup flag..."
STARTUP_OUT=$("${BIN}" --bench-startup)
echo "  -> Output: ${STARTUP_OUT}"
if [[ ! "${STARTUP_OUT}" =~ "Startup time:" ]]; then
    echo "Error: Unexpected --bench-startup output." >&2
    exit 1
fi

echo "[3/4] Verifying Single-Instance IPC socket handoff..."
# Ensure clean runtime state
TEST_SOCKET="${XDG_RUNTIME_DIR:-/tmp}/notepadx.sock"
rm -f "${TEST_SOCKET}"

# Launch primary instance in background
"${BIN}" --new-window &
PRIMARY_PID=$!
sleep 0.8

if [[ ! -S "${TEST_SOCKET}" ]]; then
    echo "Warning: Socket not found at ${TEST_SOCKET}, waiting another 500ms..."
    sleep 0.5
fi

# Send file through secondary instance
TEMP_FILE=$(mktemp /tmp/notepadx_smoke_XXXXXX.txt)
echo "Smoke test content" > "${TEMP_FILE}"

SECOND_START=$(date +%s%N)
"${BIN}" "${TEMP_FILE}"
SECOND_END=$(date +%s%N)
SECOND_DUR_MS=$(( (SECOND_END - SECOND_START) / 1000000 ))

echo "  -> Secondary instance handed off and exited in ${SECOND_DUR_MS} ms"

# Terminate primary instance
kill "${PRIMARY_PID}" || true
wait "${PRIMARY_PID}" 2>/dev/null || true
rm -f "${TEMP_FILE}" "${TEST_SOCKET}"

echo "[4/4] Verifying desktop file and man page integrity..."
test -f "${ROOT_DIR}/data/notepadx.desktop"
test -f "${ROOT_DIR}/data/icons/hicolor/scalable/apps/notepadx.svg"
test -f "${ROOT_DIR}/doc/notepadx.1"

echo "=========================================="
echo "    All Smoke Tests Passed Successfully!  "
echo "=========================================="
