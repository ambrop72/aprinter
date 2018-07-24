{ stdenv, fetchurl, gcc-arm-embedded, python27Packages, cmake }:
let
    version = "4.0.0";
    target = "arm-none-eabi";
    targetClangFix = "arm-none--eabi";
in
stdenv.mkDerivation {
    name = "clang-arm-embedded-${version}";
    
    srcs = [
        (fetchurl {
            url = "http://releases.llvm.org/${version}/llvm-${version}.src.tar.xz";
            sha256 = "8d10511df96e73b8ff9e7abbfb4d4d432edbdbe965f1f4f07afaf370b8a533be";
        })
        (fetchurl {
            url = "http://releases.llvm.org/${version}/cfe-${version}.src.tar.xz";
            sha256 = "cea5f88ebddb30e296ca89130c83b9d46c2d833685e2912303c828054c4dc98a";
        })
    ];
    
    sourceRoot = "llvm-${version}.src";
    
    buildInputs = [ gcc-arm-embedded python27Packages.python cmake ];
    
    configurePhase = ''
        # Create tool symlinks for clang to find, due to an issue where
        # it will look for e.g. arm-none--eabi instead of arm-none-eabi.
        mkdir -p $out/bin
        (
            cd ${gcc-arm-embedded}/bin
            for targetTool in ${target}-*; do
                linkTargetTool=''${targetTool#${target}-}
                ln -s "${gcc-arm-embedded}/bin/$targetTool" "$out/bin/${targetClangFix}-$linkTargetTool"
            done
        )
        
        # Figure out the GCC version in the folder name
        gccVersion=$(cd ${gcc-arm-embedded}/lib/gcc/${target} && echo *)
        
        # Move clang code to the right place.
        mv ../cfe-${version}.src tools/clang

        # Patch for Clang bug https://bugs.llvm.org/show_bug.cgi?id=38257
        ( cd tools/clang && patch -p1 < ${ ../patches/clang-pointer-to-member-no-adl.diff } )
        
        # Hardcode paths which clang can't figure out on its own.
        ( cd tools/clang && patch -p1 < ${ ../patches/clang-paths.patch } )
        sedReplace="s|<GCC_PROGRAM_PATH>|${gcc-arm-embedded}/bin|"
        libcxxInclude=${gcc-arm-embedded}/${target}/include/c++/$gccVersion
        sedReplace+=";s|<LIBCXX_INCLUDE_PATH>|$libcxxInclude|"
        sedReplace+=";s|<LIBCXX_INCLUDE_PATH_TARGET>|$libcxxInclude/${target}|"
        sed -i "$sedReplace" tools/clang/lib/Driver/ToolChains.cpp
        
        mkdir build
        cd build
        
        cmake .. \
            -DCMAKE_INSTALL_PREFIX=$out \
            -DCMAKE_BUILD_TYPE=Release \
            -DLLVM_TARGETS_TO_BUILD=ARM \
            -DLLVM_DEFAULT_TARGET_TRIPLE=${target} \
            -DLLVM_INCLUDE_EXAMPLES=OFF \
            -DLLVM_INCLUDE_TESTS=OFF \
            -DLLVM_INCLUDE_DOCS=OFF \
            -DGCC_INSTALL_PREFIX=${gcc-arm-embedded}/lib/gcc/${target}/$gccVersion \
            -DC_INCLUDE_DIRS=${gcc-arm-embedded}/${target}/include \
            -DCLANG_DEFAULT_CXX_STDLIB=libstdc++
    '';
    
    enableParallelBuilding = true;
}
