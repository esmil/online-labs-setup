Name:           online-labs-setup
Version:        %(date +%Y%m%d)
Release:        1%{?dist}
Summary:        Automatic configuration of online-labs servers

License:        GPL
URL:            https://github.com/esmil/online-labs-setup
Source0:        https://github.com/esmil/online-labs-setup/archive/master.tar.gz

BuildRequires:  glibc-static
Requires:       systemd curl wget

%description
Automatic configuration of online-labs servers


#%prep
#%setup -n online-labs-setup
%define _builddir %(echo $PWD)


%build
make %{?_smp_mflags}


%install
%make_install


%post
if [ $1 = 1 ]; then
  for i in oc-sync-kernel-modules oc-set-root-sshkeys oc-pre-shutdown; do
    systemctl preset "${i}.service"
  done
fi

%postun
for i in oc-sync-kernel-modules oc-set-root-sshkeys oc-pre-shutdown; do
  [ -f "%{_libdir}/systemd/system/${i}.service" ] || systemctl disable "${i}.service"
done


%files
%{_libdir}/%{name}/*
%{_libdir}/systemd/*


%changelog

