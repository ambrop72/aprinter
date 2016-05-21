JSONEditor.defaults.templates.javascript = function() {
  return {
    compile: function(template) {
      /* jshint ignore:start */
      return Function("vars", template);
      /* jshint ignore:end */
    }
  };
};
