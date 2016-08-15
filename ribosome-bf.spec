Summary:	Reverbrain core utility libraries
Name:		ribosome
Version:	0.3.0
Release:	1%{?dist}.1

License:	Apachev2+
Group:		System Environment/Libraries
URL:		http://reverbrain.com/
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)


BuildRequires:	libicu-devel, eigen3-devel, glog-devel, gflags-devel, libtidy-devel
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
%{cmake} -DWANT_GTEST=OFF .
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
* Mon Aug 15 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.3.0
- split: the last argument to convert_split_words() is an alphabet to split text on. This is additional to unicode word delimiters set.
- charset: get rid of obscure encoding detection, it can only find utf8/latin and couple iso encodings, nothing similar to many russian or asian encodings
- dir: throw exception if trying to open non-directory
- rans: added example rans coder

* Mon Aug 08 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.17
- rANS: range asymmetric numeral systems implementation, simple binary encoder/decoder
- cmake: added hint for PATH_SUFFIX when locating tidy library
- html: added feed_text() method which accepts data pointer and size

* Sun Jul 31 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.16
- Added libtidy-based HTML parser
- Added generic error class and helpers to create errors and exceptions

* Fri Jul 29 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.15
- lstring: added locale get/set helpers, use that locale when converting strings and splitting text
- lstring: convert string to utf8 and not into native codepage in to_string() method
- debian: updated rules to allow building multiple packages

* Tue Jul 12 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.14
- debian: split ribosome to devel and simple package, the latter contains libribosome.so and cmake file

* Wed Jun 15 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.13
- fpool: use glog instead of plain printf()
- fpool: speed up pool destruction
- package: disable downloading and building gtests
- file: added implementation of RAII classes for opened and mapped files

* Fri Jun 10 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.12
- fpool: refactor ready() method, return error and event structure, added logs

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

