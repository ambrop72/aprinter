JSONEditor.defaults.editors.number = JSONEditor.defaults.editors.string.extend({
  sanitize: function(value) {
    return (value+"").replace(/[^0-9\.\-eE]/g,'');
  },
  getValue: function() {
    return this.value*1;
  }
});
