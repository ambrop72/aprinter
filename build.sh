#!/usr/bin/env bash
# 
# Simple build script crafted for the APrinter project to support multiple 
# architecture targets and build actions using an elegant commandline.
# 
# Copyright (c) 2014 Bernard `Guyzmo` Pratz
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
SPATH=$ROOT/main
BUILD=$ROOT/build

#####################################################################################
# Guard to avoid running this script with another shell than bash

[ -z "$BASH_VERSION" ] && (
    echo "This script shall be ran only using bash"
    exit 1
)

set -e

#####################################################################################
# source config and build functions

TARGETS=()

for f in config/*.sh; do
    source $f
done

for f in scripts/*.sh; do
    source $f
done

#####################################################################################
# main

TARGETS_STR=$(IFS=\| ; echo "${TARGETS[*]}")
USAGE="Usage: $0 (${TARGETS_STR}|all) [-v] build"

if [ $# -lt 2 ]; then
    echo $USAGE
    exit 1
fi

# PARSE BUILD TARGET

if [ "$1" != "all" ]; then
    in_array $1 ${TARGETS[*]} || (
        echo $USAGE
        echo "Error: unknown target $1"
        exit 1
    )
fi

DEST=$1

# PARSE BUILD ACTION

ACT=()

shift

while true; do
    case "$1" in
        -v|--verbose)
            shift;
            V="set -x"
            ;;
        build)
            shift;
            ACT+=("build")
            ;;
        *)
            echo "Unknown command: $1"
            exit 1
            ;;
    esac
    if [ $# -eq 0 ]; then
        break
    fi
done

# LAUNCH ACTION ON TARGET

if [ "$DEST" = "all" ]; then
    for DEST in ${TARGETS[@]}; do
         in_array "build" ${ACT[*]} && (
            configure $DEST
            build
         )
    done
else
    configure $DEST
    in_array "build" ${ACT[*]} && (
        build
    )
fi

exit 0

#####################################################################################

