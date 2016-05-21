JSONEditor.defaults.editors.table = JSONEditor.defaults.editors.array.extend({
  arrayBuildImpl: function() {
    var self = this;
    
    var item_schema = this.schema.items || {};
    
    this.item_title = item_schema.title || 'row';
    this.item_default = item_schema.default || null;
    this.item_has_child_editors = item_schema.properties;
    
    this.table = this.theme.getTable();
    this.container.appendChild(this.table);
    this.thead = this.theme.getTableHead();
    this.table.appendChild(this.thead);
    this.header_row = this.theme.getTableRow();
    this.thead.appendChild(this.header_row);
    this.row_holder = this.theme.getTableBody();
    this.table.appendChild(this.row_holder);

    if(!this.options.compact) {
      this.title = this.theme.getHeader(this.getTitle());
      this.container.appendChild(this.title);
      this.title_controls = this.theme.getHeaderButtonHolder();
      this.title.appendChild(this.title_controls);
      if(this.schema.description) {
        this.description = this.theme.getDescription(this.schema.description);
        this.container.appendChild(this.description);
      }
      this.panel = this.theme.getIndentedPanel();
      this.container.appendChild(this.panel);
    }
    else {
      this.panel = document.createElement('div');
      this.container.appendChild(this.panel);
    }

    this.panel.appendChild(this.table);
    this.controls = this.theme.getButtonHolder();
    this.panel.appendChild(this.controls);

    if(this.item_has_child_editors) {
      var ordered = $utils.orderProperties(item_schema.properties, function(i) { return item_schema.properties[i].propertyOrder; });
      $utils.each(ordered, function(index, prop_name) {
        var prop_schema = item_schema.properties[prop_name];
        var title = prop_schema.title ? prop_schema.title : prop_name;
        var th = self.theme.getTableHeaderCell(title);
        if (typeof prop_schema.options !== 'undefined' && prop_schema.options.hidden) {
          th.style.display = 'none';
        }
        self.header_row.appendChild(th);
      });
    }
    else {
      self.header_row.appendChild(self.theme.getTableHeaderCell(this.item_title));
    }

    this.row_holder.innerHTML = '';

    // Row Controls column
    this.controls_header_cell = self.theme.getTableHeaderCell(" ");
    self.header_row.appendChild(this.controls_header_cell);
  },
  arrayDestroyImpl: function() {
    if(this.table && this.table.parentNode) this.table.parentNode.removeChild(this.table);
    this.table = null;
  },
  getItemTitle: function() {
    return this.item_title;
  },
  getElementEditor: function(i) {
    var schema = this.schema.items;
    var editor = this.jsoneditor.getEditorClass(schema);
    var row = this.row_holder.appendChild(this.theme.getTableRow());
    var holder = row;
    if(!this.item_has_child_editors) {
      holder = this.theme.getTableCell();
      row.appendChild(holder);
    }

    var ret = this.jsoneditor.createEditor(editor,{
      jsoneditor: this.jsoneditor,
      schema: schema,
      container: holder,
      path: this.path+'.'+i,
      parent: this,
      compact: true,
      table_row: true
    });
    ret.build();
    
    ret.controls_cell = row.appendChild(this.theme.getTableCell());
    ret.row = row;
    ret.table_controls = this.theme.getButtonHolder();
    ret.controls_cell.appendChild(ret.table_controls);
    ret.table_controls.style.margin = 0;
    ret.table_controls.style.padding = 0;
    
    return ret;
  },
  destroyRow: function(row) {
    var holder = row.container;
    if(!this.item_has_child_editors) {
      row.row.parentNode.removeChild(row.row);
    }
    row.destroy();
    if (holder.parentNode) {
      holder.parentNode.removeChild(holder);
    }
  },
  refreshButtonsExtraEditor: function(i, editor) {
    return !!editor.moveup_button;
  },
  refreshButtonsExtra: function(need_row_buttons, controls_needed) {
    // Show/hide controls column in table
    $utils.each(this.rows,function(i,editor) {
      editor.controls_cell.style.display = need_row_buttons ? '' : 'none';
    });
    this.controls_header_cell.style.display = need_row_buttons ? '' : 'none';
    this.table.style.display = this.value.length === 0 ? 'none' : '';
    this.controls.style.display = controls_needed ? '' : 'none';
  },
  addRow: function(value) {
    this.addRowBase(value, false, function(row) { return row.table_controls; });
  },
  toggleSetup: function() {
  },
  toggleHandlerExtra: function(expanding) {
  }
});
