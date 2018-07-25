{ stdenv, fetchurl, texinfo, gmp, mpfr, libmpc, zlib }:

stdenv.mkDerivation {
  name = "avr-gcc-libc";

  srcs = [
    (fetchurl {
        url = "mirror://gnu/binutils/binutils-2.31.1.tar.bz2";
        sha256 = "ffcc382695bf947da6135e7436b8ed52d991cf270db897190f19d6f9838564d0";
    })
    (fetchurl {
        url = "mirror://gcc/releases/gcc-7.3.0/gcc-7.3.0.tar.bz2";
        sha256 = "0p71bij6bfhzyrs8676a8jmpjsfz392s2rg862sdnsk30jpacb43";
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

    # Rename environment variables to prevent using native tools as if they were
    # cross-compile tools.
    for varname in LD AS AR CC CXX RANLIB STRIP; do
      if [[ -n ''${!varname} ]]; then
        export BUILD_''${varname}="''${!varname}"
        unset ''${varname}
      fi
    done

    pushd avr-libc*
    ./configure --prefix="$out" --build=`./config.guess` --host=avr
    make $MAKE_FLAGS
    make install
    popd
  '';
}
