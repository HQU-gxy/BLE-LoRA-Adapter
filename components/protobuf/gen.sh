#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
$SCRIPT_DIR/nanopb/generator/nanopb_generator.py -D $SCRIPT_DIR/out -I $SCRIPT_DIR/proto $SCRIPT_DIR/proto/ble.proto
