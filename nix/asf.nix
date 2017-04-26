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
        url = "http://ww1.microchip.com/downloads/en/DeviceDoc/asf-standalone-archive-3.33.0.50.zip";
        sha256 = "d4593c9036686441f9e05b4a76c2dd23765d5b782260337fa67bf32e855995c1";
    };
in
stdenv.mkDerivation rec {
    name = "atmel-software-framework";
    
    unpackPhase = "true";
    
    nativeBuildInputs = [ unzip ];
    
    installPhase = ''
        # Extract it and move stuff so we have it in the root.
        mkdir -p "$out"/EXTRACT
        unzip -q ${source} -d "$out"/EXTRACT
        mv "$out"/EXTRACT/*xdk-asf*/* "$out"/
        rm -rf "$out"/EXTRACT
        
        # Fix __always_inline conflicts with gcc-arm-embedded headers.
        find "$out" \( -name '*.h' -o -name '*.c' \) -exec sed -i 's/__always_inline\(\s\|$\)/__asf_always_inline\1/g' {} \;
        
        # Apply patches.
        patch -d "$out" -p1 < ${ ../patches/asf-emac.patch }
        patch -d "$out" -p1 < ${ ../patches/asf-macros.patch }
    '';
    
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
