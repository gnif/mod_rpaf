Summary: Reverse Proxy Add Forward module for Apache
Name: mod_rpaf
Version: 0.8
Release: 1
License: Apache
Group: System Environment/Daemons
URL: https://github.com/gnif/mod_rpaf
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: httpd-devel
Requires: httpd httpd-devel

%description
rpaf is for backend Apache servers what mod_proxy_add_forward is for
frontend Apache servers. It does excactly the opposite of
mod_proxy_add_forward written by Ask Bjørn Hansen. It will also work
with mod_proxy in Apache starting with release 1.3.25 and mod_proxy
that is distributed with Apache2 from version 2.0.36.

%prep
%setup -q

%build
make rpaf

%install
rm -rf $RPM_BUILD_ROOT
install -m0755 -d $RPM_BUILD_ROOT$(apxs -q LIBEXECDIR)
make DESTDIR=$RPM_BUILD_ROOT install
install -m0644 -D debian/conf/rpaf.conf $RPM_BUILD_ROOT/etc/httpd/conf.d/mod_rpaf.conf

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_libdir}/httpd/modules/mod_rpaf.so
%config(noreplace) %{_sysconfdir}/httpd/conf.d/mod_rpaf.conf

%post
/usr/sbin/apxs -e -A -n rpaf $(apxs -q LIBEXECDIR)/mod_rpaf.so

%preun
/usr/sbin/apxs -e -A -n rpaf $(apxs -q LIBEXECDIR)/mod_rpaf.so


%changelog
* Mon Oct 17 2011 Ben Walton <bwalton@artsci.utoronto.ca> - 0.7
- Initial spec file creation
