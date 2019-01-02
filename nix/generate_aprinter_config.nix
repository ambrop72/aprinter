# This implements running the generate.py script and getting the resulting
# output configuration data into a Nix based data structure. The top function
# here is designed to be called via callPackage, even though the result is not
# a package but a function.
{ runCommand, python27, lib }:
{ aprinterSource, aprinterConfigFile, aprinterConfigName }:
    let
        # Get the configuration file into the Nix store.
        configFileDrv = builtins.toFile "aprinter-config-file"
            (builtins.readFile aprinterConfigFile);
        
        # Handle null aprinterConfigName.
        actualConfigName =
            if (aprinterConfigName == null) then "" else "${aprinterConfigName}";

        # Run the generate.py script and get its output (JSON) into a file.
        generatedConfigFile = runCommand "aprinter-generated-config.json" {} ''
            ${python27}/bin/python -B \
                ${aprinterSource}/config_system/generator/generate.py \
                --config ${lib.escapeShellArg "${configFileDrv}"} \
                --cfg-name ${lib.escapeShellArg actualConfigName} \
                > "$out"
        '';

    # Read and parse the JSON.
    in lib.importJSON generatedConfigFile
