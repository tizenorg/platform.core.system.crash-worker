%define sys_assert on

Name:      crash-worker
Summary:    Crash-manager
Version:    0.2.0
Release:    1
Group:      Framework/system
License:    Apache-2.0 and PD
Source0:    %{name}-%{version}.tar.gz
Source1001:    crash-worker.manifest
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  cmake
%if "%{?sys_assert}" == "on"
BuildRequires:  pkgconfig(libunwind)
%else
BuildRequires:  libelf-devel libelf
BuildRequires:  libebl-devel libebl
BuildRequires:  libdw-devel libdw
%endif

Requires(post): coreutils
Requires(post): tar
Requires(post): gzip
Requires: libebl

%description
crash-manager

%prep
%setup -q

#Path to store logs and coredump files
%define crash_root_path %{TZ_SYS_SHARE}/crash
%define crash_path      %{TZ_SYS_CRASH}
%define crash_temp      %{crash_root_path}/temp

%build
cp %{SOURCE1001} .

export CFLAGS+=" -Werror"

%ifarch %{arm}
export CFLAGS+=" -DARM"
%else
%ifarch %{ix86}
export CFLAGS+=" -DX86"
%endif
%endif


%cmake . \
	   -DCMAKE_INSTALL_PREFIX=%{_prefix} \
	   -DTZ_SYS_BIN=%{TZ_SYS_BIN} \
	   -DCRASH_PATH=%{crash_path} \
	   -DCRASH_TEMP=%{crash_temp} \
	   -DCRASH_PIPE_PATH=%{_libexecdir}/crash-pipe \
	   -DCRASH_STACK_PATH=%{_libexecdir}/crash-stack \
	   -DSYS_ASSERT=%{sys_assert}

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}%{crash_root_path}
mkdir -p %{buildroot}%{crash_path}
mkdir -p %{buildroot}%{crash_temp}



%if "%{?sys_assert}" == "on"

%post
if [ ! -d /.build ]; then
	orig="%{_libdir}/libsys-assert.so"
	pattern=$(echo $orig | sed -e 's|/|\\/|g')
	ret=$(sed -n "/${pattern}/p"  %{_sysconfdir}/ld.so.preload)
	if [ -z "$ret" ]; then
		echo "%{_libdir}/libsys-assert.so" >> %{_sysconfdir}/ld.so.preload
	fi
	chmod 644 %{_sysconfdir}/ld.so.preload
fi
/sbin/ldconfig

%postun
orig="%{_libdir}/libsys-assert.so"
pattern=$(echo $orig | sed -e 's|/|\\/|g')
sed -i "/${pattern}/D" %{_sysconfdir}/ld.so.preload
/sbin/ldconfig

%endif



%files
%license LICENSE
%manifest crash-worker.manifest
%defattr(-,system,system,-)
%dir %{crash_root_path}
%dir %{crash_path}
%dir %{crash_temp}
%attr(0755,system,system) %{_bindir}/dump_systemstate
%{_bindir}/crash-manager.sh
%{_prefix}/lib/sysctl.d/99-crash-manager.conf

%if "%{?sys_assert}" == "on"
%{_libdir}/libsys-assert.so
/usr/lib/sysctl.d/sys-assert.conf
%else
%{_libexecdir}/crash-pipe
%{_libexecdir}/crash-stack
%endif

