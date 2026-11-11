#!/bin/sh
set -eu

DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITPRO

if [ ! -f "$DEVKITPRO/switchvars.sh" ]; then
  echo "devkitPro switchvars.sh not found at $DEVKITPRO" >&2
  exit 1
fi
. "$DEVKITPRO/switchvars.sh"

for tool in tar patch sha256sum make autoconf automake libtoolize; do
  command -v "$tool" >/dev/null 2>&1 || {
    echo "Required build tool is missing: $tool" >&2
    exit 1
  }
done

WORK="${RUNNER_TEMP:-/tmp}/untis-next-switch-curl"
mkdir -p "$WORK"
cd "$WORK"

fetch_file() {
  url="$1"
  output="$2"
  if command -v curl >/dev/null 2>&1; then
    curl --fail --location --silent --show-error "$url" -o "$output"
  elif command -v wget >/dev/null 2>&1; then
    wget -q "$url" -O "$output"
  else
    echo "Neither curl nor wget is available to download the pinned dependency." >&2
    exit 1
  fi
}

fetch_file "https://curl.se/download/curl-7.69.1.tar.xz" "curl-7.69.1.tar.xz"
printf '%s  %s\n' \
  "03c7d5e6697f7b7e40ada1b2256e565a555657398e6c1fcfa4cb251ccd819d4f" \
  "curl-7.69.1.tar.xz" | sha256sum --check -

fetch_file "https://raw.githubusercontent.com/devkitPro/pacman-packages/master/switch/curl/switch-curl.patch" "switch-curl.patch"
printf '%s  %s\n' \
  "723c7d884fc7c39ae1a3115ba245bb8c1415da47bbd60ab8f943ca98f92ebc9a" \
  "switch-curl.patch" | sha256sum --check -

tar -xf curl-7.69.1.tar.xz
cd curl-7.69.1
patch -Np1 -i ../switch-curl.patch

# curl 7.69.1 contains an invalid AC_REQUIRE([AC_RUN_IFELSE]) wrapper.
# Remove the exact upstream-bug line so configure works when cross-compiling
# for aarch64-none-elf (curl/curl#5126; upstream fix: curl/curl#5130).
BROKEN_CROSS_COMPILE_LINE='AC_REQUIRE([AC_RUN_IFELSE])dnl'
if ! grep -Fq "$BROKEN_CROSS_COMPILE_LINE" m4/curl-functions.m4; then
  echo "Expected curl 7.69.1 cross-compile bug line was not found." >&2
  exit 1
fi
grep -Fv "$BROKEN_CROSS_COMPILE_LINE" m4/curl-functions.m4 > m4/curl-functions.m4.tmp
mv m4/curl-functions.m4.tmp m4/curl-functions.m4
if grep -Fq "$BROKEN_CROSS_COMPILE_LINE" m4/curl-functions.m4; then
  echo "Could not remove curl cross-compile bug line." >&2
  exit 1
fi

./buildconf

export LDFLAGS="-specs=$DEVKITPRO/libnx/switch.specs ${LDFLAGS:-}"
./configure \
  --prefix="$PORTLIBS_PREFIX" \
  --host=aarch64-none-elf \
  --disable-shared \
  --enable-static \
  --disable-ipv6 \
  --disable-unix-sockets \
  --disable-manual \
  --disable-ntlm-wb \
  --disable-threaded-resolver \
  --without-ssl \
  --without-polar-ssl \
  --without-cyassl \
  --without-wolfssl \
  --without-mbedtls \
  --with-libnx \
  --with-default-ssl-backend=libnx

make -C lib -j2
make -C lib install
make -C include install
make install-binSCRIPTS install-pkgconfigDATA

test -s "$PORTLIBS_PREFIX/lib/libcurl.a"
echo "switch-curl installed at $PORTLIBS_PREFIX"
