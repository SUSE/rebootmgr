#!/bin/sh -ex

opts='-Doptimization=2 -Dwerror=true'

echo 'BEGIN OF BUILD ENVIRONMENT INFORMATION'
cat /etc/os-release
uname -a |head -1
libc="$(ldd /bin/sh |sed -n 's|^[^/]*\(/[^ ]*/libc\.so[^ ]*\).*|\1|p' |head -1)"
$libc |head -1
gcc --version |head -1
meson --version |head -1
ninja --version |head -1
echo 'END OF BUILD ENVIRONMENT INFORMATION'

mkdir build
meson setup $opts build

meson compile -v -C build
mkdir build/destdir
DESTDIR=$(pwd)/build/destdir meson install -C build
meson test -v -C build

if git status --porcelain |grep '^?'; then
	echo >&2 'git status reported untracked files'
	exit 1
fi
