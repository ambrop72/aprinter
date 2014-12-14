{ stdenv, fetchurl, unzip }:
let
    source = fetchurl {
        url = http://www.atmel.com/images/asf-standalone-archive-3.20.1.101.zip;
        sha256 = "c9fecef57c9dd57bcc3a5265fba7382e022fa911bbf97ba2d14c2a6b92f1e8cc";
    };
in
stdenv.mkDerivation rec {
    name = "atmel-software-framework";
    
    unpackPhase = "true";
    
    nativeBuildInputs = [ unzip ];
    
    installPhase = ''
        mkdir -p "$out"/EXTRACT
        unzip -q ${source} -d "$out"/EXTRACT
        mv "$out"/EXTRACT/*xdk-asf*/* "$out"/
        rm -rf "$out"/EXTRACT
    '';
    
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
