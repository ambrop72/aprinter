JSONEditor.defaults.editors.derived = JSONEditor.AbstractEditor.extend({
    buildImpl: function() {
        if ($utils.has(this.schema, 'valueTemplate')) {
            this.derived_mode = 'template';
            this.template = this.jsoneditor.compileTemplate(this.schema.valueTemplate, this.template_engine);
        } else if ($utils.has(this.schema, 'constantValue')) {
            this.derived_mode = 'constant';
            this.value = this.schema.constantValue;
        } else {
            throw "'derived' editor requires the valueTemplate or constantValue property to be set.";
        }
    },
    destroyImpl: function() {
        this.template = null;
    },
    setValueImpl: function(val) {
    },
    onWatchedFieldChange: function() {
        if (this.derived_mode == 'template') {
            var vars = this.getWatchedFieldValues();
            this.value = this.template(vars);
            this.onChange();
        }
    }
});
