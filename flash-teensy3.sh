#!/bin/bash

set -e
set -x

TEENSY_LOADER=/home/ambro/teensy_loader_cli/teensy_loader_cli

"$TEENSY_LOADER" -mmcu=mk20dx128 out/aprinter.hex
