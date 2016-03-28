
// Hardcoded constants

var statusRefreshInterval = 2000;
var axisPrecision = 6;
var heaterPrecision = 4;
var fanPrecision = 3;
var speedPrecision = 4;


// Commonly used styles/elements

var controlTableClass = 'table table-condensed table-striped control-table';
var controlInputClass = 'form-control control-input';
var controlButtonClass = function(type) { return 'btn btn-'+type+' control-button'; }
var controlEditingClass = 'control-editing';
var controlCancelButtonClass = controlButtonClass('default')+' control-cancel-button';
var removeIcon = <span className="glyphicon glyphicon-remove" style={{verticalAlign: 'middle', marginTop: '-0.1em'}} aria-hidden="true"></span>;


// Utility functions

function orderObject(obj) {
    var arr = $.map(obj, function(val, key) { return {key: key, val: val}; });
    arr.sort(function(x, y) { return (x.key > y.key) - (x.key < y.key); });
    return arr;
}

function getNumberInput(input) {
    return $.trim(input.value) - 0;
}

function showError(error_str) {
    console.log('ERROR: '+error_str);
}

function $has(obj, prop) {
    return Object.prototype.hasOwnProperty.call(obj, prop);
}

function $bind(obj, func) {
    var objfunc = obj[func];
    return objfunc.bind.apply(objfunc, [obj].concat(Array.prototype.slice.call(arguments, 2)));
}


// Main React classes

var AxesTable = React.createClass({
    axisGo: function(axis_name) {
        var target = getNumberInput(this.refs['target_'+axis_name]);
        if (isNaN(target)) {
            return showError('Target value for axis '+axis_name+' is incorrect');
        }
        sendGcode('G0 R '+axis_name+target.toString());
        this.props.editController.cancel(axis_name);
    },
    render: function() { return (
        <table className={controlTableClass}>
            <colgroup>
                <col span="1" style={{width: '55px'}} />
                <col span="1" style={{width: '115px'}} />
                <col span="1" />
                <col span="1" style={{width: '205px'}} />
            </colgroup>
            <thead>
                <tr>
                    <th>Axis</th>
                    <th>Planned</th>
                    <th></th>
                    <th>Go to</th>
                </tr>
            </thead>
            <tbody>
                {$.map(orderObject(this.props.axes), function(axis) {
                    var dispPos = axis.val.pos.toPrecision(axisPrecision);
                    var ecInputs = this.props.editController.getRenderInputs(axis.key, dispPos, this);
                    return (
                        <tr key={axis.key}>
                            <td><b>{axis.key}</b></td>
                            <td>{dispPos}</td>
                            <td></td>
                            <td>
                                <div className="input-group">
                                    <input type="number" className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref={'target_'+axis.key} onInput={ecInputs.onEditing} onChange={ecInputs.onEditing} />
                                    <span className="input-group-btn">
                                        <button type="button" className={controlCancelButtonClass} disabled={!ecInputs.editing} onClick={ecInputs.onCancel} aria-label="Cancel">{removeIcon}</button>
                                        <button type="button" className={controlButtonClass('warning')} onClick={$bind(this, 'axisGo', axis.key)}>Go</button>
                                    </span>
                                </div>
                            </td>
                        </tr>
                    );
                }.bind(this))}
            </tbody>
        </table>
    );},
    componentDidUpdate: function() {
        this.props.editController.componentDidUpdate(this, this.props.axes);
    }
});

