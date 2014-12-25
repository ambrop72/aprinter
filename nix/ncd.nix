{stdenv, fetchgit, cmake, pkgconfig, bash, debug ? false}:
let
    compileFlags = "-O3 ${stdenv.lib.optionalString (!debug) "-DNDEBUG"}";
in
stdenv.mkDerivation {
  name = "ncd";
  
  buildInputs = [cmake pkgconfig];
  
  src = fetchgit {
    url = https://github.com/ambrop72/badvpn;
    rev = "cce6adc03994f46c10f58a91a313211272a41de3";
    sha256 = "1x3l4m8yl86bm0p3lfw8glfc7l16f8pxjnhsawfa8bavyrmfbai0";
  };

  preConfigure = ''
    find . -name '*.sh' -exec sed -e 's@#!/bin/sh@${stdenv.shell}@' -i '{}' ';'
    find . -name '*.sh' -exec sed -e 's@#!/bin/bash@${bash}/bin/bash@' -i '{}' ';'
    cmakeFlagsArray=("-DCMAKE_BUILD_TYPE=" "-DCMAKE_C_FLAGS=${compileFlags}" -DBUILD_NOTHING_BY_DEFAULT=1 -DBUILD_NCD=1);
  '';
}
