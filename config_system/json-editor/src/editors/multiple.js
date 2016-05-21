// Multiple Editor (for when `type` is an array)
JSONEditor.defaults.editors.multiple = JSONEditor.AbstractEditor.extend({
  switchEditor: function(i) {
    var self = this;
    
    if (self.editor && self.type !== i) {
      self.editor.destroy();
      self.editor = null;
    }
    
    if (!self.editor) {
      self.buildChildEditor(i);
    }
    
    self.type = i;

    self.editor.setValue(self.value);
    
    self.refreshValue();
  },
  buildChildEditor: function(i) {
    var self = this;
    
    var holder = self.theme.getChildEditorHolder();
    self.editor_holder.appendChild(holder);

    var schema = self.child_schemas[i];

    var editor_class = self.jsoneditor.getEditorClass(schema);

    self.editor = self.jsoneditor.createEditor(editor_class,{
      jsoneditor: self.jsoneditor,
      schema: schema,
      container: holder,
      path: self.path,
      parent: self,
      no_label: true
    });
    self.editor.build();
  },
  buildImpl: function() {
    var self = this;
    
    // basic checks
    if (!Array.isArray($utils.get(this.schema, 'oneOf'))) {
      throw "'multiple' editor requires an array 'oneOf'";
    }
    if (this.schema.oneOf.length === 0) {
      throw "'multiple' editor requires non-empty 'oneOf'";
    }
    if (!$utils.has(this.schema, 'selectKey')) {
      throw "'multiple' editor requires 'selectKey'";
    }

    this.child_schemas = this.schema.oneOf;
    this.select_key = this.schema.selectKey;
    this.type = 0;
    this.editor = null;
    this.value = null;
    this.select_values = [];
    
    var select_titles = [];
    var select_indices = [];
    
    $utils.each(this.child_schemas, function(i, schema) {
      var selectValue = $utils.getNested(schema, 'properties', self.select_key, 'constantValue');
      if ($utils.isUndefined(selectValue)) {
        self.debugPrint(schema);
        throw "'multiple' editor requires each 'oneOf' schema to have properties.(selectKey).constantValue";
      }
      self.select_values.push(selectValue);
      select_indices.push(i);
      var title = $utils.has(schema, 'title') ? schema.title : selectValue;
      select_titles.push(title);
    });
    
    var container = this.container;

    this.header = this.label = this.theme.getFormInputLabel(this.getTitle());
    this.container.appendChild(this.header);

    this.switcher = this.theme.getSwitcher(select_indices);
    this.theme.setSwitcherOptions(this.switcher, select_indices, select_titles);
    container.appendChild(this.switcher);
    this.switcher.addEventListener('change',function(e) {
      e.preventDefault();
      e.stopPropagation();
      
      self.withProcessingContext(function() {
        self.switchEditor(self.switcher.value);
        self.onChange();
      }, 'switcher_input');
    });
    this.switcher.style.marginBottom = 0;
    this.switcher.style.width = 'auto';
    this.switcher.style.display = 'inline-block';
    this.switcher.style.marginLeft = '5px';

    this.editor_holder = document.createElement('div');
    container.appendChild(this.editor_holder);

    this.switchEditor(0);
  },
  onChildEditorChange: function(editor) {
    this.refreshValue();
    this.onChange();
  },
  refreshValue: function() {
    this.value = this.editor.getValue();
  },
  setValueImpl: function(val) {
    var self = this;
    
    // Determine the type by looking at the value of the selectKey property.
    var type = 0;
    if ($utils.isObject(val) && $utils.has(val, self.select_key)) {
      var the_select_value = val[self.select_key];
      $utils.each(self.select_values, function(i, select_value) {
        if (select_value === the_select_value) {
          type = i;
          return false;
        }
      });
    }
    
    self.switcher.value = type;
    
    this.value = val;
    this.switchEditor(type);

    self.onChange();
  },
  destroyImpl: function() {
    if (this.editor) {
      this.editor.destroy();
    }
    if(this.editor_holder && this.editor_holder.parentNode) this.editor_holder.parentNode.removeChild(this.editor_holder);
    if(this.switcher && this.switcher.parentNode) this.switcher.parentNode.removeChild(this.switcher);
  }
});
