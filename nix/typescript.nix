{ stdenv, fetchurl, nodejs }:
stdenv.mkDerivation {
    name = "typescript";
    
    src = fetchurl {
        url = https://github.com/Microsoft/TypeScript/archive/v2.1.5.tar.gz;
        sha256 = "5a7cfc8e51cdc8c79597b255935225a86db251723546a481632dcd6200cc0dbb";
    };
    
    propagatedBuildInputs = [nodejs];
    
    installPhase = ''
        mkdir $out
        cp -r . $out/
    '';
    
    dontStrip = true;
    dontPatchELF = true;
}
