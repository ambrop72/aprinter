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

{ stdenv, lib, callPackage, clangNative, aprinterSource, toolchain-avr,
  toolchain-arm, toolchain-arm-optsize, toolchain-microblaze, clang-arm,
  clang-arm-optsize, asf, stm32cubef4, teensyCores, aprinterConfigFile,
  aprinterConfigName }:

let
    ## Configuration file handling.

    # Get the parsed output of the generate.py script (attrset at the top).
    cfg = (callPackage ./generate_aprinter_config.nix {}) {
        inherit aprinterSource aprinterConfigFile aprinterConfigName;
    };

    ## Toolchain and tools handling.

    # Embedded GNU toolchain package for the platform (null means use native tools).
    toolchain = {
        avr = toolchain-avr;
        arm = if cfg.toolchainOptSize then toolchain-arm-optsize else toolchain-arm;
        microblaze = toolchain-microblaze;
        linux = null;
    }.${cfg.platformType};

    # Clang compiler package (null means none).
    clangPackage = if !cfg.buildWithClang then null else
        {
            avr = throw "Clang is not supported for AVR.";
            arm = if cfg.toolchainOptSize then clang-arm-optsize else clang-arm;
            microblaze = throw "Clang is not supported for MicroBlaze.";
            linux = clangNative;
        }.${cfg.platformType};

    # Prefix of tools, based on the target name.
    toolPrefix = {
        avr = "avr-";
        arm = "arm-none-eabi-";
        microblaze = "microblaze-xilinx-elf-";
        linux = "";
    }.${cfg.platformType};

    # Get the command for a specific tool.
    getTool = toolName: toolPrefix + toolName;

    # Specific tools used for the build.
    buildToolDefault = getTool (if cfg.buildWithClang then "clang" else "gcc");
    buildToolGcc = getTool "gcc";
    sizeTool = getTool "size";
    objCopyTool = getTool "objcopy";

    ## Dependency handling.

    # Listing of all possible depenencies. The key represents the dependency name
    # and the valus is an attrset with keys "dirName" (name of symlink in the
    # build directory) and "package" (the actual package for the depenency).
    allDependencies = {
        asf = { dirName = "asf"; package = asf; };
        stm32cubef4 = { dirName = "stm32cubef4"; package = stm32cubef4; };
        teensyCores = { dirName = "teensyCores"; package = teensyCores; };
    };

    # List of only the needed dependencies.
    neededDependencies =
        let # IMPORTANT: allPaths must be updated so it contains all paths that
            # are used for the build (via resolvePath).
            allPaths =
                cfg.includeDirs ++
                (lib.optional (cfg.linkerScript != null) cfg.linkerScript) ++
                allSources;
            
            # A dependency is needed if any element of allPaths refers to it
            # in the "base" attribute.
            isDependencyNeeded = depName:
                lib.any (path: path.base == depName) allPaths;
        in
            # Take only the needed dependencies.
            lib.filterAttrs (depName: depSpec: isDependencyNeeded depName) allDependencies;
    
    ## Path specification handling.

    # Get the path where a specific item is found, identified by a name.
    # This works such that appending a relative path results in a relative path
    # for use from within the build directory. The name can be:
    # - A dependency name (key in allDependencies above); it will resolve to the
    #   corresponding directory symlink in the build directory (e.g. "asf/").
    # - "aprinter" for APrinter source code; it will resolve to "" since the
    #   relevant source code subdirectories are symlinked into the build
    #   directory.
    # - "" for paths that are already relative to the build directory; this will
    #   resolve to "".
    resolvePathBase = name:
        if allDependencies ? ${name} then "${allDependencies.${name}.dirName}/"
        else if name == "aprinter" then ""
        else if name == "" then ""
        else throw "Invalid path base name (${name}).";
    
    # Resolve a path specification (attrset with keys "base" and "path").
    resolvePath = path: (resolvePathBase path.base) + path.path;
    
    ## Definition of flags for use with various tools.

    # Common flags used compiling, linking and assembling.
    flagsCommon =
        (if cfg.optimizeForSize then ["-Os"] else ["-O2"]) ++
        (lib.optional cfg.enableDebugSymbols "-g") ++
        ["-fno-math-errno" "-fno-trapping-math"] ++
        ["-ffunction-sections" "-fdata-sections"] ++
        cfg.platformFlags;
    
    # Common flags for compiling C++ and C.
    flagsCompileCommon = flagsCommon ++
        ["-I."] ++
        (map (include: "-I${resolvePath include}") cfg.includeDirs) ++
        ["-DNDEBUG"] ++
        ["-D__STDC_LIMIT_MACROS" "-D__STDC_FORMAT_MACROS" "-D__STDC_CONSTANT_MACROS"] ++
        (map (define: "-D${define.name}" +
            lib.optionalString (define.value != "") "=${define.value}") cfg.defines);
    
    # Flags for compiling C++.
    flagsCompileCXX = flagsCompileCommon ++
        ["-std=c++17" "-fno-rtti" "-fno-exceptions"] ++
        ["-fno-access-control" "-ftemplate-depth=1024"] ++
        cfg.platformFlagsCXX;
    
    # Flags for compiling C.
    flagsCompileC = flagsCompileCommon ++
        ["-std=c99"];
    
    # Flags for assembling.
    flagsAssemble = flagsCommon;

    # Flags for linking.
    flagsLink = flagsCommon ++
        (lib.optional (cfg.linkerScript != null) "-T${resolvePath cfg.linkerScript}") ++
        (map (sym: "-Wl,--defsym,${sym.name}=${sym.value}") cfg.linkerSymbols) ++
        ["-Wl,--gc-sections"] ++
        cfg.extraLinkFlags;
    
    ## Source language handling.

    # Helper to check if a string has any of the given suffixes.
    hasSuffixes = suffixes: str:
        lib.any (suffix: lib.hasSuffix suffix str) suffixes;

    # Get the source language based on the file extension, for use with the -x
    # (language) argument of the compiler.
    getSourceLanguage = source:
        if hasSuffixes [".cpp" ".cxx" ".c++"] source.path then "c++"
        else if hasSuffixes [".c"] source.path then "c"
        else if hasSuffixes [".asm" ".S" ".s"] source.path then "assembler"
        else throw "Cannot determine source file language (${source.path}).";
    
    # Get a build action description for a specific source file, for printing.
    getSourceBuildActionDesc = source:
        {
            "c++" = "Compile C++: ${source.path}";
            "c" = "Compile C: ${source.path}";
            "assembler" = "Assemble: ${source.path}";
        }.${getSourceLanguage source};
    
    ## Commands for building sources and linking.

    # Get the name of the object file for a specific source file.
    getSourceObjPath = source:
        (lib.replaceStrings ["/"] ["_"] source.path) + ".o";

    # Get the flags for building a specific source file.
    getSourceFlags = source:
        {
            "c++" = flagsCompileCXX;
            "c" = flagsCompileC;
            "assembler" = flagsAssemble;
        }.${getSourceLanguage source};
    
    # Get the build tool to be used for a specific source file.
    # Use the selected buildToolDefault unless GCC is specifically requested.
    getSourceBuildTool = source:
        if source ? useGcc && source.useGcc then buildToolGcc else buildToolDefault;
    
    # Get the command for building a specific source file.
    getCompileCommandForSource = source: ''
        ${getSourceBuildTool source} -x ${getSourceLanguage source} \
            -c ${lib.escapeShellArg (resolvePath source)} \
            -o ${lib.escapeShellArg (getSourceObjPath source)} \
            ${lib.escapeShellArgs (getSourceFlags source)}
    '';

    # Get the command for linking object files resulting from specific source
    # files into the specified output executable.
    getLinkCommand = sources: outputExec: ''
        ${buildToolDefault} \
            ${lib.escapeShellArgs (map getSourceObjPath sources)} \
            -o ${lib.escapeShellArg outputExec} \
            ${lib.escapeShellArgs flagsLink}
    '';

    # Wraps a command to print what is being done. The command can also be
    # multiple commands (separated with newlines).
    wrapBuildCommand = message: command: ''
        (
            echo ${lib.escapeShellArg ("  " + message)}
            ${lib.optionalString cfg.verboseBuild "set -x"}
            ${command}
        )
    '';

    ## Definition for objcopy.

    # Get the --output-target value for a specific output type.
    getObjcopyTarget = outputType:
        {
            bin = "binary";
            hex = "ihex";
        }.${outputType};
    
    # Get the command for objcopy.
    getObjCopyCommand = inputFile: outputFile: outputType: ''
        ${objCopyTool} \
            -O ${getObjcopyTarget outputType} \
            ${lib.escapeShellArgs cfg.extraObjCopyFlags} \
            ${lib.escapeShellArg inputFile} \
            ${lib.escapeShellArg outputFile}
    '';

    ## Register address extraction for AVR.

    # These commands perform the required address extraction.
    extractAvrRegAddrsCommands =
        let ppHeaderPath = "aprinter/platform/avr/avr_reg_addr_preprocess.h";
            ppHeaderOut = "avr_reg_addr_preprocess.i";
            genScript = "aprinter/platform/avr/avr_reg_addr_gen.sh";
            genOut = "aprinter_avr_reg_addrs.h";
        in
            # Preprocess avr_reg_addr_helper.h (-P to get no line number comments).
            ''${buildToolDefault} -x c++ -E \
                  ${lib.escapeShellArg ppHeaderPath} -o ${lib.escapeShellArg ppHeaderOut} \
                  ${lib.escapeShellArgs flagsCompileCXX} -P
            '' +
            # Generate the header using another script.
            ''bash ${lib.escapeShellArg genScript} \
                  ${lib.escapeShellArg ppHeaderOut} ${lib.escapeShellArg genOut}
            '';

    ## Definition of sources and outputs.

    # Source file entry representing the main file.
    mainSource = {
        base = "";
        path = "aprinter_main.cpp";
    };

    # All source files (the main file plus extra sources).
    allSources = [mainSource] ++ cfg.extraSourceFiles;

    # Program name used as the base for file name.
    programName = "aprinter";

    # File name for a specific output type.
    fileNameForType = outputType: "${programName}.${outputType}";

    # Name of the program as an ELF executable.
    execFileName = fileNameForType "elf";

