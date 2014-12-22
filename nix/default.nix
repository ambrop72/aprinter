with import <nixpkgs> {};
rec {
    gccAvrAtmel = pkgs.callPackage ./gcc_avr_atmel.nix {};
    
    asf = pkgs.callPackage ./asf.nix {};
    
    teensyCores = pkgs.callPackage ./teensy_cores.nix {};
    
    /*
        Extra options accepted by aprinterFunc:
        
        assertionsEnabled = true;
        eventLoopBenchmarkEnabled = true;
        detectOverloadEnabled = true;
        forceUartSerial = true;
        
        Either aprinterFunc can be modified to pass them, or .override can be used,
        like this:
        
        aprinterTestMelziWithAssertions = aprinterTestMelzi.override { assertionsEnabled = true; };
    */
    
    aprinterFunc = aprinterConfig: pkgs.callPackage ./aprinter.nix (
        {
            inherit gccAvrAtmel asf teensyCores;
        } // aprinterConfig
    );
    
    aprinterSymlinksFunc = aprinterBuilds: pkgs.callPackage ./symlinks.nix { inherit aprinterBuilds; };
    
    aprinterTestFunc = boardName: { sourceName ? boardName }: aprinterFunc {
        inherit boardName;
        buildName = "test-${sourceName}";
        mainText = builtins.readFile ( ../main + "/aprinter-${sourceName}.cpp" );
        desiredOutputs = [ "elf" "bin" "hex" ];
    };
    
    aprinterTestMelzi = aprinterTestFunc "melzi" {};
    aprinterTestRamps13 = aprinterTestFunc "ramps13" {};
    aprinterTestMegatronics3 = aprinterTestFunc "megatronics3" {};
    aprinterTestRampsfd = aprinterTestFunc "rampsfd" {};
    aprinterTestRampsfdUart = aprinterTestRampsfd.override { forceUartSerial = true; };
    aprinterTestRadds = aprinterTestFunc "radds" {};
    aprinterTestRaddsUart = aprinterTestRadds.override { forceUartSerial = true; };
    aprinterTest4pi = aprinterTestFunc "4pi" {};
    aprinterTestMlab = aprinterTestFunc "mlab" {};
    aprinterTestTeensy3 = aprinterTestFunc "teensy3" {};
    aprinterTestCoreXyLaser = aprinterTestFunc "teensy3" { sourceName = "teensy3-corexy-laser"; };
    
    allTestTargets = [
        aprinterTestMelzi aprinterTestRamps13 aprinterTestMegatronics3
        aprinterTestRampsfd aprinterTestRampsfdUart aprinterTestRadds
        aprinterTestRaddsUart aprinterTest4pi aprinterTestMlab aprinterTestTeensy3
        aprinterTestCoreXyLaser
    ];
    aprinterTestAll = aprinterSymlinksFunc allTestTargets;
    aprinterTestAllDebug = aprinterSymlinksFunc (map (t: t.override { assertionsEnabled = true; }) allTestTargets);
    
    valvePlay = aprinterTestFunc "teensy3" { sourceName = "teensy3-valve"; };
}
