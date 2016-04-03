{ stdenv, fetchurl, gcc-arm-embedded, python27Packages }:
let
    version = "3.8.0";
in
stdenv.mkDerivation {
    name = "clang-arm-embedded-${version}";
    
    srcs = [
        (fetchurl {
            url = "http://llvm.org/releases/${version}/llvm-${version}.src.tar.xz";
            sha256 = "0ikfq0gxac8xpvxj23l4hk8f12ydx48fljgrz1gl9xp0ks704nsm";
        })
        (fetchurl {
            url = "http://llvm.org/releases/${version}/cfe-${version}.src.tar.xz";
            sha256 = "1ybcac8hlr9vl3wg8s4v6cp0c0qgqnwprsv85lihbkq3vqv94504";
        })
    ];
    
    sourceRoot = "llvm-${version}.src";
    
    buildInputs = [ gcc-arm-embedded python27Packages.python ];
    
    configurePhase = ''
        gccVersion=$(cd ${gcc-arm-embedded}/lib/gcc/arm-none-eabi && echo *)
        
        mv ../cfe-${version}.src tools/clang
        
        ( cd tools/clang && patch -p1 < ${ ../patches/clang-constexpr-math.patch } )
        
        ( cd tools/clang && patch -p1 < ${ ../patches/clang-custom-program-path.patch } )
        sed -i 's|<GCC_PROGRAM_PATH>|${gcc-arm-embedded}/bin|g' tools/clang/lib/Driver/ToolChains.cpp
        
        mkdir build
        cd build
        ../configure --enable-targets=arm --target=arm-none-eabi --enable-cxx11 \
            --disable-docs --prefix=$out \
            --with-gcc-toolchain=${gcc-arm-embedded}/lib/gcc/arm-none-eabi/$gccVersion \
            --with-c-include-dirs=${gcc-arm-embedded}/arm-none-eabi/include
    '';
    
    enableParallelBuilding = true;
}