var HeatersTable = React.createClass({
    heaterSet: function(heater_name) {
        var target = getNumberInput(this.refs['target_'+heater_name]);
        if (isNaN(target)) {
            return showError('Target value for heater '+heater_name+' is incorrect');
        }
        sendGcode('M104 F '+heater_name+' S'+target.toString());
        this.props.editController.cancel(heater_name);
    },
    heaterOff: function(heater_name) {
        sendGcode('M104 F '+heater_name+' Snan');
        this.props.editController.cancel(heater_name);
    },
    render: function() { return (
        <table className={controlTableClass}>
            <colgroup>
                <col span="1" style={{width: '60px'}} />
                <col span="1" style={{width: '80px'}} />
                <col span="1" />
                <col span="1" style={{width: '200px'}} />
            </colgroup>
            <thead>
                <tr>
                    <th>Heater</th>
                    <th>Actual <span className="notbold">[C]</span></th>
                    <th>Target</th>
                    <th>Control</th>
                </tr>
            </thead>
            <tbody>
                {$.map(orderObject(this.props.heaters), function(heater) {
                    var dispActual = heater.val.current.toPrecision(heaterPrecision);
                    var isOff = (heater.val.target === -Infinity);
                    var dispTarget = isOff ? 'off' : heater.val.target.toPrecision(heaterPrecision);
                    var editTarget = isOff ? '' : dispTarget;
                    var ecInputs = this.props.editController.getRenderInputs(heater.key, editTarget, this);
                    return (
                        <tr key={heater.key}>
                            <td><b>{heater.key}</b>{(heater.val.error ? " ERR" : "")}</td>
                            <td>{dispActual}</td>
                            <td>{dispTarget}</td>
                            <td>
                                <div className="input-group">
                                    <input type="number" className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref={'target_'+heater.key}
                                           onInput={ecInputs.onEditing} onChange={ecInputs.onEditing} />
                                    <span className="input-group-btn">
                                        <button type="button" className={controlCancelButtonClass} disabled={!ecInputs.editing} onClick={ecInputs.onCancel} aria-label="Cancel">{removeIcon}</button>
                                        <button type="button" className={controlButtonClass('warning')} onClick={$bind(this, 'heaterSet', heater.key)}>Set</button>
                                        <button type="button" className={controlButtonClass('primary')} onClick={$bind(this, 'heaterOff', heater.key)}>Off</button>
                                    </span>
                                </div>
                            </td>
                        </tr>
                    );
                }.bind(this))}
            </tbody>
        </table>
    );},
    componentDidUpdate: function() {
        this.props.editController.componentDidUpdate(this, this.props.heaters);
    }
});

var FansTable = React.createClass({
    fanSet: function(fan_name) {
        var target = getNumberInput(this.refs['target_'+fan_name]);
        if (isNaN(target)) {
            return showError('Target value for fan '+fan_name+' is incorrect');
        }
        sendGcode('M106 F '+fan_name+' S'+(target/100*255).toPrecision(fanPrecision+3));
        this.props.editController.cancel(fan_name);
    },
    fanOff: function(fan_name) {
        sendGcode('M106 F '+fan_name+' S0');
        this.props.editController.cancel(fan_name);
    },
    render: function() { return (
        <table className={controlTableClass}>
            <colgroup>
                <col span="1" style={{width: '55px'}} />
                <col span="1" style={{width: '83px'}} />
                <col span="1" />
                <col span="1" style={{width: '200px'}} />
            </colgroup>
            <thead>
                <tr>
                    <th>Fan</th>
                    <th>Target <span className="notbold">[%]</span></th>
                    <th></th>
                    <th>Control</th>
                </tr>
            </thead>
            <tbody>
                {$.map(orderObject(this.props.fans), function(fan) {
                    var isOff = (fan.val.target === 0);
                    var editTarget = (fan.val.target * 100).toPrecision(fanPrecision);
                    var dispTarget = isOff ? 'off' : editTarget;
                    var ecInputs = this.props.editController.getRenderInputs(fan.key, editTarget, this);
                    return (
                        <tr key={fan.key}>
                            <td><b>{fan.key}</b></td>
                            <td>{dispTarget}</td>
                            <td></td>
                            <td>
                                <div className="input-group">
                                    <input type="number" className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref={'target_'+fan.key}
                                           onInput={ecInputs.onEditing} onChange={ecInputs.onEditing} min="0" max="100" />
                                    <span className="input-group-btn">
                                        <button type="button" className={controlCancelButtonClass} disabled={!ecInputs.editing} onClick={ecInputs.onCancel} aria-label="Cancel">{removeIcon}</button>
                                        <button type="button" className={controlButtonClass('warning')} onClick={$bind(this, 'fanSet', fan.key)}>Set</button>
                                        <button type="button" className={controlButtonClass('primary')} onClick={$bind(this, 'fanOff', fan.key)}>Off</button>
                                    </span>
                                </div>
                            </td>
                        </tr>
                    );
                }.bind(this))}
            </tbody>
        </table>
    );},
    componentDidUpdate: function() {
        this.props.editController.componentDidUpdate(this, this.props.fans);
    }
});

