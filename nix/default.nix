{ pkgs ? (import <nixpkgs> {}) }:
with pkgs;
rec {
    /* This is where the APrinter source is taken from. */
    aprinterSource = stdenv.lib.cleanSource ./..;
    
    /* Atmel AVR toolchain. */
    gccAvrAtmel = pkgs.callPackage ./gcc_avr_atmel.nix {};
    
    /* Atmel Software Framework (chip support for Atmel ARM chips). */
    asf = pkgs.callPackage ./asf.nix {};
    
    /* Teensy-cores (chip support for Teensy 3). */
    teensyCores = pkgs.callPackage ./teensy_cores.nix {};
    
    /*
        The primary APrinter build function.
        It takes an aprinterConfig attrset with specific settings.
        
        The mandatory attributes are: buildName, boardName, mainText, desiredOutputs.        
        The optional attributes are shown below, with examples of non-default values.
        
        assertionsEnabled = true;
        eventLoopBenchmarkEnabled = true;
        detectOverloadEnabled = true;
        forceUartSerial = true;
        
        To pass them, either modify aprinterTestFunc to use them for all targets,
        or use .override, like this:
        
        aprinterTestMelziWithAssertions = aprinterTestMelzi.override { assertionsEnabled = true; };
    */    
    aprinterFunc = aprinterConfig: pkgs.callPackage ./aprinter.nix (
        {
            inherit gccAvrAtmel asf teensyCores aprinterSource;
        } // aprinterConfig
    );
    
    /*
        This function can be used to build multiple APrinter build targets,
        and put the results into a directory, with a subdirectory for each
        target.
    */
    aprinterSymlinksFunc = aprinterBuilds: pkgs.callPackage ./symlinks.nix { inherit aprinterBuilds; };
    
    /*
        Shortcut to aprinterFunc for defining the common supported targets.
    */
    aprinterTestFunc = boardName: { sourceName ? boardName }: aprinterFunc {
        inherit boardName;
        buildName = "test-${sourceName}";
        mainText = builtins.readFile ( ../main + "/aprinter-${sourceName}.cpp" );
        desiredOutputs = [ "elf" "bin" "hex" ];
    };
    
    /*
        Supported targets.
    */
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
    
    /*
        Shortcuts for building all targets, possibly with assertions enabled.
    */
    allTestTargets = [
        aprinterTestMelzi aprinterTestRamps13 aprinterTestMegatronics3
        aprinterTestRampsfd aprinterTestRampsfdUart aprinterTestRadds
        aprinterTestRaddsUart aprinterTest4pi aprinterTestMlab aprinterTestTeensy3
        aprinterTestCoreXyLaser
    ];
    aprinterTestAll = aprinterSymlinksFunc allTestTargets;
    aprinterTestAllDebug = aprinterSymlinksFunc (map (t: t.override { assertionsEnabled = true; }) allTestTargets);
    
    /*
        We need a specific version of NCD for the service.
    */
    ncd = pkgs.callPackage ./ncd.nix {};
    
    /*
        Expose service stuff to allow proper deployment.
    */
    aprinterServiceExprs = pkgs.callPackage ./service.nix { inherit aprinterSource ncd; };
    
    /*
        A simple setup of the configuration/compilation service, as as a single executable.
    */
    aprinterService = aprinterServiceExprs.withHttpServer;
}
