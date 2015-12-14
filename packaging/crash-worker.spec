Name:      crash-worker
Summary:    Crash-manager
Version:    0.2.0
Release:    1
Group:      Framework/system
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001:    crash-worker.manifest
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  cmake

Requires(post): coreutils
Requires(post): tar
Requires(post): gzip

%description
crash-manager

%prep
%setup -q

#Path to store logs and coredump files
%define crash_root_path %{TZ_SYS_SHARE}/crash
%define crash_path %{crash_root_path}/dump

%build
cp %{SOURCE1001} .

export CFLAGS+=" -Werror"

%cmake . \
	   -DCMAKE_INSTALL_PREFIX=%{_prefix} \
	   -DTZ_SYS_BIN=%{TZ_SYS_BIN} \
	   -DCRASH_PATH=%{crash_path}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}%{crash_root_path}
mkdir -p %{buildroot}%{crash_path}

%files
%license LICENSE
%manifest crash-worker.manifest
%defattr(-,system,system,-)
%dir %{crash_root_path}
%dir %{crash_path}
%attr(0755,system,system) %{_bindir}/dump_systemstate
%{_bindir}/crash-manager.sh
%{_prefix}/lib/sysctl.d/99-crash-manager.conf
