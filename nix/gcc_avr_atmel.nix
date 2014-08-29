{ stdenv, patchelf, glibc, gcc, fetchurl }:
stdenv.mkDerivation {
  name = "gcc-avr-atmel1";

  src = fetchurl {
    url = "http://www.atmel.com/images/avr8-gnu-toolchain-3.4.3.1072-linux.any.x86.tar.gz";
    sha256 = "fa815c9e966b67353a16fb37b78e4b7d3e4eec72e8416f2d933a89262a46cbfb";
  };

  buildInputs = [ patchelf ];
  
  dontPatchELF = true;
  
  phases = "unpackPhase patchPhase installPhase";
  
  installPhase = ''
    mkdir -pv $out
    cp -r ./* $out

    for f in $(find $out); do
      if [ -f "$f" ] && patchelf "$f" 2> /dev/null; then
        patchelf --set-interpreter ${glibc}/lib/ld-linux.so.2 \
                 --set-rpath $out/lib:${gcc}/lib \
                 "$f" || true
      fi
    done
  '';

  meta = with stdenv.lib; {
    description = "Pre-built GCC toolchain for AVR microcontrollers";
    homepage = "http://www.atmel.com/tools/atmelavrtoolchainforlinux.aspx";
    license = licenses.gpl3;
    platforms = platforms.linux;
  };
}
