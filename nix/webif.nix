{ stdenv, nodePackages, aprinterSource }:
stdenv.mkDerivation {
    name = "aprinter-webif";
    
    buildInputs = [nodePackages.react-tools];
    
    unpackPhase = "true";
    
    installPhase = ''
        mkdir $out
        cp -r ${aprinterSource}/webif/* $out/
        jsx $out/reprap.jsx > $out/reprap.js
        rm $out/reprap.jsx
    '';
}


