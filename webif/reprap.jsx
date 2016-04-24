
// Hardcoded constants

var statusRefreshInterval = 2000;
var configRefreshInterval = 120000;
var axisPrecision = 6;
var heaterPrecision = 4;
var fanPrecision = 3;
var speedPrecision = 4;
var configPrecision = 15;


// Commonly used styles/elements

var controlTableClass = 'table table-condensed table-striped control-table';
var controlInputClass = 'form-control control-input';
var controlButtonClass = function(type) { return 'btn btn-'+type+' control-button'; }
var controlEditingClass = 'control-editing';
var controlCancelButtonClass = controlButtonClass('default')+' control-cancel-button';

var removeIcon = <span className="glyphicon glyphicon-remove" style={{verticalAlign: 'middle', marginTop: '-0.1em'}} aria-hidden="true"></span>;

function controlAllTable(labelText, buttons) {
    return (
        <table className="control-all-table">
            <tbody>
                <tr>
                    <td style={{verticalAlign: 'bottom'}}>{labelText}</td>
                    <td style={{float: 'right'}}>
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
        this.props.controller.rendering(this.props.axes.obj);
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
                        <th>{controlAllTable('Go to', [{text: 'Go', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allAxesGo}}])}</th>
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
            sendGcodes(cmds);
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
                        <th>{controlAllTable('Control', [{text: 'Set', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allHeatersSet}}])}</th>
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
            sendGcodes(cmds);
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
                        <th>{controlAllTable('Control', [{text: 'Set', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allFansSet}}])}</th>
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
        sendGcode(makeRes.res, function(status) { configUpdater.requestUpdate(); });
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
            sendGcodes(cmds, function(status) { configUpdater.requestUpdate(); });
            this.props.controller.cancelAll();
        }
    },
    applyConfig: function() {
        sendGcode('M930');
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
            <div className="flex-column">
                <table className={controlTableClass} style={{width: width}}>
                    {colgroup}
                    <thead>
                        <tr>
                            <th>Option</th>
                            <th>Type</th>
                            <th>Value</th>
                            <th>{controlAllTable('New value', [
                                {text: 'Set', class: 'success', attrs: {disabled: !this.props.controller.isEditingAny(), onClick: this.allOptionsSet}},
                                {text: 'Apply', highlighted: this.props.configDirty, attrs: {disabled: !this.props.configDirty, onClick: this.applyConfig}}
                            ])}</th>
                        </tr>
                    </thead>
                </table>
                <div style={{overflowY: 'scroll', overflowX: 'hidden'}}>
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
    speedRatio: 1,
    configDirty: false,
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
function render_config() {
    return <ConfigTable options={machine_options} configDirty={machine_state.configDirty} controller={controller_config} />;
}

var wrapper_axes     = ReactDOM.render(<ComponentWrapper render={render_axes} />,     document.getElementById('axes_div'));
var wrapper_heaters  = ReactDOM.render(<ComponentWrapper render={render_heaters} />,  document.getElementById('heaters_div'));
var wrapper_fans     = ReactDOM.render(<ComponentWrapper render={render_fans} />,     document.getElementById('fans_div'));
var wrapper_speed    = ReactDOM.render(<ComponentWrapper render={render_speed} />,    document.getElementById('speed_div'));
var wrapper_buttons1 = ReactDOM.render(<ComponentWrapper render={render_buttons1} />, document.getElementById('buttons1_div'));
var wrapper_buttons2 = ReactDOM.render(<ComponentWrapper render={render_buttons2} />, document.getElementById('buttons2_div'));
var wrapper_config   = ReactDOM.render(<ComponentWrapper render={render_config} />,   document.getElementById('config_div'));

function updateStatus() {
    controller_axes.forceUpdateVia(wrapper_axes);
    controller_heaters.forceUpdateVia(wrapper_heaters);
    controller_fans.forceUpdateVia(wrapper_fans);
    controller_speed.forceUpdateVia(wrapper_speed);
    wrapper_buttons1.forceUpdate();
    controller_config.forceUpdateVia(wrapper_config);
}

function updateConfig() {
    controller_config.forceUpdateVia(wrapper_config);
}

// Generic status updating

function StatusUpdater(reqPath, refreshInterval, handleNewStatus) {
    this._reqPath = reqPath;
    this._refreshInterval = refreshInterval;
    this._handleNewStatus = handleNewStatus;
    this._reqestInProgress = false;
    this._needsAnotherUpdate = false;
    this._timerId = null;
}

StatusUpdater.prototype.requestUpdate = function() {
    if (this._reqestInProgress) {
        this._needsAnotherUpdate = true;
    } else {
        this._startRequest();
    }
};

StatusUpdater.prototype._startRequest = function() {
    if (this._timerId !== null) {
        clearTimeout(this._timerId);
        this._timerId = null;
    }
    this._reqestInProgress = true;
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
    if (this._needsAnotherUpdate) {
        this._needsAnotherUpdate = false;
        this._startRequest();
    } else {
        this._timerId = setTimeout(this._timerHandler.bind(this), this._refreshInterval);
    }
};

StatusUpdater.prototype._timerHandler = function() {
    if (!this._reqestInProgress) {
        this._startRequest();
    }
}


// Status updating

function fixupStateObject(state, name) {
    return preprocessObjectForState($has(state, name) ? state[name] : {});
}

var statusUpdater = new StatusUpdater('/rr_status', statusRefreshInterval, function(new_machine_state) {
    machine_state = new_machine_state;
    machine_state.speedRatio = $has(machine_state, 'speedRatio') ? machine_state.speedRatio : 1.0;
    machine_state.configDirty = $has(machine_state, 'configDirty') ? machine_state.configDirty : false;
    machine_state.axes    = fixupStateObject(machine_state, 'axes');
    machine_state.heaters = fixupStateObject(machine_state, 'heaters');
    machine_state.fans    = fixupStateObject(machine_state, 'fans');
    updateStatus();
});


// Configuration updating

var configUpdater = new StatusUpdater('/rr_config', configRefreshInterval, function(new_config) {
    machine_options = preprocessObjectForState(preprocessOptionsList(new_config.options));
    updateConfig();
});


// Gcode execution

var gcodeQueue = [];

function sendGcode(cmd, callback) {
    gcodeQueue.push({cmd: cmd, callback: callback});
    if (gcodeQueue.length === 1) {
        sendNextQueuedGcode();
    }
}

function sendGcodes(cmds, callback) {
    sendGcode(cmds.join('\n'), callback);
}

function sendNextQueuedGcode() {
    var entry = gcodeQueue[0];
    console.log('>>> '+entry.cmd);
    $.ajax({
        url: '/rr_gcode',
        type: 'POST',
        data: entry.cmd + '\n',
        dataType: 'text',
        success: function(response) {
            console.log('<<< '+response);
            currentGcodeCompleted();
            statusUpdater.requestUpdate();
            if (entry.callback) {
                entry.callback(true);
            }
        },
        error: function(xhr, status, err) {
            console.error('/rr_gcode', status, err.toString());
            currentGcodeCompleted();
            showError('Error sending gcode: '+entry.cmd);
            if (entry.callback) {
                entry.callback(false);
            }
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
    statusUpdater.requestUpdate();
    configUpdater.requestUpdate();
}


// Initial actions

statusUpdater.requestUpdate();
configUpdater.requestUpdate();
