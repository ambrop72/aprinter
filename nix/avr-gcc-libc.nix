{ stdenv, fetchurl, texinfo, gmp, mpfr, libmpc, zlib }:

stdenv.mkDerivation {
  name = "avr-gcc-libc";

  srcs = [
    (fetchurl {
        url = "mirror://gnu/binutils/binutils-2.28.tar.bz2";
        sha256 = "6297433ee120b11b4b0a1c8f3512d7d73501753142ab9e2daa13c5a3edd32a72";
    })
    (fetchurl {
        url = "mirror://gcc/releases/gcc-6.3.0/gcc-6.3.0.tar.bz2";
        sha256 = "f06ae7f3f790fbf0f018f6d40e844451e6bc3b7bc96e128e63b09825c1f8b29f";
    })
    (fetchurl {
        url = "http://download.savannah.gnu.org/releases/avr-libc/avr-libc-2.0.0.tar.bz2";
        sha256 = "b2dd7fd2eefd8d8646ef6a325f6f0665537e2f604ed02828ced748d49dc85b97";
    })
  ];
  
  sourceRoot = ".";

  nativeBuildInputs = [ texinfo ];

  hardeningDisable = [ "format" ];
  
  buildInputs = [ gmp mpfr libmpc zlib ];
  
  # Make sure we don't strip the libraries in lib/gcc/avr.
  stripDebugList= [ "bin" "avr/bin" "libexec" ];
  
  # Fix for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60040
  patchPhase = ''
    patch -d gcc* -p1 < ${ ../patches/gcc-avr-bug60040.patch }
  '';
  
  installPhase = ''
    # Make sure gcc finds the binutils.
    export PATH=$PATH:$out/bin
    
    pushd binutils*
    mkdir build && cd build
    ../configure --target=avr --prefix="$out" --disable-nls --disable-debug --disable-dependency-tracking
    make $MAKE_FLAGS
    make install
    popd

    pushd gcc*
    mkdir build && cd build
    ../configure --target=avr --prefix="$out" --disable-nls --disable-libssp --with-dwarf2 --disable-install-libiberty --with-system-zlib --enable-languages=c,c++
    make $MAKE_FLAGS
    make install
    popd

    # We don't want avr-libc to use the native compiler.
    export BUILD_CC=$CC
    export BUILD_CXX=$CXX
    unset CC
    unset CXX

    pushd avr-libc*
    ./configure --prefix="$out" --build=`./config.guess` --host=avr
    make $MAKE_FLAGS
    make install
    popd
  '';
}
