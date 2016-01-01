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
    
    /* Atmel Software Framework (chip support for Atmel ARM chips). */
    asf = pkgs.callPackage ./asf.nix {};
    
    /* STM32CubeF4 (chip support for STM32F4). */
    stm32cubef4 = pkgs.callPackage ./stm32cubef4.nix {};
    /* stm32cubef4 = stdenv.lib.cleanSource /home/ambro/cube/STM32Cube_FW_F4_V1.5.0; */
    
    /* Teensy-cores (chip support for Teensy 3). */
    teensyCores = pkgs.callPackage ./teensy_cores.nix {};
    
    /* The primary APrinter build function. */    
    aprinterFunc = aprinterConfig: pkgs.callPackage ./aprinter.nix (
        {
            inherit clang-arm-embedded asf stm32cubef4 teensyCores aprinterSource;
            inherit avrgcclibc;
            gcc-arm-embedded = gcc-arm-embedded-fromsrc;
        } // aprinterConfig
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
    
    buildDeps = [aprinterSource avrgcclibc gcc-arm-embedded-fromsrc clang-arm-embedded asf stm32cubef4 teensyCores];
}
