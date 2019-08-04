{ stdenv, fetchurl, nodejs }:
stdenv.mkDerivation {
    name = "typescript";
    
    src = fetchurl {
        url = https://github.com/microsoft/TypeScript/releases/download/v3.5.3/typescript-3.5.3.tgz;
        sha256 = "0xrbh1jyr2nzpd8h8820w1a1hl8szf5apzzdz2zzjw4qf9hn81ky";
    };
    
    propagatedBuildInputs = [nodejs];
    
    installPhase = ''
        mkdir $out
        cp -r . $out/
    '';
    
    dontStrip = true;
    dontPatchELF = true;
}
