
var statusTableWidth = "400px";
var statusRefreshInterval = 5000;

function orderObject(obj) {
    var arr = $.map(obj, function(val, key) { return {key: key, val: val}; });
    arr.sort(function(x, y) { return (x.key > y.key) - (x.key < y.key); });
    return arr;
}

var AxesTable = React.createClass({render: function() { return (
    <table className="table table-condensed" style={{tableLayout: 'fixed', width: statusTableWidth}}>
        <colgroup>
            <col span="1" style={{width: '55px'}} />
            <col span="1" style={{width: '115px'}} />
            <col span="1" />
            <col span="1" style={{width: '150px'}} />
        </colgroup>
        <thead>
            <tr>
                <th className="nowrap">Axis</th>
                <th className="nowrap">Position (planned)</th>
                <th></th>
                <th className="nowrap">Go to</th>
            </tr>
        </thead>
        <tbody>
            {$.map(orderObject(this.props.axes), function(axis) { return (
                <tr key={axis.key}>
                    <td><b>{axis.key}</b></td>
                    <td>{axis.val.pos.toPrecision(6)}</td>
                    <td></td>
                    <td>
                        <div className="input-group">
                            <input type="number" className="form-control" defaultValue="0" />
                            <span className="input-group-btn">
                                <button type="button" className="btn btn-warning">Go</button>
                            </span>
                        </div>
                    </td>
                </tr>
            );})}
        </tbody>
    </table>
);}});

var HeatersTable = React.createClass({render: function() { return (
    <table className="table table-condensed" style={{tableLayout: 'fixed', width: statusTableWidth}}>
        <colgroup>
            <col span="1" style={{width: '55px'}} />
            <col span="1" style={{width: '78px'}} />
            <col span="1" />
            <col span="1" style={{width: '183px'}} />
        </colgroup>
        <thead>
            <tr>
                <th className="nowrap">Heater</th>
                <th className="nowrap">Actual [C]</th>
                <th className="nowrap">Target [C]</th>
                <th className="nowrap">Control [C]</th>
            </tr>
        </thead>
        <tbody>
            {$.map(orderObject(this.props.heaters), function(heater) { return (
                <tr key={heater.key}>
                    <td><b>{heater.key}</b></td>
                    <td>{heater.val.current.toPrecision(4)}</td>
                    <td>{((heater.val.target < -1000) ? "off" : heater.val.target.toPrecision(4)) + (heater.val.error ? " ERR" : "")}</td>
                    <td>
                        <div className="input-group">
                            <input type="number" className="form-control" defaultValue="220" />
                            <span className="input-group-btn">
                                <button type="button" className="btn btn-warning">Set</button>
                                <button type="button" className="btn btn-primary">Off</button>
                            </span>
                        </div>
                    </td>
                </tr>
            );})}
        </tbody>
    </table>
);}});

var FansTable = React.createClass({render: function() { return (
    <table className="table table-condensed" style={{tableLayout: 'fixed', width: statusTableWidth}}>
        <colgroup>
            <col span="1" style={{width: '55px'}} />
            <col span="1" style={{width: '83px'}} />
            <col span="1" />
            <col span="1" style={{width: '183px'}} />
        </colgroup>
        <thead>
            <tr>
                <th className="nowrap">Fan</th>
                <th className="nowrap">Target [%]</th>
                <th></th>
                <th className="nowrap">Control [%]</th>
            </tr>
        </thead>
        <tbody>
            {$.map(orderObject(this.props.fans), function(fan) { return (
                <tr key={fan.key}>
                    <td><b>{fan.key}</b></td>
                    <td>{(fan.val.target * 100).toPrecision(3)}</td>
                    <td></td>
                    <td>
                        <div className="input-group">
                            <input type="number" className="form-control" defaultValue="100" />
                            <span className="input-group-btn">
                                <button type="button" className="btn btn-warning">Set</button>
                                <button type="button" className="btn btn-primary">Off</button>
                            </span>
                        </div>
                    </td>
                </tr>
            );})}
        </tbody>
    </table>
);}});

var StateWrapper = React.createClass({
    getInitialState: function() {
        return this.props.initial_state;
    },
    render: function() {
        return this.props.render(this.state);
    }
});

var initial_state = {
    axes: {},
    heaters: {},
    fans: {}
};

function render_axes(state) {
    return <AxesTable axes={state.axes} />;
}
function render_heaters(state) {
    return <HeatersTable heaters={state.heaters} />;
}
function render_fans(state) {
    return <FansTable fans={state.fans} />;
}

var axes_wrapper = ReactDOM.render(
    <StateWrapper initial_state={initial_state} render={render_axes} />,
    document.getElementById('axes_div')
);
var heaters_wrapper = ReactDOM.render(
    <StateWrapper initial_state={initial_state} render={render_heaters} />,
    document.getElementById('heaters_div')
);
var fans_wrapper = ReactDOM.render(
    <StateWrapper initial_state={initial_state} render={render_fans} />,
    document.getElementById('fans_div')
);

function scheduleNextStatus() {
    setTimeout(requestStatus, statusRefreshInterval);
}

function requestStatus() {
    $.ajax({
        url: '/rr_status',
        dataType: 'json',
        cache: false,
        success: function(status) {
            scheduleNextStatus();
            axes_wrapper.setState(status);
            heaters_wrapper.setState(status);
            fans_wrapper.setState(status);
        },
        error: function(xhr, status, err) {
            scheduleNextStatus();
            console.error('/rr_status', status, err.toString());
        }
    });
}

requestStatus();
