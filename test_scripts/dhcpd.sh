
DIR="$(dirname "${BASH_SOURCE[0]}")"

exec dhcpd -f -d -lf /var/lib/dhcpd-tap9.leases -pf /run/dhcpd-tap9.pid -cf "$DIR/dhcpd.conf" tap9
