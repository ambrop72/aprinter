{ stdenv, fetchurl, nodejs }:
stdenv.mkDerivation {
    name = "typescript";
    
    src = fetchurl {
        url = https://github.com/Microsoft/TypeScript/archive/v1.8.10.tar.gz;
        sha256 = "59afafd9840f946cda999f6ebe698618eafdcd9e31daefa0ea5410d3f08fc656";
    };
    
    propagatedBuildInputs = [nodejs];
    
    installPhase = ''
        mkdir $out
        cp -r . $out/
    '';
    
    dontStrip = true;
    dontPatchELF = true;
}
