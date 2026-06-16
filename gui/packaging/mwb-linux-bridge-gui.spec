%global debug_package %{nil}

Name:           mwb-linux-bridge-gui
Version:        0.1.0
Release:        1%{?dist}
Summary:        Electron GUI for MWB Linux Bridge

License:        MIT
URL:            https://github.com/benm-dev/mwb-linux-bridge
Source0:        %{name}-%{version}-linux-unpacked.tar.gz
Source1:        mwb-linux-bridge-gui.desktop
Source2:        mwb-linux-bridge-gui.svg
Source3:        LICENSE

Requires:       mwb-linux-bridge
Requires:       gtk3
Requires:       libnotify
Requires:       nss
Requires:       libXScrnSaver
Requires:       libXtst
Requires:       xdg-utils
Requires:       at-spi2-core

%description
Electron desktop GUI for configuring and running MWB Linux Bridge. The GUI
edits the same XDG config used by the native mwb-client binary and launches
that binary after checking session, uinput, and binary-path status.

%prep
%autosetup

%build
# Prebuilt Electron application directory produced by electron-builder.

%install
install -d %{buildroot}/opt/%{name}
cp -a . %{buildroot}/opt/%{name}/
install -Dpm 0755 /dev/null %{buildroot}%{_bindir}/%{name}
rm %{buildroot}%{_bindir}/%{name}
ln -s ../../opt/%{name}/mwb-linux-bridge-gui %{buildroot}%{_bindir}/%{name}
install -Dpm 0644 %{SOURCE1} %{buildroot}%{_datadir}/applications/mwb-linux-bridge-gui.desktop
install -Dpm 0644 %{SOURCE2} %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/mwb-linux-bridge-gui.svg
install -Dpm 0644 %{SOURCE3} %{buildroot}%{_licensedir}/%{name}/LICENSE

%post
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database %{_datadir}/applications >/dev/null 2>&1 || :
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q %{_datadir}/icons/hicolor >/dev/null 2>&1 || :
fi

%postun
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database %{_datadir}/applications >/dev/null 2>&1 || :
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q %{_datadir}/icons/hicolor >/dev/null 2>&1 || :
fi

%files
%license %{_licensedir}/%{name}/LICENSE
/opt/%{name}
%{_bindir}/%{name}
%{_datadir}/applications/mwb-linux-bridge-gui.desktop
%{_datadir}/icons/hicolor/scalable/apps/mwb-linux-bridge-gui.svg

%changelog
* Tue Jun 16 2026 Ben Local <ben@localhost> - 0.1.0-1
- Initial GUI RPM package.
