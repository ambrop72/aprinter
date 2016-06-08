{ stdenv, fetchurl, unzip, typescript, definitelyTyped, aprinterSource }:
let
    jquery = fetchurl {
        url = https://code.jquery.com/jquery-2.2.4.min.js;
        sha256 = "13jglpbvm4cjqpbi82fsq8bi0b0ynwxd1nh8yvc19zqzyjb5vf05";
    };
    react = fetchurl {
        url = https://fb.me/react-15.1.0.min.js;
        sha256 = "1xgwxsm26qcjjr03181hynzj64y028i6x22s0xdaw7mxvaf67zzm";
    };
    reactDom = fetchurl {
        url = https://fb.me/react-dom-15.1.0.min.js;
        sha256 = "07jbb8f4ky1p9nzmfmxq65w278p0lnlm0pra02qmxmyqh2zhp5sq";
    };
    bootstrap = fetchurl {
        url = https://github.com/twbs/bootstrap/releases/download/v3.3.6/bootstrap-3.3.6-dist.zip;
        sha256 = "05h3i83fwknqzdzm339ngjif0k87g4qx5jqgspnf8yjnshww4yj1";
    };
    
in
stdenv.mkDerivation {
    name = "aprinter-webif";
    
    nativeBuildInputs = [unzip typescript];
    
    unpackPhase = "true";
    
    installPhase = ''
        mkdir $out
        
        cp -r ${aprinterSource}/webif/{reprap.css,reprap.htm} $out/
        
        mkdir $out/jquery
        cp ${jquery} $out/jquery/jquery.js
        
        mkdir $out/react
        cp ${react} $out/react/react.js
        cp ${reactDom} $out/react/react-dom.js
        
        unzip -q ${bootstrap} -d $out/
        mv $out/bootstrap-*-dist $out/bootstrap
        
        tsc \
            --allowSyntheticDefaultImports \
            --jsx react \
            ${definitelyTyped}/jquery/jquery.d.ts \
            ${definitelyTyped}/react/react-global.d.ts \
            ${aprinterSource}/webif/reprap.tsx \
            --outDir $out \
            || [[ $? = 2 ]]
    '';
}


