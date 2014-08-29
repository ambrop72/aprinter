{ stdenv, fetchurl, unzip }:
let
    source = fetchurl {
        url = http://www.atmel.com/images/asf-standalone-archive-3.14.0.86.zip;
        sha256 = "9739afa2c8192bd181f2d4e50fa312dc4c943b7a6a093213e755c0c7de9c3ed3";
    };
in
stdenv.mkDerivation rec {
    name = "atmel-software-framework";
    
    unpackPhase = "true";
    
    nativeBuildInputs = [ unzip ];
    
    installPhase = ''
        mkdir -p $out/EXTRACT
        unzip -q ${source} -d $out/EXTRACT
        mv $out/EXTRACT/asf-standalone-*/*xdk-asf*/* $out/
        echo rm -rf $out/EXTRACT
    '';
    
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
