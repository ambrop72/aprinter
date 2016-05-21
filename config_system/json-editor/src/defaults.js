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