in
stdenv.mkDerivation rec {
    # Package name, appearing in the store path.
    name = "aprinter-build";

    # Build input definitions for dependencies to be available in PATH etc.
    nativeBuildInputs =
        lib.optional (toolchain != null) toolchain ++
        lib.optional (clangPackage != null) clangPackage;

    # Do not unpack anything (we refer to aprinterSource directly).
    unpackPhase = "true";

    # Pass the main file source code as a file. This will make Nix set an
    # environment variable "mainSourceCodePath" to the path where the file
    # is located.
    passAsFile = ["mainSourceCode"];
    mainSourceCode = cfg.mainSourceCode;

    # In the configure phase, we prepare the build directory for building.
    configurePhase = wrapBuildCommand "Prepare build directory" (
        # Create symlinks to source code for APrinter and AIpStack. Since the build
        # directory is in the include path, this directly resolves includes like 
        # <aprinter/...> and <aipstack/...>.
        ''ln -s ${aprinterSource}/aprinter aprinter
          ln -s ${aprinterSource}/aipstack/src/aipstack aipstack
        '' +
        # Create symlinks to dependencies.
        lib.concatStrings (lib.mapAttrsToList (depName: depSpec:
            ''ln -s ${depSpec.package} ${lib.escapeShellArg depSpec.dirName}
            ''
        ) neededDependencies) +
        # Create a symlink pointing to the main source file.
        ''ln -s "$mainSourceCodePath" ${lib.escapeShellArg mainSource.path}
        '' +
        # Create the output directory (where the executable will be written) and
        # also create a symlink to that in the build directory.
        ''mkdir "$out"
          ln -s "$out" out
        '');

    # In the build phase we compile, link, print information and produce the
    # desired output formats.
    buildPhase =
        # For AVR, extract register addresses before compiling.
        lib.optionalString (cfg.platformType == "avr") (wrapBuildCommand
            "Extract AVR register addresses" extractAvrRegAddrsCommands) +
        # Compile source files.
        lib.concatMapStrings (source: wrapBuildCommand
            (getSourceBuildActionDesc source)
            (getCompileCommandForSource source)) allSources +
        # Link the executable.
        wrapBuildCommand "Link: ${execFileName}"
            (getLinkCommand allSources "out/${execFileName}") +
        # Print the program size information.
        wrapBuildCommand "Program size:"
          ''${sizeTool} out/${lib.escapeShellArg execFileName} | sed 's/^/    /'
          '' +
        # Generate additional output types using objcopy.
        lib.concatMapStrings (outputType:
            let fileName = fileNameForType outputType;
            in wrapBuildCommand "Generate ${fileName}"
                (getObjCopyCommand "out/${execFileName}" "out/${fileName}" outputType)
            ) (lib.filter (outputType: outputType != "elf") cfg.desiredOutputs) +
        # If the elf output is not desired, delete it.
        lib.optionalString (!(lib.any (t: t == "elf") cfg.desiredOutputs))
            (wrapBuildCommand "Remove ${execFileName}"
                ''rm out/${lib.escapeShellArg execFileName}
                '');
    
    # Nothing needs to be done in the install phase, outputs were written
    # directly to the output directory.
    installPhase = "true";
    
    # Prevent the stdenv packaging code from damaging the outputs.
    dontStrip = true;
    dontPatchELF = true;
    dontPatchShebangs = true;
}
