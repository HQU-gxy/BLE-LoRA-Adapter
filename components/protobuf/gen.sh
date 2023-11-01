#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
$SCRIPT_DIR/nanopb/generator/nanopb_generator.py -D $SCRIPT_DIR/out -I proto proto/ble.proto
