%define with_sybase %(test -z "$SYBASE"; echo $?)

%define module_api %(qore --latest-module-api 2>/dev/null)
%define module_dir %{_libdir}/qore-modules

%if 0%{?sles_version}

%define dist .sles%{?sles_version}

%else
%if 0%{?suse_version}

# get *suse release major version
%define os_maj %(echo %suse_version|rev|cut -b3-|rev)
# get *suse release minor version without trailing zeros
%define os_min %(echo %suse_version|rev|cut -b-2|rev|sed s/0*$//)

%if %suse_version > 1010
%define dist .opensuse%{os_maj}_%{os_min}
%else
%define dist .suse%{os_maj}_%{os_min}
%endif

%endif
%endif

# see if we can determine the distribution type
%if 0%{!?dist:1}
%define rh_dist %(if [ -f /etc/redhat-release ];then cat /etc/redhat-release|sed "s/[^0-9.]*//"|cut -f1 -d.;fi)
%if 0%{?rh_dist}
%define dist .rhel%{rh_dist}
%else
%define dist .unknown
%endif
%endif

Summary: Sybase and FreeTDS Modules for Qore
Name: qore-sybase-modules
Version: 1.0.3
Release: 1%{dist}
License: LGPL
Group: Development/Languages
URL: http://www.qoretechnologies.com/qore
Source: http://prdownloads.sourceforge.net/qore/%{name}-%{version}.tar.gz
#Source0: %{name}-%{version}.tar.gz
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
prodedure and function execution, and SQL execution with native binding and
output placeholders.


%files -n qore-sybase-module
%defattr(-,root,root,-)
%dir %{module_dir}
%{module_dir}/sybase-api-%{module_api}.qmod
%doc COPYING.MIT COPYING.LGPL README RELEASE-NOTES ChangeLog AUTHORS test/db-test.q docs/sybase-module-doc.html
%endif


%package -n qore-freetds-module
Summary: FreeTDS-based MS-SQL and Sybase DBI module for Qore
Group: Development/Languages
%if 0%{?mdkversion}
%ifarch x86_64 ppc64 x390x
BuildRequires: lib64freetds-devel
%else
BuildRequires: libfreetds-devel
%endif
%else
BuildRequires: freetds-devel
%endif

%description -n qore-freetds-module
FreeTDS-based MS-SQL Server and Sybase DBI driver module for the Qore
Programming Language. This driver is character set aware, supports
multithreading, transaction management, stored prodedure and function
execution, SQL execution with native binding and output placeholders,
and can be used to connect to Sybase and Microsoft SQL Server
databases.


%files -n qore-freetds-module
%defattr(-,root,root,-)
%dir %{module_dir}
%{module_dir}/freetds-api-%{module_api}.qmod
%doc COPYING README RELEASE-NOTES ChangeLog AUTHORS test/db-test.q docs/sybase-module-doc.html

%prep
%setup -q
%ifarch x86_64 ppc64 x390x
c64=--enable-64bit
%endif
CXXFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=/usr --disable-debug $c64

%build
%{__make}

%install
mkdir -p $RPM_BUILD_ROOT/%{module_dir}
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/qore-sybase-module
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%changelog
* Thu Jun 25 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated version to 1.0.3

* Sat Jan 3 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated version to 1.0.2

* Tue Sep 2 2008 David Nichols <david_nichols@users.sourceforge.net>
- initial spec file for separate sybase and freetds module release
