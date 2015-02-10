/*! JSON Editor v0.7.10 - JSON Schema -> HTML Editor
 * By Jeremy Dorn - https://github.com/jdorn/json-editor/
 * Released under the MIT license
 *
 * Date: 2014-09-15
 */

/**
 * See README.md for requirements and usage info
 */

(function() {

/*jshint loopfunc: true */
/* Simple JavaScript Inheritance
 * By John Resig http://ejohn.org/
 * MIT Licensed.
 */
// Inspired by base2 and Prototype
var Class;
(function(){
  var initializing = false, fnTest = /xyz/.test(function(){window.postMessage("xyz");}) ? /\b_super\b/ : /.*/;
 
  // The base Class implementation (does nothing)
  Class = function(){};
 
  // Create a new Class that inherits from this class
  Class.extend = function(prop) {
    var _super = this.prototype;
   
    // Instantiate a base class (but only create the instance,
    // don't run the init constructor)
    initializing = true;
    var prototype = new this();
    initializing = false;
   
    // Copy the properties over onto the new prototype
    for (var name in prop) {
      // Check if we're overwriting an existing function
      prototype[name] = typeof prop[name] == "function" &&
        typeof _super[name] == "function" && fnTest.test(prop[name]) ?
        (function(name, fn){
          return function() {
            var tmp = this._super;
           
            // Add a new ._super() method that is the same method
            // but on the super-class
            this._super = _super[name];
           
            // The method only need to be bound temporarily, so we
            // remove it when we're done executing
            var ret = fn.apply(this, arguments);        
            this._super = tmp;
           
            return ret;
          };
        })(name, prop[name]) :
        prop[name];
    }
   
    // The dummy class constructor
    function Class() {
      // All construction is actually done in the init method
      if ( !initializing && this.init )
        this.init.apply(this, arguments);
    }
   
    // Populate our constructed prototype object
    Class.prototype = prototype;
   
    // Enforce the constructor to be what we expect
    Class.prototype.constructor = Class;
 
    // And make this class extendable
    Class.extend = arguments.callee;
   
    return Class;
  };
  
  return Class;
})();

// Array.isArray polyfill
// From MDN
(function() {
	if(!Array.isArray) {
	  Array.isArray = function(arg) {
		return Object.prototype.toString.call(arg) === '[object Array]';
	  };
	}
}());
var $utils = {

isplainobject: function( obj ) {
  // Not own constructor property must be Object
  if ( obj.constructor &&
    !obj.hasOwnProperty('constructor') &&
    !obj.constructor.prototype.hasOwnProperty('isPrototypeOf')) {
    return false;
  }

  // Own properties are enumerated firstly, so to speed up,
  // if last one is own, then all properties are own.

  var key;
  for ( key in obj ) {}

  return key === undefined || obj.hasOwnProperty(key);
},

shallowCopy: function(obj) {
  var res = {};
  for (var property in obj) {
    if (!obj.hasOwnProperty(property)) {
      continue;
    }
    res[property] = obj[property];
  }
  return res;
},

isEmpty: function(obj) {
  for (var property in obj) {
    if (obj.hasOwnProperty(property)) {
      return false;
    }
  }
  return true;
},

extendExt: function(obj, sources, sources_start) {
  // optimization for empty obj
  if ($utils.isEmpty(obj) && sources.length > sources_start) {
    return $utils.extendExt(sources[sources_start], sources, sources_start + 1);
  }
  
  // we'll make a copy of obj the first time we need to change it
  var copied = false;
  
  for (var i = sources_start; i < sources.length; i++) {
    var source = sources[i];
    
    for (var property in source) {
      if (!source.hasOwnProperty(property)) {
        continue;
      }
      
      // compute new value of this property
      var new_value;
      if (obj.hasOwnProperty(property) && $utils.isplainobject(obj[property]) && $utils.isplainobject(source[property])) {
        new_value = $utils.extendExt(obj[property], [source[property]], 0);
      } else {
        new_value = source[property];
      }
      
      // possibly do the assigment of the new value to the old value
      if (!obj.hasOwnProperty(property) || new_value !== obj[property]) {
        if (!copied) {
          obj = $utils.shallowCopy(obj);
          copied = true;
        }
        obj[property] = new_value;
      }
    }
  }
  
  return obj;
},

extend: function(obj) {
  return $utils.extendExt(obj, arguments, 1);
},

each: function(obj,callback) {
  if(!obj) return;
  var i;
  if(Array.isArray(obj)) {
    for(i=0; i<obj.length; i++) {
      if(callback(i,obj[i])===false) return;
    }
  }
  else {
    for(i in obj) {
      if(!obj.hasOwnProperty(i)) continue;
      if(callback(i,obj[i])===false) return;
    }
  }
},

has: function(obj, property) {
  return obj.hasOwnProperty(property);
},

get: function(obj, property) {
  return obj.hasOwnProperty(property) ? obj[property] : undefined;
},

getNested: function(obj) {
  for (var i = 1; i < arguments.length; i++) {
    if (!obj.hasOwnProperty(arguments[i])) {
      return undefined;
    }
    obj = obj[arguments[i]];
  }
  return obj;
},

isUndefined: function(x) {
  return (typeof x === 'undefined');
},

isObject: function(x) {
  return (x !== null && typeof x === 'object');
},

orderProperties: function(obj, get_order_func) {
  var property_order = Object.keys(obj);
  property_order = property_order.sort(function(a,b) {
    var ordera = get_order_func(a);
    var orderb = get_order_func(b);
    if(typeof ordera !== "number") ordera = 1000;
    if(typeof orderb !== "number") orderb = 1000;
    return ordera - orderb;
  });
  return property_order;
}

};

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

/**
 * All editors should extend from this class
 */
JSONEditor.AbstractEditor = Class.extend({
  onChildEditorChange: function(editor) {
    this.onChange();
  },
  isAnyParentProcessing: function() {
    for (var editor = this; editor; editor = editor.parent) {
      if (editor.processing_active) {
        return true;
      }
    }
    return false;
  },
  onChange: function() {
    console.assert(this.processing_active, "onChange called outside of processing context??");
    
    this.processing_dirty = true;
  },
  init: function(options) {
    this.jsoneditor = options.jsoneditor;
    
    this.theme = this.jsoneditor.theme;
    this.template_engine = this.jsoneditor.template;
    this.iconlib = this.jsoneditor.iconlib;
    this.schema = options.schema;
    this.options = $utils.extend((options.schema.options || {}), options);
    this.path = options.path || 'root';
    this.formname = options.formname || this.path.replace(/\.([^.]+)/g,'[$1]');
    if(this.jsoneditor.options.form_name_root) this.formname = this.formname.replace(/^root\[/,this.jsoneditor.options.form_name_root+'[');
    this.key = this.path.split('.').pop();
    this.parent = options.parent;
    this.watch_id = this.schema.id || null;
    this.container = options.container || null;
    this.header_template = null;
    if(this.schema.headerTemplate) {
      this.header_template = this.jsoneditor.compileTemplate(this.schema.headerTemplate, this.template_engine);
    }
    
    this.processing_active = false;
    this.processing_dirty = false;
    this.processing_watch_dirty = false;
  },
  debugPrint: function(msg) {
    console.log(this.path + ': ' + msg);
  },
  build: function() {
    var self = this;
    self.withProcessingContext(function() {
      self.buildImpl();
      self.jsoneditor.registerEditor(self);
      self.setupWatchListeners();
      self.updateHeaderText();
      self.onChange();
    }, 'build');
  },
  setupWatchListeners: function() {
    var self = this;
    
    // Watched fields
    this.watched = {};
    this.watched_values = {};
    this.watch_listener = function() {
      self.withProcessingContext(function() {
        self.processing_watch_dirty = true;
      }, 'watch_listener');
    };
    
    if(this.schema.hasOwnProperty('watch')) {
      for(var name in this.schema.watch) {
        if(!this.schema.watch.hasOwnProperty(name)) continue;
        var path = this.schema.watch[name];

        // Get the ID of the first node and the path within.
        if (typeof path != 'string') throw "Watch path must be a string";
        var path_parts = path.split('.');
        var first_id = path_parts.shift();
        
        // Look for the first node in the ancestor nodes.
        var first_node = null;
        for (var node = self.parent; node; node = node.parent) {
          if (node.watch_id == first_id) {
            first_node = node;
            break;
          }
        }
        if (!first_node) {
          throw "Could not find ancestor node with id " + first_id;
        }
        
        // Construct the final watch path.
        var adjusted_path = first_node.path + (path_parts.length === 0 ? '' : ('.' + path_parts.join('.')));
        
        self.jsoneditor.doWatch(adjusted_path, self.watch_listener);
        
        self.watched[name] = adjusted_path;
      }
    }
  },
  getButton: function(text, icon, title) {
    var btnClass = 'json-editor-btn-'+icon;
    if(!this.iconlib) icon = null;
    else icon = this.iconlib.getIcon(icon);
    
    if(!icon && title) {
      text = title;
      title = null;
    }
    
    var btn = this.theme.getButton(text, icon, title);
    btn.className += ' ' + btnClass + ' ';
    return btn;
  },
  setButtonText: function(button, text, icon, title) {
    if(!this.iconlib) icon = null;
    else icon = this.iconlib.getIcon(icon);
    
    if(!icon && title) {
      text = title;
      title = null;
    }
    
    return this.theme.setButtonText(button, text, icon, title);
  },
  refreshWatchedFieldValues: function() {
    var watched = {};
    var changed = false;
    var self = this;
    
    var val,editor;
    for(var name in this.watched) {
      if(!this.watched.hasOwnProperty(name)) continue;
      editor = self.jsoneditor.getEditor(this.watched[name]);
      val = editor? editor.getValue() : null;
      if(self.watched_values[name] !== val) changed = true;
      watched[name] = val;
    }
    
    watched.self = this.getValue();
    if(this.watched_values.self !== watched.self) changed = true;
    
    this.watched_values = watched;
    
    return changed;
  },
  getWatchedFieldValues: function() {
    return this.watched_values;
  },
  updateHeaderText: function() {
    if(this.header) {
      this.header.textContent = this.getHeaderText();
    }
  },
  getHeaderText: function(title_only) {
    if(this.header_text) return this.header_text;
    else if(title_only) return this.schema.title;
    else return this.getTitle();
  },
  onWatchedFieldChange: function() {
  },
  _onWatchedFieldChange: function() {
    this.onWatchedFieldChange();
    
    var vars;
    if(this.header_template) {      
      vars = $utils.extend(this.getWatchedFieldValues(), {
        key: this.key,
        i: this.key,
        i0: (this.key*1),
        i1: (this.key*1+1),
        title: this.getTitle()
      });
      var header_text = this.header_template(vars);
      
      if(header_text !== this.header_text) {
        this.header_text = header_text;
        this.updateHeaderText();
        this.onChange();
      }
    }
  },
  withProcessingContext: function(func, name) {
    var self = this;
    console.assert(!self.processing_active, "already processing??");
    
    var tpro = !!self.jsoneditor.options.trace_processing;
    if (tpro) {
      if (typeof name === 'undefined') {
        name = 'unknown_context';
      }
      self.debugPrint(name + ' ' + '{');
    }
    
    self.processing_active = true;
    self.processing_dirty = false;
    self.processing_watch_dirty = false;
    
    func();
    
    if (self.processing_dirty || self.processing_watch_dirty) {
      if (self.refreshWatchedFieldValues()) {
        self._onWatchedFieldChange();
      }
    }
    
    self.processing_active = false;    
    
    if (tpro) {
      self.debugPrint('}');
    }
    
    if (self.processing_dirty) {
      self.jsoneditor.notifyWatchers(self.path);
      
      if (!self.isAnyParentProcessing()) {
        if (self.parent) {
          self.parent.withProcessingContext(function() {
            self.parent.onChildEditorChange(self);
          }, 'child_editor_change');
        } else {
          self.jsoneditor.onChange();
        }
      }
    }
  },
  setValue: function(value) {
    var self = this;
    self.withProcessingContext(function() {
      self.setValueImpl(value);
    }, 'setValue');
  },
  setValueImpl: function(value) {
    this.value = value;
    this.onChange();
  },
  getValue: function() {
    return this.value;
  },
  getFinalValue: function() {
    return this.getValue();
  },
  destroyImpl: function() {
  },
  destroy: function() {
    var self = this;
    self.destroyImpl();
    $utils.each(this.watched,function(name,adjusted_path) {
      self.jsoneditor.doUnwatch(adjusted_path,self.watch_listener);
    });
    this.jsoneditor.unregisterEditor(this);
    this.watched = null;
    this.watched_values = null;
    this.watch_listener = null;
    this.header_text = null;
    this.header_template = null;
    this.value = null;
    if(this.container && this.container.parentNode) this.container.parentNode.removeChild(this.container);
    this.container = null;
    this.jsoneditor = null;
    this.schema = null;
    this.path = null;
    this.key = null;
    this.parent = null;
  },
  getDefault: function() {
    if(this.schema.default) return this.schema.default;
    if(this.schema.enum) return this.schema.enum[0];
    
    var type = this.schema.type || this.schema.oneOf;
    if(type && Array.isArray(type)) type = type[0];
    if(type && typeof type === "object") type = type.type;
    if(type && Array.isArray(type)) type = type[0];
    
    if(typeof type === "string") {
      if(type === "number") return 0.0;
      if(type === "boolean") return false;
      if(type === "integer") return 0;
      if(type === "string") return "";
      if(type === "object") return {};
      if(type === "array") return [];
    }
    
    return null;
  },
  getTitle: function() {
    return this.schema.title || this.key;
  }
});

JSONEditor.defaults.editors.string = JSONEditor.AbstractEditor.extend({
  setValueImpl: function(value) {
    var self = this;
    
    if(value === null) value = "";
    else if(typeof value === "object") value = JSON.stringify(value);
    else if(typeof value !== "string") value = ""+value;
    
    // Sanitize value before setting it
    var sanitized = this.sanitize(value);

    if(this.input.value === sanitized) {
      return;
    }

    this.input.value = sanitized;
    
    this.refreshValue();
    
    this.onChange();
  },
  buildImpl: function() {
    var self = this, i;
    
    if(!this.options.compact) this.header = this.label = this.theme.getFormInputLabel(this.getTitle());
    if(this.schema.description) this.description = this.theme.getFormInputDescription(this.schema.description);

    this.format = this.schema.format;
    if(!this.format && this.options.default_format) {
      this.format = this.options.default_format;
    }
    if(this.options.format) {
      this.format = this.options.format;
    }

    // Specific format
    if(this.format) {
      // Text Area
      if(this.format === 'textarea') {
        this.input = this.theme.getTextareaInput();
      }
      // Range Input
      else if(this.format === 'range') {
        var min = this.schema.minimum || 0;
        var max = this.schema.maximum || Math.max(100,min+1);
        var step = 1;
        if(this.schema.multipleOf) {
          if(min%this.schema.multipleOf) min = Math.ceil(min/this.schema.multipleOf)*this.schema.multipleOf;
          if(max%this.schema.multipleOf) max = Math.floor(max/this.schema.multipleOf)*this.schema.multipleOf;
          step = this.schema.multipleOf;
        }

        this.input = this.theme.getRangeInput(min,max,step);
      }
      // HTML5 Input type
      else {
        this.input = this.theme.getFormInputField(this.format);
      }
    }
    // Normal text input
    else {
      this.input = this.theme.getFormInputField('text');
    }
    
    this.input.setAttribute('name',this.formname);
    
    // minLength, maxLength, and pattern
    if(typeof this.schema.maxLength !== "undefined") this.input.setAttribute('maxlength',this.schema.maxLength);
    if(typeof this.schema.pattern !== "undefined") this.input.setAttribute('pattern',this.schema.pattern);
    else if(typeof this.schema.minLength !== "undefined") this.input.setAttribute('pattern','.{'+this.schema.minLength+',}');

    if(this.options.compact) {
      this.container.className += ' compact';
    }

    if(this.schema.readOnly || this.schema.readonly) {
      this.input.disabled = true;
    }

    this.input.addEventListener('change',function(e) {
        e.preventDefault();
        e.stopPropagation();
        
        var val = this.value;
        
        // sanitize value
        var sanitized = self.sanitize(val);
        if(val !== sanitized) {
          this.value = sanitized;
        }
        
        self.withProcessingContext(function() {
          self.refreshValue();
          self.onChange();
        }, 'input_change');
      });

    this.control = this.theme.getFormControl(this.label, this.input, this.description);
    this.container.appendChild(this.control);

    this.refreshValue();
    
    this.theme.afterInputReady(this.input);
  },
  refreshValue: function() {
    this.value = this.input.value;
    if(typeof this.value !== "string") this.value = '';
  },
  destroyImpl: function() {
    if(this.input && this.input.parentNode) this.input.parentNode.removeChild(this.input);
    if(this.label && this.label.parentNode) this.label.parentNode.removeChild(this.label);
    if(this.description && this.description.parentNode) this.description.parentNode.removeChild(this.description);
  },
  /**
   * This is overridden in derivative editors
   */
  sanitize: function(value) {
    return value;
  }
});

JSONEditor.defaults.editors.number = JSONEditor.defaults.editors.string.extend({
  sanitize: function(value) {
    return (value+"").replace(/[^0-9\.\-eE]/g,'');
  },
  getValue: function() {
    return this.value*1;
  }
});

JSONEditor.defaults.editors.integer = JSONEditor.defaults.editors.number.extend({
  sanitize: function(value) {
    value = value + "";
    return value.replace(/[^0-9\-]/g,'');
  }
});

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

JSONEditor.defaults.editors.array = JSONEditor.AbstractEditor.extend({
  getDefault: function() {
    return this.schema.default || [];
  },
  buildImpl: function() {
    this.rows = [];
    this.buttons_dirty = true;

    this.hide_delete_buttons = this.options.disable_array_delete || this.jsoneditor.options.disable_array_delete;
    this.hide_move_buttons = this.options.disable_array_reorder || this.jsoneditor.options.disable_array_reorder;
    this.hide_copy_buttons = this.options.hide_copy_buttons || this.jsoneditor.options.hide_copy_buttons;
    this.hide_add_button = this.options.disable_array_add || this.jsoneditor.options.disable_array_add;
    
    if ($utils.has(this.schema, 'copyTemplate')) {
      this.copy_template = this.jsoneditor.compileTemplate(this.schema.copyTemplate, this.template_engine);
    }
    
    this.arrayBuildImpl();

    this.addControls();
    this.setValueImpl([]);
  },
  destroyImpl: function() {
    this.arrayDestroyImpl();
    
    this.empty();
    this.rows = null;
  },
  arrayBuildImpl: function() {
    var self = this;
    
    if(!this.options.compact) {
      this.header = document.createElement('span');
      this.header.textContent = this.getTitle();
      this.title = this.theme.getHeader(this.header);
      this.container.appendChild(this.title);
      this.title_controls = this.theme.getHeaderButtonHolder();
      this.title.appendChild(this.title_controls);
      if(this.schema.description) {
        this.description = this.theme.getDescription(this.schema.description);
        this.container.appendChild(this.description);
      }

      this.panel = this.theme.getIndentedPanel();
      this.container.appendChild(this.panel);
      this.row_holder = document.createElement('div');
      this.panel.appendChild(this.row_holder);
      this.controls = this.theme.getButtonHolder();
      this.panel.appendChild(this.controls);
    }
    else {
        this.panel = this.theme.getIndentedPanel();
        this.container.appendChild(this.panel);
        this.controls = this.theme.getButtonHolder();
        this.panel.appendChild(this.controls);
        this.row_holder = document.createElement('div');
        this.panel.appendChild(this.row_holder);
    }
  },
  arrayDestroyImpl: function() {
    if(this.title && this.title.parentNode) this.title.parentNode.removeChild(this.title);
    if(this.description && this.description.parentNode) this.description.parentNode.removeChild(this.description);
    if(this.row_holder && this.row_holder.parentNode) this.row_holder.parentNode.removeChild(this.row_holder);
    if(this.controls && this.controls.parentNode) this.controls.parentNode.removeChild(this.controls);
    if(this.panel && this.panel.parentNode) this.panel.parentNode.removeChild(this.panel);
    
    this.title = this.description = this.row_holder = this.panel = this.controls = null;
  },
  onChildEditorChange: function(editor) {
    this.refreshValue();
    this.onChange();
  },
  getItemTitle: function() {
    if(!this.item_title) {
      this.item_title = this.schema.items.title || 'item';
    }
    return this.item_title;
  },
  getElementEditor: function(i) {
    var schema = $utils.extend(this.schema.items, {
      title: this.getItemTitle() + ' ' + (i + 1)
    });

    var editor = this.jsoneditor.getEditorClass(schema);

    var holder;
    if(schema.properties || schema.items) {
      holder = this.theme.getChildEditorHolder();
    }
    else {
      holder = this.theme.getIndentedPanel();
    }

    this.row_holder.appendChild(holder);

    var ret = this.jsoneditor.createEditor(editor,{
      jsoneditor: this.jsoneditor,
      schema: schema,
      container: holder,
      path: this.path+'.'+i,
      parent: this
    });
    ret.build();

    if(!ret.title_controls) {
      ret.array_controls = this.theme.getButtonHolder();
      holder.appendChild(ret.array_controls);
    }
    
    return ret;
  },
  empty: function() {
    var self = this;
    $utils.each(this.rows,function(i,row) {
      self.destroyRow(row);
      self.rows[i] = null;
    });
    self.rows = [];
  },
  destroyRow: function(row) {
    var holder = row.container;
    row.destroy();
    if (holder.parentNode) {
      holder.parentNode.removeChild(holder);
    }
  },
  setValueImpl: function(value) {
    var self = this;
    
    value = value || [];
    if(!(Array.isArray(value))) value = [value];
    
    $utils.each(value,function(i,val) {
      if (self.rows[i]) {
        self.rows[i].setValue(val);
      } else {
        self.addRow(val);
      }
    });

    for(var j=value.length; j<self.rows.length; j++) {
      self.destroyRow(self.rows[j]);
      self.rows[j] = null;
    }
    self.rows = self.rows.slice(0,value.length);

    self.refreshValue();
    self.refreshButtons();

    self.onChange();
  },
  getFinalValue: function() {
    var result = [];
    $utils.each(this.rows,function(i,editor) {
      result[i] = editor.getFinalValue();
    });
    return result;
  },
  refreshValue: function() {
    var self = this;
    
    var oldi = this.value? this.value.length : 0;
    
    self.value = [];
    $utils.each(this.rows,function(i,editor) {
      self.value[i] = editor.getValue();
    });
    
    if (oldi !== this.value.length) {
      self.buttons_dirty = true;
    }
  },
  refreshButtons: function() {
    var self = this;
    
    if (!self.buttons_dirty) {
      return;
    }
    self.buttons_dirty = false;
    
    var need_row_buttons = false;
    $utils.each(this.rows,function(i,editor) {
      // Hide the move down button for the last row
      if(editor.movedown_button) {
        if(i === self.rows.length - 1) {
          editor.movedown_button.style.display = 'none';
        }
        else {
          need_row_buttons = true;
          editor.movedown_button.style.display = '';
        }
      }

      if(editor.delete_button) {
        need_row_buttons = true;
        editor.delete_button.style.display = '';
      }
      
      if (self.refreshButtonsExtraEditor(i, editor)) {
        need_row_buttons = true;
      }
    });
    
    var controls_needed = false;
    
    if (this.rows.length === 0 || this.hide_delete_buttons) {
      this.remove_all_rows_button.style.display = 'none';
    } else {
      this.remove_all_rows_button.style.display = '';
      controls_needed = true;
    }

    this.add_row_button.style.display = '';
    controls_needed = true;
    
    self.refreshButtonsExtra(need_row_buttons, controls_needed);
  },
  refreshButtonsExtraEditor: function(i, editor) {
    return false;
  },
  refreshButtonsExtra: function(need_row_buttons, controls_needed) {
    if(!this.collapsed && controls_needed) {
      this.controls.style.display = 'inline-block';
    }
    else {
      this.controls.style.display = 'none';
    }
  },
  addRow: function(value) {
    this.addRowBase(value, true, function(row) { return row.title_controls || row.array_controls; });
  },
  addRowBase: function(value, titled_delete, controls_holder_func) {
    var self = this;
    var i = this.rows.length;
    
    self.rows[i] = this.getElementEditor(i);

    var controls_holder = controls_holder_func(self.rows[i]);
    
    // Buttons to delete row, move row up, and move row down
    if(!self.hide_delete_buttons) {
      var delete_title1 = titled_delete ? self.getItemTitle() : '';
      var delete_title2 = 'Delete' + (titled_delete ? (' ' + self.getItemTitle()) : '');
      self.rows[i].delete_button = this.getButton(delete_title1,'delete',delete_title2);
      self.rows[i].delete_button.className += ' delete';
      self.rows[i].delete_button.setAttribute('data-i',i);
      self.rows[i].delete_button.addEventListener('click',function(e) {
        e.preventDefault();
        e.stopPropagation();
        var i = this.getAttribute('data-i')*1;

        var value = self.getValue();

        var newval = [];
        $utils.each(value,function(j,row) {
          if(j===i) {
            return; // If this is the one we're deleting
          }
          newval.push(row);
        });
        
        self.withProcessingContext(function() {
          self.setValueImpl(newval);
        }, 'delete_button_click');
      });
      
      if(controls_holder) {
        controls_holder.appendChild(self.rows[i].delete_button);
      }
    }
    
    if(!self.hide_copy_buttons) {
      self.rows[i].copy_button = this.getButton('','copy','Copy');
      self.rows[i].copy_button.className += ' copy';
      self.rows[i].copy_button.setAttribute('data-i',i);
      self.rows[i].copy_button.addEventListener('click',function(e) {
        e.preventDefault();
        e.stopPropagation();
        var i = this.getAttribute('data-i')*1;

        var rows = self.value.slice();
        var new_row = rows[i];
        if (self.copy_template) {
          new_row = self.copy_template({rows: rows, index: i, row: new_row});
        }
        rows.splice(i + 1, 0, new_row);

        self.withProcessingContext(function() {
          self.setValueImpl(rows);
        }, 'move_button_click');
      });
      
      if(controls_holder) {
        controls_holder.appendChild(self.rows[i].copy_button);
      }
    }

    if(i && !self.hide_move_buttons) {
      self.rows[i].moveup_button = this.getButton('','moveup','Move up');
      self.rows[i].moveup_button.className += ' moveup';
      self.rows[i].moveup_button.setAttribute('data-i',i);
      self.rows[i].moveup_button.addEventListener('click',function(e) {
        self.moveClickHandler(e, this, false);
      });
      
      if(controls_holder) {
        controls_holder.appendChild(self.rows[i].moveup_button);
      }
    }
    
    if(!self.hide_move_buttons) {
      self.rows[i].movedown_button = this.getButton('','movedown','Move down');
      self.rows[i].movedown_button.className += ' movedown';
      self.rows[i].movedown_button.setAttribute('data-i',i);
      self.rows[i].movedown_button.addEventListener('click',function(e) {
        self.moveClickHandler(e, this, true);
      });
      
      if(controls_holder) {
        controls_holder.appendChild(self.rows[i].movedown_button);
      }
    }
    
    if(value) self.rows[i].setValue(value);
  },
  moveClickHandler: function(e, button, down) {
    var self = this;
    
    e.preventDefault();
    e.stopPropagation();
    var i = button.getAttribute('data-i')*1;
    if (down) {
      i++;
    }

    if(i<=0) return;
    var rows = self.value.slice();
    var tmp = rows[i-1];
    rows[i-1] = rows[i];
    rows[i] = tmp;

    self.withProcessingContext(function() {
      self.setValueImpl(rows);
    }, 'move_button_click');
  },
  addRowButtons: function() {
  },
  addControls: function() {
    var self = this;
    
    this.collapsed = false;
    if (this.title_controls) {
      this.toggle_button = this.getButton('','collapse','Collapse');
      this.title_controls.appendChild(this.toggle_button);
      this.toggleSetup();
      this.toggle_button.addEventListener('click',function(e) {
        e.preventDefault();
        e.stopPropagation();
        self.toggleCollapsed();
      });

      // If it should start collapsed
      if(this.options.collapsed) {
        this.toggleCollapsed();
      }
      
      // Collapse button disabled
      if(this.schema.options && typeof this.schema.options.disable_collapse !== "undefined") {
        if(this.schema.options.disable_collapse) this.toggle_button.style.display = 'none';
      }
      else if(this.jsoneditor.options.disable_collapse) {
        this.toggle_button.style.display = 'none';
      }
    }
    
    this.add_row_button = this.getButton(this.getItemTitle(),'add','Add '+this.getItemTitle());
    this.add_row_button.addEventListener('click',function(e) {
      e.preventDefault();
      e.stopPropagation();
      self.withProcessingContext(function() {
        self.addRowButtonHandler();
      }, 'add_row_button_click');
    });
    self.controls.appendChild(this.add_row_button);

    this.remove_all_rows_button = this.getButton('All','delete','Delete All');
    this.remove_all_rows_button.addEventListener('click',function(e) {
      e.preventDefault();
      e.stopPropagation();
      
      self.withProcessingContext(function() {
        self.setValueImpl([]);
      }, 'delete_all_rows_button_click');
    });
    self.controls.appendChild(this.remove_all_rows_button);
  },
  addRowButtonHandler: function() {
    var self = this;
    self.addRow();
    self.refreshValue();
    self.refreshButtons();
    self.onChange();
  },
  toggleSetup: function() {
    var self = this;
    self.row_holder_display = self.row_holder.style.display;
    self.controls_display = self.controls.style.display;
  },
  toggleHandlerExtra: function(expanding) {
    var self = this;
    self.row_holder.style.display = expanding ? self.row_holder_display : 'none';
    self.controls.style.display = expanding ? self.controls_display : 'none';
  },
  toggleCollapsed: function() {
    var self = this;
    self.collapsed = !self.collapsed;
    if(self.collapsed) {
      self.toggleHandlerExtra(false);
      if(self.panel) self.panel.style.display = 'none';
      self.setButtonText(self.toggle_button,'','expand','Expand');
    }
    else {
      if(self.panel) self.panel.style.display = '';
      self.toggleHandlerExtra(true);
      self.setButtonText(self.toggle_button,'','collapse','Collapse');
    }
  }
});

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

JSONEditor.AbstractTheme = Class.extend({
  getContainer: function() {
    return document.createElement('div');
  },
  getFloatRightLinkHolder: function() {
    var el = document.createElement('div');
    el.style = el.style || {};
    el.style.cssFloat = 'right';
    el.style.marginLeft = '10px';
    return el;
  },
  getModal: function() {
    var el = document.createElement('div');
    el.style.backgroundColor = 'white';
    el.style.border = '1px solid black';
    el.style.boxShadow = '3px 3px black';
    el.style.position = 'absolute';
    el.style.zIndex = '10';
    el.style.display = 'none';
    return el;
  },
  getGridContainer: function() {
    var el = document.createElement('div');
    return el;
  },
  getGridRow: function() {
    var el = document.createElement('div');
    el.className = 'row';
    return el;
  },
  getGridColumn: function() {
    var el = document.createElement('div');
    return el;
  },
  setGridColumnSize: function(el,size) {
    
  },
  getLink: function(text) {
    var el = document.createElement('a');
    el.setAttribute('href','#');
    el.appendChild(document.createTextNode(text));
    return el;
  },
  disableHeader: function(header) {
    header.style.color = '#ccc';
  },
  disableLabel: function(label) {
    label.style.color = '#ccc';
  },
  enableHeader: function(header) {
    header.style.color = '';
  },
  enableLabel: function(label) {
    label.style.color = '';
  },
  getFormInputLabel: function(text) {
    var el = document.createElement('label');
    el.appendChild(document.createTextNode(text));
    return el;
  },
  getCheckboxLabel: function(text) {
    var el = this.getFormInputLabel(text);
    el.style.fontWeight = 'normal';
    return el;
  },
  getHeader: function(text) {
    var el = document.createElement('h3');
    if(typeof text === "string") {
      el.textContent = text;
    }
    else {
      el.appendChild(text);
    }
    
    return el;
  },
  getCheckbox: function() {
    var el = this.getFormInputField('checkbox');
    el.style.display = 'inline-block';
    el.style.width = 'auto';
    return el;
  },
  getMultiCheckboxHolder: function(controls,label,description) {
    var el = document.createElement('div');

    if(label) {
      label.style.display = 'block';
      el.appendChild(label);
    }

    for(var i in controls) {
      if(!controls.hasOwnProperty(i)) continue;
      controls[i].style.display = 'inline-block';
      controls[i].style.marginRight = '20px';
      el.appendChild(controls[i]);
    }

    if(description) el.appendChild(description);

    return el;
  },
  getSelectInput: function(options) {
    var select = document.createElement('select');
    if(options) this.setSelectOptions(select, options);
    return select;
  },
  getSwitcher: function(options) {
    var switcher = this.getSelectInput(options);
    switcher.style.backgroundColor = 'transparent';
    switcher.style.height = 'auto';
    switcher.style.fontStyle = 'italic';
    switcher.style.fontWeight = 'normal';
    switcher.style.padding = '0 0 0 3px';
    return switcher;
  },
  getSwitcherOptions: function(switcher) {
    return switcher.getElementsByTagName('option');
  },
  setSwitcherOptions: function(switcher, options, titles) {
    this.setSelectOptions(switcher, options, titles);
  },
  setSelectOptions: function(select, options, titles) {
    titles = titles || [];
    select.innerHTML = '';
    for(var i=0; i<options.length; i++) {
      var option = document.createElement('option');
      option.setAttribute('value',options[i]);
      option.textContent = titles[i] || options[i];
      select.appendChild(option);
    }
  },
  getTextareaInput: function() {
    var el = document.createElement('textarea');
    el.style = el.style || {};
    el.style.width = '100%';
    el.style.height = '300px';
    el.style.boxSizing = 'border-box';
    return el;
  },
  getRangeInput: function(min,max,step) {
    var el = this.getFormInputField('range');
    el.setAttribute('min',min);
    el.setAttribute('max',max);
    el.setAttribute('step',step);
    return el;
  },
  getFormInputField: function(type) {
    var el = document.createElement('input');
    el.setAttribute('type',type);
    return el;
  },
  afterInputReady: function(input) {
    
  },
  getFormControl: function(label, input, description) {
    var el = document.createElement('div');
    el.className = 'form-control';
    if(label) el.appendChild(label);
    if(input.type === 'checkbox') {
      label.insertBefore(input,label.firstChild);
    }
    else {
      el.appendChild(input);
    }
    
    if(description) el.appendChild(description);
    return el;
  },
  getIndentedPanel: function() {
    var el = document.createElement('div');
    el.style = el.style || {};
    el.style.paddingLeft = '10px';
    el.style.marginLeft = '10px';
    el.style.borderLeft = '1px solid #ccc';
    return el;
  },
  getChildEditorHolder: function() {
    return document.createElement('div');
  },
  getDescription: function(text) {
    var el = document.createElement('p');
    el.appendChild(document.createTextNode(text));
    return el;
  },
  getCheckboxDescription: function(text) {
    return this.getDescription(text);
  },
  getFormInputDescription: function(text) {
    return this.getDescription(text);
  },
  getHeaderButtonHolder: function() {
    return this.getButtonHolder();
  },
  getButtonHolder: function() {
    return document.createElement('div');
  },
  getButton: function(text, icon, title) {
    var el = document.createElement('button');
    this.setButtonText(el,text,icon,title);
    return el;
  },
  setButtonText: function(button, text, icon, title) {
    button.innerHTML = '';
    if(icon) {
      button.appendChild(icon);
      button.innerHTML += ' ';
    }
    button.appendChild(document.createTextNode(text));
    if(title) button.setAttribute('title',title);
  },
  getTable: function() {
    return document.createElement('table');
  },
  getTableRow: function() {
    return document.createElement('tr');
  },
  getTableHead: function() {
    return document.createElement('thead');
  },
  getTableBody: function() {
    return document.createElement('tbody');
  },
  getTableHeaderCell: function(text) {
    var el = document.createElement('th');
    el.textContent = text;
    return el;
  },
  getTableCell: function() {
    var el = document.createElement('td');
    return el;
  },
  getErrorMessage: function(text) {
    var el = document.createElement('p');
    el.style = el.style || {};
    el.style.color = 'red';
    el.appendChild(document.createTextNode(text));
    return el;
  },
  addInputError: function(input, text) {
  },
  removeInputError: function(input) {
  },
  addTableRowError: function(row) {
  },
  removeTableRowError: function(row) {
  },
  getTabHolder: function() {
    var el = document.createElement('div');
    el.innerHTML = "<div style='float: left; width: 130px;' class='tabs'></div><div class='content' style='margin-left: 130px;'></div><div style='clear:both;'></div>";
    return el;
  },
  applyStyles: function(el,styles) {
    el.style = el.style || {};
    for(var i in styles) {
      if(!styles.hasOwnProperty(i)) continue;
      el.style[i] = styles[i];
    }
  },
  closest: function(elem, selector) {
    var matchesSelector = elem.matches || elem.webkitMatchesSelector || elem.mozMatchesSelector || elem.msMatchesSelector;

    while (elem && elem !== document) {
      try {
        var f = matchesSelector.bind(elem);
        if (f(selector)) {
          return elem;
        } else {
          elem = elem.parentNode;
        }
      }
      catch(e) {
        return false;
      }
    }
    return false;
  },
  getTab: function(span) {
    var el = document.createElement('div');
    el.appendChild(span);
    el.style = el.style || {};
    this.applyStyles(el,{
      border: '1px solid #ccc',
      borderWidth: '1px 0 1px 1px',
      textAlign: 'center',
      lineHeight: '30px',
      borderRadius: '5px',
      borderBottomRightRadius: 0,
      borderTopRightRadius: 0,
      fontWeight: 'bold',
      cursor: 'pointer'
    });
    return el;
  },
  getTabContentHolder: function(tab_holder) {
    return tab_holder.children[1];
  },
  getTabContent: function() {
    return this.getIndentedPanel();
  },
  markTabActive: function(tab) {
    this.applyStyles(tab,{
      opacity: 1,
      background: 'white'
    });
  },
  markTabInactive: function(tab) {
    this.applyStyles(tab,{
      opacity:0.5,
      background: ''
    });
  },
  addTab: function(holder, tab) {
    holder.children[0].appendChild(tab);
  },
  getBlockLink: function() {
    var link = document.createElement('a');
    link.style.display = 'block';
    return link;
  },
  getBlockLinkHolder: function() {
    var el = document.createElement('div');
    return el;
  },
  getLinksHolder: function() {
    var el = document.createElement('div');
    return el;
  },
  createMediaLink: function(holder,link,media) {
    holder.appendChild(link);
    media.style.width='100%';
    holder.appendChild(media);
  },
  createImageLink: function(holder,link,image) {
    holder.appendChild(link);
    link.appendChild(image);
  }
});

JSONEditor.defaults.themes.bootstrap3 = JSONEditor.AbstractTheme.extend({
  getSelectInput: function(options) {
    var el = this._super(options);
    el.className += 'form-control';
    //el.style.width = 'auto';
    return el;
  },
  setGridColumnSize: function(el,size) {
    el.className = 'col-md-'+size;
  },
  afterInputReady: function(input) {
    if(input.controlgroup) return;
    input.controlgroup = this.closest(input,'.form-group');
    if(this.closest(input,'.compact')) {
      input.controlgroup.style.marginBottom = 0;
    }

    // TODO: use bootstrap slider
  },
  getTextareaInput: function() {
    var el = document.createElement('textarea');
    el.className = 'form-control';
    return el;
  },
  getRangeInput: function(min, max, step) {
    // TODO: use better slider
    return this._super(min, max, step);
  },
  getFormInputField: function(type) {
    var el = this._super(type);
    if(type !== 'checkbox') {
      el.className += 'form-control';
    }
    return el;
  },
  getFormControl: function(label, input, description) {
    var group = document.createElement('div');

    if(label && input.type === 'checkbox') {
      group.className += ' checkbox';
      label.appendChild(input);
      label.style.fontSize = '14px';
      group.style.marginTop = '0';
      group.appendChild(label);
      input.style.position = 'relative';
      input.style.cssFloat = 'left';
    } 
    else {
      group.className += ' form-group';
      if(label) {
        label.className += ' control-label';
        group.appendChild(label);
      }
      group.appendChild(input);
    }

    if(description) group.appendChild(description);

    return group;
  },
  getIndentedPanel: function() {
    var el = document.createElement('div');
    el.className = 'well well-sm';
    return el;
  },
  getFormInputDescription: function(text) {
    var el = document.createElement('p');
    el.className = 'help-block';
    el.textContent = text;
    return el;
  },
  getHeaderButtonHolder: function() {
    var el = this.getButtonHolder();
    el.style.marginLeft = '10px';
    return el;
  },
  getButtonHolder: function() {
    var el = document.createElement('div');
    el.className = 'btn-group';
    return el;
  },
  getButton: function(text, icon, title) {
    var el = this._super(text, icon, title);
    el.className += 'btn btn-default';
    return el;
  },
  getTable: function() {
    var el = document.createElement('table');
    el.className = 'table table-bordered';
    el.style.width = 'auto';
    el.style.maxWidth = 'none';
    return el;
  },

  addInputError: function(input,text) {
    if(!input.controlgroup) return;
    input.controlgroup.className += ' has-error';
    if(!input.errmsg) {
      input.errmsg = document.createElement('p');
      input.errmsg.className = 'help-block errormsg';
      input.controlgroup.appendChild(input.errmsg);
    }
    else {
      input.errmsg.style.display = '';
    }

    input.errmsg.textContent = text;
  },
  removeInputError: function(input) {
    if(!input.errmsg) return;
    input.errmsg.style.display = 'none';
    input.controlgroup.className = input.controlgroup.className.replace(/\s?has-error/g,'');
  },
  getTabHolder: function() {
    var el = document.createElement('div');
    el.innerHTML = "<div class='tabs list-group col-md-2'></div><div class='col-md-10'></div>";
    el.className = 'rows';
    return el;
  },
  getTab: function(text) {
    var el = document.createElement('a');
    el.className = 'list-group-item';
    el.setAttribute('href','#');
    el.appendChild(text);
    return el;
  },
  markTabActive: function(tab) {
    tab.className += ' active';
  },
  markTabInactive: function(tab) {
    tab.className = tab.className.replace(/\s?active/g,'');
  },
  getProgressBar: function() {
    var min = 0, max = 100, start = 0;

    var container = document.createElement('div');
    container.className = 'progress';

    var bar = document.createElement('div');
    bar.className = 'progress-bar';
    bar.setAttribute('role', 'progressbar');
    bar.setAttribute('aria-valuenow', start);
    bar.setAttribute('aria-valuemin', min);
    bar.setAttribute('aria-valuenax', max);
    bar.innerHTML = start + "%";
    container.appendChild(bar);

    return container;
  },
  updateProgressBar: function(progressBar, progress) {
    if (!progressBar) return;

    var bar = progressBar.firstChild;
    var percentage = progress + "%";
    bar.setAttribute('aria-valuenow', progress);
    bar.style.width = percentage;
    bar.innerHTML = percentage;
  },
  updateProgressBarUnknown: function(progressBar) {
    if (!progressBar) return;

    var bar = progressBar.firstChild;
    progressBar.className = 'progress progress-striped active';
    bar.removeAttribute('aria-valuenow');
    bar.style.width = '100%';
    bar.innerHTML = '';
  }
});

JSONEditor.AbstractIconLib = Class.extend({
  mapping: {
    collapse: '',
    expand: '',
    delete: '',
    edit: '',
    add: '',
    cancel: '',
    save: '',
    moveup: '',
    movedown: ''
  },
  icon_prefix: '',
  getIconClass: function(key) {
    if(this.mapping[key]) return this.icon_prefix+this.mapping[key];
    else return null;
  },
  getIcon: function(key) {
    var iconclass = this.getIconClass(key);
    
    if(!iconclass) return null;
    
    var i = document.createElement('i');
    i.className = iconclass;
    return i;
  }
});

JSONEditor.defaults.iconlibs.bootstrap3 = JSONEditor.AbstractIconLib.extend({
  mapping: {
    collapse: 'chevron-down',
    expand: 'chevron-right',
    delete: 'remove',
    edit: 'pencil',
    add: 'plus',
    cancel: 'floppy-remove',
    save: 'floppy-saved',
    moveup: 'arrow-up',
    movedown: 'arrow-down'
  },
  icon_prefix: 'glyphicon glyphicon-'
});

JSONEditor.defaults.templates.javascript = function() {
  return {
    compile: function(template) {
      /* jshint ignore:start */
      return Function("vars", template);
      /* jshint ignore:end */
    }
  };
};

// Set the default theme
JSONEditor.defaults.theme = 'bootstrap3';

// Set the default icon library
JSONEditor.defaults.iconlib = 'bootstrap3';

// Set the default template engine
JSONEditor.defaults.template = 'javascript';

// Default options when initializing JSON Editor
JSONEditor.defaults.options = {};

// Default per-editor options
for(var i in JSONEditor.defaults.editors) {
  if(!JSONEditor.defaults.editors.hasOwnProperty(i)) continue;
  JSONEditor.defaults.editors[i].options = JSONEditor.defaults.editors.options || {};
}

// Set the default resolvers
// If the type is set and it's a basic type, use the primitive editor
JSONEditor.defaults.resolvers.unshift(function(schema) {
  // If the schema is a simple type
  if(typeof schema.type === "string") return schema.type;
});
// Use the select editor for all boolean values
JSONEditor.defaults.resolvers.unshift(function(schema) {
  if(schema.type === 'boolean') {
    return "select";
  }
});
// Use the table editor for arrays with the format set to `table`
JSONEditor.defaults.resolvers.unshift(function(schema) {
  // Type `array` with format set to `table`
  if(schema.type == "array" && schema.format == "table") {
    return "table";
  }
});
// Use the `select` editor for dynamic enumSource enums
JSONEditor.defaults.resolvers.unshift(function(schema) {
  if(schema.enumSource) return "select";
});
// Use the `enum` or `select` editors for schemas with enumerated properties
JSONEditor.defaults.resolvers.unshift(function(schema) {
  if(schema.enum) {
    if(schema.type === "number" || schema.type === "integer" || schema.type === "string") {
      return "select";
    }
  }
});
// Use the multiple editor for schemas with `oneOf` set
JSONEditor.defaults.resolvers.unshift(function(schema) {
  // If this schema uses `oneOf`
  if(schema.oneOf) return "multiple";
});
// Use the `derived` editor if it has the right config.
JSONEditor.defaults.resolvers.unshift(function(schema) {
  if($utils.has(schema, 'valueTemplate') || $utils.has(schema, 'constantValue')) {
    return "derived";
  }
});

  window.JSONEditor = JSONEditor;
  window.JSONEditor_utils = $utils;
})();
