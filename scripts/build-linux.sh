
configure_linux() {
    echo "  Configuring Linux build"
    
    HOST_CC=gcc
    HOST_SIZE=size

    FLAGS_OPT=( -O$( [[ $OPTIMIZE_FOR_SIZE = "1" ]] && echo s || echo 2 ) )
    CXXFLAGS=(
        -std=c++14 -DNDEBUG "${FLAGS_OPT[@]}" \
        -fno-math-errno -fno-trapping-math
        -fno-rtti -fno-exceptions
        -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -fno-access-control -ftemplate-depth=1024 \
        -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS \
        -I.
        -Wfatal-errors
        -lpthread -lm
        ${CXXFLAGS} ${CCXXLDFLAGS}
    )
    
    RUNBUILD=build_linux
}

build_linux() {
    echo "  Compiling for Linux"
    ${CHECK}
    
    echo "   Compiling and linking"
    ($V; "$HOST_CC" -x c++ "${CXXFLAGS[@]}" "$SOURCE" aprinter/platform/linux/linux_support.cpp -o "$TARGET.elf" || exit 2)
    
    echo "   Size of build: "
    "$HOST_SIZE" "${TARGET}.elf" | sed 's/^/    /'
}
