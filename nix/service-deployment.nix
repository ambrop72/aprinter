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
            ncdArgs = "--logger syslog --loglevel notice";
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
        
        time.timeZone = "CET";
    };
} // (if test then {} else {
    resources.ec2KeyPairs.my-key-pair = { inherit region accessKeyId; };
})
