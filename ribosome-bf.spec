Summary:	Reverbrain core utility libraries
Name:		ribosome
Version:	0.2.11
Release:	1%{?dist}.1

License:	Apachev2+
Group:		System Environment/Libraries
URL:		http://reverbrain.com/
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)


BuildRequires:	libicu-devel, eigen3-devel, glog-devel, gflags-devel
BuildRequires:	cmake >= 2.6

%define debug_package %{nil}

%description
Ribosome is a set of utility building blocks made at Reverbrain
 * directory iterator
 * UTF8 std-like string
 * timer
 * Levenstein-Damerau edit distance


%package devel
Summary: Development files for %{name}
Group: Development/Libraries
#Requires: %{name} = %{version}-%{release}
Requires: libicu-devel, eigen3-devel, glog-devel, gflags-devel


%description devel
Ribosome is a set of utility building blocks made at Reverbrain

This package contains libraries, header files and developer documentation
needed for developing software which uses ribosome utils.

%prep
%setup -q

%build
export LDFLAGS="-Wl,-z,defs"
export DESTDIR="%{buildroot}"
%{cmake} .
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR="%{buildroot}"
rm -f %{buildroot}%{_libdir}/*.a
rm -f %{buildroot}%{_libdir}/*.la

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

#%files
#%defattr(-,root,root,-)
#%{_bindir}/*
#%{_libdir}/lib*.so.*


%files devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_datadir}/ribosome/cmake/*
%{_libdir}/lib*.so*

%changelog
* Wed Jun 08 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.11
- fpool: made schedule() method asynchronous
- cmake: added ribosome library into exported list of libs
- fpool: use reply message status to place returned status instead of separate field in completion callback
- fpool: added id/status into message header
- spec: put *.so files into the package
- cmake: added -pthread linker flag

* Sun Jun 05 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.10
- fpool: initial implementation of the pool of forked processes which perform the same task and have io channels to communicate with the controller
- fpool: added worker process restart if it has been killed

* Fri May 27 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.9
- spec: added build dependencies as dependencies, since it is header-only development package

* Fri May 27 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.8
- debian: added build dependencies as dependencies, since it is header-only development package
- spec: depend on gflags
- expiration: get rid of std::put_time(), it is not available in gcc 4.8
- cmake: include/library macros should include eigen3 and glog files

* Wed May 11 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.7
- vector_lock: added lock abstraction which can safely lock multiple locks indexed by string keys

* Mon Apr 25 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.6
- expiration: stop() should wait for processing thread to exit, thread must be initialized after control structures
- expiration: added missing include

* Fri Apr 22 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.5
- expiration: added operation logs
- expiration: fixed expired vector or callbacks assignment (it had reference to vector of to-be-freed objects)
- expiration: remove timeout entry if there are no callbacks for that time anymore
- package: depend on google log package

* Thu Apr 21 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.4
- test: added google test framework (icu, expiration tests)
- expiration: insert() returns unique token which can be used to remove() registered callback

* Wed Apr 20 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.3
- Added expiration checking module, which invokes provided callback as soon as associated timeout fires

* Sat Sep 19 2015 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.2
- Updated debianization

* Sat Sep 19 2015 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.1
- package: updated devel package deps
- charset: fixed includes
- copyright: updated dates
- lstring: added missing include
- cmake: export libraris too
- lstring: added string-to-string lowering method
- package: include cmake files into devel rpm package
- lstring: added to_lower() conversion method
- split: added word-split class

* Fri Jul 03 2015 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.0
- Switched to ICU from Boost::Locale, added charset detector/utf8-converter

* Sat Feb 14 2015 Evgeniy Polyakov <zbr@ioremap.net> - 0.1.0
- Ribosome is a set of utility building blocks made at Reverbrain

