JSONEditor.defaults.editors.select = JSONEditor.AbstractEditor.extend({
  setValueImpl: function(value) {
    value = this.typecast(value||'');

    // Sanitize value before setting it
    var sanitized = value;
    if(this.enum_values.indexOf(sanitized) < 0) {
      sanitized = this.enum_values[0];
    }
    
    if(this.value === sanitized) {
      return;
    }

    this.input.value = this.enum_options[this.enum_values.indexOf(sanitized)];
    this.value = sanitized;
    
    this.onChange();
  },
  typecast: function(value) {
    if(this.schema.type === "boolean") {
      return !!value;
    }
    else if(this.schema.type === "number") {
      return 1*value;
    }
    else if(this.schema.type === "integer") {
      return Math.floor(value*1);
    }
    else {
      return ""+value;
    }
  },
  buildImpl: function() {
    var self = this;
    this.enum_options = [];
    this.enum_values = [];
    this.enum_display = [];

    // Enum options enumerated
    if(this.schema.enum) {
      var display = this.schema.options && this.schema.options.enum_titles || [];
      
      $utils.each(this.schema.enum,function(i,option) {
        self.enum_options[i] = ""+option;
        self.enum_display[i] = ""+(display[i] || option);
        self.enum_values[i] = self.typecast(option);
      });
    }
    // Boolean
    else if(this.schema.type === "boolean") {
      self.enum_display = ['true','false'];
      self.enum_options = ['1',''];
      self.enum_values = [true,false];
    }
    // Dynamic Enum
    else if(this.schema.enumSource) {
      this.enum_source_template = this.jsoneditor.compileTemplate(this.schema.enumSource.sourceTemplate, this.template_engine);
      this.enum_value_template = this.jsoneditor.compileTemplate(this.schema.enumSource.value, this.template_engine);
      this.enum_title_template = this.jsoneditor.compileTemplate(this.schema.enumSource.title, this.template_engine);
    }
    // Other, not supported
    else {
      throw "'select' editor requires the enum property to be set.";
    }
    
    if(!this.options.compact) this.header = this.label = this.theme.getFormInputLabel(this.getTitle());
    if(this.schema.description) this.description = this.theme.getFormInputDescription(this.schema.description);

    if(this.options.compact) this.container.className += ' compact';

    this.input = this.theme.getSelectInput(this.enum_options);
    this.theme.setSelectOptions(this.input,this.enum_options,this.enum_display);
    this.input.setAttribute('name',this.formname);

    if(this.schema.readOnly || this.schema.readonly) {
      this.input.disabled = true;
    }

    this.input.addEventListener('change',function(e) {
      e.preventDefault();
      e.stopPropagation();
      self.onInputChange();
    });

    this.control = this.theme.getFormControl(this.label, this.input, this.description);
    this.container.appendChild(this.control);

    this.value = this.enum_values[0];
    
    this.theme.afterInputReady(this.input);
  },
  onInputChange: function() {
    var self = this;
    self.withProcessingContext(function() {
      var val = self.input.value;

      var sanitized = val;
      if(self.enum_options.indexOf(val) === -1) {
        sanitized = self.enum_options[0];
      }

      self.value = self.enum_values[self.enum_options.indexOf(val)];

      self.onChange();
    }, 'input_change');
  },
  onWatchedFieldChange: function() {
    // If this editor uses a dynamic select box
    if (this.enum_source_template) {
      var vars = this.getWatchedFieldValues();
      
      var items = this.enum_source_template(vars);
      
      var select_options = [];
      var select_titles = [];
      for (var j = 0; j < items.length; j++) {
        var item_vars = {watch_vars: vars, i: j, item: items[j]};
        select_options[j] = this.enum_value_template(item_vars);
        select_titles[j] = this.enum_title_template(item_vars);
      }
      
      var prev_value = this.value;
      
      this.theme.setSelectOptions(this.input, select_options, select_titles);
      this.enum_options = select_options;
      this.enum_display = select_titles;
      this.enum_values = select_options;
      
      // If the previous value is still in the new select options, stick with it
      if(select_options.indexOf(prev_value) !== -1) {
        this.input.value = prev_value;
        this.value = prev_value;
      }
      // Otherwise, set the value to the first select option
      else {
        this.input.value = select_options[0];
        this.value = select_options[0] || "";
        this.onChange();
      }
    }
  },
  destroyImpl: function() {
    if(this.label && this.label.parentNode) this.label.parentNode.removeChild(this.label);
    if(this.description && this.description.parentNode) this.description.parentNode.removeChild(this.description);
    if(this.input && this.input.parentNode) this.input.parentNode.removeChild(this.input);
  }
});
