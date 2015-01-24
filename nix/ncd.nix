{stdenv, fetchgit, cmake, pkgconfig, bash, debug ? false}:
let
    compileFlags = "-O3 ${stdenv.lib.optionalString (!debug) "-DNDEBUG"}";
in
stdenv.mkDerivation {
  name = "ncd";
  
  buildInputs = [cmake pkgconfig];
  
  src = fetchgit {
    url = https://github.com/ambrop72/badvpn;
    rev = "f282fbf4d2bcf4747fa4c14cddd83c4d6f4f135f";
    sha256 = "038dsk89d3yf3pk07w7gkmw588i6178q6x4dv2amdjfgv8mclq37";
  };

  preConfigure = ''
    cmakeFlagsArray=("-DCMAKE_BUILD_TYPE=" "-DCMAKE_C_FLAGS=${compileFlags}" -DBUILD_NOTHING_BY_DEFAULT=1 -DBUILD_NCD=1);
  '';
}
