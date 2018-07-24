# This was written partly based on https://github.com/EliasOenal/TNT,
# and the patches were also taken from there.

{ stdenv, fetchurl, gmp, mpfr, libmpc, isl
, zlib, libelf, texinfo, bison, flex
, optimizeForSize ? false
}:
let
    gcc_version = "7.3.0";
    binutils_version = "2.28";
    newlib_version = "2.5.0.20170421";
    
    target = "arm-none-eabi";
    
    common_flags = ''
        --target=${target} \
        --prefix=$out \
        --with-sysroot=$out/${target} \
        --enable-interwork \
    '';
    
    binutils_flags = common_flags + ''
        --disable-nls \
        --enable-gold \
        --enable-plugins \
        --enable-lto \
        --disable-werror \
        --enable-multilib \
        --disable-debug \
        --disable-dependency-tracking \
    '';
    
    gcc_common_flags = common_flags + ''
        --disable-nls \
        --with-build-time-tools=$out/${target}/bin \
        --enable-poison-system-directories \
        --enable-lto \
        --enable-gold \
        --disable-decimal-float \
        --disable-libffi \
        --disable-libgomp \
        --disable-libquadmath \
        --disable-libssp \
        --disable-libstdcxx-pch \
        --disable-threads \
        --disable-shared \
        --disable-tls \
        --with-newlib \
        --disable-libunwind-exceptions \
        --enable-checking=release \
    '';
    
    gcc_stage1_flags = gcc_common_flags + ''
        --without-headers \
        --enable-languages=c \
    '';
    
    gcc_stage2_flags = gcc_common_flags + ''
        --enable-languages=c,c++ \
    '';
    
    # We intentionally do not pass --enable-newlib-nano-formatted-io and do
    # pass --enable-newlib-io-c99-formats, because otherwise the 8-bit
    # printf macros PRI*x do not work.
    # See: https://www.sourceware.org/ml/newlib/2016/msg00000.html
    # Also we want the zu (size_t) format to work.
    newlib_flags = common_flags + ''
        --with-build-time-tools=$out/${target}/bin \
        --disable-shared \
        --enable-multilib \
        --enable-lto \
        --disable-newlib-supplied-syscalls \
        --enable-newlib-reent-small \
        --disable-newlib-fvwrite-in-streamio \
        --disable-newlib-fseek-optimization \
        --disable-newlib-wide-orient \
        --enable-newlib-nano-malloc \
        --disable-newlib-unbuf-stream-opt \
        --enable-lite-exit \
        --enable-newlib-global-atexit \
        --enable-newlib-io-c99-formats \
        --enable-newlib-io-long-long \
    '' + stdenv.lib.optionalString optimizeForSize ''
        --enable-target-optspace \
    '';
    
    # Optimization flags when building the toolchain libraries.
    # Note that we must not have newlines in here.
    target_opt_cflags =
        # This allows the linker to remove unused code and data.
        "-ffunction-sections -fdata-sections"
        # We don't rely on FP operations setting errno and FP traps (we even assume FP does not trap).
        # But don't use the full -ffast-math, because we rely on certain IEEE semantics.
     +  " -fno-math-errno -fno-trapping-math";
    
    # Use the same flags in the linking steps.
    target_opt_ldflags = target_opt_cflags;

in
stdenv.mkDerivation {
    name = "gcc-arm-embedded-fromsrc-${gcc_version}";
    
    srcs = [
        (fetchurl {
            url = "mirror://gnu/binutils/binutils-${binutils_version}.tar.bz2";
            sha256 = "6297433ee120b11b4b0a1c8f3512d7d73501753142ab9e2daa13c5a3edd32a72";
        })
        (fetchurl {
            url = "mirror://gnu/gcc/gcc-${gcc_version}/gcc-${gcc_version}.tar.xz";
            sha256 = "0p71bij6bfhzyrs8676a8jmpjsfz392s2rg862sdnsk30jpacb43";
        })
        (fetchurl {
            url = "ftp://sourceware.org/pub/newlib/newlib-${newlib_version}.tar.gz";
            sha256 = "714599a373c4e60da628533cc0cb281446d2e3baf75fabceb14c5919c5809618";
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
        # This patch configures multilib for the different ARM microcontroller
        # architectures. So, different sets of libraries (libgcc, libc) will be
        # build for different combinations of compiler options (e.g. march, fpu).
        patch -N gcc-*/gcc/config/arm/t-arm-elf ${ ../patches/gcc-multilib.patch }
        
        # I'm not sure why/if this is needed. Maybe it just fixes the build.
        patch -N newlib*/libgloss/arm/linux-crt0.c ${ ../patches/newlib-optimize.patch }
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
        make $MAKE_FLAGS all
        make install
        popd
        
        rm -rf binutils-build "$binutils_src"
        
        # GCC stage 1
        mkdir gcc-build-stage1
        pushd gcc-build-stage1
        ../"$gcc_src"/configure ${gcc_stage1_flags}
        make $MAKE_FLAGS all-gcc all-target-libgcc CFLAGS_FOR_TARGET="${target_opt_cflags}" LDFLAGS_FOR_TARGET="${target_opt_ldflags}"
        make install-gcc install-target-libgcc 
        popd
        
        rm -rf gcc-build-stage1
        
        # Newlib
        mkdir newlib-build
        pushd newlib-build
        ../"$newlib_src"/configure ${newlib_flags}
        make $MAKE_FLAGS all CFLAGS_FOR_TARGET="${target_opt_cflags}" LDFLAGS_FOR_TARGET="${target_opt_ldflags}"
        make install
        popd
        
        rm -rf newlib-build "$newlib_src"
        
        # GCC stage 2
        mkdir gcc-build-stage2
        pushd gcc-build-stage2
        ../"$gcc_src"/configure ${gcc_stage2_flags}
        make $MAKE_FLAGS all CFLAGS_FOR_TARGET="${target_opt_cflags}" LDFLAGS_FOR_TARGET="${target_opt_ldflags}"
        make install
        popd
        
        rm -rf gcc-build-stage2 "$gcc_src"
    '';
}
