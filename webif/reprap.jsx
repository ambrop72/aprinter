
// Hardcoded constants

var statusRefreshInterval = 2000;
var configRefreshInterval = 120000;
var axisPrecision = 6;
var heaterPrecision = 4;
var fanPrecision = 3;
var speedPrecision = 4;
var configPrecision = 15;
var defaultSpeed = 50;
var gcodeHistorySize = 20;


// Commonly used styles/elements

var controlTableClass = 'table table-condensed table-striped control-table';
var controlInputClass = 'form-control control-input';
var controlButtonClass = function(type) { return 'btn btn-'+type+' control-button'; }
var controlEditingClass = 'control-editing';
var controlCancelButtonClass = controlButtonClass('default')+' control-cancel-button';
var gcodeTableClass = 'table table-condensed control-table gcode-table';

var removeIcon = <span className="glyphicon glyphicon-remove" style={{verticalAlign: 'middle', marginTop: '-0.1em'}} aria-hidden="true"></span>;

function controlAllHead(labelText, buttons) {
    return (
        <div className="flex-row">
            <div style={{display: 'inline-block', alignSelf: 'flex-end'}}>
                {labelText}
            </div>
            <div style={{flexGrow: '1'}}>
            </div>
            <div className="btn-group">
                {$.map(buttons, function(button, btnIndex) {
                    var buttonText = button.text;
                    var inputAttrs = button.attrs;
                    var buttonClass = $has(button, 'class') ? button.class : 'warning';
                    var highlighted = $has(button, 'highlighted') ? button.highlighted : false;
                    var className = controlButtonClass(buttonClass) + (highlighted ? ' control-button-highlight' : '');
                    return (
                        <button key={btnIndex} type="button" className={className} {...inputAttrs} >{buttonText}</button>
                    );
                })}
            </div>
        </div>
    );
}


// Utility functions

function orderObject(obj) {
    var arr = $.map(obj, function(val, key) { return {key: key, val: val}; });
    arr.sort(function(x, y) { return (x.key > y.key) - (x.key < y.key); });
    return arr;
}

