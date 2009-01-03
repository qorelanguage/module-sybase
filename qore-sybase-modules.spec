%define with_sybase %(test -z "$SYBASE"; echo $?)

%define module_api %(qore --module-api 2>/dev/null)
%define module_dir %(qore --module-dir 2>/dev/null)

%if 0%{?sles_version}

%if 0%{?sles_version} == 11
%define dist .sle11
%endif

%if 0%{?sles_version} == 10
%define dist .sle10
%endif

%if 0%{?sles_version} == 9
%define dist .sle9
%endif

%else
%if 0%{?suse_version}

%if 0%{?suse_version} == 1110
%define dist .opensuse11_1
%endif

%if 0%{?suse_version} == 1100
%define dist .opensuse11
%endif

%if 0%{?suse_version} == 1030
%define dist .opensuse10.3
%endif

%if 0%{?suse_version} == 1020
%define dist .opensuse10.2
%endif

%if 0%{?suse_version} == 1010
%define dist .suse10.1
%endif

%if 0%{?suse_version} == 1000
%define dist .suse10
%endif

%if 0%{?suse_version} == 930
%define dist .suse9.3
%endif

%endif
%endif

Summary: Sybase and FreeTDS Modules for Qore
Name: qore-sybase-modules
Version: 1.0.2
Release: 1%{dist}
License: LGPL
Group: Development/Languages
URL: http://www.qoretechnologies.com/qore
Source: http://prdownloads.sourceforge.net/qore/%{name}-%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: /usr/bin/env
Requires: qore-module-api-%{module_api}
BuildRequires: gcc-c++
BuildRequires: qore-devel
BuildRequires: qore

%description
This is the master module for the sybase and freetds modules; it does not
contain any files.


%if 0%{?suse_version}
%debug_package
%endif

%if 0%{?with_sybase}
%package -n qore-sybase-module
Summary: Sybase DBI module for Qore
Group: Development/Languages

%description -n qore-sybase-module
Sybase DBI driver module for the Qore Programming Language. The Sybase driver is
character set aware, supports multithreading, transaction management, stored
prodedure and function execution, etc.


%files -n qore-sybase-module
%defattr(-,root,root,-)
%{module_dir}/sybase-api-%{module_api}.qmod
%doc COPYING README RELEASE-NOTES ChangeLog AUTHORS test/db-test.q docs/sybase-module-doc.html
%endif


%package -n qore-freetds-module
Summary: FreeTDS-based MS-SQL and Sybase DBI module for Qore
Group: Development/Languages
Requires: freetds
BuildRequires: freetds-devel

%description -n qore-freetds-module
FreeTDS-based MS-SQL Server and Sybase DBI driver module for the Qore
Programming Language. This driver is character set aware, supports
multithreading, transaction management, stored prodedure and function
execution, etc, and can be used to connect to Sybase and Microsoft SQL Server
databases.


%files -n qore-freetds-module
%defattr(-,root,root,-)
%{module_dir}/freetds-api-%{module_api}.qmod
%doc COPYING README RELEASE-NOTES ChangeLog AUTHORS test/db-test.q docs/sybase-module-doc.html

%prep
%setup -q
%ifarch x86_64 ppc64 x390x
c64=--enable-64bit
%endif
./configure RPM_OPT_FLAGS="$RPM_OPT_FLAGS" --prefix=/usr --disable-debug $c64

%build
%{__make}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{module_dir}
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/qore-sybase-module
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%changelog
* Sat Jan 3 2008 David Nichols <david_nichols@users.sourceforge.net>
- updated version to 1.0.2

* Tue Sep 2 2008 David Nichols <david_nichols@users.sourceforge.net>
- initial spec file for separate sybase and freetds module release
