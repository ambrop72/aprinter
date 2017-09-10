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

{ stdenv, writeText, bash, gcc-arm-embedded, clang-arm-embedded, avrgcclibc
, clang, asf, stm32cubef4, teensyCores
, aprinterSource, buildVars, extraSources
, extraIncludes, defines, linkerSymbols
, mainText, boardName, buildName, desiredOutputs
, optimizeForSize ? false
, buildWithClang ? false
, verboseBuild ? false
, debugSymbols ? false
}:

let
    boardDefinitions = import ./boards.nix;
    
    board = builtins.getAttr boardName boardDefinitions;
    
    collectSources = suffix:
        (stdenv.lib.concatStringsSep " " (map
            (src: if (stdenv.lib.hasPrefix "$" src) then src else "\${ROOT}/${src}")
            (stdenv.lib.filter (stdenv.lib.hasSuffix suffix) extraSources)));
    
    targetVars = {
        PLATFORM = board.platform;
        OPTIMIZE_FOR_SIZE = if optimizeForSize then "1" else "0";
    } // (if board.platform == "sam3x" then {
        USE_USB_SERIAL = "1";
    } else {}) // board.targetVars // buildVars // {
        EXTRA_C_SOURCES = collectSources ".c";
        EXTRA_CXX_SOURCES = collectSources ".cpp";
        EXTRA_ASM_SOURCES = collectSources ".S";
        EXTRA_COMPILE_FLAGS = 
            (map (f: "-I" + f) extraIncludes) ++
            (map (define: "-D${define.name}" + (if define.value=="" then "" else "=${define.value}")) defines);
        EXTRA_LINK_FLAGS = (map (sym: "-Wl,--defsym,${sym.name}=${sym.value}") linkerSymbols);
    };
    
    targetVarEncoding = value: if builtins.isList value then (
        "( " + (stdenv.lib.concatStringsSep " " (map (elem: stdenv.lib.escapeShellArg elem) value)) + " )"
    ) else (stdenv.lib.escapeShellArg value);
    
    targetVarsText = stdenv.lib.concatStrings (
        stdenv.lib.mapAttrsToList (name: value: "    ${name}=${targetVarEncoding value}\n") targetVars
    );
    
    isAvr = board.platform == "avr";
    
    isArm = builtins.elem board.platform [ "sam3x" "teensy" "stm32f4" ];
    
    isLinux = board.platform == "linux";
    
    needAsf = board.platform == "sam3x";
    
    needStm32CubeF4 = board.platform == "stm32f4";
    
    needTeensyCores = board.platform == "teensy";
    
    targetFile = writeText "aprinter-nixbuild.sh" ''
        ${stdenv.lib.optionalString isAvr "AVR_GCC_PREFIX=${avrgcclibc}/bin/avr-"}
        ${stdenv.lib.optionalString isArm "ARM_GCC_PREFIX=${gcc-arm-embedded}/bin/arm-none-eabi-"}
        ${stdenv.lib.optionalString buildWithClang "BUILD_WITH_CLANG=1"}
        ${stdenv.lib.optionalString (buildWithClang && isArm) "CLANG_ARM_EMBEDDED=${clang-arm-embedded}/bin/"}
        ${stdenv.lib.optionalString needAsf "ASF_DIR=${asf}"}
        ${stdenv.lib.optionalString needStm32CubeF4 "STM32CUBEF4_DIR=${stm32cubef4}"}
        ${stdenv.lib.optionalString needTeensyCores "TEENSY_CORES=${teensyCores}"}
        
        TARGETS+=( "nixbuild" )
        target_nixbuild() {
        ${targetVarsText}}
    '';
    
    mainFile = writeText "aprinter-main.cpp" mainText;
    
    ccxxldFlags = stdenv.lib.concatStringsSep " " [
        (stdenv.lib.optionalString debugSymbols "-g")
    ];
in

assert isAvr -> avrgcclibc != null;
assert isArm -> gcc-arm-embedded != null;
assert buildWithClang -> isArm || isLinux;
assert buildWithClang && isArm -> clang-arm-embedded != null;
assert buildWithClang && isLinux -> clang != null;
assert needAsf -> asf != null;
assert needStm32CubeF4 -> stm32cubef4 != null;
assert needTeensyCores -> teensyCores != null;

stdenv.mkDerivation rec {
    aprinterBuildName = buildName;
    
    name = "aprinter-${buildName}";
    
    buildInputs = stdenv.lib.optional (buildWithClang && isLinux) clang;
    
    unpackPhase = "true";
    
    configurePhase = ''
        echo configure
        mkdir build
        cd build
    '';
    
    buildPhase = ''
        CCXXLDFLAGS="${ccxxldFlags}" \
        ${bash}/bin/bash ${aprinterSource}/build.sh ${targetFile} ${mainFile} ${if verboseBuild then "-v" else ""}
    '';
    
    installPhase = ''
        mkdir -p $out
    '' + stdenv.lib.concatStrings (map (outputType: ''
        [ -e aprinter.${outputType} ] && cp aprinter.${outputType} $out/
    '') desiredOutputs);
    
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
