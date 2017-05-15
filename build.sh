#!/usr/bin/env bash
# 
# Copyright (c) 2014 Bernard `Guyzmo` Pratz
# Copyright (c) 2017 Ambroz Bizjak
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
#####################################################################################
# MAIN BUILD SCRIPT

#####################################################################################
# configure paths

ROOT=$(dirname $0)
BUILD=.

#####################################################################################
# Guard to avoid running this script with another shell than bash

[ -z "$BASH_VERSION" ] && (
    echo "This script shall be ran only using bash"
    exit 1
)

set -e

#####################################################################################
# main

if [ $# -lt 2 ]; then
    echo "Usage: $0 <target-script> <main-file> [-v]"
    exit 1
fi

TARGET_SCRIPT=$1
MAIN_FILE=$2

shift
shift

# PARSE EXTRA OPTIONS

while [ $# -gt 0 ]; do
    case "$1" in
        -v|--verbose)
            shift;
            V="set -x"
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Source target script
source "$TARGET_SCRIPT"

# Source build scripts
TARGETS=()
for f in "$ROOT"/scripts/*.sh; do
    source $f
done

# Call configure
configure nixbuild "$MAIN_FILE"

# Build
build

exit 0

#####################################################################################

