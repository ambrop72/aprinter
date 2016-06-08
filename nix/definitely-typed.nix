{ stdenv, fetchgit }:
stdenv.mkDerivation {
    name = "definitely-typed";
    
    src = fetchgit {
        url = https://github.com/DefinitelyTyped/DefinitelyTyped;
        rev = "e3df7a21b96cb3a5afc3d0e128c68c66b84af89c";
        sha256 = "0kj86prprdl5rg18wb6icm47w8ag2v9fpw8dr4m6yfyyjfyijq9l";
    };
    
    installPhase = ''
        mkdir $out
        cp -r . $out/
    '';
    
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
