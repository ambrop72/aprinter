var JSONEditor = function(element,options) {
  this.element = element;
  this.options = $utils.extend(JSONEditor.defaults.options, options || {});
  this.init();
};
JSONEditor.prototype = {
  init: function() {
    var self = this;
    
    this.destroyed = false;
    this.firing_change = false;
    this.callbacks = {};
    this.editors = {};
    this.watchlist = {};
    
    var theme_class = JSONEditor.defaults.themes[this.options.theme || JSONEditor.defaults.theme];
    if(!theme_class) throw "Unknown theme " + (this.options.theme || JSONEditor.defaults.theme);
    
    this.schema = this.options.schema;
    this.theme = new theme_class();
    this.template = this.options.template;
    
    var icon_class = JSONEditor.defaults.iconlibs[this.options.iconlib || JSONEditor.defaults.iconlib];
    if(icon_class) this.iconlib = new icon_class();

    this.root_container = this.theme.getContainer();
    this.element.appendChild(this.root_container);
    
    // Create the root editor
    var editor_class = self.getEditorClass(self.schema);
    self.root = self.createEditor(editor_class, {
      jsoneditor: self,
      schema: self.schema,
      container: self.root_container
    });
    self.root.build();

    // Starting data
    if(self.options.startval) self.root.setValue(self.options.startval);

    // Schedule a change event.
    self._scheduleChange();
  },
  getValue: function() {
    if(this.destroyed) throw "JSON Editor destroyed";

    return this.root.getFinalValue();
  },
  setValue: function(value) {
    if(this.destroyed) throw "JSON Editor destroyed";
    
    this.root.setValue(value);
  },
  destroy: function() {
    if(this.destroyed) return;
    
    this.schema = null;
    this.options = null;
    this.root.destroy();
    this.root = null;
    this.root_container = null;
    this.theme = null;
    this.iconlib = null;
    this.template = null;
    this.element.innerHTML = '';
    
    this.destroyed = true;
  },
  on: function(event, callback) {
    this.callbacks[event] = this.callbacks[event] || [];
    this.callbacks[event].push(callback);
  },
  off: function(event, callback) {
    if (this.callbacks[event]) {
      var newcallbacks = [];
      for(var i=0; i<this.callbacks[event].length; i++) {
        if(this.callbacks[event][i]===callback) continue;
        newcallbacks.push(this.callbacks[event][i]);
      }
      this.callbacks[event] = newcallbacks;
    }
  },
  trigger: function(event) {
    if(this.callbacks[event]) {
      var cbs = this.callbacks[event].slice();
      for(var i=0; i<cbs.length; i++) {
        cbs[i]();
      }
    }
  },
  getEditorClass: function(schema) {
    var classname;

    $utils.each(JSONEditor.defaults.resolvers,function(i,resolver) {
      var tmp = resolver(schema);
      if(tmp && JSONEditor.defaults.editors[tmp]) {
        classname = tmp;
        return false;
      }
    });

    if(!classname) throw "Unknown editor for schema "+JSON.stringify(schema);

    return JSONEditor.defaults.editors[classname];
  },
  createEditor: function(editor_class, options) {
    if (editor_class.options) {
      options = $utils.extend(editor_class.options, options);
    }
    return new editor_class(options);
  },
  _scheduleChange: function() {
    var self = this;
    
    if (self.firing_change) {
      return;
    }
    self.firing_change = true;
    
    window.setTimeout(function() {
      if(self.destroyed) return;
      self.firing_change = false;
      self.trigger('change');
    }, 0);
  },
  onChange: function() {
    if (this.destroyed) {
      return;
    }
    this._scheduleChange();
  },
  compileTemplate: function(template, name) {
    name = name || JSONEditor.defaults.template;

    var engine;

    // Specifying a preset engine
    if(typeof name === 'string') {
      if(!JSONEditor.defaults.templates[name]) throw "Unknown template engine "+name;
      engine = JSONEditor.defaults.templates[name]();

      if(!engine) throw "Template engine "+name+" missing required library.";
    }
    // Specifying a custom engine
    else {
      engine = name;
    }

    if(!engine) throw "No template engine set";
    if(!engine.compile) throw "Invalid template engine set";

    return engine.compile(template);
  },
  registerEditor: function(editor) {
    this.editors[editor.path] = editor;
  },
  unregisterEditor: function(editor) {
    this.editors[editor.path] = null;
  },
  getEditor: function(path) {
    if(!this.editors) return;
    return this.editors[path];
  },
  doWatch: function(path,callback) {
    this.watchlist[path] = this.watchlist[path] || [];
    this.watchlist[path].push(callback);
  },
  doUnwatch: function(path,callback) {
    if(!this.watchlist[path]) return;
    var newlist = [];
    for(var i=0; i<this.watchlist[path].length; i++) {
      if(this.watchlist[path][i] === callback) continue;
      else newlist.push(this.watchlist[path][i]);
    }
    this.watchlist[path] = newlist.length? newlist : null;
  },
  notifyWatchers: function(path) {
    if(!this.watchlist[path]) return;
    for(var i=0; i<this.watchlist[path].length; i++) {
      this.watchlist[path][i]();
    }
  }
};

JSONEditor.defaults = {
  themes: {},
  templates: {},
  iconlibs: {},
  editors: {},
  resolvers: [],
};
