/*
 * Copyright (c) 2015 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

{ pkgs ? (import <nixpkgs> {}) }:
with pkgs;
rec {
    # This is where the APrinter source is taken from.
    aprinterSource = stdenv.lib.cleanSource ./..;
    
    # GNU toolchains.
    toolchain-avr = pkgs.callPackage ./gnu-toolchain-avr.nix {};
    toolchain-arm = pkgs.callPackage ./gnu-toolchain.nix { target = "arm-none-eabi"; };
    toolchain-arm-optsize = toolchain-arm.override { optimizeForSize = true; };

    # Clang compilers.
    clang-arm = pkgs.callPackage ./clang-arm.nix { gnu-toolchain = toolchain-arm; };
    clang-arm-optsize = clang-arm.override { gnu-toolchain = toolchain-arm-optsize; };
    
    # GDB (for manual use).
    gdb-arm = pkgs.callPackage ./gdb.nix { target = "arm-none-eabi"; };
    
    # Microcontroller support packages.
    asf = pkgs.callPackage ./asf.nix {};
    stm32cubef4 = pkgs.callPackage ./stm32cubef4.nix {};
    teensyCores = pkgs.callPackage ./teensy_cores.nix {};
    
    # Primary APrinter build function.
    aprinterFunc = aprinterConfig@{ optimizeLibcForSize, ... }:
        # Call the aprinter package with dependencies.
        pkgs.callPackage ./aprinter.nix ({
            # Pass these as-is.
            inherit aprinterSource toolchain-avr asf stm32cubef4 teensyCores;

            # Choose normal or size-optimized toolchain based on optimizeLibcForSize.
            toolchain-arm = if optimizeLibcForSize then toolchain-arm-optsize else toolchain-arm;
            clang-arm = if optimizeLibcForSize then clang-arm-optsize else clang-arm;
        } //
            # Do not pass through optimizeLibcForSize which was handled here.
            (removeAttrs aprinterConfig ["optimizeLibcForSize"]));
    
    # We need a specific version of NCD for the service.
    ncd = pkgs.callPackage ./ncd.nix {};
    
    # The configuration/compilation web service.
    # aprinterService is for local use, while aprinterServiceExprs provided access to
    # specific components to be used for deployment via nixops. If you want to deploy
    # the service, use service-deployment.nix.
    aprinterServiceExprs = pkgs.callPackage ./service.nix { inherit aprinterSource ncd; };
    aprinterService = aprinterServiceExprs.service;
    
    # TypeScript compiler.
    typescript = pkgs.callPackage ./typescript.nix {};
    
    # Builds the web interface.
    aprinterWebif = pkgs.callPackage ./webif.nix { inherit aprinterSource typescript; };
    
    # Hosts the web interface locally while proxying API requests to a device.
    aprinterWebifTest = pkgs.callPackage ./webif-test.nix { inherit aprinterWebif ncd; };
    
    # Various build dependencies split into groups for easy building.
    buildDepsAvr = [ toolchain-avr ];
    buildDepsArmCommon = [ toolchain-arm asf ];
    buildDepsArmOther = [ toolchain-arm-optsize clang-arm
        clang-arm-optsize stm32cubef4 teensyCores ];
    
    # Build dependencies above joined. This can be used from service deployment
    # to ensure that they are already in the Nix store and will not need to be
    # build at the time a build needs them.
    buildDeps = buildDepsAvr ++ buildDepsArmCommon ++ buildDepsArmOther;
}
