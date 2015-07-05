{ stdenv, fetchurl, gcc-arm-embedded, python27Packages }:
let
    version = "3.6.1";
in
stdenv.mkDerivation {
    name = "clang-arm-embedded-${version}";
    
    srcs = [
        (fetchurl {
            url = "http://llvm.org/releases/${version}/llvm-${version}.src.tar.xz";
            sha256 = "0ypwcqrld91jn0zz4mkdksl2mbb0ds9lh5gf0xkbb81sj4awc01g";
        })
        (fetchurl {
            url = "http://llvm.org/releases/${version}/cfe-${version}.src.tar.xz";
            sha256 = "1myssbxlyln0nggfz04nfrbzdckljksmaxp82nq7hrmqjc62vybl";
        })
    ];
    
    sourceRoot = "llvm-${version}.src";
    
    buildInputs = [ gcc-arm-embedded python27Packages.python ];
    
    configurePhase = ''
        gccVersion=$(cd ${gcc-arm-embedded}/lib/gcc/arm-none-eabi && echo *)
        
        mv ../cfe-${version}.src tools/clang
        
        ( cd tools/clang && patch -p1 < ${ ./clang-constexpr-math.patch } )
        
        ( cd tools/clang && patch -p1 < ${ ./clang-custom-program-path.patch } )
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
