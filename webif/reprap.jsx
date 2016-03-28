
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

function controlAllTable(labelText, buttonText, inputAttrs) {
    return (
        <table className="control-all-table">
            <tbody>
                <tr>
                    <td className="control-all-table-td-text">{labelText}</td>
                    <td className="control-all-table-td-button">
                        <button type="button" className={controlButtonClass('warning')} {...inputAttrs} >{buttonText}</button>
                    </td>
                </tr>
            </tbody>
        </table>
    );
}


// Utility functions

function orderObject(obj) {
    var arr = $.map(obj, function(val, key) { return {key: key, val: val}; });
    arr.sort(function(x, y) { return (x.key > y.key) - (x.key < y.key); });
    return arr;
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

function $toNumber(val) {
    return val - 0;
}


// Main React classes

var AxesTable = React.createClass({
    componentWillMount: function() {
        this.props.controller.setComponent(this);
    },
    onInputEnter: function(axis_name) {
        this.axisGo(axis_name);
    },
    axisGo: function(axis_name) {
        var target = this.props.controller.getNumberValue(axis_name);
        if (isNaN(target)) {
            return showError('Target value for axis '+axis_name+' is incorrect');
        }
        sendGcode('G0 R '+axis_name+target.toString());
        this.props.controller.cancel(axis_name);
    },
    allAxesGo: function() {
        var cmdAxes = '';
        var error = null;
        $.each(orderObject(this.props.axes), function(idx, axis) {
            var axis_name = axis.key;
            if (!this.props.controller.isEditing(axis_name)) {
                return;
            }
            var target = this.props.controller.getNumberValue(axis_name);
            if (isNaN(target)) {
                if (error === null) error = 'Target value for axis '+axis_name+' is incorrect';
                return;
            }
            cmdAxes += ' '+axis_name+target.toString();
        }.bind(this));
        if (error !== null) {
            return showError(error);
        }
        if (cmdAxes.length !== 0) {
            sendGcode('G0 R'+cmdAxes);
            this.props.controller.cancelAll();
        }
    },
    render: function() {
        this.props.controller.rendering(this.props.axes);
        return (
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
                        <th>{controlAllTable('Go to', 'Go', {disabled: !this.props.controller.isEditingAny(), onClick: this.allAxesGo})}</th>
                    </tr>
                </thead>
                <tbody>
                    {$.map(orderObject(this.props.axes), function(axis) {
                        var dispPos = axis.val.pos.toPrecision(axisPrecision);
                        var ecInputs = this.props.controller.getRenderInputs(axis.key, dispPos);
                        return (
                            <tr key={axis.key}>
                                <td><b>{axis.key}</b></td>
                                <td>{dispPos}</td>
                                <td></td>
                                <td>
                                    <div className="input-group">
                                        <input type="number" className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref={'target_'+axis.key}
                                            onChange={ecInputs.onChange} onKeyDown={ecInputs.onKeyDown} onKeyPress={ecInputs.onKeyPress} />
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
        );
    },
    componentDidUpdate: function() {
        this.props.controller.componentDidUpdate(this.props.axes);
    }
});

var HeatersTable = React.createClass({
    componentWillMount: function() {
        this.props.controller.setComponent(this);
    },
    onInputEnter: function(heater_name) {
        this.heaterSet(heater_name);
    },
    makeSetHeaterGcode: function(heater_name) {
        var target = this.props.controller.getNumberValue(heater_name);
        if (isNaN(target)) {
            return {err: 'Target value for heater '+heater_name+' is incorrect'};
        }
        return {res: 'M104 F '+heater_name+' S'+target.toString()};
    },
    heaterSet: function(heater_name) {
        var makeRes = this.makeSetHeaterGcode(heater_name);
        if ($has(makeRes, 'err')) {
            return showError(makeRes.err);
        }
        sendGcode(makeRes.res);
        this.props.controller.cancel(heater_name);
    },
    heaterOff: function(heater_name) {
        sendGcode('M104 F '+heater_name+' Snan');
        this.props.controller.cancel(heater_name);
    },
    allHeatersSet: function() {
        var cmds = [];
        var error = null;
        $.each(orderObject(this.props.heaters), function(idx, heater) {
            var heater_name = heater.key;
            if (this.props.controller.isEditing(heater_name)) {
                var makeRes = this.makeSetHeaterGcode(heater_name);
                if ($has(makeRes, 'err')) {
                    if (error === null) error = makeRes.err;
                    return;
                }
                cmds.push(makeRes.res);
            }
        }.bind(this));
        if (error !== null) {
            return showError(error);
        }
        if (cmds.length !== 0) {
            sendGcodes(cmds);
            this.props.controller.cancelAll();
        }
    },
    render: function() {
        this.props.controller.rendering(this.props.heaters);
        return (
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
                        <th>{controlAllTable('Control', 'Set', {disabled: !this.props.controller.isEditingAny(), onClick: this.allHeatersSet})}</th>
                    </tr>
                </thead>
                <tbody>
                    {$.map(orderObject(this.props.heaters), function(heater) {
                        var dispActual = heater.val.current.toPrecision(heaterPrecision);
                        var isOff = (heater.val.target === -Infinity);
                        var dispTarget = isOff ? 'off' : heater.val.target.toPrecision(heaterPrecision);
                        var editTarget = isOff ? '' : dispTarget;
                        var ecInputs = this.props.controller.getRenderInputs(heater.key, editTarget);
                        return (
                            <tr key={heater.key}>
                                <td><b>{heater.key}</b>{(heater.val.error ? " ERR" : "")}</td>
                                <td>{dispActual}</td>
                                <td>{dispTarget}</td>
                                <td>
                                    <div className="input-group">
                                        <input type="number" className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref={'target_'+heater.key}
                                            onChange={ecInputs.onChange} onKeyDown={ecInputs.onKeyDown} onKeyPress={ecInputs.onKeyPress} />
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
        );
    },
    componentDidUpdate: function() {
        this.props.controller.componentDidUpdate(this.props.heaters);
    }
});

var FansTable = React.createClass({
    componentWillMount: function() {
        this.props.controller.setComponent(this);
    },
    onInputEnter: function(fan_name) {
        this.fanSet(fan_name);
    },
    makeSetFanGcode: function(fan_name) {
        var target = this.props.controller.getNumberValue(fan_name);
        if (isNaN(target)) {
            return {err: 'Target value for fan '+fan_name+' is incorrect'};
        }
        return {res: 'M106 F '+fan_name+' S'+(target/100*255).toPrecision(fanPrecision+3)};
    },
    fanSet: function(fan_name) {
        var makeRes = this.makeSetFanGcode(fan_name);
        if ($has(makeRes, 'err')) {
            return showError(makeRes.err);
        }
        sendGcode(makeRes.res);
        this.props.controller.cancel(fan_name);
    },
    fanOff: function(fan_name) {
        sendGcode('M106 F '+fan_name+' S0');
        this.props.controller.cancel(fan_name);
    },
    allFansSet: function() {
        var cmds = [];
        var error = null;
        $.each(orderObject(this.props.fans), function(idx, fan) {
            var fan_name = fan.key;
            if (this.props.controller.isEditing(fan_name)) {
                var makeRes = this.makeSetFanGcode(fan_name);
                if ($has(makeRes, 'err')) {
                    if (error === null) error = makeRes.err;
                    return;
                }
                cmds.push(makeRes.res);
            }
        }.bind(this));
        if (error !== null) {
            return showError(error);
        }
        if (cmds.length !== 0) {
            sendGcodes(cmds);
            this.props.controller.cancelAll();
        }
    },
    render: function() {
        this.props.controller.rendering(this.props.fans);
        return (
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
                        <th>{controlAllTable('Control', 'Set', {disabled: !this.props.controller.isEditingAny(), onClick: this.allFansSet})}</th>
                    </tr>
                </thead>
                <tbody>
                    {$.map(orderObject(this.props.fans), function(fan) {
                        var isOff = (fan.val.target === 0);
                        var editTarget = (fan.val.target * 100).toPrecision(fanPrecision);
                        var dispTarget = isOff ? 'off' : editTarget;
                        var ecInputs = this.props.controller.getRenderInputs(fan.key, editTarget);
                        return (
                            <tr key={fan.key}>
                                <td><b>{fan.key}</b></td>
                                <td>{dispTarget}</td>
                                <td></td>
                                <td>
                                    <div className="input-group">
                                        <input type="number" className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref={'target_'+fan.key}
                                            min="0" max="100"
                                            onChange={ecInputs.onChange} onKeyDown={ecInputs.onKeyDown} onKeyPress={ecInputs.onKeyPress} />
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
        );
    },
    componentDidUpdate: function() {
        this.props.controller.componentDidUpdate(this.props.fans);
    }
});

var SpeedTable = React.createClass({
    componentWillMount: function() {
        this.props.controller.setComponent(this);
    },
    onInputEnter: function(id) {
        this.speedRatioSet();
    },
    speedRatioSet: function() {
        var target = this.props.controller.getNumberValue('S');
        if (isNaN(target)) {
            return showError('Speed ratio value is incorrect');
        }
        sendGcode('M220 S'+target.toPrecision(speedPrecision+3));
        this.props.controller.cancel('S');
    },
    speedRatioReset: function() {
        sendGcode('M220 S100');
        this.props.controller.cancel('S');
    },
    render: function() {
        this.props.controller.rendering({'S': null});
        var dispRatio = (this.props.speedRatio*100).toPrecision(speedPrecision);
        var ecInputs = this.props.controller.getRenderInputs('S', dispRatio);
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
                                       min="10" max="1000"
                                       onChange={ecInputs.onChange} onKeyDown={ecInputs.onKeyDown} onKeyPress={ecInputs.onKeyPress} />
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
        this.props.controller.componentDidUpdate({'S': null});
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
    this._update_comp = null;
    this._comp = null;
    this._input_ref_prefix = input_ref_prefix;
    this._editing = {Q: 1};
}

EditController.prototype.setUpdateComponent = function(update_component) {
    this._update_comp = update_component;
};

EditController.prototype.setComponent = function(comp) {
    this._comp = comp;
};

EditController.prototype.getValue = function(id) {
    return this._input(id).value;
};

EditController.prototype.getNumberValue = function(id) {
    return $toNumber(this.getValue(id));
};

EditController.prototype.isEditing = function(id) {
    return $has(this._editing, id);
};

EditController.prototype.isEditingAny = function() {
    return !$.isEmptyObject(this._editing);
};

EditController.prototype._input = function(id) {
    return this._comp.refs[this._input_ref_prefix+id];
};

EditController.prototype._onChange = function(id) {
    var value = this.getValue(id);
    this._editing[id] = value;
    this._update_comp.forceUpdate();
};

EditController.prototype._onKeyDown = function(id, event) {
    if (event.key === 'Escape') {
        this._input(id).blur();
        this.cancel(id);
    }
};

EditController.prototype._onKeyPress = function(id, event) {
    if (event.key === 'Enter') {
        if ($has(this._comp, 'onInputEnter')) {
            return this._comp.onInputEnter(id);
        }
    }
};

EditController.prototype.cancel = function(id) {
    if (this.isEditing(id)) {
        delete this._editing[id];
        this._update_comp.forceUpdate();
    }
};

EditController.prototype.cancelAll = function(id) {
    this._editing = {};
    this._update_comp.forceUpdate();
};

EditController.prototype.rendering = function(id_datas) {
    $.each(this._editing, function(id, value) {
        if (!$has(id_datas, id)) {
            delete this._editing[id];
        }
    }.bind(this));
};

EditController.prototype.getRenderInputs = function(id, live_value) {
    var editing = this.isEditing(id);
    return {
        editing:    editing,
        class:      editing ? controlEditingClass : '',
        value:      editing ? this._editing[id] : live_value,
        onChange:   this._onChange.bind(this, id),
        onCancel:   this.cancel.bind(this, id),
        onKeyDown:  this._onKeyDown.bind(this, id),
        onKeyPress: this._onKeyPress.bind(this, id)
    };
};

EditController.prototype.componentDidUpdate = function(id_datas) {
    $.each(id_datas, function(id, data) {
        var input = this._input(id);
        if (!this.isEditing(id)) {
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

var controller_axes    = new EditController('target_');
var controller_heaters = new EditController('target_');
var controller_fans    = new EditController('target_');
var controller_speed   = new EditController('target_');

function render_axes() {
    return <AxesTable axes={machine_state.axes} controller={controller_axes} />;
}
function render_heaters() {
    return <HeatersTable heaters={machine_state.heaters} controller={controller_heaters} />;
}
function render_fans() {
    return <FansTable fans={machine_state.fans} controller={controller_fans} />;
}
function render_speed() {
    return <SpeedTable speedRatio={machine_state.speedRatio} controller={controller_speed} />;
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

controller_axes.setUpdateComponent(wrapper_axes);
controller_heaters.setUpdateComponent(wrapper_heaters);
controller_fans.setUpdateComponent(wrapper_fans);
controller_speed.setUpdateComponent(wrapper_speed);

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

function sendGcodes(gcodes) {
    sendGcode(gcodes.join('\n'));
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
