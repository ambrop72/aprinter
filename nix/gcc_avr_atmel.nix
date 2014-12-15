{ stdenv, patchelf, glibc, gcc, fetchurl }:
let
  source_table = {
    "i686-linux" = ["avr8-gnu-toolchain-3.4.5.1522-linux.any.x86.tar.gz" "9d73e7eb489a1ac4916810d8907dced3352e4a1b36d412ac8107078502143391"];
    "x86_64-linux" = ["avr8-gnu-toolchain-3.4.5.1522-linux.any.x86_64.tar.gz" "988c82efff99380b88132f9e05a5ba1cf4a857ae2fbde5a8b0f783f625dce9a1"];
  };
  source_info = source_table.${stdenv.system};
  
in stdenv.mkDerivation {
  name = "gcc-avr-atmel1";

  src = fetchurl {
    url = "http://www.atmel.com/images/${builtins.elemAt source_info 0}";
    sha256 = builtins.elemAt source_info 1;
  };

  buildInputs = [ patchelf ];
  
  dontPatchELF = true;
  
  phases = "unpackPhase patchPhase installPhase";
  
  installPhase = ''
    mkdir -pv $out
    cp -r ./* $out
  '';

  meta = with stdenv.lib; {
    description = "Pre-built GCC toolchain for AVR microcontrollers";
    homepage = "http://www.atmel.com/tools/atmelavrtoolchainforlinux.aspx";
    license = licenses.gpl3;
    platforms = platforms.linux;
  };
}
