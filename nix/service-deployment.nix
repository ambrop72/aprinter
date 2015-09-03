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
    aprinterInstances = { pkgs, ... }: [
        { name = "default"; path = "/"; src = pkgs.stdenv.lib.cleanSource ./..; }
        { name = "old";     path = "/old"; src = pkgs.stdenv.lib.cleanSource /home/ambro/dev/aprinter-old; }
    ];
    nixpkgsPath = <nixpkgs>;
    nixpkgsHost = (import nixpkgsPath) {};
    nixpkgsForAprinter = nixpkgsHost.stdenv.lib.cleanSource nixpkgsPath;
    lib = nixpkgsHost.lib;
in
{ test ? false }:
lib.foldl lib.recursiveUpdate {} [
    (lib.optionalAttrs (!test) {
        resources.ec2KeyPairs.my-key-pair = { inherit region accessKeyId; };
    })
    {
        aprinterService = args@{ pkgs, resources, ... }: let
            theInstances = aprinterInstances args;
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
            
            time.timeZone = "CET";
            
            users.extraUsers.aprinter_service = {
                group = "aprinter_service";
                isSystemUser = true;
            };
            
            users.extraGroups.aprinter_service = {};
            
            system.extraDependencies = [
                pkgs.stdenv
            ];
            
            networking.firewall.enable = false;
            
            environment.systemPackages = [
                pkgs.psmisc
                pkgs.tmux
            ];
            
            nix.buildCores = 2;
            nix.maxJobs = 1;
            
            services.lighttpd.enable = true;
            services.lighttpd.enableModules = [ "mod_proxy" "mod_alias" ];
            
            imports = map (inst_index: let
                inst = builtins.elemAt theInstances inst_index;
                name = inst.name;
                pathFirstChar = builtins.substring 0 1 inst.path;
                pathLastChar = builtins.substring ((builtins.stringLength inst.path) - 1) 1 inst.path;
                path = (
                    assert pathFirstChar == "/";
                    assert inst.path == "/" || pathLastChar != "/";
                    inst.path
                );
                pathEmptyIfRoot = if path != "/" then path else "";
                aprinterExprs = (import (builtins.toPath ((toString inst.src) + "/nix"))) { inherit pkgs; };
                backendPort = 4001 + inst_index;
                aprinterServicePkg = aprinterExprs.aprinterServiceExprs.override {
                    withHttpServer = false;
                    ncdArgs = "--logger syslog --loglevel notice --signal-exit-code 0";
                    serviceHost = "0.0.0.0";
                    inherit backendPort;
                    servicePrefix = pathEmptyIfRoot;
                };
            in {
                systemd.services."aprinter_service_${name}" = {
                    description = "APrinter Configuration and Compilation Service (${name})";
                    wantedBy = [ "multi-user.target" ];
                    after = [ "syslog.target" "network-setup.service" ];
                    environment = {
                        APRINTER_SERVICE_TMPDIR = "/run/aprinter-service-temp-${name}";
                        NIX_REMOTE = "daemon";
                        NIX_PATH = "nixpkgs=${nixpkgsForAprinter}";
                    };
                    serviceConfig = {
                        User = "aprinter_service";
                        Group = "aprinter_service";
                        RuntimeDirectory = "aprinter-service-temp-${name}";
                        ExecStart = "${aprinterServicePkg.service}/bin/aprinter-service";
                        Restart = "always";
                        RestartSec = "5";
                    };
                };
                
                services.lighttpd.extraConfig = ''
                    alias.url += ( "${pathEmptyIfRoot}" => "${aprinterServicePkg.gui_dist}" )
                    $HTTP["url"] =~ "^${pathEmptyIfRoot}/compile$" {
                        proxy.server = ("" => (
                            ("host" => "127.0.0.1", "port" => ${toString backendPort})
                        ))
                    }
                '';
                
                system.extraDependencies = aprinterExprs.buildDeps;
            }) (lib.range 0 ((builtins.length theInstances) - 1));
        };
    }
]
