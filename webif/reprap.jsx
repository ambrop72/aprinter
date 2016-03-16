var AxisStatus = React.createClass({
    render: function() {
        return (
            <div className="axisStatus">
                Axis {this.props.name}: {this.props.pos}
            </div>
        );
    }
});

function orderObject(obj) {
    var arr = $.map(obj, function(val, key) { return {key: key, val: val}; });
    arr.sort(function(x, y) { return (x.key > y.key) - (x.key < y.key); });
    return arr;
}

var StatusBox = React.createClass({
    getInitialState: function() {
        return {axes: {}, heaters: {}, fans: {}};
    },
    render: function() {
        var axesNodes = $.map(orderObject(this.state.axes), function(axis) {
            return (
                <AxisStatus name={axis.key} pos={axis.val.pos} />
            );
        });
        return (
            <div className="statusBox">
                <div className="statusSection">
                    <h1>Axes</h1>
                    {axesNodes}
                </div>
            </div>
        );
    }
});

var status_box = ReactDOM.render(
    <StatusBox />,
    document.getElementById('content')
);

var statusRefreshInterval = 5000;

function scheduleNextStatus() {
    setTimeout(requestStatus, statusRefreshInterval);
}

function requestStatus() {
    $.ajax({
        url: '/rr_status',
        dataType: 'json',
        cache: false,
        success: function(status) {
            status_box.setState(status);
            scheduleNextStatus();
        },
        error: function(xhr, status, err) {
            console.error('/rr_status', status, err.toString());
            scheduleNextStatus();
        }
    });
}

requestStatus();
