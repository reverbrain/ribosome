Summary:	Reverbrain core utility libraries
Name:		ribosome
Version:	0.1.0
Release:	1%{?dist}.1

License:	Apachev2+
Group:		System Environment/Libraries
URL:		http://reverbrain.com/
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)


BuildRequires:	boost-devel, boost-system, boost-locale
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
Requires: %{name} = %{version}-%{release}


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

%files
%defattr(-,root,root,-)
%doc AUTHORS README.rst
%{_bindir}/*
%{_libdir}/lib*.so.*


%files devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/lib*.so

%changelog
* Sat Feb 14 2015 Evgeniy Polyakov <zbr@ioremap.net> - 0.1.0
- Ribosome is a set of utility building blocks made at Reverbrain

