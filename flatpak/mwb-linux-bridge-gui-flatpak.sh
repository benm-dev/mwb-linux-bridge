#!/bin/sh
export ELECTRON_OZONE_PLATFORM_HINT="${ELECTRON_OZONE_PLATFORM_HINT:-auto}"
exec /app/mwb-linux-bridge-gui/mwb-linux-bridge-gui --no-sandbox "$@"
