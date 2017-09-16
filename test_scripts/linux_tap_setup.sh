#!/usr/bin/env bash

DEV=tap9
ADDR=192.168.64.1/24
USER=ambro
CREATE_NETEM=0
NETEM_OUT=(delay 1ms)
NETEM_IN=(delay 1ms)

# don't change this, ifb0 is created by modprobe
IFB_DEV=ifb0

if [[ $1 = "init" ]]; then
    set -e
    set -x
    ip tuntap add dev "$DEV" mode tap user "$USER"
    ip addr add "$ADDR" dev "$DEV"
    ip link set "$DEV" up
    if [[ $CREATE_NETEM = 1 ]]; then
        tc qdisc add dev "$DEV" root netem "${NETEM_OUT[@]}"
        modprobe ifb
        ip link set dev "$IFB_DEV" up
        tc qdisc add dev "$DEV" ingress
        tc filter add dev "$DEV" parent ffff: protocol ip u32 match u32 0 0 flowid 1:1 action mirred egress redirect dev "$IFB_DEV"
        tc qdisc add dev "$IFB_DEV" root netem "${NETEM_IN[@]}"
    fi
elif [[ $1 = "cleanup" ]]; then
    set -x
    ip tuntap del dev "$DEV" mode tap
else
    echo "ERROR: Must call with init or close!"
    exit 1
fi
