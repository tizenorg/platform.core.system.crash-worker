Name:      crash-worker
Summary:    Crash-manager
Version:    0.2.0
Release:    1
Group:      Framework/system
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001:    crash-worker.manifest
BuildRequires:  pkgconfig(dlog)
BuildRequires:  cmake

Requires(post): coreutils
Requires(post): tar
Requires(post): gzip

%description
crash-manager

%prep
%setup -q

%build
cp %{SOURCE1001} .

export CFLAGS+=" -Werror"

%cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}/opt/usr/share/crash
mkdir -p %{buildroot}/opt/usr/share/crash/dump

%install_service sysinit.target.wants crash-manager.service

%files
%license LICENSE
%manifest crash-worker.manifest
%defattr(-,system,system,-)
%dir /opt/usr/share/crash
%dir /opt/usr/share/crash/dump
%attr(0755,system,system)/usr/bin/dump_systemstate
%{_bindir}/crash-manager.sh
%{_bindir}/set_corepattern.sh
%{_unitdir}/crash-manager.service
%{_unitdir}/sysinit.target.wants/crash-manager.service
