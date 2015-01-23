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
# COMMON STUFF

create_depends_dir() {
    mkdir -p "${DEPS}"
}

# base actions functions

configure() {
    local target_name=$1
    echo "  Configuring for target ${target_name}"
    
    target_${target_name}
    
    : ${SOURCE_NAME:=${target_name}}
    SOURCE=$SPATH/aprinter-${SOURCE_NAME}.cpp
    TARGET=$BUILD/aprinter-${target_name}
    
    configure_${PLATFORM}
}

build() {
    mkdir -p "${BUILD}"
    ${RUNBUILD}
}

# Utility functions

checksum() {
    echo "   Checksum validation"
    declare -a checksums=("${!1}")
    printf "%s\n" "${checksums[@]}" | shasum -a 256 -c -
}

retr_and_extract() {
    declare -a urls=("${!1}")
    declare -a cs=("${!2}")

    (
    create_depends_dir
    cd "${DEPS}"

    files=()
    for url in "${urls[@]}"; do
        f=$(basename $url)
        echo "   Downloading '$f'"
        curl "${url}" -C- -L -o "$f"
        files+=( "$f" )
    done

    checksum cs[@]
    extract files[@]
    )
}

extract() {
    declare -a files=("${!1}")

    for f in "${files[@]}"; do
        echo "   Extraction of $f"
        case "${f}" in
            *.zip)
                ($V; unzip -qq "$f" && rm "$f")
                ;;
            *.tar.gz)
                ($V; tar xzf "$f" && rm "$f")
                ;;
            *.tar.bz2)
                ($V; tar xjf "$f" && rm "$f")
                ;;
            *.7z)
                ($V; 7z x "$f" && rm "$f")
                ;;
            *)
                echo "extension '${f#*.}' not handled for '$f'"
                ;;
        esac
    done
}

# http://stackoverflow.com/a/15736713/1290438
in_array() {
    local -r NEEDLE=$1
    shift
    local value
    for value in "$@"
    do
        [[ $value == "$NEEDLE" ]] && return 0
    done
    return 1
}

fail() {
    echo $1
    exit 5
}

check_build_tool() {
    "$1" --version >/dev/null 2>&1 || fail "Missing or broken ${2} (tried to run: ${1})" 
}
