#!/bin/bash


CFLAGS="-O2 -Wall -Wextra -std=c99"
OUTPUT="gpx_demo"
LIBS="-lm"

echo "start build $OUTPUT..."

# Resolve script directory so the script can be run from anywhere
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/../src"
MAIN="$SCRIPT_DIR/main_example.c"

# compile all .c sources from src directory
shopt -s nullglob
src_files=($SRC_DIR/*.c)
if [ ${#src_files[@]} -eq 0 ]; then
    echo "Error: no .c sources found in $SRC_DIR"
    exit 1
fi

if [ ! -f "$MAIN" ]; then
    echo "Error: demo main file not found: $MAIN"
    exit 1
fi

gcc $CFLAGS -I"$SRC_DIR" -o "$OUTPUT" "$MAIN" ${src_files[@]} $LIBS
rc=$?

if [ $rc -eq 0 ]; then
    echo "build $OUTPUT success!"
else
    echo "build $OUTPUT failed!!!"
    exit $rc
fi