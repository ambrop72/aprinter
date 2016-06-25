
// Hardcoded constants

var statusRefreshInterval = 2000;
var configRefreshInterval = 120000;
var statusWaitingRespTime = 1000;
var configWaitingRespTime = 1500;
var updateConfigAfterSendingGcodeTime = 200;
var axisPrecision = 6;
var heaterPrecision = 4;
var fanPrecision = 3;
var speedPrecision = 4;
var configPrecision = 15;
var defaultSpeed = 50;
var gcodeHistorySize = 20;
var sdRootAccessPrefix = '/sdcard';


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
    arr.sort(function(x, y) { return +(x.key > y.key) - +(x.key < y.key); });
    return arr;
}

function preprocessObjectForState(obj) {
    return {
        obj: obj,
        arr: orderObject(obj)
    };
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

function $startsWith(data, start) {
    return data.substring(0, start.length) === start;
}

function $endsWith(data, end) {
    return data.substring(data.length - end.length, data.length) === end;
}

function encodeStrForCmd(val) {
    val = encodeURIComponent(val);   // URI encoding
    val = val.replace(/%/g, '\\');   // Change % to \
    val = val.replace(/\\2F/g, '/'); // Unescape forward-slashes
    return val;
}

function removeTrailingZerosInNumStr(num_str) {
    // 123.0200 => 123.02
    // 123.0000 => 123
    // 1200 => 1200
    return num_str.replace(/(\.|(\.[0-9]*[1-9]))0*$/, function(match, p1, p2) {
        return typeof(p2) === 'string' ? p2 : '';
    });
}

function makeAjaxErrorStr(status, error) {
    var status_str = status+'';
    var error_str = error+'';
    var join_str = (status_str === '' || error_str === '') ? '' : ': ';
    return status_str+join_str+error_str;
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
        var action = 'Move axis';
        var speedRes = this.getSpeed();
        if ($has(speedRes, 'err')) {
            return showError(action, speedRes.err, null);
        }
        var target = this.props.controller.getNumberValue(axis_name);
        if (isNaN(target)) {
            return showError(action, 'Target value for axis '+axis_name+' is incorrect', null);
        }
        sendGcode(action, 'G0 R F'+speedRes.res.toString()+' '+axis_name+target.toString());
        this.props.controller.cancel(axis_name);
    },
    allAxesGo: function() {
        var cmdAxes = '';
        var reasonAxes = [];
        var error = null;
        $.each(this.props.axes.arr, function(idx, axis) {
            var axis_name = axis.key;
            if (!this.props.controller.isEditing(axis_name)) {
                return;
            }
            reasonAxes.push(axis_name);
            var target = this.props.controller.getNumberValue(axis_name);
            if (isNaN(target)) {
                if (error === null) error = 'Target value for axis '+axis_name+' is incorrect';
                return;
            }
            cmdAxes += ' '+axis_name+target.toString();
        }.bind(this));
        var axes = (reasonAxes.length > 1) ? 'axes' : 'axis';
        var action = 'Move '+axes;
        if (error !== null) {
            return showError(action, error, null);
        }
        var speedRes = this.getSpeed();
        if ($has(speedRes, 'err')) {
            return showError(action, speedRes.err, null);
        }
        if (cmdAxes.length !== 0) {
            sendGcode(action, 'G0 R F'+speedRes.res.toString()+cmdAxes);
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
    componentDidUpdate: function() {
        this.props.controller.componentDidUpdate(this.props.axes.obj);
    },
    render: function() {
        this.props.controller.rendering(this.props.axes.obj);
        
        return (
            <div className="flex-column">
                <div className="flex-row control-bottom-margin">
                    <button type="button" className={controlButtonClass('primary')+' control-right-margin'} onClick={this.btnHomeDefault}>Home</button>
                    {this.props.probe_present &&
                    <button type="button" className={controlButtonClass('primary')+' control-right-margin'} onClick={this.btnBedProbing}>Probe</button>
                    }
                    <button type="button" className={controlButtonClass('primary')} onClick={this.btnMotorsOff}>Motors off</button>
                    <div className="flex-grow1"></div>
                    <label htmlFor="speed" className="control-right-margin control-label">Speed [/s]</label>
                    <input ref="speed" id="speed" type="number" className={controlInputClass} style={{width: '60px'}} defaultValue={defaultSpeed} />
                </div>
                <table className={controlTableClass}>
                    <colgroup>
                        <col span="1" style={{width: '55px'}} />
                        <col span="1" style={{width: '115px'}} />
                        <col span="1" style={{width: '205px'}} />
                    </colgroup>
                    <thead>
                        <tr>
                            <th>Axis</th>
                            <th>Planned</th>
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
        var action = 'Set heater setpoint';
        var makeRes = this.makeSetHeaterGcode(heater_name);
        if ($has(makeRes, 'err')) {
            return showError(action, makeRes.err, null);
        }
        sendGcode(action, makeRes.res);
        this.props.controller.cancel(heater_name);
    },
    heaterOff: function(heater_name) {
        sendGcode('Turn off heater', 'M104 F '+heater_name+' Snan');
        this.props.controller.cancel(heater_name);
    },
    allHeatersSet: function() {
        var cmds = [];
        var error = null;
        var reasonHeaters = [];
        $.each(this.props.heaters.arr, function(idx, heater) {
            var heater_name = heater.key;
            if (this.props.controller.isEditing(heater_name)) {
                reasonHeaters.push(heater_name);
                var makeRes = this.makeSetHeaterGcode(heater_name);
                if ($has(makeRes, 'err')) {
                    if (error === null) error = makeRes.err;
                    return;
                }
                cmds.push(makeRes.res);
            }
        }.bind(this));
        var setpoints = (reasonHeaters.length > 1) ? 'setpoints' : 'setpoint';
        var action = 'Set heater '+setpoints;
        if (error !== null) {
            return showError(action, error, null);
        }
        if (cmds.length !== 0) {
            sendGcodes(action, cmds);
            this.props.controller.cancelAll();
        }
    },
    render: function() {
        this.props.controller.rendering(this.props.heaters.obj);
        return (
            <table className={controlTableClass}>
                <colgroup>
                    <col span="1" style={{width: '55px'}} />
                    <col span="1" style={{width: '60px'}} />
                    <col span="1" style={{width: '78px'}} />
                    <col span="1" style={{width: '185px'}} />
                </colgroup>
                <thead>
                    <tr>
                        <th>Heater</th>
                        <th>Actual</th>
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
        var action = 'Set fan target';
        var makeRes = this.makeSetFanGcode(fan_name);
        if ($has(makeRes, 'err')) {
            return showError(action, makeRes.err, null);
        }
        sendGcode(action, makeRes.res);
        this.props.controller.cancel(fan_name);
    },
    fanOff: function(fan_name) {
        sendGcode('Turn off fan', 'M106 F '+fan_name+' S0');
        this.props.controller.cancel(fan_name);
    },
    allFansSet: function() {
        var cmds = [];
        var error = null;
        var reasonFans = [];
        $.each(this.props.fans.arr, function(idx, fan) {
            var fan_name = fan.key;
            if (this.props.controller.isEditing(fan_name)) {
                reasonFans.push(fan_name);
                var makeRes = this.makeSetFanGcode(fan_name);
                if ($has(makeRes, 'err')) {
                    if (error === null) error = makeRes.err;
                    return;
                }
                cmds.push(makeRes.res);
            }
        }.bind(this));
        var targets = (reasonFans.length > 1) ? 'targets' : 'target';
        var action = 'Set fan '+targets;
        if (error !== null) {
            return showError(action, error, null);
        }
        if (cmds.length !== 0) {
            sendGcodes(action, cmds);
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
        var action = 'Set speed ratio';
        var target = this.props.controller.getNumberValue('S');
        if (isNaN(target)) {
            return showError(action, 'Speed ratio value is incorrect', null);
        }
        sendGcode(action, 'M220 S'+target.toPrecision(speedPrecision+3));
        this.props.controller.cancel('S');
    },
    speedRatioReset: function() {
        sendGcode('Reset speed ratio', 'M220 S100');
        this.props.controller.cancel('S');
    },
    componentDidUpdate: function() {
        this.props.controller.componentDidUpdate({'S': null});
    },
    render: function() {
        this.props.controller.rendering({'S': null});
        var dispRatio = (this.props.speedRatio*100).toPrecision(speedPrecision);
        var ecInputs = this.props.controller.getRenderInputs('S', dispRatio);
        return (
            <table className={controlTableClass}>
                <colgroup>
                    <col span="1" style={{width: '170px'}} />
                    <col span="1" style={{width: '200px'}} />
                </colgroup>
                <thead>
                    <tr>
                        <th>Speed ratio <span className="notbold">[%]</span></th>
                        <th>Control</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>{dispRatio}</td>
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
    }
});


// Panel at the top

var TopPanel = React.createClass({
    render: function() {
        var cmdStatusClass = '';
        var cmdStatusText = '';
        if (this.props.gcodeQueue.length > 0) {
            var entry = this.props.gcodeQueue[0];
            cmdStatusClass = 'constatus-waitresp';
            cmdStatusText = 'Executing: '+entry.reason;
        }
        
        var activeStatusClass = '';
        var activeStatusText = '';
        if (this.props.active === true) {
            activeStatusClass = 'activestatus-active';
            activeStatusText = 'Active';
        }
        else if (this.props.active === false) {
            activeStatusClass = 'activestatus-inactive';
            activeStatusText = 'Inactive';
        }
        
        var condition = this.props.statusUpdater.getCondition();
        var statusText = '';
        var statusClass = '';
        if (condition === 'WaitingResponse') {
            statusText = 'Waiting for machine status';
            statusClass = 'constatus-waitresp';
        }
        else if (condition === 'Error') {
            statusText = 'Communication error';
            statusClass = 'constatus-error';
        }
        
        var fullscreenIcon = this.props.is_fullscreen ? 'glyphicon-unchecked' : 'glyphicon-fullscreen';
        var fullscreenAction = this.props.is_fullscreen ? leaveFullscreen : goFullscreen;
        
        return (
            <div className="flex-row flex-align-center">
                <h1>Aprinter Web Interface</h1>
                <div className="toppanel-spacediv"></div>
                <span className={'activestatus '+activeStatusClass}>{activeStatusText}</span>
                <div className="toppanel-spacediv"></div>
                <span className={'constatus '+cmdStatusClass}>{cmdStatusText}</span>
                <div style={{flexGrow: '1'}}></div>
                <span className={'constatus '+statusClass}>{statusText}</span>
                <div className="toppanel-spacediv"></div>
                <button type="button" className={controlButtonClass('info')+' top-btn-margin'} onClick={startRefreshAll}>Refresh</button>
                <button type="button" className={controlButtonClass('default')+' top-btn-margin'} onClick={() => fullscreenAction()}>
                    <span className={'glyphicon '+fullscreenIcon}></span>
                </button>
            </div>
        );
    }
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
        display: 'bool',
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
        display: 'double',
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
        display: 'ip-addr',
        input: {type: 'text'},
        convertForDisp: function(string) {
            return normalizeIpAddr(string);
        },
        convertForSet: function(string) {
            return normalizeIpAddr(string);
        }
    },
    'mac_addr': {
        display: 'mac-addr',
        input: {type: 'text'},
        convertForDisp: function(string) {
            return normalizeMacAddr(string);
        },
        convertForSet: function(string) {
            return normalizeMacAddr(string);
        }
    },
    'text': {
        display: 'unknown',
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
    configUpdater.requestUpdate(true);
}

var ConfigTab = React.createClass({
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
        var action = 'Set option';
        var makeRes = this.makeSetOptionGcode(option_name);
        if ($has(makeRes, 'err')) {
            return showError(action, makeRes.err, null);
        }
        sendGcode(action, makeRes.res, updateConfigAfterGcode);
        this.props.controller.cancel(option_name);
    },
    allOptionsSet: function() {
        var cmds = [];
        var error = null;
        var reasonOptions = [];
        $.each(this.props.options.arr, function(idx, option) {
            var option_name = option.key;
            if (this.props.controller.isEditing(option_name)) {
                reasonOptions.push(option_name);
                var makeRes = this.makeSetOptionGcode(option_name);
                if ($has(makeRes, 'err')) {
                    if (error === null) error = makeRes.err;
                    return;
                }
                cmds.push(makeRes.res);
            }
        }.bind(this));
        var options = (reasonOptions.length > 1) ? 'options' : 'option';
        var action = 'Set '+options;
        if (error !== null) {
            return showError(action, error, null);
        }
        if (cmds.length !== 0) {
            sendGcodes(action, cmds, updateConfigAfterGcode);
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
    componentDidUpdate: function() {
        this.props.controller.componentDidUpdate(this.props.options.obj);
    },
    render: function() {
        this.props.controller.rendering(this.props.options.obj);
        var condition = this.props.configUpdater.getCondition();
        
        var statusText = '';
        var statusClass = '';
        if (condition === 'WaitingResponse') {
            statusText = 'Waiting for current configuration';
            statusClass = 'constatus-waitresp';
        }
        else if (condition === 'Error') {
            statusText = 'Error refreshing configuration';
            statusClass = 'constatus-error';
        }
        
        var colgroup = (
            <colgroup>
                <col span="1" style={{width: '192px'}} />
                <col span="1" style={{width: '72px'}} />
                <col span="1" style={{width: '232px'}} />
            </colgroup>
        );
        
        return (
            <div className="flex-column flex-shrink1 flex-grow1 min-height0">
                <div className="flex-row control-bottom-margin">
                    <button type="button" className={controlButtonClass('primary')+' control-right-margin'} onClick={this.saveConfig}>Save to SD</button>
                    <button type="button" className={controlButtonClass('primary')} onClick={this.restoreConfig}>Restore from SD</button>
                    <div className="flex-grow1"></div>
                    <span className={'constatus-control '+statusClass}>{statusText}</span>
                </div>
                <div className="flex-row">
                    <div className="flex-grow1" style={{width: 0}}>
                        <table className={controlTableClass}>
                            {colgroup}
                            <thead>
                                <tr>
                                    <th>Option</th>
                                    <th>Type</th>
                                    <th>{controlAllHead('Value', [
                                        {text: 'Set', class: 'success', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allOptionsSet}},
                                        {text: 'Apply', highlighted: this.props.configDirty, attrs: {disabled: !this.props.configDirty, onClick: this.applyConfig}}
                                    ])}</th>
                                </tr>
                            </thead>
                        </table>
                    </div>
                    <div className="scroll-y" style={{visibility: 'hidden'}} />
                </div>
                <div className="flex-shrink1 flex-grow1 scroll-y config-options-area">
                    <table className={controlTableClass}>
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
    }
});

var ConfigRow = React.createClass({
    shouldComponentUpdate: function(nextProps, nextState) {
        return this.props.parent.props.controller.rowIsDirty(this.props.option.name);
    },
    componentDidUpdate: function() {
        this.props.parent.props.controller.rowComponentDidUpdate(this.props.option.name);
    },
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
                <td>{typeImpl.display}</td>
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
    }
});


// SD-card tab

var SdCardTab = React.createClass({
    getInitialState: function() {
        return {
            desiredDir: '/',
            uploadFileName: null,
            destinationPath: '/upload.gcode',
        };
    },
    doMount: function() {
        sendGcode('Mount SD-card', 'M21');
    },
    doUnmount: function() {
        sendGcode('Unmount SD-card', 'M22');
    },
    doMountRw: function() {
        sendGcode('Mount SD-card read-write', 'M21 W');
    },
    doRemountRo: function() {
        sendGcode('Remount SD-card read-only', 'M22 R');
    },
    onDirInputChange: function(event) {
        this.setState({desiredDir: event.target.value});
    },
    onDirInputKeyPress: function(event) {
        if (event.key === 'Enter') {
            this.doNavigateTo(this.state.desiredDir);
        }
    },
    navigateToDesiredDir: function() {
        this.doNavigateTo(this.state.desiredDir);
    },
    onDirUpClick: function() {
        var controller_dirlist = this.props.controller_dirlist;
        var loadedDir = controller_dirlist.getLoadedDir();
        var loadedResult = controller_dirlist.getLoadedResult();
        if (loadedDir !== null && loadedResult.dir !== '/') {
            var parentDir = getParentDirectory(loadedResult.dir);
            this.doNavigateTo(parentDir);
        }
    },
    doNavigateTo: function(desiredDir) {
        if ($startsWith(desiredDir, '/')) {
            desiredDir = removeRedundantSlashes(desiredDir);
            if (this.state.desiredDir !== desiredDir) {
                this.setState({desiredDir: desiredDir});
            }
            this.props.controller_dirlist.requestDir(desiredDir);
            this.forceUpdate();
        }
    },
    onFileInputChange: function(event) {
        var uploadFileName = (event.target.files.length > 0) ? event.target.files[0].name : null;
        // IE raises this event when we manually clear the file-input after
        // a completed upload (in componentDidUpdate), so we need this check
        // so that the success message does not disappear immediately.
        if (uploadFileName !== null) {
            this.setState({uploadFileName: uploadFileName});
            this.props.controller_upload.clearResult();
        }
    },
    onDestinationInputChange: function(event) {
        this.setState({destinationPath: event.target.value});
        this.props.controller_upload.clearResult();
    },
    onUploadClick: function(event) {
        var controller_upload = this.props.controller_upload;
        if (!controller_upload.isUploading() && this.refs.file_input.files.length > 0 && isDestPathValid(this.state.destinationPath)) {
            var file = this.refs.file_input.files[0];
            controller_upload.startUpload(file.name, removeRedundantSlashes(this.state.destinationPath), file);
            this.forceUpdate();
        }
    },
    setDestinationPath: function(file_path) {
        this.setState({destinationPath: file_path});
    },
    componentDidUpdate: function() {
        var controller_upload = this.props.controller_upload;
        if (controller_upload.isResultPending()) {
            controller_upload.ackResult();
            var dest_path = controller_upload.getDestinationPath();
            var loaded_dir = this.props.controller_dirlist.getLoadedDir();
            if (loaded_dir !== null && pathIsInDirectory(dest_path, loaded_dir)) {
                this.doNavigateTo(loaded_dir);
            }
            if (controller_upload.getUploadError() === null) {
                this.refs.file_input.value = null;
                this.setState({uploadFileName: null});
            }
        }
    },
    render: function() {
        var sdcard = this.props.sdcard;
        var controller_dirlist = this.props.controller_dirlist;
        var controller_upload = this.props.controller_upload;
        
        var canMount = (sdcard !== null && sdcard.mntState === 'NotMounted');
        var canUnmount = (sdcard !== null && sdcard.mntState === 'Mounted');
        var canMountRw = (sdcard !== null && (sdcard.mntState === 'NotMounted' || (sdcard.mntState === 'Mounted' && sdcard.rwState === 'ReadOnly')));
        var canUnmountRo = (sdcard !== null && sdcard.mntState === 'Mounted' && sdcard.rwState == 'ReadWrite');
        var stateText = (sdcard === null) ? 'Disabled' : ((sdcard.mntState === 'Mounted') ? translateRwState(sdcard.rwState) : translateMntState(sdcard.mntState));
        
        var canNavigate = $startsWith(this.state.desiredDir, '/');
        var loadedDir = controller_dirlist.getLoadedDir();
        var loadedResult = controller_dirlist.getLoadedResult();
        var canGoUp = (loadedDir !== null && loadedResult.dir !== '/');
        var loadingDir = controller_dirlist.getLoadingDir();
        
        var uploadFileText = (this.state.uploadFileName !== null) ? this.state.uploadFileName : 'No file chosen';
        var uploadFileClass = (this.state.uploadFileName !== null) ? 'label-info' : 'label-default';
        var isUploading = controller_upload.isUploading();
        var canUpload = (!isUploading && this.state.uploadFileName !== null && isDestPathValid(this.state.destinationPath));
        
        var uploadStatus;
        var uploadStatusClass;
        var uploadedFile = null;
        if (isUploading) {
            var srcFileName = controller_upload.getSourceFileName();
            var destPath = controller_upload.getDestinationPath();
            var totalBytes = controller_upload.getTotalBytes();
            var uploadedBytes = controller_upload.getUploadedBytes();
            var percent = ((totalBytes == 0 ? 0 : (uploadedBytes / totalBytes)) * 100.0).toFixed(0);
            uploadStatus = 'Uploading '+srcFileName+' to '+destPath+' ('+percent+'%, '+uploadedBytes+'/'+totalBytes+')';
            uploadStatusClass = 'upload-status-running';
        }
        else if (controller_upload.haveResult()) {
            var srcFileName = controller_upload.getSourceFileName();
            var destPath = controller_upload.getDestinationPath();
            var error = controller_upload.getUploadError();
            var result = (error !== null) ? ('failed: '+error) : 'succeeded';
            uploadStatus = 'Upload of '+srcFileName+' to '+destPath+' '+result+'.';
            uploadStatusClass = (error !== null) ? 'upload-status-error' : 'upload-status-success';
            if (error === null) {
                uploadedFile = destPath;
            }
        }
        else {
            uploadStatus = 'Upload not started.';
            uploadStatusClass = '';
        }
        
        return (
            <div className="flex-column flex-shrink1 flex-grow1 min-height0">
                <div className="flex-row control-bottom-margin">
                    <button type="button" className={controlButtonClass('primary')+' control-right-margin'} disabled={!canUnmount}   onClick={this.doUnmount}>Unmount</button>
                    <button type="button" className={controlButtonClass('primary')+' control-right-margin'} disabled={!canMount}     onClick={this.doMount}>Mount</button>
                    <button type="button" className={controlButtonClass('primary')+' control-right-margin'} disabled={!canUnmountRo} onClick={this.doRemountRo}>Remount R/O</button>
                    <button type="button" className={controlButtonClass('primary')+' control-right-margin'}                         disabled={!canMountRw}   onClick={this.doMountRw}>Mount R/W</button>
                    <span className="flex-grow1 sdcard-state">{stateText}</span>
                </div>
                <div className="flex-column control-bottom-margin">
                    <div className="flex-row flex-align-center control-bottom-margin">
                        <div className="control-right-margin">
                            <label className="btn btn-default btn-file control-button control-right-margin">
                                Select file
                                <input ref="file_input" type="file" style={{display: 'none'}} onChange={this.onFileInputChange} />
                            </label>
                            <span className={'label '+uploadFileClass}>{uploadFileText}</span>
                        </div>
                        <label htmlFor="upload_path" className="control-label control-right-margin">Save to</label>
                        <input type="text" className={controlInputClass+' flex-grow1 control-right-margin'} style={{width: '160px'}}
                               value={this.state.destinationPath} onChange={this.onDestinationInputChange}
                               placeholder="Destination file path" />
                        <button type="button" className={controlButtonClass('primary')} onClick={this.onUploadClick} disabled={!canUpload}>Upload</button>
                    </div>
                    <div className="flex-row flex-align-center">
                        <span className={uploadStatusClass+ ' flex-shrink1'}>
                            {uploadStatus}
                            {(uploadedFile === null) ? null : ' '}
                            {(uploadedFile === null) ? null : (
                            <a href="javascript:void(0)" onClick={() => executeSdCardFile(uploadedFile)}>Execute</a>
                            )}
                        </span>
                        <div className="flex-grow1"></div>
                        {(loadingDir === null) ? null : <span className='dirlist-loading-text constatus-waitresp'>Loading directory {loadingDir}</span>}
                    </div>
                </div>
                <div className="control-bottom-margin" style={{display: 'table', width: '100%'}}>
                    <div style={{display: 'table-cell', 'width': '100%'}}>
                        <div className="input-group control-right-margin">
                            <label htmlFor="sdcard_set_dir" className="input-group-addon control-ig-label">Show directory</label>
                            <input id="sdcard_set_dir" type="text" className={controlInputClass} placeholder="Directory to list"
                                   value={this.state.desiredDir} onChange={this.onDirInputChange} onKeyPress={this.onDirInputKeyPress} />
                            <span className="input-group-btn">
                                <button type="button" className={controlButtonClass('primary')} disabled={!canNavigate} onClick={this.navigateToDesiredDir}>Show</button>
                            </span>
                        </div>
                    </div>
                    <span style={{display: 'table-cell', width: '1%', verticalAlign: 'middle'}}>
                        <button type="button" className={controlButtonClass('primary')} disabled={!canGoUp} onClick={this.onDirUpClick}>Up</button>
                    </span>
                </div>
                <SdCardDirList dirlist={loadedResult} navigateTo={this.doNavigateTo} saveTo={this.setDestinationPath} controller={this.props.controller_dirlist} />
            </div>
        );
    }
});

var SdCardDirList = React.createClass({
    onDirClicked: function(dir_path) {
        this.props.navigateTo(dir_path);
    },
    onUploadOverClicked: function(file_path) {
        this.props.saveTo(file_path);
    },
    shouldComponentUpdate: function(nextProps, nextState) {
        return this.props.controller.isDirty();
    },
    render: function() {
        this.props.controller.clearDirty();
        var type_width = '65px';
        var dirlist = this.props.dirlist;
        if (dirlist === null) {
            dirlist = {dir: 'No directory shown', files: []};
        }
        var files_sorted = Array.prototype.slice.call(dirlist.files).sort();
        var dir = dirlist.dir;
        return (
            <div className="flex-column flex-shrink1 flex-grow1 min-height0">
                <table className={controlTableClass}>
                    <colgroup>
                        <col span="1" style={{width: type_width}} />
                        <col span="1" style={{width: '60px'}} />
                        <col span="1" />
                    </colgroup>
                    <thead>
                        <tr>
                            <th>Type</th>
                            <th>Name</th>
                            <th style={{textAlign: 'right'}}>{dirlist.dir}</th>
                        </tr>
                    </thead>
                </table>
                <div ref="scroll_div" className="flex-shrink1 flex-grow1 scroll-y dir-list-area">
                    <table className={controlTableClass}>
                        <colgroup>
                            <col span="1" style={{width: type_width}} />
                            <col span="1" />
                        </colgroup>
                        <tbody>
                            {$.map(files_sorted, function(file, file_idx) {
                                var is_dir = $startsWith(file, '*');
                                var file_type;
                                var file_name;
                                if (is_dir) {
                                    file_type = 'Folder';
                                    file_name = file.substring(1);
                                } else {
                                    file_type = 'File';
                                    file_name = file;
                                }
                                var file_path = dir+($endsWith(dir, '/')?'':'/')+file_name;
                                return (
                                    <tr key={file_path} className="dirlist-row">
                                        <td>{file_type}</td>
                                        {is_dir ? (
                                            <td className="control-table-link">
                                                <a href="javascript:void(0)" onClick={this.onDirClicked.bind(this, file_path)}>{file_name}</a>
                                            </td>
                                        ) : (
                                            <td>
                                                <div className="flex-row">
                                                    <div className="flex-grow1 files-right-margin">{file_name}</div>
                                                    <a className="files-right-margin" href={sdRootAccessPrefix+file_path} target="_blank">Download</a>
                                                    <a className="files-right-margin" href="javascript:void(0)" onClick={this.onUploadOverClicked.bind(this, file_path)}>Upload over</a>
                                                    <a href="javascript:void(0)" onClick={() => executeSdCardFile(file_path)}>Execute</a>
                                                </div>
                                            </td>
                                        )}
                                    </tr>
                                );
                            }.bind(this))}
                        </tbody>
                    </table>
                </div>
            </div>
        );
    },
    componentDidUpdate: function() {
        var node = this.refs.scroll_div;
        node.scrollTop = 0;
    }
});

function translateMntState(mntState) {
    if (mntState === 'NotMounted') return 'Not mounted';
    if (mntState === 'Mounting')   return 'Mounting';
    return mntState;
}

function translateRwState(rwState) {
    if (rwState === 'ReadOnly')     return 'Mounted R/O';
    if (rwState === 'MountingRW')   return 'Mounting R/W';
    if (rwState === 'ReadWrite')    return 'Mounted R/W';
    if (rwState === 'RemountingRO') return 'Remounting R/O';
    return rwState;
}

function removeRedundantSlashes(str) {
    str = str.replace(/\/\/+/g, '/');
    if (str.length > 1) {
        str = str.replace(/\/$/, '');
    }
    return str;
}

function isDestPathValid(str: string) {
    return ($startsWith(str, '/') && !$endsWith(str, '/'));
}

function pathIsInDirectory(path: string, dir_path: string) {
    var dir_prefix = $endsWith(dir_path, '/') ? dir_path : dir_path+'/';
    return $startsWith(path, dir_prefix);
}

function getParentDirectory(path: string) {
    var parentDir = path.replace(/\/[^\/]+$/, '');
    return (path !== '' && parentDir === '') ? '/' : parentDir;
}

function executeSdCardFile(file_path) {
    showConfirmDialog('Confirm execution of file from SD-card', file_path, 'Cancel', 'Execute', () => {
        var cmd = 'M32 F'+encodeStrForCmd(file_path);
        sendGcode('Execute file', cmd);
    });
}


// Gcode execution component

var GcodeTable = React.createClass({
    render: function() {
        var colgroup = (
            <colgroup>
                <col span="1" style={{width: '190px'}} />
                <col span="1" />
            </colgroup>
        );
        return (
            <div className="flex-column flex-grow1">
                <table className={gcodeTableClass}>
                    {colgroup}
                    <thead>
                        <tr>
                            <th>Command</th>
                            <th>Result</th>
                        </tr>
                    </thead>
                </table>
                <div ref="scroll_div" className="flex-column flex-shrink1 flex-grow1 scroll-y gcode-table-area">
                    <div style={{flexGrow: '1'}}></div>
                    <table className={gcodeTableClass} style={{width: '100%'}}>
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
                    <GcodeInput />
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
    shouldComponentUpdate: function(nextProps, nextState) {
        return this.props.entry.dirty;
    },
    componentDidUpdate: function() {
        this.props.entry.dirty = false;
    },
    render: function() {
        var entry = this.props.entry;
        var cmdText = linesToSpans(entry.cmds);
        
        var result = $.trim(entry.response);
        var resultExtra = !entry.completed ? '(pending)' : (entry.error !== null) ? 'Error: '+entry.error : null;
        if (resultExtra !== null) {
            if (result.length !== 0) {
                result += '\n';
            }
            result += resultExtra;
        }
        result = textToSpans(result);
        
        return (
            <tr data-mod2={entry.id%2} data-pending={!entry.completed} data-error={entry.isError} title={'User action: '+entry.reason}>
                <td>{cmdText}</td>
                <td>{result}</td>
            </tr>
        );
    },
    
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
            <div className="input-group gcode-input-group">
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


// Generic status updating

function StatusUpdater(reqPath, refreshInterval, waitingRespTime, handleNewStatus, handleCondition) {
    this._reqPath = reqPath;
    this._refreshInterval = refreshInterval;
    this._waitingRespTime = waitingRespTime;
    this._handleNewStatus = handleNewStatus;
    this._handleCondition = handleCondition;
    this._reqestInProgress = false;
    this._needsAnotherUpdate = false;
    this._timerId = null;
    this._waitingTimerId = null;
    this._running = false;
    this._condition = 'Disabled';
}

StatusUpdater.prototype.getCondition = function() {
    return this._condition;
};

StatusUpdater.prototype.setRunning = function(running) {
    if (running) {
        if (!this._running) {
            this._running = true;
            this.requestUpdate(true);
        }
    } else {
        if (this._running) {
            this._running = false;
            this._changeCondition('Disabled');
            this._stopTimer();
            this._stopWaitingTimer();
            this._handleCondition();
        }
    }
};

StatusUpdater.prototype.requestUpdate = function(setWaiting) {
    if (!this._running) {
        return;
    }
    if (setWaiting) {
        this._changeCondition('WaitingResponse');
        this._stopWaitingTimer();
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

StatusUpdater.prototype._stopWaitingTimer = function() {
    if (this._waitingTimerId !== null) {
        clearTimeout(this._waitingTimerId);
        this._waitingTimerId = null;
    }
};

StatusUpdater.prototype._startRequest = function() {
    this._stopTimer();
    this._reqestInProgress = true;
    this._needsAnotherUpdate = false;
    this._waitingTimerId = setTimeout(this._waitingTimerHandler.bind(this), this._waitingRespTime);
    
    $.ajax({
        url: this._reqPath,
        dataType: 'json',
        cache: false,
        success: function(new_status) {
            this._requestCompleted(true, new_status);
        }.bind(this),
        error: function(xhr, status, err) {
            this._requestCompleted(false, null);
        }.bind(this)
    });
};

StatusUpdater.prototype._requestCompleted = function(success, new_status) {
    this._reqestInProgress = false;
    if (!this._running) {
        return;
    }
    this._stopWaitingTimer();
    if (!(this._condition === 'WaitingResponse' && this._needsAnotherUpdate)) {
        this._changeCondition(success ? 'Okay' : 'Error');
    }
    if (this._needsAnotherUpdate) {
        this._startRequest();
    } else {
        this._timerId = setTimeout(this._timerHandler.bind(this), this._refreshInterval);
    }
    if (success) {
        this._handleNewStatus(new_status);
    }
};

StatusUpdater.prototype._timerHandler = function() {
    if (this._running && !this._reqestInProgress) {
        this._startRequest();
    }
}

StatusUpdater.prototype._waitingTimerHandler = function() {
    if (this._running && this._waitingTimerId !== null) {
        this._waitingTimerId = null;
        if (this._condition !== 'Error') {
            this._changeCondition('WaitingResponse');
        }
    }
}

StatusUpdater.prototype._changeCondition = function(condition) {
    if (this._condition !== condition) {
        this._condition = condition;
        this._handleCondition();
    }
}


// Gcode execution

var gcodeQueue = [];
var gcodeHistory = [];
var gcodeIdCounter = 1;
var gcodeStatusUpdateTimer = null;

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
        response: '',
        isError: false,
        dirty: true,
    };
    gcodeQueue.push(entry);
    
    gcodeIdCounter = (gcodeIdCounter >= 1000000) ? 1 : (gcodeIdCounter+1);
    
    if (gcodeQueue.length === 1) {
        _sendNextQueuedGcodes();
    }
    
    while (gcodeQueue.length + gcodeHistory.length > gcodeHistorySize && gcodeHistory.length > 0) {
        gcodeHistory.shift();
    }
    
    updateGcode();
}

function _sendNextQueuedGcodes() {
    var entry = gcodeQueue[0];
    var cmds_disp = entry.cmds.join('; ');
    var cmds_exec = entry.cmds.join('\n')+'\n';
    
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/rr_gcode', true);
    xhr.setRequestHeader('Content-Type', 'text/plain');
    xhr.addEventListener('progress', evt => _gcodeXhrProgressEvent(entry, xhr, evt), false);
    xhr.addEventListener('load', evt => _gcodeXhrLoadEvent(entry, xhr, evt), false);
    xhr.addEventListener('error', evt => _gcodeXhrErrorEvent(entry, evt), false);
    xhr.send(cmds_exec);
    
    // Start a timer to request an updated machine status shortly,
    // unless the gcode completes sooner. The intention is that
    // the active/inactive status on the top-panel doesn't display
    // "inactive" for too long after the command made the machine active.
    gcodeStatusUpdateTimer = setTimeout(_gcodeStatusUpdateTimerHandler, updateConfigAfterSendingGcodeTime);
}

function _checkGcodeRequestInProgress(entry) {
    return (gcodeQueue.length > 0 && gcodeQueue[0] === entry);
}

function _gcodeStatusUpdateTimerHandler() {
    gcodeStatusUpdateTimer = null;
    statusUpdater.requestUpdate(false);
}

function _gcodeXhrProgressEvent(entry, xhr, evt) {
    if (!_checkGcodeRequestInProgress(entry)) {
        return;
    }
    
    if (xhr.responseText !== null) {
        entry.response = xhr.responseText;
        entry.dirty = true;
        
        updateGcode();
    }
}

function _gcodeXhrLoadEvent(entry, xhr, evt) {
    var error = (xhr.status === 200) ? null : ''+xhr.status+' '+xhr.statusText;
    _currentGcodeCompleted(entry, error, xhr.responseText);
}

function _gcodeXhrErrorEvent(entry, evt) {
    _currentGcodeCompleted(entry, 'Network error', null);
}

function _currentGcodeCompleted(entry, error, response) {
    if (!_checkGcodeRequestInProgress(entry)) {
        return;
    }
    
    if (gcodeStatusUpdateTimer !== null) {
        clearTimeout(gcodeStatusUpdateTimer);
        gcodeStatusUpdateTimer = null;
    }
    
    var entry = gcodeQueue.shift();
    var callback = entry.callback;
    
    entry.callback = null;
    entry.completed = true;
    entry.error = error;
    if (response !== null) {
        entry.response = response;
    }
    entry.isError = error !== null || /^Error:/gm.test(entry.response);
    entry.dirty = true;
    
    gcodeHistory.push(entry);
    
    if (entry.isError) {
        var head_str;
        var body_str;
        if (entry.error !== null) {
            head_str = 'Communication error';
            body_str = entry.error;
        } else {
            head_str = 'The machine responded with:';
            body_str = entry.response;
        }
        showError(entry.reason, head_str, body_str);
    }
    
    if (gcodeQueue.length !== 0) {
        _sendNextQueuedGcodes();
    }
    
    updateGcode();
    
    statusUpdater.requestUpdate(false);
    
    if (callback) {
        callback(entry);
    }
}


// Error message display

//var dialogIsOpen = false;
var currentDialogInfo = null;
var dialogQueue = [];

var dialogModal = $('#dialog_modal');
var dialogModalLabel   = document.getElementById('dialog_modal_label');
var dialogModalBody    = document.getElementById('dialog_modal_body');
var dialogModalClose   = document.getElementById('dialog_modal_close');
var dialogModalConfirm = document.getElementById('dialog_modal_confirm');

function showError(action_str: string, head_str: string, body_str: string = null) {
    var haveHead = (head_str !== null);
    var haveBody = (body_str !== null);
    
    console.error('Error in '+action_str+'.'+(haveHead?' '+head_str:'')+(haveBody?'\n'+body_str:''));
    
    var label_str = 'Error in "'+action_str+'".'+(haveHead ? '\n'+head_str : '');
    
    _queueDialog({
        label_str: label_str,
        body_str: body_str,
        close_text: 'Close',
        confirm_text: null,
        confirm_action: null,
    });
}

function showConfirmDialog(label_str: string, body_str: string, cancel_text: string, confirm_text: string, confirm_action: () => void) {
    _queueDialog({
        label_str: label_str,
        body_str: body_str,
        close_text: cancel_text,
        confirm_text: confirm_text,
        confirm_action: confirm_action,
    });
}

function _queueDialog(info) {
    if (currentDialogInfo === null) {
        _showDialog(info);
    } else {
        dialogQueue.push(info);
    }
}

function _showDialog(info) {
    var haveBody = info.body_str !== null;
    
    dialogModalLabel.innerText = info.label_str;
    dialogModalBody.innerText = haveBody ? info.body_str : '';
    dialogModalBody.hidden = !haveBody;
    dialogModalClose.innerText = info.close_text;
    dialogModalConfirm.hidden = info.confirm_action === null;
    dialogModalConfirm.innerText = (info.confirm_text === null) ? 'Confirm' : info.confirm_text;
    dialogModalConfirm.onclick = () => _dialogConfirmClicked(info);
    
    dialogModal.modal({});
    
    currentDialogInfo = info;
}

dialogModal.on('hidden.bs.modal', function() {
    dialogModalLabel.innerText = '';
    dialogModalBody.innerText = '';
    dialogModalConfirm.innerText = '';
    dialogModalConfirm.onclick = null;
    
    currentDialogInfo = null;
    
    if (dialogQueue.length > 0) {
        var info = dialogQueue.shift();
        _showDialog(info);
    }
});

function _dialogConfirmClicked(info) {
    if (currentDialogInfo !== info) {
        return;
    }
    
    dialogModal.modal('hide');
    
    if (info.confirm_action !== null) {
        var confirm_action = info.confirm_action;
        info.confirm_action = null;
        confirm_action();
    }
}


// Directory list controller.

class DirListController {
    private _handle_dir_loaded: () => void;
    private _requested_dir: string;
    private _need_rerequest: boolean;
    private _update_status_then: boolean;
    private _loaded_dir: string;
    private _loaded_result: any;
    private _is_dirty: boolean;
    private _ever_requested: boolean;
    
    constructor(handle_dir_loaded: () => void) {
        this._handle_dir_loaded = handle_dir_loaded;
        this._requested_dir = null;
        this._need_rerequest = false;
        this._update_status_then = false;
        this._loaded_dir = null;
        this._loaded_result = null;
        this._is_dirty = true;
        this._ever_requested = false;
    }
    
    requestDir(requested_dir: string) {
        var old_dir = this._requested_dir;
        this._requested_dir = requested_dir;
        if (old_dir !== null) {
            this._need_rerequest = true;
        } else {
            this._startRequest();
        }
    }
    
    getLoadingDir(): string {
        return this._requested_dir;
    }
    
    getLoadedDir(): string {
        return this._loaded_dir;
    }
    
    getLoadedResult(): any {
        return this._loaded_result;
    }
    
    getEverRequested(): boolean {
        return this._ever_requested;
    }
    
    isDirty(): boolean {
        return this._is_dirty;
    }
    
    clearDirty() {
        this._is_dirty = false;
    }
    
    private _startRequest() {
        this._need_rerequest = false;
        this._update_status_then = (machine_state.sdcard !== null && machine_state.sdcard.mntState !== 'Mounted');
        this._ever_requested = true;
        
        $.ajax({
            url: '/rr_files?flagDirs=1&dir='+encodeURIComponent(this._requested_dir),
            dataType: 'json',
            cache: false,
            success: function(files_resp) {
                this._requestCompleted(true, files_resp, null);
            }.bind(this),
            error: function(xhr, status, error) {
                this._requestCompleted(false, null, makeAjaxErrorStr(status, error));
            }.bind(this)
        });
    }
    
    private _requestCompleted(success: boolean, files_resp: any, error: string) {
        if (this._update_status_then) {
            statusUpdater.requestUpdate(false);
        }
        if (this._need_rerequest) {
            this._startRequest();
            return;
        }
        var requested_dir = this._requested_dir;
        this._requested_dir = null;
        if (!success) {
            showError('Load directory '+requested_dir, error, null);
            return;
        }
        this._loaded_dir = requested_dir;
        this._loaded_result = files_resp;
        this._is_dirty = true;
        this._handle_dir_loaded();
    }
}


// File upload controller

class FileUploadController {
    private _handle_update: () => void;
    private _uploading: boolean;
    private _sourceFileName: string;
    private _destinationPath: string;
    private _totalBytes: number;
    private _uploadedBytes: number;
    private _resultPending: boolean;
    private _haveResult: boolean;
    private _uploadError: string;
    
    constructor(handle_update: () => void) {
        this._handle_update = handle_update;
        this._uploading = false;
        this._sourceFileName = null;
        this._destinationPath = null;
        this._totalBytes = 0;
        this._uploadedBytes = 0;
        this._resultPending = false;
        this._haveResult = false;
        this._uploadError = null;
    }
    
    isUploading(): boolean {
        return this._uploading;
    }
    
    getSourceFileName(): string {
        return this._sourceFileName;
    }
    
    getDestinationPath(): string {
        return this._destinationPath;
    }
    
    getTotalBytes(): number {
        return this._totalBytes;
    }
    
    getUploadedBytes(): number {
        return this._uploadedBytes;
    }
    
    isResultPending(): boolean {
        return this._resultPending;
    }
    
    haveResult(): boolean {
        return this._haveResult;
    }
    
    getUploadError(): string {
        return this._uploadError;
    }
    
    ackResult() {
        console.assert(this._resultPending);
        
        this._resultPending = false;
    }
    
    clearResult() {
        if (!this._uploading) {
            this._sourceFileName = null;
            this._destinationPath = null;
            this._resultPending = false;
            this._haveResult = false;
            this._uploadError = null;
        }
    }
    
    startUpload(sourceFileName: string, destinationPath: string, data: Blob) {
        console.assert(!this._uploading);
        
        this.clearResult();
        
        this._uploading = true;
        this._sourceFileName = sourceFileName;
        this._destinationPath = destinationPath;
        this._totalBytes = data.size;
        this._uploadedBytes = 0;
        
        $.ajax({
            type: 'POST',
            url: '/rr_upload?name='+encodeURIComponent(destinationPath),
            data: data,
            processData: false,
            contentType: false,
            xhr: function() {
                var xhr = $.ajaxSettings.xhr();
                xhr.upload.addEventListener('progress', evt => this._responseProgress(evt), false);
                return xhr;
            }.bind(this),
            success: function(result) {
                this._requestCompleted(null);
            }.bind(this),
            error: function(xhr, status, error) {
                this._requestCompleted(makeAjaxErrorStr(status, error));
            }.bind(this),
        });
    }
    
    private _responseProgress(evt) {
        if (this._uploading) {
            if (evt.lengthComputable) {
                this._totalBytes = evt.total;
            }
            this._uploadedBytes = evt.loaded;
            
            this._handle_update();
        }
    }
    
    private _requestCompleted(error: string) {
        console.assert(this._uploading);
        
        if (error !== null) {
            showError('Upload file '+this._sourceFileName+' to '+this._destinationPath, error, null);
        }
        
        this._uploading = false;
        this._resultPending = true;
        this._haveResult = true;
        this._uploadError = error;
        
        this._handle_update();
    }
}


// Status updating

function fixupStateObject(state, name) {
    return preprocessObjectForState($has(state, name) ? state[name] : {});
}

function handleNewStatus(new_machine_state) {
    setNewMachineState(new_machine_state);
    machineStateChanged();
    if (configUpdater.getCondition() === 'Error') {
        configUpdater.requestUpdate(false);
    }
}

function handleStatusCondition() {
    wrapper_toppanel.forceUpdate();
}

var statusUpdater = new StatusUpdater('/rr_status', statusRefreshInterval, statusWaitingRespTime, handleNewStatus, handleStatusCondition);


// Configuration updating

function handleNewConfig(new_config) {
    machine_options = preprocessObjectForState(preprocessOptionsList(new_config.options));
    updateConfig();
}

function handleConfigCondition() {
    updateConfig();
}

var configUpdater = new StatusUpdater('/rr_config', configRefreshInterval, configWaitingRespTime, handleNewConfig, handleConfigCondition);


// Instantiating the dirlist controller

function handleDirLoaded() {
    wrapper_sdcard.forceUpdate();
}

var controller_dirlist = new DirListController(handleDirLoaded);


// Instantiating the file upload controller

function handleUploadUpdate() {
    wrapper_sdcard.forceUpdate();
}

var controller_upload = new FileUploadController(handleUploadUpdate);


// Fullscreen logic

var fullscreen_elem = document.body;

function isFullscreen() {
    var fullscreenElement = document.fullscreenElement || document.webkitFullscreenElement || document.mozFullScreenElement || document.msFullscreenElement;
    return fullscreenElement ? true : false;
}

function goFullscreen() {
    var requestFullscreen = fullscreen_elem.requestFullscreen || fullscreen_elem.webkitRequestFullscreen || fullscreen_elem.mozRequestFullScreen || fullscreen_elem.msRequestFullscreen;
    if (requestFullscreen) {
        requestFullscreen.call(fullscreen_elem);
    }
}

function leaveFullscreen() {
    var exitFullscreen = document.exitFullscreen || document.webkitExitFullscreen || document.mozCancelFullScreen || document.msExitFullscreen;
    if (exitFullscreen) {
        exitFullscreen.call(document);
    }
}

function onFullscreenChange(evt) {
    wrapper_toppanel.forceUpdate();
}
for (var eventName of ['fullscreenchange', 'webkitfullscreenchange', 'mozfullscreenchange', 'MSFullscreenChange']) {
    document.addEventListener(eventName, onFullscreenChange, false);
}


// Gluing things together

var ComponentWrapper = React.createClass({
    render: function() {
        return this.props.render();
    }
});

var machine_state = {
    active:      null,
    speedRatio:  null,
    configDirty: null,
    sdcard:      null,
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
function render_toppanel() {
    return <TopPanel statusUpdater={statusUpdater} gcodeQueue={gcodeQueue} active={machine_state.active} is_fullscreen={isFullscreen()} />;
}
function render_config() {
    return <ConfigTab options={machine_options} configDirty={machine_state.configDirty} controller={controller_config} configUpdater={configUpdater} />;
}
function render_sdcard() {
    return <SdCardTab ref="component" sdcard={machine_state.sdcard} controller_dirlist={controller_dirlist} controller_upload={controller_upload} />;
}
function render_gcode() {
    return <GcodeTable gcodeHistory={gcodeHistory} gcodeQueue={gcodeQueue} />;
}

var wrapper_axes     = ReactDOM.render(<ComponentWrapper render={render_axes} />,     document.getElementById('axes_div'));
var wrapper_heaters  = ReactDOM.render(<ComponentWrapper render={render_heaters} />,  document.getElementById('heaters_div'));
var wrapper_fans     = ReactDOM.render(<ComponentWrapper render={render_fans} />,     document.getElementById('fans_div'));
var wrapper_speed    = ReactDOM.render(<ComponentWrapper render={render_speed} />,    document.getElementById('speed_div'));
var wrapper_toppanel = ReactDOM.render(<ComponentWrapper render={render_toppanel} />, document.getElementById('toppanel_div'));
var wrapper_config   = ReactDOM.render(<ComponentWrapper render={render_config} />,   document.getElementById('config_div'));
var wrapper_sdcard   = ReactDOM.render(<ComponentWrapper render={render_sdcard} />,   document.getElementById('sdcard_div'));
var wrapper_gcode    = ReactDOM.render(<ComponentWrapper render={render_gcode} />,    document.getElementById('gcode_div'));

function setNewMachineState(new_machine_state) {
    machine_state = new_machine_state;
    machine_state.active      = $has(machine_state, 'active') ? machine_state.active : null;
    machine_state.speedRatio  = $has(machine_state, 'speedRatio') ? machine_state.speedRatio : null;
    machine_state.configDirty = $has(machine_state, 'configDirty') ? machine_state.configDirty : null;
    machine_state.sdcard      = $has(machine_state, 'sdcard') ? machine_state.sdcard : null
    machine_state.axes    = fixupStateObject(machine_state, 'axes');
    machine_state.heaters = fixupStateObject(machine_state, 'heaters');
    machine_state.fans    = fixupStateObject(machine_state, 'fans');
}

function machineStateChanged() {
    document.getElementById('axes_panel').hidden    = (machine_state.axes.arr.length === 0);
    document.getElementById('heaters_panel').hidden = (machine_state.heaters.arr.length === 0);
    document.getElementById('fans_panel').hidden    = (machine_state.fans.arr.length === 0);
    document.getElementById('speed_panel').hidden   = (machine_state.speedRatio === null);
    document.getElementById('config_tab').hidden    = (machine_state.configDirty === null);
    document.getElementById('sdcard_tab').hidden    = (machine_state.sdcard === null);
    
    updateTabs();
    
    configUpdater.setRunning(machine_state.configDirty !== null);
    
    controller_axes.forceUpdateVia(wrapper_axes);
    controller_heaters.forceUpdateVia(wrapper_heaters);
    controller_fans.forceUpdateVia(wrapper_fans);
    controller_speed.forceUpdateVia(wrapper_speed);
    controller_config.forceUpdateVia(wrapper_config);
    
    wrapper_toppanel.forceUpdate();
    wrapper_sdcard.forceUpdate();
    
    if (machine_state.sdcard !== null && !controller_dirlist.getEverRequested()) {
        wrapper_sdcard.refs.component.navigateToDesiredDir();
    }
}

function updateConfig() {
    controller_config.forceUpdateVia(wrapper_config);
}

function updateGcode() {
    wrapper_toppanel.forceUpdate();
    wrapper_gcode.forceUpdate();
}

function startRefreshAll() {
    statusUpdater.requestUpdate(true);
    configUpdater.requestUpdate(true);
}

function updateTabs() {
    // If there is no tab selected and there is at least one non-hidden tab,
    // activate the first non-hidden tab.
    var active_tab = $('#main_tabs_ul>.active');
    if (active_tab.length == 0) {
        var nonhidden_tabs = $('#main_tabs_ul>*:not([hidden])');
        if (nonhidden_tabs.length > 0) {
            nonhidden_tabs.first().find('>a').tab('show');
        }
    }
}


// Initial actions

statusUpdater.setRunning(true);
