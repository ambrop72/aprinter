{ stdenv, fetchurl, pkgconfig, texinfo, perl, ncurses
, readline, gmp, mpfr, expat, zlib, python, guile }:
let
    version = "7.12.1";
    sha256 = "4607680b973d3ec92c30ad029f1b7dbde3876869e6b3a117d8a7e90081113186";
    target = "arm-none-eabi";
in
stdenv.mkDerivation {
    name = "${target}-gdb-${version}";
    
    src = fetchurl {
        url = "mirror://gnu/gdb/gdb-${version}.tar.xz";
        inherit sha256;
    };
    
    nativeBuildInputs = [ pkgconfig texinfo perl ];
    
    buildInputs = [ ncurses readline gmp mpfr expat zlib python guile ];
    
    enableParallelBuilding = true;
    
    configureFlags = [
        "--target=${target}" "--enable-interwork" "--enable-multilib"
        "--with-gmp=${gmp.dev}" "--with-mpfr=${mpfr.dev}" "--with-system-readline"
        "--with-system-zlib" "--with-expat" "--with-libexpat-prefix=${expat.dev}"
    ];
    
    postInstall = ''
        rm -v $out/share/info/bfd.info
    '';
    
    doCheck = false;
}