var SpeedTable = React.createClass({
    speedRatioSet: function() {
        var target = getNumberInput(this.refs.target_S);
        if (isNaN(target)) {
            return showError('Speed ratio value is incorrect');
        }
        sendGcode('M220 S'+target.toPrecision(speedPrecision+3));
        this.props.editController.cancel('S');
    },
    speedRatioReset: function() {
        sendGcode('M220 S100');
        this.props.editController.cancel('S');
    },
    render: function() {
        var dispRatio = (this.props.speedRatio*100).toPrecision(speedPrecision);
        var ecInputs = this.props.editController.getRenderInputs('S', dispRatio, this);
        return (
            <table className={controlTableClass}>
                <colgroup>
                    <col span="1" style={{width: '83px'}} />
                    <col span="1" />
                    <col span="1" style={{width: '200px'}} />
                </colgroup>
                <thead>
                    <tr>
                        <th>Speed ratio <span className="notbold">[%]</span></th>
                        <th></th>
                        <th>Control</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>{dispRatio}</td>
                        <td></td>
                        <td>
                            <div className="input-group">
                                <input type="number" className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref="target_S"
                                       onInput={ecInputs.onEditing} onChange={ecInputs.onEditing} min="10" max="1000" />
                                <span className="input-group-btn">
                                    <button type="button" className={controlCancelButtonClass} disabled={!ecInputs.editing} onClick={ecInputs.onCancel} aria-label="Cancel">{removeIcon}</button>
                                    <button type="button" className={controlButtonClass('warning')} onClick={this.speedRatioSet}>Set</button>
                                    <button type="button" className={controlButtonClass('primary')} onClick={this.speedRatioReset}>Off</button>
                                </span>
                            </div>
                        </td>
                    </tr>
                </tbody>
            </table>
        );
    },
    componentDidUpdate: function() {
        this.props.editController.componentDidUpdate(this, {'S': null});
    }
});

var Buttons1 = React.createClass({
    render: function() { return (
        <div>
            <button type="button" className="btn btn-primary top-btn-margin" onClick={onBtnHomeAxes}>Home axes</button>
            {this.props.probe_present &&
            <button type="button" className="btn btn-primary top-btn-margin" onClick={onBtnBedProbing}>Bed probing</button>
            }
            <button type="button" className="btn btn-info top-btn-margin" onClick={onBtnMotorsOff}>Motors off</button>
        </div>
    );}
});

var Buttons2 = React.createClass({
    render: function() { return (
        <div>
            <button type="button" className="btn btn-info top-btn-margin" onClick={onBtnRefresh}>Refresh</button>
        </div>
    );}
});


// Field editing logic

function EditController(input_ref_prefix) {
    this._component = null;
    this._input_ref_prefix = input_ref_prefix;
    this._editing = {};
}

EditController.prototype.setComponent = function(component) {
    this._component = component;
};

EditController.prototype.editing = function(id, comp) {
    var value = comp.refs[this._input_ref_prefix+id].value;
    this._editing[id] = value;
    this._component.forceUpdate();
};

EditController.prototype.cancel = function(id) {
    if ($has(this._editing, id)) {
        delete this._editing[id];
        this._component.forceUpdate();
    }
};

EditController.prototype.getRenderInputs = function(id, live_value, comp) {
    var editing = $has(this._editing, id);
    return {
        editing:   editing,
        class:     editing ? controlEditingClass : '',
        value:     editing ? this._editing[id] : live_value,
        onEditing: this.editing.bind(this, id, comp),
        onCancel:  this.cancel.bind(this, id)
    };
};

EditController.prototype.componentDidUpdate = function(comp, id_datas) {
    $.each(id_datas, function(id, data) {
        var input = comp.refs[this._input_ref_prefix+id];
        if (!$has(this._editing, id)) {
            input.defaultValue = input.value;
        }
    }.bind(this));
};


// Gluing of react classes into page

var ComponentWrapper = React.createClass({
    render: function() {
        return this.props.render();
    }
});

var machine_state = {
    speedRatio: 1,
    axes: {},
    heaters: {},
    fans: {}
};

var edit_controller_axes    = new EditController('target_');
var edit_controller_heaters = new EditController('target_');
var edit_controller_fans    = new EditController('target_');
var edit_controller_speed   = new EditController('target_');

