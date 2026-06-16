#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

name="mwb-linux-bridge-gui"
version="$(node -p "require('./package.json').version")"
topdir="${RPM_TOPDIR:-/tmp/${name}-rpmbuild}"
repo_root="$(cd .. && pwd)"

npm run dist:dir

rm -rf "${topdir}"
mkdir -p "${topdir}/BUILD" "${topdir}/BUILDROOT" "${topdir}/RPMS" "${topdir}/SOURCES" "${topdir}/SPECS" "${topdir}/SRPMS"

tar -C dist \
  --transform "s#^linux-unpacked#${name}-${version}#" \
  -czf "${topdir}/SOURCES/${name}-${version}-linux-unpacked.tar.gz" \
  linux-unpacked

cp packaging/mwb-linux-bridge-gui.spec "${topdir}/SPECS/"
cp packaging/mwb-linux-bridge-gui.desktop "${topdir}/SOURCES/"
cp packaging/mwb-linux-bridge-gui.svg "${topdir}/SOURCES/"
cp "${repo_root}/LICENSE" "${topdir}/SOURCES/LICENSE"

rpmbuild --define "_topdir ${topdir}" -ba "${topdir}/SPECS/mwb-linux-bridge-gui.spec"

mkdir -p "${repo_root}/dist"
cp "${topdir}/RPMS/"*"/${name}-${version}-"*.rpm "${repo_root}/dist/"
cp "${topdir}/SRPMS/${name}-${version}-"*.src.rpm "${repo_root}/dist/"

echo "Built GUI RPMs in ${repo_root}/dist"
