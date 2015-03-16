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
        We need a specific version of NCD for the service.
    */
    ncd = pkgs.callPackage ./ncd.nix {};
    
    /*
        The configuration/compilation web service.
        This default package is suitable for local use from command line.
        If you want to deploy the service, use service-deployment.nix.
    */
    aprinterServiceExprs = pkgs.callPackage ./service.nix { inherit aprinterSource ncd; };
    aprinterService = aprinterServiceExprs.service;
}
