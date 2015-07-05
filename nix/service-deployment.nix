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

let
    region = "us-west-2";
    accessKeyId = "mine";
    
in
{ test ? false }:
{
    aprinterService = { pkgs, resources, ... }: let
        aprinterSource = pkgs.stdenv.lib.cleanSource ./..;
        aprinterExprs = (import (builtins.toPath ((toString aprinterSource) + "/nix"))) { inherit pkgs; };
        
        aprinterServicePkg = aprinterExprs.aprinterServiceExprs.override {
            withHttpServer = false;
            ncdArgs = "--logger syslog --loglevel notice --signal-exit-code 0";
            serviceHost = "0.0.0.0";
            servicePort = 80;
        };
        
        nixpkgsForAprinter = pkgs.stdenv.lib.cleanSource <nixpkgs>;
        
    in {
        deployment = if test == true then {
            targetEnv = "virtualbox";
            virtualbox.memorySize = 1024;
        } else {
            targetEnv = "ec2";
            ec2.accessKeyId = accessKeyId;
            ec2.region = region;
            ec2.instanceType = "t2.micro";
            ec2.keyPair = resources.ec2KeyPairs.my-key-pair;
        };
        
        users.extraUsers.aprinter_service = {
            group = "aprinter_service";
            isSystemUser = true;
        };
        
        users.extraGroups.aprinter_service = {};
        
        systemd.services.aprinter_service = {
            description = "APrinter Configuration and Compilation Service";
            wantedBy = [ "multi-user.target" ];
            after = [ "syslog.target" "network-setup.service" ];
            environment = {
                APRINTER_SERVICE_TMPDIR = "/run/aprinter-service-temp";
                NIX_REMOTE = "daemon";
                NIX_PATH = "nixpkgs=${nixpkgsForAprinter}";
            };
            serviceConfig = {
                User = "aprinter_service";
                Group = "aprinter_service";
                RuntimeDirectory = "aprinter-service-temp";
                ExecStart = "${aprinterServicePkg.service}/bin/aprinter-service";
                Restart = "always";
                RestartSec = "5";
            };
        };
        
        services.lighttpd.enable = true;
        services.lighttpd.document-root = aprinterServicePkg.gui_dist;
        services.lighttpd.enableModules = [ "mod_proxy" ];
        services.lighttpd.extraConfig = aprinterServicePkg.lighttpd_proxy_config;
        
        networking.firewall.enable = false;
        
        environment.systemPackages = [
            pkgs.psmisc
        ];
        
        environment.etc."aprinter-stdenv".source = pkgs.stdenv;
        environment.etc."aprinter-gcc-arm-embedded".source = pkgs.gcc-arm-embedded;
        environment.etc."aprinter-clang-arm-embedded".source = aprinterExprs.clang-arm-embedded;
        environment.etc."aprinter-gccAvrAtmel".source = aprinterExprs.gccAvrAtmel;
        environment.etc."aprinter-asf".source = aprinterExprs.asf;
        environment.etc."aprinter-stm32cubef4".source = aprinterExprs.stm32cubef4;
        environment.etc."aprinter-teensyCores".source = aprinterExprs.teensyCores;
        
        time.timeZone = "CET";
    };
} // (if test then {} else {
    resources.ec2KeyPairs.my-key-pair = { inherit region accessKeyId; };
})
