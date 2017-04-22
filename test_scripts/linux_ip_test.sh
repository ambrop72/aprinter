#!/usr/bin/env bash

set -e
set -x

# TAP interface
ip tuntap add dev tap9 mode tap user ambro
ifconfig tap9 192.168.64.1/24

# Queing setup with netem.
tc qdisc add dev tap9 root netem delay 1ms
modprobe ifb
ip link set dev ifb0 up
tc qdisc add dev tap9 ingress
tc filter add dev tap9 parent ffff: protocol ip u32 match u32 0 0 flowid 1:1 action mirred egress redirect dev ifb0
tc qdisc add dev ifb0 root netem delay 1ms

# Extra network namespaces and interfaces for testing PMTUD.
ip netns add atest1
ip netns add atest2
ip link add aveth0 type veth peer name aveth1
ip link add aveth2 type veth peer name aveth3
ip link set aveth1 netns atest1
ip link set aveth2 netns atest1
ip link set aveth3 netns atest2
ifconfig aveth0 192.168.65.129/25
ip netns exec atest1 ifconfig aveth1 192.168.65.130/25
ip netns exec atest1 ifconfig aveth2 192.168.65.65/26
ip netns exec atest2 ifconfig aveth3 192.168.65.66/26
ip route add 192.168.65.0/24 via 192.168.65.130
ip netns exec atest1 ip route add 0.0.0.0/0 via 192.168.65.129
ip netns exec atest1 ip route add 192.168.65.0/25 via 192.168.65.66
ip netns exec atest2 ip route add 0.0.0.0/0 via 192.168.65.65
iptables -A FORWARD -i tap9 -o aveth0 -j ACCEPT
iptables -A FORWARD -i aveth0 -o tap9 -j ACCEPT
ip link set aveth0 mtu 1400
ip netns exec atest1 ip link set aveth1 mtu 1400
ip netns exec atest2 tc qdisc add dev aveth3 root netem delay 2ms

# Possible commands for testing
# python -B upload.py -s 4096 -l 10000 | pv | ip netns exec atest2 nc 192.168.64.10 80
# ip netns exec atest2 su - ambro -c wireshark
# ip netns exec atest2 curl http://192.168.64.10/downloadTest >/dev/null