function preprocessObjectForState(obj) {
    return {
        obj: obj,
        arr: orderObject(obj)
    };
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

function encodeStrForCmd(val) {
    return encodeURIComponent(val).replace(/%/g, "\\");
}

function removeTrailingZerosInNumStr(num_str) {
    // 123.0200 => 123.02
    // 123.0000 => 123
    // 1200 => 1200
    return num_str.replace(/(\.|(\.[0-9]*[1-9]))0*$/, function(match, p1, p2) {
        return typeof(p2) === 'string' ? p2 : '';
    });
}


// Status stables (axes, heaters, fans, speed)

var AxesTable = React.createClass({
    componentWillMount: function() {
        this.props.controller.setComponent(this);
    },
    onInputEnter: function(axis_name) {
        this.axisGo(axis_name);
    },
    getSpeed: function() {
        var speed = $toNumber(this.refs.speed.value);
        if (isNaN(speed) || speed == 0) {
            return {err: 'Bad speed'};
        }
        return {res: speed*60};
    },
    axisGo: function(axis_name) {
        var speedRes = this.getSpeed();
        if ($has(speedRes, 'err')) {
            return showError(speedRes.err);
        }
        var target = this.props.controller.getNumberValue(axis_name);
        if (isNaN(target)) {
            return showError('Target value for axis '+axis_name+' is incorrect');
        }
        sendGcode('Move axis', 'G0 R F'+speedRes.res.toString()+' '+axis_name+target.toString());
        this.props.controller.cancel(axis_name);
    },
    allAxesGo: function() {
        var speedRes = this.getSpeed();
        if ($has(speedRes, 'err')) {
            return showError(speedRes.err);
        }
        var cmdAxes = '';
        var reasonAxes = [];
        var error = null;
        $.each(this.props.axes.arr, function(idx, axis) {
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
            reasonAxes.push(axis_name);
        }.bind(this));
        if (error !== null) {
            return showError(error);
        }
        if (cmdAxes.length !== 0) {
            var axes = (reasonAxes.length > 1) ? 'axes' : 'axis';
            sendGcode('Move '+axes, 'G0 R F'+speedRes.res.toString()+cmdAxes);
            this.props.controller.cancelAll();
        }
    },
    btnHomeDefault: function() {
        sendGcode('Home axes', 'G28');
    },
    btnBedProbing: function() {
        sendGcode('Probe bed', 'G32');
    },
    btnMotorsOff: function() {
        sendGcode('Turn motors off', 'M18');
    },
    render: function() {
        this.props.controller.rendering(this.props.axes.obj);
        return (
            <div className="flex-column">
                <div className="flex-row">
                    <div className="form-inline">
                        <div className="form-group">
                            <button type="button" className={controlButtonClass('primary')+' control-right-margin'} onClick={this.btnHomeDefault}>Home</button>
                            {this.props.probe_present &&
                            <button type="button" className={controlButtonClass('primary')+' control-right-margin'} onClick={this.btnBedProbing}>Probe</button>
                            }
                            <button type="button" className={controlButtonClass('primary')} onClick={this.btnMotorsOff}>Motors off</button>
                        </div>
                    </div>
                    <div style={{flexGrow: '1'}}>
                    </div>
                    <div className="form-inline">
                        <div className="form-group">
                            <label htmlFor="speed" className="control-right-margin control-label">Speed [/s]</label>
                            <input ref="speed" id="speed" type="number" className={controlInputClass} style={{width: '80px'}} defaultValue={defaultSpeed} />
                        </div>
                    </div>
                </div>
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
                            <th>{controlAllHead('Go to', [{text: 'Go', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allAxesGo}}])}</th>
                        </tr>
                    </thead>
                    <tbody>
                        {$.map(this.props.axes.arr, function(axis) {
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
                                                {...makeEcInputProps(ecInputs)} />
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
            </div>
        );
    },
    componentDidUpdate: function() {
        this.props.controller.componentDidUpdate(this.props.axes.obj);
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
        sendGcode('Set heater setpoint', makeRes.res);
        this.props.controller.cancel(heater_name);
    },
    heaterOff: function(heater_name) {
        sendGcode('Turn off heater', 'M104 F '+heater_name+' Snan');
        this.props.controller.cancel(heater_name);
    },
    allHeatersSet: function() {
        var cmds = [];
        var error = null;
        $.each(this.props.heaters.arr, function(idx, heater) {
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
            var setpoints = (cmds.length > 1) ? 'setpoints' : 'setpoint';
            sendGcodes('Set heater '+setpoints, cmds);
            this.props.controller.cancelAll();
        }
    },
    render: function() {
        this.props.controller.rendering(this.props.heaters.obj);
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
                        <th>{controlAllHead('Control', [{text: 'Set', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allHeatersSet}}])}</th>
                    </tr>
                </thead>
                <tbody>
                    {$.map(this.props.heaters.arr, function(heater) {
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
                                               {...makeEcInputProps(ecInputs)} />
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
        this.props.controller.componentDidUpdate(this.props.heaters.obj);
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
        sendGcode('Set fan target', makeRes.res);
        this.props.controller.cancel(fan_name);
    },
    fanOff: function(fan_name) {
        sendGcode('Turn off fan', 'M106 F '+fan_name+' S0');
        this.props.controller.cancel(fan_name);
    },
    allFansSet: function() {
        var cmds = [];
        var error = null;
        $.each(this.props.fans.arr, function(idx, fan) {
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
            var targets = (cmds.length > 1) ? 'targets' : 'target';
            sendGcodes('Set fan '+targets, cmds);
            this.props.controller.cancelAll();
        }
    },
    render: function() {
        this.props.controller.rendering(this.props.fans.obj);
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
                        <th>{controlAllHead('Control', [{text: 'Set', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allFansSet}}])}</th>
                    </tr>
                </thead>
                <tbody>
                    {$.map(this.props.fans.arr, function(fan) {
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
                                               min="0" max="100" {...makeEcInputProps(ecInputs)} />
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
        this.props.controller.componentDidUpdate(this.props.fans.obj);
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
        sendGcode('Set speed ratio', 'M220 S'+target.toPrecision(speedPrecision+3));
        this.props.controller.cancel('S');
    },
    speedRatioReset: function() {
        sendGcode('Reset speed ratio', 'M220 S100');
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
                                       min="10" max="1000" {...makeEcInputProps(ecInputs)} />
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


// Buttons at the top row

var Buttons1 = React.createClass({
    render: function() { return (
        <div>
            <h1>Aprinter Web Interface</h1>
        </div>
    );}
});

var Buttons2 = React.createClass({
    render: function() { return (
        <div>
            <button type="button" className="btn btn-info top-btn-margin" onClick={startRefreshAll}>Refresh</button>
        </div>
    );}
});


// Configuration options table

function normalizeIpAddr(input) {
    var comps = input.split('.');
    if (comps.length != 4) {
        return {err: 'Invalid number of address components'};
    }
    var res_comps = [];
    $.each(comps, function(idx, comp) {
        if (/^[0-9]{1,3}$/.test(comp)) {
            var comp_val = parseInt(comp, 10);
            if (comp_val <= 255) {
                res_comps.push(comp_val.toString(10));
            }
        }
    });
    if (res_comps.length != 4) {
        return {err: 'Invalid address component'};
    }
    return {res: res_comps.join('.')};
}

function normalizeMacAddr(input) {
    var comps = input.split(':');
    if (comps.length != 6) {
        return {err: 'Invalid number of address components'};
    }
    var res_comps = [];
    $.each(comps, function(idx, comp) {
        if (/^[0-9A-Fa-f]{1,2}$/.test(comp)) {
            var res_str = parseInt(comp, 16).toString(16);
            if (res_str.length == 1) {
                res_str = "0" + res_str;
            }
            res_comps.push(res_str);
        }
    });
    if (res_comps.length != 6) {
        return {err: 'Invalid address component'};
    }
    return {res: res_comps.join(':').toUpperCase()};
}

var ConfigTypes = {
    'bool': {
        input: {type: 'select', options: ['false', 'true']},
        convertForDisp: function(string) {
            if (string !== '0' && string !== '1') {
                return {err: 'Not 0 or 1'};
            }
            return {res: string === '0' ? 'false' : 'true'};
        },
        convertForSet: function(string) {
            if (string !== 'false' && string !== 'true') {
                return {err: 'Not false or true'};
            }
            return {res: string === 'false' ? '0' : '1'};
        }
    },
    'double': {
        input: {type: 'number'},
        convertForDisp: function(string) {
            var num = Number(string);
            if (isNaN(num)) {
                return {err: 'Not a numeric string'};
            }
            return {res: removeTrailingZerosInNumStr(num.toPrecision(configPrecision))};
        },
        convertForSet: function(string) {
            return {res: string};
        }
    },
    'ip_addr': {
        input: {type: 'text'},
        convertForDisp: function(string) {
            return normalizeIpAddr(string);
        },
        convertForSet: function(string) {
            return normalizeIpAddr(string);
        }
    },
    'mac_addr': {
        input: {type: 'text'},
        convertForDisp: function(string) {
            return normalizeMacAddr(string);
        },
        convertForSet: function(string) {
            return normalizeMacAddr(string);
        }
    },
    'text': {
        input: {type: 'text'},
        convertForDisp: function(string) {
            return {res: string};
        },
        convertForSet: function(string) {
            return {res: string};
        }
    }
};

function getOptionTypeImpl(type) {
    return $has(ConfigTypes, type) ? ConfigTypes[type] : ConfigTypes['text'];
}

function preprocessOptionsList(options) {
    var result = {};
    $.map(options, function(option) {
        var eqIndex = option.nameval.indexOf('=');
        var optionName = option.nameval.substring(0, eqIndex);
        var optionValue = option.nameval.substring(eqIndex+1);
        result[optionName] = $.extend({name: optionName, value: optionValue}, option);
    });
    return result;
}

function updateConfigAfterGcode(entry) {
    configUpdater.requestUpdate();
}

var ConfigTable = React.createClass({
    componentWillMount: function() {
        this.props.controller.setComponent(this);
    },
    onInputEnter: function(option_name) {
        this.optionSet(option_name);
    },
    makeSetOptionGcode: function(option_name) {
        var target = this.props.controller.getValue(option_name);
        var typeImpl = getOptionTypeImpl(this.props.options.obj[option_name].type);
        var convRes = typeImpl.convertForSet(target);
        if ($has(convRes, 'err')) {
            return convRes;
        }
        return {res: 'M926 I'+option_name+' V'+encodeStrForCmd(convRes.res)};
    },
    optionSet: function(option_name) {
        var makeRes = this.makeSetOptionGcode(option_name);
        if ($has(makeRes, 'err')) {
            return showError(makeRes.err);
        }
        sendGcode('Set option', makeRes.res, updateConfigAfterGcode);
        this.props.controller.cancel(option_name);
    },
    allOptionsSet: function() {
        var cmds = [];
        var error = null;
        $.each(this.props.options.arr, function(idx, option) {
            var option_name = option.key;
            if (this.props.controller.isEditing(option_name)) {
                var makeRes = this.makeSetOptionGcode(option_name);
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
            var options = (cmds.length > 1) ? 'options' : 'option';
            sendGcodes('Set '+options, cmds, updateConfigAfterGcode);
            this.props.controller.cancelAll();
        }
    },
    applyConfig: function() {
        sendGcode('Apply config', 'M930');
    },
    saveConfig: function() {
        sendGcode('Save config to SD', 'M500');
    },
    restoreConfig: function() {
        sendGcode('Restore config from SD', 'M501', updateConfigAfterGcode);
    },
    render: function() {
        this.props.controller.rendering(this.props.options.obj);
        var width = '670px';
        var colgroup = (
            <colgroup>
                <col span="1" style={{width: '200px'}} />
                <col span="1" style={{width: '75px'}} />
                <col span="1" style={{width: '150px'}} />
                <col span="1" />
            </colgroup>
        );
        return (
            <div className="flex-column" style={{flexShrink: '1'}}>
                <div className="flex-row">
                    <div className="form-inline">
                        <div className="form-group">
                            <button type="button" className={controlButtonClass('primary')+' control-right-margin'} onClick={this.saveConfig}>Save to SD</button>
                            <button type="button" className={controlButtonClass('primary')} onClick={this.restoreConfig}>Restore from SD</button>
                        </div>
                    </div>
                    <div style={{flexGrow: '1'}}></div>
                </div>
                <table className={controlTableClass} style={{width: width}}>
                    {colgroup}
                    <thead>
                        <tr>
                            <th>Option</th>
                            <th>Type</th>
                            <th>Value</th>
                            <th>{controlAllHead('New value', [
                                {text: 'Set', class: 'success', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allOptionsSet}},
                                {text: 'Apply', highlighted: this.props.configDirty, attrs: {disabled: !this.props.configDirty, onClick: this.applyConfig}}
                            ])}</th>
                        </tr>
                    </thead>
                </table>
                <div style={{overflowY: 'scroll', overflowX: 'hidden', flexShrink: '1'}}>
                    <table className={controlTableClass} style={{width: width}}>
                        {colgroup}
                        <tbody>
                            {$.map(this.props.options.arr, function(option) {
                                var optionName = option.key;
                                return <ConfigRow key={optionName} ref={optionName} option={option.val} parent={this} />;
                            }.bind(this))}
                        </tbody>
                    </table>
                </div>
            </div>
        );
    },
    componentDidUpdate: function() {
        this.props.controller.componentDidUpdate(this.props.options.obj);
    }
});

var ConfigRow = React.createClass({
    render: function() {
        var option = this.props.option;
        var typeImpl = getOptionTypeImpl(option.type);
        var valueParsed = typeImpl.convertForDisp(option.value);
        var valueConv = $has(valueParsed, 'err') ? option.value : valueParsed.res;
        var ecInputs = this.props.parent.props.controller.getRenderInputs(option.name, valueConv);
        var onClickSet = $bind(this.props.parent, 'optionSet', option.name);
        return (
            <tr>
                <td><b>{option.name}</b></td>
                <td>{option.type}</td>
                <td>{valueConv}</td>
                <td>
                    <div className="input-group">
                        {typeImpl.input.type === 'select' ? (
                        <select className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref="target"
                                {...makeEcInputProps(ecInputs)} >
                            {$.map(typeImpl.input.options, function(option_value, optionIndex) {
                                return <option key={optionIndex} value={option_value}>{option_value}</option>;
                            })}
                        </select>
                        ) : (
                        <input type={typeImpl.input.type} className={controlInputClass+' '+ecInputs.class} value={ecInputs.value} ref="target"
                               {...makeEcInputProps(ecInputs)} />
                        )}
                        <span className="input-group-btn">
                            <button type="button" className={controlCancelButtonClass} disabled={!ecInputs.editing} onClick={ecInputs.onCancel} aria-label="Cancel">{removeIcon}</button>
                            <button type="button" className={controlButtonClass('success')} onClick={onClickSet}>Set</button>
                        </span>
                    </div>
                </td>
            </tr>
        );
    },
    shouldComponentUpdate: function(nextProps, nextState) {
        return this.props.parent.props.controller.rowIsDirty(this.props.option.name);
    },
    componentDidUpdate: function() {
        this.props.parent.props.controller.rowComponentDidUpdate(this.props.option.name);
    }
});


// Gcode execution component

var GcodeTable = React.createClass({
    render: function() {
        var width = '670px';
        var colgroup = (
            <colgroup>
                <col span="1" style={{width: '220px'}} />
                <col span="1" style={{width: '190px'}} />
                <col span="1" />
            </colgroup>
        );
        return (
            <div className="flex-column">
                <table className={gcodeTableClass} style={{width: width}}>
                    {colgroup}
                    <thead>
                        <tr>
                            <th>Command</th>
                            <th>User interface action</th>
                            <th>Result</th>
                        </tr>
                    </thead>
                </table>
                <div ref="scroll_div" className="flex-column" style={{overflowY: 'scroll', overflowX: 'hidden', flexShrink: '1', height: '125px'}}>
                    <div style={{flexGrow: '1'}}></div>
                    <table className={gcodeTableClass} style={{width: width}}>
                        {colgroup}
                        <tbody>
                            {$.map(this.props.gcodeHistory, function(entry) {
                                return <GcodeRow key={entry.id} entry={entry} />;
                            }.bind(this))}
                            {$.map(this.props.gcodeQueue, function(entry) {
                                return <GcodeRow key={entry.id} entry={entry} />;
                            }.bind(this))}
                        </tbody>
                    </table>
                </div>
                <div className="flex-row">
                    <GcodeInput width={width} />
                </div>
            </div>
        );
    },
    componentDidUpdate: function() {
        var node = this.refs.scroll_div;
        node.scrollTop = node.scrollHeight;
    }
});

function textToSpans(text) {
    var lines = text.split('\n');
    return linesToSpans(lines);
}

function linesToSpans(lines) {
    var lineToSpan = function(line) { return (<span>{line}<br /></span>); };
    return $.map(lines, lineToSpan);
}

var GcodeRow = React.createClass({
    render: function() {
        var entry = this.props.entry;
        var cmdText = linesToSpans(entry.cmds);
        var result;
        var isError = false;
        if (entry.completed) {
            if (entry.error === null) {
                result = textToSpans($.trim(entry.response));
                isError = /^Error:/gm.test(entry.response);
            } else {
                result = 'Error: '+entry.error;
                isError = true;
            }
        } else {
            result = 'Pending';
        }
        return (
            <tr data-mod2={entry.id%2} data-pending={!entry.completed} data-error={isError}>
                <td>{cmdText}</td>
                <td>{entry.reason}</td>
                <td>{result}</td>
            </tr>
        );
    },
    shouldComponentUpdate: function(nextProps, nextState) {
        return (nextProps.completed !== this.props.completed);
    }
});

var GcodeInput = React.createClass({
    doSendCommand: function() {
        var cmd = this.state.gcodeInput;
        if (cmd !== '') {
            sendGcode('Send command', cmd);
            this.setState({gcodeInput: ''});
        }
    },
    onSendClick: function() {
        this.doSendCommand();
    },
    onInputChange: function(event) {
        this.setState({gcodeInput: event.target.value});
    },
    onInputKeyPress: function(event) {
        if (event.key === 'Enter') {
            this.doSendCommand();
        }
    },
    getInitialState: function() {
        return {gcodeInput: ''};
    },
    render: function() {
        var sendDisabled = (this.state.gcodeInput === '');
        return (
            <div className="input-group" style={{width: this.props.width}}>
                <input type="text" className={controlInputClass} placeholder="Command to send"
                       value={this.state.gcodeInput} onChange={this.onInputChange} onKeyPress={this.onInputKeyPress} />
                <span className="input-group-btn">
                    <button type="button" className={controlButtonClass('primary')} onClick={this.onSendClick} disabled={sendDisabled}>Send</button>
                </span>
            </div>
        );
    }
});


// Field editing logic

function EditController(row_ref) {
    this._comp = null;
    this._row_ref = row_ref;
    this._editing = {};
    this._dirty_all_rows = false;
    this._dirty_rows = {};
}

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

EditController.prototype.rowIsDirty = function(id) {
    return (this._dirty_all_rows || $has(this._dirty_rows, id));
};

EditController.prototype._input = function(id) {
    return this._row_ref.getRowInput(this._comp, id);
};

EditController.prototype._onChange = function(id) {
    var value = this.getValue(id);
    var wasEditingAny = this.isEditingAny();
    this._editing[id] = value;
    this.markDirtyRow(id);
    if (wasEditingAny) {
        this.updateRow(id);
    } else {
        // The combined "set" button may need to change to enabled.
        this.updateTable();
    }
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

EditController.prototype.markDirtyAllRows = function() {
    this._dirty_all_rows = true;
};

EditController.prototype.markDirtyRow = function(id) {
    this._dirty_rows[id] = true;
};

EditController.prototype.updateTable = function() {
    this._comp.forceUpdate();
};

EditController.prototype.updateRow = function(id) {
    this._row_ref.updateRow(this._comp, id);
};

EditController.prototype.forceUpdateVia = function(update_comp) {
    this.markDirtyAllRows();
    update_comp.forceUpdate();
};

EditController.prototype.cancel = function(id) {
    if (this.isEditing(id)) {
        delete this._editing[id];
        this.markDirtyRow(id);
        if (this.isEditingAny()) {
            this.updateRow(id);
        } else {
            // The combined "set" button may need to change to disabled.
            this.updateTable();
        }
    }
};

EditController.prototype.cancelAll = function() {
    $.each(this._editing, function(id, value) {
        this.markDirtyRow(id);
    }.bind(this));
    this._editing = {};
    this.updateTable();
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
        onCancel:   this.cancel.bind(this, id),
        onChange:   this._onChange.bind(this, id),
        onKeyDown:  this._onKeyDown.bind(this, id),
        onKeyPress: this._onKeyPress.bind(this, id)
    };
};

EditController.prototype._forAllDirtyRows = function(id_datas, func) {
    if (this._dirty_all_rows) {
        $.each(id_datas, func);
    } else {
        $.each(this._dirty_rows, function(id) {
            if ($has(id_datas, id)) {
                func(id, id_datas[id]);
            }
        });
    }
};

EditController.prototype._updateRowInput = function(id) {
    var input = this._input(id);
    if (!this.isEditing(id)) {
        input.defaultValue = input.value;
    }
};

EditController.prototype.componentDidUpdate = function(id_datas) {
    this._forAllDirtyRows(id_datas, function(id, data) {
        this._updateRowInput(id);
    }.bind(this));
    this._dirty_all_rows = false;
    this._dirty_rows = {};
};

EditController.prototype.rowComponentDidUpdate = function(id) {
    this._updateRowInput(id);
    delete this._dirty_rows[id];
};

function RowRefSameComp(prefix) {
    return {
        getRowInput: function(comp, id) {
            return comp.refs[prefix+id];
        },
        updateRow: function(comp, id) {
            comp.forceUpdate();
        }
    };
}

function RowRefChildComp(child_ref_name) {
    return {
        getRowInput: function(comp, id) {
            return comp.refs[id].refs[child_ref_name];
        },
        updateRow: function(comp, id) {
            comp.refs[id].forceUpdate();
        }
    };
}

function makeEcInputProps(ecInputs) {
    return {onChange: ecInputs.onChange, onKeyDown: ecInputs.onKeyDown, onKeyPress: ecInputs.onKeyPress};
}

// Gluing of react classes into page

var ComponentWrapper = React.createClass({
    render: function() {
        return this.props.render();
    }
});

var machine_state = {
    speedRatio:  null,
    configDirty: null,
    axes:    preprocessObjectForState({}),
    heaters: preprocessObjectForState({}),
    fans:    preprocessObjectForState({})
};

var machine_options = preprocessObjectForState({});

var controller_axes    = new EditController(RowRefSameComp('target_'));
var controller_heaters = new EditController(RowRefSameComp('target_'));
var controller_fans    = new EditController(RowRefSameComp('target_'));
var controller_speed   = new EditController(RowRefSameComp('target_'));
var controller_config  = new EditController(RowRefChildComp('target'));

function render_axes() {
    return <AxesTable axes={machine_state.axes} probe_present={$has(machine_state, 'bedProbe')} controller={controller_axes} />;
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
    return <Buttons1 />;
}
function render_buttons2() {
    return <Buttons2 />;
}
function render_config() {
    return <ConfigTable options={machine_options} configDirty={machine_state.configDirty} controller={controller_config} />;
}
function render_gcode() {
    return <GcodeTable gcodeHistory={gcodeHistory} gcodeQueue={gcodeQueue} />;
}

var wrapper_axes     = ReactDOM.render(<ComponentWrapper render={render_axes} />,     document.getElementById('axes_div'));
var wrapper_heaters  = ReactDOM.render(<ComponentWrapper render={render_heaters} />,  document.getElementById('heaters_div'));
var wrapper_fans     = ReactDOM.render(<ComponentWrapper render={render_fans} />,     document.getElementById('fans_div'));
var wrapper_speed    = ReactDOM.render(<ComponentWrapper render={render_speed} />,    document.getElementById('speed_div'));
var wrapper_buttons1 = ReactDOM.render(<ComponentWrapper render={render_buttons1} />, document.getElementById('buttons1_div'));
var wrapper_buttons2 = ReactDOM.render(<ComponentWrapper render={render_buttons2} />, document.getElementById('buttons2_div'));
var wrapper_config   = ReactDOM.render(<ComponentWrapper render={render_config} />,   document.getElementById('config_div'));
var wrapper_gcode    = ReactDOM.render(<ComponentWrapper render={render_gcode} />,    document.getElementById('gcode_div'));

function setNewMachineState(new_machine_state) {
    machine_state = new_machine_state;
    machine_state.speedRatio  = $has(machine_state, 'speedRatio') ? machine_state.speedRatio : null;
    machine_state.configDirty = $has(machine_state, 'configDirty') ? machine_state.configDirty : null;
    machine_state.axes    = fixupStateObject(machine_state, 'axes');
    machine_state.heaters = fixupStateObject(machine_state, 'heaters');
    machine_state.fans    = fixupStateObject(machine_state, 'fans');
}

function machineStateChanged() {
    document.getElementById('axes_panel').hidden    = (machine_state.axes.arr.length === 0);
    document.getElementById('heaters_panel').hidden = (machine_state.heaters.arr.length === 0);
    document.getElementById('fans_panel').hidden    = (machine_state.fans.arr.length === 0);
    document.getElementById('speed_panel').hidden   = (machine_state.speedRatio === null);
    document.getElementById('config_panel').hidden  = (machine_state.configDirty === null);
    
    configUpdater.setRunning(machine_state.configDirty !== null);
    
    controller_axes.forceUpdateVia(wrapper_axes);
    controller_heaters.forceUpdateVia(wrapper_heaters);
    controller_fans.forceUpdateVia(wrapper_fans);
    controller_speed.forceUpdateVia(wrapper_speed);
    controller_config.forceUpdateVia(wrapper_config);
}

function updateConfig() {
    controller_config.forceUpdateVia(wrapper_config);
}

function updateGcode() {
    wrapper_gcode.forceUpdate();
}

// Generic status updating

function StatusUpdater(reqPath, refreshInterval, handleNewStatus) {
    this._reqPath = reqPath;
    this._refreshInterval = refreshInterval;
    this._handleNewStatus = handleNewStatus;
    this._reqestInProgress = false;
    this._needsAnotherUpdate = false;
    this._timerId = null;
    this._running = false;
}

StatusUpdater.prototype.setRunning = function(running) {
    if (running) {
        if (!this._running) {
            this._running = true;
            this.requestUpdate();
        }
    } else {
        if (this._running) {
            this._running = false;
            this._stopTimer();
        }
    }
};

StatusUpdater.prototype.requestUpdate = function() {
    if (!this._running) {
        return;
    }
    if (this._reqestInProgress) {
        this._needsAnotherUpdate = true;
    } else {
        this._startRequest();
    }
};

StatusUpdater.prototype._stopTimer = function() {
    if (this._timerId !== null) {
        clearTimeout(this._timerId);
        this._timerId = null;
    }
};

StatusUpdater.prototype._startRequest = function() {
    this._stopTimer();
    this._reqestInProgress = true;
    this._needsAnotherUpdate = false;
    $.ajax({
        url: this._reqPath,
        dataType: 'json',
        cache: false,
        success: function(new_status) {
            this._requestCompleted();
            this._handleNewStatus(new_status);
        }.bind(this),
        error: function(xhr, status, err) {
            console.error(this._reqPath, status, err.toString());
            this._requestCompleted();
        }.bind(this)
    });
};

StatusUpdater.prototype._requestCompleted = function() {
    this._reqestInProgress = false;
    if (!this._running) {
        return;
    }
    if (this._needsAnotherUpdate) {
        this._startRequest();
    } else {
        this._timerId = setTimeout(this._timerHandler.bind(this), this._refreshInterval);
    }
};

StatusUpdater.prototype._timerHandler = function() {
    if (this._running && !this._reqestInProgress) {
        this._startRequest();
    }
}


// Status updating

function fixupStateObject(state, name) {
    return preprocessObjectForState($has(state, name) ? state[name] : {});
}

var statusUpdater = new StatusUpdater('/rr_status', statusRefreshInterval, function(new_machine_state) {
    setNewMachineState(new_machine_state);
    machineStateChanged();
});


// Configuration updating

var configUpdater = new StatusUpdater('/rr_config', configRefreshInterval, function(new_config) {
    machine_options = preprocessObjectForState(preprocessOptionsList(new_config.options));
    updateConfig();
});


// Refresh all info

function startRefreshAll() {
    statusUpdater.requestUpdate();
    configUpdater.requestUpdate();
}


// Gcode execution

var gcodeQueue = [];
var gcodeHistory = [];
var gcodeIdCounter = 1;

function sendGcode(reason, cmd, callback) {
    sendGcodes(reason, [cmd], callback);
}

function sendGcodes(reason, cmds, callback) {
    var entry = {
        id: gcodeIdCounter,
        reason: reason,
        cmds: cmds,
        callback: callback,
        completed: false,
        error: null,
        response: null
    };
    gcodeQueue.push(entry);
    
    gcodeIdCounter = (gcodeIdCounter >= 1000000) ? 1 : (gcodeIdCounter+1);
    
    if (gcodeQueue.length === 1) {
        sendNextQueuedGcodes();
    }
    
    while (gcodeQueue.length + gcodeHistory.length > gcodeHistorySize && gcodeHistory.length > 0) {
        gcodeHistory.shift();
    }
    
    updateGcode();
}

function sendNextQueuedGcodes() {
    var entry = gcodeQueue[0];
    var cmds_disp = entry.cmds.join('; ');
    var cmds_exec = entry.cmds.join('\n')+'\n';
    console.log('>>> '+cmds_disp);
    $.ajax({
        url: '/rr_gcode',
        type: 'POST',
        data: cmds_exec,
        dataType: 'text',
        success: function(response) {
            console.log('<<< '+response);
            currentGcodeCompleted(null, response);
        },
        error: function(xhr, status, err) {
            var error_str = err.toString();
            console.error('/rr_gcode', status, error_str);
            showError('Command "'+cmds_disp+'" failed: '+error_str);
            currentGcodeCompleted(error_str, null);
        }
    });
}

function currentGcodeCompleted(error, response) {
    var entry = gcodeQueue.shift();
    var callback = entry.callback;
    
    entry.callback = null;
    entry.completed = true;
    entry.error = error;
    entry.response = response;
    
    gcodeHistory.push(entry);
    
    if (gcodeQueue.length !== 0) {
        sendNextQueuedGcodes();
    }
    
    updateGcode();
    
    statusUpdater.requestUpdate();
    
    if (callback) {
        callback(entry);
    }
}



// Initial actions

statusUpdater.setRunning(true);
