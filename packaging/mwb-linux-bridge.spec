Name:           mwb-linux-bridge
Version:        0.1.0
Release:        1%{?dist}
Summary:        PowerToys Mouse Without Borders compatible Linux bridge

License:        MIT
URL:            https://github.com/benm-dev/mwb-linux-bridge
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  openssl-devel
Requires:       openssl-libs
Requires:       bash
Requires:       iproute

%description
Native C++17 Linux bridge for interoperating with Microsoft PowerToys
Mouse Without Borders over the local network. It injects input through
Linux uinput and speaks the PowerToys-compatible TCP protocol.

%prep
%autosetup

%build
mkdir -p build
%{__cxx} %{optflags} -std=c++17 -D_FORTIFY_SOURCE=3 -fstack-protector-strong -fPIE -I src \
    src/main.cpp \
    src/CryptoHelper.cpp \
    src/InputManager.cpp \
    src/NetworkManager.cpp \
    %{build_ldflags} -pie -lssl -lcrypto -lpthread \
    -o build/mwb_client

%install
install -Dpm 0755 build/mwb_client %{buildroot}%{_bindir}/mwb-client
install -Dpm 0755 tui/mwb-tui %{buildroot}%{_bindir}/mwb-tui
install -Dpm 0644 packaging/90-mwb-client-uinput.rules %{buildroot}/usr/lib/udev/rules.d/90-mwb-client-uinput.rules

%post
if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules >/dev/null 2>&1 || :
fi

%files
%license LICENSE
%doc README.md
%doc packaging/README.Fedora.md
%doc packaging/config.example
%{_bindir}/mwb-client
%{_bindir}/mwb-tui
/usr/lib/udev/rules.d/90-mwb-client-uinput.rules

%changelog
* Tue Jun 16 2026 Ben Local <ben@localhost> - 0.1.0-1
- Initial local RPM package.
