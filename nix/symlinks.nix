{ stdenv, aprinterBuilds }:
stdenv.mkDerivation {
    name = "aprinter-symlinks";
    
    unpackPhase = "true";
    
    installPhase = ''
        mkdir $out
    '' + stdenv.lib.concatStrings (map (build: ''
        ln -s ${build} $out/${build.aprinterBuildName}
    '') aprinterBuilds);
}
