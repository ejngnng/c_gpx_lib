#!/usr/bin/env bash
set -euo pipefail

# run.sh - invoke compress_gpx.py on every GPX file in ../gpx
# Outputs are written into this directory.
# Usage: ./run.sh [tolerance]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY_SCRIPT="$SCRIPT_DIR/compress_gpx.py"
GPX_DIR="$SCRIPT_DIR/../gpx"
OUT_DIR="$SCRIPT_DIR"
TOL="${1:-10}"

if [ ! -f "$PY_SCRIPT" ]; then
    echo "Error: compress_gpx.py not found in $SCRIPT_DIR" >&2
    exit 1
fi

if command -v python3 >/dev/null 2>&1; then
    PY=python3
elif command -v python >/dev/null 2>&1; then
    PY=python
else
    echo "Error: python3 or python not found in PATH" >&2
    exit 2
fi

shopt -s nullglob
files=("$GPX_DIR"/*.gpx "$GPX_DIR"/*.GPX)
if [ ${#files[@]} -eq 0 ]; then
    echo "No GPX files found in: $GPX_DIR"
    exit 0
fi

for f in "${files[@]}"; do
    base="$(basename -- "$f")"
    name="${base%.*}"
    out="$OUT_DIR/${name}_comp.GPX"
    echo "Processing: $base -> $(basename -- "$out") (tol=${TOL})"
    "$PY" "$PY_SCRIPT" "$f" --output "$out" --tolerance "$TOL"
    rc=$?
    if [ $rc -ne 0 ]; then
        echo "Error: processing failed for $f (rc=$rc)" >&2
        exit $rc
    fi
done

echo "All GPX files processed. Outputs in: $OUT_DIR"
