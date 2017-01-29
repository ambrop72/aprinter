{ stdenv, fetchurl, unzip, typescript, aprinterSource
, useDebugReact ? false
}:
let
    jquery = fetchurl {
        url = https://code.jquery.com/jquery-2.2.4.min.js;
        sha256 = "13jglpbvm4cjqpbi82fsq8bi0b0ynwxd1nh8yvc19zqzyjb5vf05";
    };
    
    react = fetchurl (if useDebugReact then {
        url = https://fb.me/react-15.1.0.js;
        sha256 = "1jvxwx7cvmi5wyd6p0jbghl27nlicd6x7l53bsc9xy3sjc128kv7";
    } else  {
        url = https://fb.me/react-15.1.0.min.js;
        sha256 = "1xgwxsm26qcjjr03181hynzj64y028i6x22s0xdaw7mxvaf67zzm";
    });
    
    reactDom = fetchurl (if useDebugReact then {
        url = https://fb.me/react-dom-15.1.0.js;
        sha256 = "07mrwxmrzarhrva3djp3ld717lhi9pi18aw2wlh434qbsvxnwc5i";
    } else {
        url = https://fb.me/react-dom-15.1.0.min.js;
        sha256 = "07jbb8f4ky1p9nzmfmxq65w278p0lnlm0pra02qmxmyqh2zhp5sq";
    });
    
    bootstrap = fetchurl {
        url = https://github.com/twbs/bootstrap/releases/download/v3.3.6/bootstrap-3.3.6-dist.zip;
        sha256 = "05h3i83fwknqzdzm339ngjif0k87g4qx5jqgspnf8yjnshww4yj1";
    };
    
    requirejs = fetchurl {
        url = http://requirejs.org/docs/release/2.2.0/minified/require.js;
        sha256 = "1nlf1f37rj4c95pw041gj3qrgpwsavdxi7s6arwdlzmll5jk0y4b";
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
        
        mkdir $out/requirejs
        cp ${requirejs} $out/requirejs/require.js
        
        mkdir $out/ResizeSensor
        cat ${aprinterSource}/webif/ResizeSensor/*.js > $out/ResizeSensor/resize-sensor.js
        
        tsc \
            --allowSyntheticDefaultImports \
            --jsx react \
            ${aprinterSource}/webif/typings/react/index.d.ts \
            ${aprinterSource}/webif/typings/jquery/index.d.ts \
            ${aprinterSource}/webif/reprap.tsx \
            --outDir $out \
            || [[ $? = 2 ]]
    '';
}


