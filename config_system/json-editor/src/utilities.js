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
