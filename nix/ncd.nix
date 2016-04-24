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

{stdenv, fetchgit, cmake, pkgconfig, bash, debug ? false}:
let
    compileFlags = "-O3 ${stdenv.lib.optionalString (!debug) "-DNDEBUG"}";
in
stdenv.mkDerivation {
  name = "ncd";
  
  buildInputs = [cmake pkgconfig];
  
  src = fetchgit {
    url = https://github.com/ambrop72/badvpn;
    rev = "3897cdf4aa7deb1fc863cb198fb7381b224ff793";
    sha256 = "03d695w9574y4dsbqyr69dayjnr3dygpddv4jl2519v4pdaxh6h0";
  };

  preConfigure = ''
    cmakeFlagsArray=("-DCMAKE_BUILD_TYPE=" "-DCMAKE_C_FLAGS=${compileFlags}" -DBUILD_NOTHING_BY_DEFAULT=1 -DBUILD_NCD=1);
  '';
}
