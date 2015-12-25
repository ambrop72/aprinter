{ stdenv, fetchurl, gcc-arm-embedded, python27Packages }:
let
    version = "3.7.0";
in
stdenv.mkDerivation {
    name = "clang-arm-embedded-${version}";
    
    srcs = [
        (fetchurl {
            url = "http://llvm.org/releases/${version}/llvm-${version}.src.tar.xz";
            sha256 = "ab45895f9dcdad1e140a3a79fd709f64b05ad7364e308c0e582c5b02e9cc3153";
        })
        (fetchurl {
            url = "http://llvm.org/releases/${version}/cfe-${version}.src.tar.xz";
            sha256 = "4ed740c5a91df1c90a4118c5154851d6a475f39a91346bdf268c1c29c13aa1cc";
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
