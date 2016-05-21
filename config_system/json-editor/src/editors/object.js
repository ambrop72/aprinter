JSONEditor.defaults.editors.object = JSONEditor.AbstractEditor.extend({
  getDefault: function() {
    return this.schema.default || {};
  },
  buildImpl: function() {
    var self = this;

    // Get the properties from the schema.
    this.schema_properties = this.schema.properties || {};
    
    // Built a list of properties sorted by processingOrder.
    var order_func = function(name) {
      var processingOrder = self.schema_properties[name].processingOrder;
      return typeof processingOrder === 'number' ? processingOrder : 0;
    };
    this.processing_order = Object.keys(self.schema_properties);
    this.processing_order.sort(function(a, b) {
      return order_func(a) - order_func(b);
    });

    // If the object should be rendered as a table row
    if(this.options.table_row) {
      this.editor_holder = this.container;
    }
    // If the object should be rendered as a div
    else {
      // Text label
      this.header = document.createElement('span');
      this.header.textContent = this.getTitle();
      
      // Container for the entire header row.
      this.title = this.theme.getHeader(this.header);
      this.container.appendChild(this.title);
      this.container.style.position = 'relative';
      
      // Description
      if(this.schema.description) {
        this.description = this.theme.getDescription(this.schema.description);
        this.container.appendChild(this.description);
      }
      
      // Container for child editor area
      this.editor_holder = this.theme.getIndentedPanel();
      this.editor_holder.style.paddingBottom = '0';
      this.container.appendChild(this.editor_holder);

      // Container for rows of child editors
      this.row_container = this.theme.getGridContainer();
      this.editor_holder.appendChild(this.row_container);
      
      // The div with editors.
      this.editors_div = document.createElement('div');
      this.row_container.appendChild(this.editors_div);

      // Control buttons
      this.title_controls = this.theme.getHeaderButtonHolder();
      this.title.appendChild(this.title_controls);

      // Show/Hide button
      this.collapsed = false;
      this.toggle_button = this.getButton('','collapse','Collapse');
      this.title_controls.appendChild(this.toggle_button);
      this.toggle_button.addEventListener('click',function(e) {
        e.preventDefault();
        e.stopPropagation();
        self.toggleCollapsed();
      });

      // If it should start collapsed
      if(this.options.collapsed) {
        this.toggleCollapsed();
      }
      
      // Disable controls as needed.
      var label_hidden = !!this.options.no_label;
      var collapse_hidden = this.jsoneditor.options.disable_collapse ||
        (this.schema.options && typeof this.schema.options.disable_collapse !== "undefined");
      if (this.options.no_header || (label_hidden && collapse_hidden)) {
        this.title.style.display = 'none';
      } else {
        if (label_hidden) {
          this.header.style.display = 'none';
        }
        if (collapse_hidden) {
          this.title_controls.style.display = 'none';
        }
      }
    }
    
    // Create editors.
    this.editors = {};
    $utils.each(this.processing_order, function(i, name) {
      var holder;
      var extra_opts = {};
      if (self.options.table_row) {
        holder = self.theme.getTableCell();
        extra_opts.compact = true;
      } else {
        holder = self.theme.getChildEditorHolder();
      }
      
      var schema = self.schema_properties[name];
      var editor = self.jsoneditor.getEditorClass(schema);
      self.editors[name] = self.jsoneditor.createEditor(editor, $utils.extend({
        jsoneditor: self.jsoneditor,
        schema: schema,
        path: self.path+'.'+name,
        parent: self,
        container: holder
      }, extra_opts));
      self.editors[name].build();
    });
    
    // Compute the display order.
    var display_order = $utils.orderProperties(self.editors, function(i) { return self.editors[i].schema.propertyOrder; });
    
    // Display the editors.
    $utils.each(display_order, function(i, name) {
      var editor = self.editors[name];
      var holder = editor.container;
      if (editor.options.hidden) {
        holder.style.display = 'none';
      }
      if (self.options.table_row) {
        self.editor_holder.appendChild(holder);
      } else {
        var row = self.theme.getGridRow();
        self.editors_div.appendChild(row);
        if (!editor.options.hidden) {
          self.theme.setGridColumnSize(holder, 12);
        }
        editor.container.className += ' container-' + name;
        row.appendChild(holder);
      }
    });
    
    // Set the initial value.
    this.setValueImpl({});
  },
  onChildEditorChange: function(editor) {
    this.refreshValue();
    this.onChange();
  },
  destroyImpl: function() {
    $utils.each(this.editors, function(i,el) {
      el.destroy();
    });
    if(this.editor_holder) this.editor_holder.innerHTML = '';
    if(this.title && this.title.parentNode) this.title.parentNode.removeChild(this.title);

    this.editors = null;
    if(this.editor_holder && this.editor_holder.parentNode) this.editor_holder.parentNode.removeChild(this.editor_holder);
    this.editor_holder = null;
  },
  getFinalValue: function() {
    var result = {};
    for (var i in this.editors) {
      if (!this.editors.hasOwnProperty(i)) {
        continue;
      }
      if (!this.editors[i].schema.excludeFromFinalValue) {
        result[i] = this.editors[i].getFinalValue();
      }
    }
    return result;
  },
  refreshValue: function() {
    this.value = {};
    for(var i in this.editors) {
      if(!this.editors.hasOwnProperty(i)) continue;
      this.value[i] = this.editors[i].getValue();
    }
  },
  setValueImpl: function(value) {
    var self = this;
    
    value = value || {};
    if (typeof value !== "object" || Array.isArray(value)) {
      value = {};
    }
    
    // Set the editor values.
    $utils.each(this.processing_order, function(i, name) {
      var prop_value = $utils.has(value, name) ? value[name] : self.editors[name].getDefault();
      self.editors[name].setValue(prop_value);
    });

    this.refreshValue();
    this.onChange();
  },
  toggleCollapsed: function() {
    var self = this;
    self.collapsed = !self.collapsed;
    if(self.collapsed) {
      self.editor_holder.style.display = 'none';
      self.setButtonText(self.toggle_button,'','expand','Expand');
    }
    else {
      self.editor_holder.style.display = '';
      self.setButtonText(self.toggle_button,'','collapse','Collapse');
    }
  }
});
