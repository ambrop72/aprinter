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
    /* This is where the APrinter source is taken from. */
    aprinterSource = stdenv.lib.cleanSource ./..;
    
    /* AVR toolchain, built from source. */
    avrgcclibc = pkgs.callPackage ./avr-gcc-libc.nix {};
    
    /* ARM microcontrollers toolchain, build from source. */
    gcc-arm-embedded-fromsrc = pkgs.callPackage ./gcc-arm-embedded-fromsrc.nix {};
    
    /* Clang compiler for ARM microcontrollers. */
    clang-arm-embedded = pkgs.callPackage ./clang-arm-embedded.nix {
        gcc-arm-embedded = gcc-arm-embedded-fromsrc;
    };
    
    /* ARM toolchain but with newlib optimized for size. */
    gcc-arm-embedded-fromsrc-optsize = gcc-arm-embedded-fromsrc.override { optimizeForSize = true; };
    
    /* GDB for ARM. */
    gdb-arm = pkgs.callPackage ./gdb-arm.nix {};
    
    /* Clang with newlib optimized for size. */
    clang-arm-embedded-optize = clang-arm-embedded.override {
        gcc-arm-embedded = gcc-arm-embedded-fromsrc-optsize;
    };
    
    /* Atmel Software Framework (chip support for Atmel ARM chips). */
    asf = pkgs.callPackage ./asf.nix {};
    
    /* STM32CubeF4 (chip support for STM32F4). */
    stm32cubef4 = pkgs.callPackage ./stm32cubef4.nix {};
    /* stm32cubef4 = stdenv.lib.cleanSource /home/ambro/cube/STM32Cube_FW_F4_V1.5.0; */
    
    /* Teensy-cores (chip support for Teensy 3). */
    teensyCores = pkgs.callPackage ./teensy_cores.nix {};
    
    /* The primary APrinter build function. */    
    aprinterFunc = aprinterConfig@{ optimizeLibcForSize, ... }: pkgs.callPackage ./aprinter.nix (
        {
            inherit aprinterSource avrgcclibc asf stm32cubef4 teensyCores;
            gcc-arm-embedded = if optimizeLibcForSize then gcc-arm-embedded-fromsrc-optsize else gcc-arm-embedded-fromsrc;
            clang-arm-embedded = if optimizeLibcForSize then clang-arm-embedded-optize else clang-arm-embedded;
        } // (removeAttrs aprinterConfig ["optimizeLibcForSize"])
    );
    
    /* We need a specific version of NCD for the service. */
    ncd = pkgs.callPackage ./ncd.nix {};
    
    /*
        The configuration/compilation web service.
        This default package is suitable for local use from command line.
        If you want to deploy the service, use service-deployment.nix.
    */
    aprinterServiceExprs = pkgs.callPackage ./service.nix { inherit aprinterSource ncd; };
    aprinterService = aprinterServiceExprs.service;
    
    /* TypeScript compiler. */
    typescript = pkgs.callPackage ./typescript.nix {};
    
    /* Builds the web interface. */
    aprinterWebif = pkgs.callPackage ./webif.nix { inherit aprinterSource typescript; };
    
    /* Hosts the web interface locally while proxying API requests to a device. */
    aprinterWebifTest = pkgs.callPackage ./webif-test.nix { inherit aprinterWebif ncd; };
    
    /* This is used by the service deployment expression to ensure that the
     * build dependencies are already in the Nix store. */
    buildDepsArmCommon = [
        gcc-arm-embedded-fromsrc
        asf
        stm32cubef4
        teensyCores
    ];
    buildDepsArmUncommon = [
        gcc-arm-embedded-fromsrc-optsize
        clang-arm-embedded
        clang-arm-embedded-optize
    ];
    buildDepsAvr = [
        avrgcclibc
    ];
    buildDeps = buildDepsArmCommon ++ buildDepsArmUncommon ++ buildDepsAvr;
}
