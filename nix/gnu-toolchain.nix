# This was written partly based on https://github.com/EliasOenal/TNT,
# and the patches were also taken from there.

{ stdenv, lib, fetchurl, gmp, mpfr, libmpc, isl
, zlib, libelf, texinfo, bison, flex
, target, optimizeForSize ? false
}:
let
    # Software versions and source hashes.
    gcc_version = "8.3.0";
    gcc_sha256 = "64baadfe6cc0f4947a84cb12d7f0dfaf45bb58b7e92461639596c21e02d97d2c";
    binutils_version = "2.31.1";
    binutils_sha256 = "ffcc382695bf947da6135e7436b8ed52d991cf270db897190f19d6f9838564d0";
    newlib_version = "3.0.0.20180831";
    newlib_sha256 = "3ad3664f227357df15ff34e954bfd9f501009a647667cd307bf0658aefd6eb5b";

    isArmNoneEabi = (target == "arm-none-eabi");
    
    # Configure flags common for all components.
    common_flags = ''
        --target=${target} \
        --prefix=$out \
        --with-sysroot=$out/${target} \
        --with-build-time-tools=$out/${target}/bin \
        --with-system-zlib \
        --enable-lto \
        --enable-multilib \
        --enable-gold \
        --disable-nls \
        --disable-libquadmath \
        --disable-libssp \
    '';
    
    # Configure flags for binutils.
    binutils_flags = common_flags + ''
        --enable-plugins \
        --disable-werror \
        --disable-debug \
        --disable-dependency-tracking \
    '';
    
    # Configure flags for GCC, both stages.
    gcc_common_flags = common_flags + ''
        --with-newlib \
        --with-gnu-as \
        --with-gnu-ld \
        ${lib.optionalString isArmNoneEabi "--with-multilib-list=rmprofile"} \
        ${lib.optionalString optimizeForSize "--enable-target-optspace"} \
        --enable-checking=release \
        --disable-decimal-float \
        --disable-libffi \
        --disable-libgomp \
        --disable-libstdcxx-pch \
        --disable-threads \
        --disable-shared \
        --disable-tls \
        --disable-libunwind-exceptions \
    '';
    
    # Configure flags for GCC, stage 1.
    gcc_stage1_flags = gcc_common_flags + ''
        --without-headers \
        --enable-languages=c \
    '';
    
    # Configure flags for GCC, stage 2.
    gcc_stage2_flags = gcc_common_flags + ''
        --enable-languages=c,c++ \
        --enable-plugins \
    '';
    
    # We intentionally do not pass --enable-newlib-nano-formatted-io and do
    # pass --enable-newlib-io-c99-formats, because otherwise the 8-bit
    # printf macros PRI*x do not work.
    # See: https://www.sourceware.org/ml/newlib/2016/msg00000.html
    # Also we want the zu (size_t) format to work.
    newlib_flags = common_flags + ''
        --enable-newlib-reent-small \
        --enable-newlib-nano-malloc \
        --enable-lite-exit \
        --enable-newlib-global-atexit \
        --enable-newlib-io-c99-formats \
        --enable-newlib-io-long-long \
        --enable-newlib-retargetable-locking \
        --enable-newlib-global-stdio-streams \
        ${lib.optionalString optimizeForSize "--enable-target-optspace"} \
        --disable-shared \
        --disable-newlib-supplied-syscalls \
        --disable-newlib-fvwrite-in-streamio \
        --disable-newlib-fseek-optimization \
        --disable-newlib-wide-orient \
        --disable-newlib-unbuf-stream-opt \
    '';

    # Optimization flags when building the toolchain libraries.
    # Note that we must not have newlines in here.
    # These are NOT used when building GCC to prevent breakage.
    targetBaseFlags = "-ffunction-sections -fdata-sections -fno-exceptions";
    targetCflags = targetBaseFlags;
    targetCxxflags = targetBaseFlags;
    targetLdflags = targetBaseFlags;

in
stdenv.mkDerivation {
    name = "gnu-toolchain-${target}-${gcc_version}";
    
    srcs = [
        (fetchurl {
            url = "mirror://gnu/binutils/binutils-${binutils_version}.tar.bz2";
            sha256 = binutils_sha256;
        })
        (fetchurl {
            url = "mirror://gnu/gcc/gcc-${gcc_version}/gcc-${gcc_version}.tar.xz";
            sha256 = gcc_sha256;
        })
        (fetchurl {
            url = "ftp://sourceware.org/pub/newlib/newlib-${newlib_version}.tar.gz";
            sha256 = newlib_sha256;
        })
    ];
    
    sourceRoot = ".";
    
    nativeBuildInputs = [ texinfo bison flex ];
    buildInputs = [ gmp mpfr libmpc isl zlib libelf ];
    
    hardeningDisable = [ "format" ];
    
    # Limit stripping to these directories so we don't strip target libraries.
    # We do miss stripping lib/libcc.so because we must not strip in lib/gcc/.
    stripDebugList= [ "bin" "${target}/bin" "libexec" ];
    
    enableParallelBuilding = true;
    
    patchPhase = ''
        # I'm not sure why/if this is needed. Maybe it just fixes the build.
        patch -N newlib*/libgloss/arm/linux-crt0.c ${ ../patches/newlib-optimize.patch }

        # Fix GCC bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88641
        patch -d gcc* -p1 < ${ ../patches/gcc-crtstuff-sections.patch }
    '';
    
    installPhase = ''
        # Make sure we can find our own installed programs.
        export PATH=$PATH:$out/bin
        
        # Get the directory names of the sources.
        binutils_src=$(echo binutils*)
        gcc_src=$(echo gcc*)
        newlib_src=$(echo newlib*)
        
        # Binutils
        mkdir binutils-build
        pushd binutils-build
        ../"$binutils_src"/configure ${binutils_flags}
        make $MAKE_FLAGS
        make install
        popd
        rm -rf binutils-build "$binutils_src"
        
        # GCC stage 1
        mkdir gcc-build-stage1
        pushd gcc-build-stage1
        ../"$gcc_src"/configure ${gcc_stage1_flags}
        make $MAKE_FLAGS all-gcc CFLAGS_FOR_TARGET="${targetCflags}" LDFLAGS_FOR_TARGET="${targetLdflags}"
        make install-gcc
        popd
        rm -rf gcc-build-stage1
        
        # Newlib
        mkdir newlib-build
        pushd newlib-build
        ../"$newlib_src"/configure ${newlib_flags}
        make $MAKE_FLAGS CFLAGS_FOR_TARGET="${targetCflags}" LDFLAGS_FOR_TARGET="${targetLdflags}"
        make install
        popd
        rm -rf newlib-build "$newlib_src"
        
        # GCC stage 2
        mkdir gcc-build-stage2
        pushd gcc-build-stage2
        ../"$gcc_src"/configure ${gcc_stage2_flags}
        make $MAKE_FLAGS CFLAGS_FOR_TARGET="${targetCflags}" CXXFLAGS_FOR_TARGET="${targetCxxflags}" LDFLAGS_FOR_TARGET="${targetLdflags}"
        make install
        popd
        rm -rf gcc-build-stage2 "$gcc_src"
    '';
}
