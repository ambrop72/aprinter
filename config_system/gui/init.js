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

// The schema. The value is provided by the build script.
var schema = $${SCHEMA};

// The default configuration.
var default_config = $${DEFAULT};

// Divs/textareas on the page
var $editor = document.getElementById('editor');

// Buttons and other stuff
var $save_data_button = document.getElementById('save_data');
var $reload_data_button = document.getElementById('reload_data');
var $delete_saved_data_button = document.getElementById('delete_saved_data');
var $load_defaults_button = document.getElementById('load_defaults');
var $export_data_button = document.getElementById('export_data');
var $import_data_button = document.getElementById('import_data');
var $import_data_file_input = document.getElementById('import_data_file');
var $compile_start_button = document.getElementById('compile_start');
var $error_modal_label = document.getElementById('error_modal_label');
var $error_modal_body = document.getElementById('error_modal_body');

// Base path to the backend.
var $backend_path = window.location.pathname.substring(0, window.location.pathname.lastIndexOf("/") + 1);

// Global variables.
var jsoneditor;

function ce_refarr(obj, attrs) {
    var len = attrs.length;
    for (var i = 0; i < len; i++) {
        obj = obj[attrs[i]];
        if (obj === null || typeof obj !== 'object') {
            return [];
        }
    }
    return obj;
}

function ce_deref(target_arr, target_id_field, target_id) {
    for (var index in target_arr) {
        var target = target_arr[index];
        if (target[target_id_field] == target_id) {
            return target;
        }
    }
    return null;
}

function ce_copyhelper(rows, row, name_key, name_suffix) {
    var name = row[name_key];
    while (true) {
        name = name + name_suffix;
        var exists = false;
        for (var i = 0; i < rows.length; i++) {
            if (rows[i][name_key] === name) {
                exists = true;
            }
        }
        if (!exists) {
            var ext = {};
            ext[name_key] = name;
            return JSONEditor_utils.extend(row, ext);
        }
    }
}

function get_default_config() {
    return JSON.parse(JSON.stringify(default_config));
}

// Function to get config from local storage or default.
function get_config(allow_default) {
    var config_json = localStorage.getItem("aprinter_config");
    if (!config_json) {
        if (allow_default) {
            return {ok: true, value: get_default_config()};
        }
        return {ok: false};
    }
    var config_value = JSON.parse(config_json);
    return {ok: true, value: config_value};
}

function set_compile_status(running) {
    $compile_start_button.disabled = running;
    
    var elems = document.getElementsByClassName("compile_progress");
    for (var i = 0; i < elems.length; i++) {
        elems[i].style.visibility = running ? "visible" : "hidden";
    }
}

function get_string_value() {
    return JSON.stringify(jsoneditor.getValue());
}

function get_pretty_string_value() {
    return JSON.stringify(jsoneditor.getValue(), undefined, 2);
}

function base64_to_blob(input, content_type) {
    var str = window.atob(input);
    var ab = new ArrayBuffer(str.length);
    var ia = new Uint8Array(ab);
    for (var i = 0; i < str.length; i++) {
        ia[i] = str.charCodeAt(i);
    }
    return new Blob([ia], {type: content_type});
}

function show_error_dialog(header, contents) {
    $error_modal_label.innerText = header;
    $error_modal_body.innerText = contents;
    $('#error_modal').modal({});
}

function each_in(obj, prop, func) {
    if (JSONEditor_utils.isObject(obj)) {
        JSONEditor_utils.each(JSONEditor_utils.get(obj, prop), func);
    }
}

function fixup_config(config) {
    each_in(config, "boards", function(i, board) {
        each_in(board, "analog_inputs", function(j, analog_input) {
            if (JSONEditor_utils.isObject(analog_input)) {
                if (!JSONEditor_utils.has(analog_input, "Driver") && JSONEditor_utils.has(analog_input, "Pin")) {
                    analog_input["Driver"] = {"_compoundName": "AdcAnalogInput", "Pin": analog_input["Pin"]};
                    delete analog_input["Pin"];
                }
            }
        });
    });
}

var load = function() {
    // Get initial configuration from local storage, if any.
    var startval = get_config(true).value;
    fixup_config(startval);

    // Create the editor.
    jsoneditor = new JSONEditor($editor, {
        schema: schema,
        startval: startval,
        trace_processing: false
    });
    
    // When the save button is pressed, save the config data to local storage.
    $save_data_button.addEventListener('click', function() {
        var config_json = get_string_value();
        localStorage.setItem("aprinter_config", config_json);
    });
    
    // When the reload button is pressed, load the config data from local storage.
    $reload_data_button.addEventListener('click', function() {
        var get_config_res = get_config(false);
        if (!get_config_res.ok) {
            alert("Cannot load - no configuration present in local storage!");
            return;
        }
        var config = get_config_res.value;
        fixup_config(config);
        jsoneditor.setValue(config);
    });
    
    // Handler for the delete-saved-data button.
    $delete_saved_data_button.addEventListener('click', function() {
        localStorage.removeItem("aprinter_config");
    });
    
    // When the load-defaults button is pressed, load the default config.
    $load_defaults_button.addEventListener('click', function() {
        var config = get_default_config();
        fixup_config(config);
        jsoneditor.setValue(config);
    });
    
    // If the window is about to be closed with unsaved data, ask for confirmation.
    window.onbeforeunload = function (evt) {
        if (JSON.stringify(get_config(true).value) == get_string_value()) {
            return null;
        }
        var message = 'APrinter Configuration: There are unsaved configuration changes!';
        if (typeof evt == 'undefined') {
            evt = window.event;
        }
        if (evt) {
            evt.returnValue = message;
        }
        return message;
    }
    
    // When the export button is pressed, trigger downloading of the configuration dump.
    $export_data_button.addEventListener('click', function() {
        var config_json = get_pretty_string_value();
        var blob = new Blob([config_json], {type: 'application/json;charset=utf-8'});
        saveAs(blob, 'aprinter_config.json')
    });
    
    // Import button handler.
    $import_data_button.addEventListener('click', function() {
        if (!$import_data_file_input.files[0]) {
            alert('Please select a configuration file to import.');
            return;
        }
        var reader = new FileReader();
        reader.onload = function() {
            var config = JSON.parse(this.result);
            fixup_config(config);
            jsoneditor.setValue(config);
        }
        reader.readAsText($import_data_file_input.files[0]);
    });
    
    // Compile button handler.
    $compile_start_button.addEventListener('click', function() {
        var compile_request = new XMLHttpRequest();
        compile_request.open("POST", $backend_path + "compile", true);
        compile_request.setRequestHeader("Content-type", "application/json; charset=utf-8")
        
        compile_request.onreadystatechange = function() {
            if (compile_request.readyState == 4) {
                set_compile_status(false);
                
                if (compile_request.status != 200) {
                    var header = "Compilation failed: ";
                    if (compile_request.status == 0) {
                        header += "communication error";
                    } else {
                        header += "HTTP error " + compile_request.status.toString();
                    }
                    var contents = compile_request.responseText !== null ? compile_request.responseText : "";
                    show_error_dialog(header, contents);
                } else {
                    var result = JSON.parse(compile_request.responseText);
                    if (!result.success) {
                        var header = "Compilation failed: " + result.message;
                        var contents = result.hasOwnProperty('error') ? result.error : "";
                        show_error_dialog(header, contents);
                    } else {
                        var blob = base64_to_blob(result.data, 'application/octet-stream');
                        saveAs(blob, result.filename)
                    }
                }
            }
        }
        
        compile_request.send(get_string_value());
        set_compile_status(true);
    });
}

load();
