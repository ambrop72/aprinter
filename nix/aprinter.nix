/*
 * Copyright (c) 2015 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

{ stdenv, writeText, bash, gcc-arm-embedded, clang-arm-embedded, gccAvrAtmel
, asf, stm32cubef4, teensyCores, aprinterSource
, mainText, boardName, buildName ? "nixbuild", desiredOutputs ? ["bin" "hex"]
, optimizeForSize ? false, assertionsEnabled ? false
, eventLoopBenchmarkEnabled ? false, detectOverloadEnabled ? false
, buildWithClang ? false, verboseBuild ? false
}:

let
    boardDefinitions = import ./boards.nix;
    
    board = builtins.getAttr boardName boardDefinitions;
    
    targetVars = {
        PLATFORM = board.platform;
        OPTIMIZE_FOR_SIZE = if optimizeForSize then "1" else "0";
    } // (if board.platform == "sam3x" then {
        USE_USB_SERIAL = "1";
    } else {}) // board.targetVars;
    
    targetVarsText = stdenv.lib.concatStrings (
        stdenv.lib.mapAttrsToList (name: value: "    ${name}=${value}\n") targetVars
    );
    
    isAvr = board.platform == "avr";
    
    isArm = builtins.elem board.platform [ "sam3x" "teensy" "stm32f4" ];
    
    needAsf = board.platform == "sam3x";
    
    needStm32CubeF4 = board.platform == "stm32f4";
    
    needTeensyCores = board.platform == "teensy";
    
    targetFile = writeText "aprinter-nixbuild.sh" ''
        ${stdenv.lib.optionalString isAvr "CUSTOM_AVR_GCC=${gccAvrAtmel}/bin/avr-"}
        ${stdenv.lib.optionalString isArm "CUSTOM_ARM_GCC=${gcc-arm-embedded}/bin/arm-none-eabi-"}
        ${stdenv.lib.optionalString buildWithClang "BUILD_WITH_CLANG=1"}
        ${stdenv.lib.optionalString buildWithClang "CLANG_ARM_EMBEDDED=${clang-arm-embedded}/bin/arm-none-eabi-"}
        ${stdenv.lib.optionalString needAsf "CUSTOM_ASF=${asf}"}
        ${stdenv.lib.optionalString needStm32CubeF4 "CUSTOM_STM32CUBEF4=${stm32cubef4}"}
        ${stdenv.lib.optionalString needTeensyCores "CUSTOM_TEENSY_CORES=${teensyCores}"}
        
        TARGETS+=( "nixbuild" )
        target_nixbuild() {
        ${targetVarsText}}
    '';
    
    mainFile = writeText "aprinter-main.cpp" mainText;
    
    compileFlags = stdenv.lib.concatStringsSep " " [
        (stdenv.lib.optionalString assertionsEnabled "-DAMBROLIB_ASSERTIONS")
        (stdenv.lib.optionalString eventLoopBenchmarkEnabled "-DEVENTLOOP_BENCHMARK")
        (stdenv.lib.optionalString detectOverloadEnabled "-DAXISDRIVER_DETECT_OVERLOAD")
    ];
    
in

assert isAvr -> gccAvrAtmel != null;
assert isArm -> gcc-arm-embedded != null;
assert buildWithClang -> isArm;
assert buildWithClang -> clang-arm-embedded != null;
assert needAsf -> asf != null;
assert needStm32CubeF4 -> stm32cubef4 != null;
assert needTeensyCores -> teensyCores != null;

stdenv.mkDerivation rec {
    aprinterBuildName = buildName;
    
    name = "aprinter-${buildName}";
    
    src = aprinterSource;
    
    configurePhase = ''
        rm -rf config
        mkdir config
        ln -s ${targetFile} config/nixbuild.sh
        mkdir -p main
        ln -s ${mainFile} main/aprinter-nixbuild.cpp
    '';
    
    buildPhase = ''
        echo "Compile flags: ${compileFlags}"
        CFLAGS="${compileFlags}" \
        CXXFLAGS="${compileFlags}" \
        ${bash}/bin/bash ./build.sh nixbuild ${if verboseBuild then "-v" else ""} build
    '';
    
    installPhase = ''
        mkdir -p $out
    '' + stdenv.lib.concatStrings (map (outputType: ''
        [ -e build/aprinter-nixbuild.${outputType} ] && cp build/aprinter-nixbuild.${outputType} $out/
    '') desiredOutputs);
    
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
