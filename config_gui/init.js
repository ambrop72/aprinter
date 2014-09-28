// The schema. The value is provided by the build script.
var schema = $${SCHEMA};

// The default configuration.
var default_config = $${DEFAULT};

// Divs/textareas on the page
var $output = document.getElementById('output');
var $editor = document.getElementById('editor');
var $validate = document.getElementById('validate');

// Buttons
var $save_data_button = document.getElementById('save_data');
var $reload_data_button = document.getElementById('reload_data');
var $load_defaults_button = document.getElementById('load_defaults');

var jsoneditor;

// Helper functions for resolving references within the editor.
function aprinter_resolve_ref(target_arr, target_id_field, target_id) {
    for (var index in target_arr) {
        var target = target_arr[index];
        if (target[target_id_field] == target_id) {
            return target;
        }
    }
    return null;
}

// Function to get config from local storage or default.
function get_config(allow_default) {
    var config_json = localStorage.getItem("aprinter_config");
    if (!config_json) {
        if (allow_default) {
            return {ok: true, value: default_config};
        }
        return {ok: false};
    }
    var config_value = JSON.parse(config_json);
    return {ok: true, value: config_value};
}

var load = function() {
    // Get initial configuration from local storage, if any.
    var startval = get_config(true).value;

    // Create the editor.
    jsoneditor = new JSONEditor($editor, {
        schema: schema,
        startval: startval,
        disable_edit_json: true,
        disable_properties: true,
        template: "javascript",
        theme: "bootstrap3",
        iconlib: "bootstrap3"
    });

    // When the value of the editor changes, update the JSON output and validation message
    jsoneditor.on('change', function() {
        // Update the output.
        var config_value = jsoneditor.getValue();
        $output.value = JSON.stringify(config_value, null, 2);

        // Show validation errors if there are any
        var validation_errors = jsoneditor.validate();
        if (validation_errors.length) {
            $validate.value = JSON.stringify(validation_errors, null, 2);
        } else {
            $validate.value = 'valid';
        }
    });
    
    // When the save button is pressed, save the config data to local storage.
    $save_data_button.addEventListener('click', function() {
        var config_json = JSON.stringify(jsoneditor.getValue());
        localStorage.setItem("aprinter_config", config_json);
    });
    
    // When the reload button is pressed, load the config data from local storage.
    $reload_data_button.addEventListener('click', function() {
        var get_config_res = get_config(false);
        if (!get_config_res.ok) {
            alert("Cannot load - no configuration present in local storage!");
            return;
        }
        jsoneditor.setValue(get_config_res.value);
    });
    
    // When the load-defaults button is pressed, load the default config.
    $load_defaults_button.addEventListener('click', function() {
        jsoneditor.setValue(default_config);
    });
    
    // If the window is about to be closed with unsaved data, ask for confirmation.
    window.onbeforeunload = function (evt) {
        if (JSON.stringify(get_config(true).value) == JSON.stringify(jsoneditor.getValue())) {
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
};

// Start the schema and output textareas with initial values
$output.value = '';

load();