function render_axes() {
    return <AxesTable axes={machine_state.axes} editController={edit_controller_axes} />;
}
function render_heaters() {
    return <HeatersTable heaters={machine_state.heaters} editController={edit_controller_heaters} />;
}
function render_fans() {
    return <FansTable fans={machine_state.fans} editController={edit_controller_fans} />;
}
function render_speed() {
    return <SpeedTable speedRatio={machine_state.speedRatio} editController={edit_controller_speed} />;
}
function render_buttons1() {
    return <Buttons1 probe_present={$has(machine_state, 'bedProbe')} />;
}
function render_buttons2() {
    return <Buttons2 />;
}

var wrapper_axes     = ReactDOM.render(<ComponentWrapper render={render_axes} />,     document.getElementById('axes_div'));
var wrapper_heaters  = ReactDOM.render(<ComponentWrapper render={render_heaters} />,  document.getElementById('heaters_div'));
var wrapper_fans     = ReactDOM.render(<ComponentWrapper render={render_fans} />,     document.getElementById('fans_div'));
var wrapper_speed    = ReactDOM.render(<ComponentWrapper render={render_speed} />,    document.getElementById('speed_div'));
var wrapper_buttons1 = ReactDOM.render(<ComponentWrapper render={render_buttons1} />, document.getElementById('buttons1_div'));
var wrapper_buttons2 = ReactDOM.render(<ComponentWrapper render={render_buttons2} />, document.getElementById('buttons2_div'));

edit_controller_axes.setComponent(wrapper_axes);
edit_controller_heaters.setComponent(wrapper_heaters);
edit_controller_fans.setComponent(wrapper_fans);
edit_controller_speed.setComponent(wrapper_speed);

function updateAll() {
    wrapper_axes.forceUpdate();
    wrapper_heaters.forceUpdate();
    wrapper_fans.forceUpdate();
    wrapper_speed.forceUpdate();
    wrapper_buttons1.forceUpdate();
    wrapper_buttons2.forceUpdate();
}


// Status updating

var statusRequestInProgesss = false;
var statusNeedsAnotherUpdate = false;
var statusTimerId = null;

function accelerateStatusUpdate() {
    if (statusRequestInProgesss) {
        statusNeedsAnotherUpdate = true;
    } else {
        requestStatus();
    }
}

function requestStatus() {
    if (statusTimerId !== null) {
        clearTimeout(statusTimerId);
        statusTimerId = null;
    }
    statusRequestInProgesss = true;
    $.ajax({
        url: '/rr_status',
        dataType: 'json',
        cache: false,
        success: function(new_machine_state) {
            statusRequestCompleted();
            machine_state = new_machine_state;
            updateAll();
        },
        error: function(xhr, status, err) {
            console.error('/rr_status', status, err.toString());
            statusRequestCompleted();
        }
    });
}

function statusRequestCompleted() {
    statusRequestInProgesss = false;
    if (statusNeedsAnotherUpdate) {
        statusNeedsAnotherUpdate = false;
        requestStatus();
    } else {
        statusTimerId = setTimeout(statusTimerHandler, statusRefreshInterval);
    }
}

function statusTimerHandler() {
    if (!statusRequestInProgesss) {
        requestStatus();
    }
}


// Gcode execution

var gcodeQueue = [];

function sendGcode(gcode_str) {
    gcodeQueue.push(gcode_str);
    if (gcodeQueue.length === 1) {
        sendNextQueuedGcode();
    }
}

function sendNextQueuedGcode() {
    var gcode_str = gcodeQueue[0];
    console.log('>>> '+gcode_str);
    $.ajax({
        url: '/rr_gcode',
        type: 'POST',
        data: gcode_str + '\n',
        dataType: 'text',
        success: function(response) {
            console.log('<<< '+response);
            currentGcodeCompleted();
            accelerateStatusUpdate();
        },
        error: function(xhr, status, err) {
            console.error('/rr_gcode', status, err.toString());
            currentGcodeCompleted();
            showError('Error sending gcode: '+gcode_str);
        }
    });
}

function currentGcodeCompleted() {
    gcodeQueue.shift();
    if (gcodeQueue.length !== 0) {
        sendNextQueuedGcode();
    }
}


// Handlers for buttons.

function onBtnHomeAxes() {
    sendGcode('G28');
}

function onBtnBedProbing() {
    sendGcode('G32');
}

function onBtnMotorsOff() {
    sendGcode('M18');
}

function onBtnRefresh() {
    accelerateStatusUpdate();
}


// Initial actions

requestStatus();
