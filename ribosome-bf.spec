Summary:	Reverbrain core utility libraries
Name:		ribosome
Version:	0.2.2
Release:	1%{?dist}.1

License:	Apachev2+
Group:		System Environment/Libraries
URL:		http://reverbrain.com/
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)


BuildRequires:	libicu-devel, eigen3-devel
BuildRequires:	cmake >= 2.6

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
Requires: libicu-devel, eigen3-devel


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
#%{_libdir}/lib*.so

%changelog
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

