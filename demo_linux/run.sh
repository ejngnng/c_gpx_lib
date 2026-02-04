#!/usr/bin/env bash
set -euo pipefail

# run.sh - build project then run gpx_demo on every GPX file in ../gpx
# Usage: ./run.sh [tolerance]
#   tolerance: optional simplification tolerance in meters (default 10)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GPX_DIR="$SCRIPT_DIR/../gpx"
TOL="${1:-10}"

echo "Building with build.sh..."
"$SCRIPT_DIR/build.sh"
echo "Build finished."

shopt -s nullglob
files=("$GPX_DIR"/*.gpx "$GPX_DIR"/*.GPX)
if [ ${#files[@]} -eq 0 ]; then
    echo "No GPX files found in: $GPX_DIR"
    exit 0
fi

for f in "${files[@]}"; do
    base="$(basename -- "$f")"
    name="${base%.*}"
    out="${name}_comp.GPX"
    echo "Processing: $base -> $(basename -- "$out") (tol=${TOL})"
    "./gpx_demo" "$f" --output "$out" --tol "$TOL"
    rc=$?
    if [ $rc -ne 0 ]; then
        echo "Error: processing failed for $f (rc=$rc)" >&2
        exit $rc
    fi
done

echo "All GPX files processed. Outputs in: $SCRIPT_DIR"
