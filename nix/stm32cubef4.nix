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

{ stdenv, fetchurl, unzip }:
let
    source = fetchurl {
        url = https://www.dropbox.com/s/vbpdqvlevdusnn5/stm32cubef4.zip?dl=1;
        sha256 = "dc1475e29b61806ff0674e61ad9710153743056b17fe4f50ffc5070de988ff3c";
    };
    patchFile = ../patches/stm32cubef4.patch;
in
stdenv.mkDerivation rec {
    name = "stm32cubef4";
    
    unpackPhase = "true";
    
    nativeBuildInputs = [ unzip ];
    
    installPhase = ''
        mkdir -p "$out"/EXTRACT
        unzip -q ${source} -d "$out"/EXTRACT
        mv "$out"/EXTRACT/STM32Cube*/* "$out"/
        rm -rf "$out"/EXTRACT
        patch -d "$out" -p1 < ${patchFile}
    '';
    
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
