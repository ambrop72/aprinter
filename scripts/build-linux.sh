
configure_linux() {
    echo "  Configuring Linux build"
    
    if [ "$BUILD_WITH_CLANG" = 1 ]; then
        HOST_CC=clang
    else
        HOST_CC=gcc
    fi
    
    HOST_SIZE=size
    
    FLAGS_OPT=( -O$( [[ $OPTIMIZE_FOR_SIZE = "1" ]] && echo s || echo 2 ) )
    FLAGS_C_CXX_LD=(
        "${FLAGS_OPT[@]}"
        -fno-math-errno -fno-trapping-math
    )
    FLAGS_CXX_LD=(
        -fno-rtti -fno-exceptions
    )
    FLAGS_C=(
        -std=c99
    )
    FLAGS_CXX=(
        -std=c++14 -ftemplate-depth=1024 -fno-access-control
    )
    FLAGS_C_CXX=(
        -DNDEBUG
        -I.
        -Wfatal-errors
        -Wno-absolute-value -Wno-undefined-internal
        "${EXTRA_COMPILE_FLAGS[@]}"
    )
    FLAGS_LD=(
        "${EXTRA_LINK_FLAGS[@]}"
    )
    
    C_SOURCES=( $(eval echo "$EXTRA_C_SOURCES") )
    CXX_SOURCES=( $(eval echo "$EXTRA_CXX_SOURCES") "${SOURCE}" aprinter/platform/linux/linux_support.cpp )

    OBJS=()
    
    RUNBUILD=build_linux
}

build_linux() {
    echo "  Compiling for Linux"
    ${CHECK}
    CFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_C[@]}" ${CFLAGS} ${CCXXLDFLAGS})
    CXXFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_C_CXX[@]}" "${FLAGS_CXX[@]}" ${CXXFLAGS} ${CCXXLDFLAGS})
    ASMFLAGS=("${FLAGS_C_CXX_LD[@]}")
    LDFLAGS=("${FLAGS_C_CXX_LD[@]}" "${FLAGS_CXX_LD[@]}" "${FLAGS_LD[@]}" ${LDFLAGS} ${CCXXLDFLAGS})
    
    echo "   Compiling C files"
    for file in "${C_SOURCES[@]}"; do
        OBJ=${BUILD}/$(basename "${file}" .c).o
        OBJS+=( "${OBJ}" )
        ( $V ; "${HOST_CC}" -x c -c "${CFLAGS[@]}" "${file}" -o "${OBJ}" )
    done

    echo "   Compiling C++ files"
    for file in "${CXX_SOURCES[@]}"; do
        OBJ=${BUILD}/$(basename "${file}" .cpp).o
        OBJS+=( "${OBJ}" )
        ( $V ; "${HOST_CC}" -x c++ -c "${CXXFLAGS[@]}" "${file}" -o "${OBJ}" )
    done
    
    echo "   Linking objects"
    ( $V ; "${HOST_CC}" "${LDFLAGS[@]}" "${OBJS[@]}" -o "${TARGET}.elf" -lpthread -lrt -lm )
    
    echo "   Size of build: "
    "$HOST_SIZE" "${TARGET}.elf" | sed 's/^/    /'
}
