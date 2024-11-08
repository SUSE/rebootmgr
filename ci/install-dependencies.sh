#!/bin/sh -ex

packages="
meson
libdbus-1-dev
libeconf-dev
"
retry_if_failed()
{
	for i in `seq 0 3`; do
		"$@" && i= && break || sleep 1
	done
	[ -z "$i" ]
}

updated=
apt_get_install()
{
	[ -n "$updated" ] || {
		retry_if_failed sudo apt-get -qq update
		updated=1
	}
	retry_if_failed sudo \
		apt-get -qq --no-install-suggests --no-install-recommends \
		install -y "$@"
}

apt_get_install $packages

