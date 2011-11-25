#* This file is part of 'kio-gallery3'
#* Copyright (C) 2011 Christian Reiner <kio-gallery3@christian-reiner.info>
#*
#* $Author: arkascha $
#* $Revision: 116 $
#* $Date: 2011-09-10 01:40:45 +0200 (Sat, 10 Sep 2011) $
#*

# norootforbuild

Name:           kio-gallery3
License:        GPLv3
Group:          Utilities/Desktop
Summary:        KDE IO Slave for file based access to a remote gallery3 server
Version:        0.1.4
Release:        1
Source:         %name-%version.tar.bz2
Url:            http://kde-apps.org/
Requires:       libkde4 >= 4.7
Requires:       libqjson0
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires:  cmake libqt4-devel libqjson-devel
BuildRequires:  libkde4-devel >= 4.7
BuildRequires:  libkdecore4-devel
%kde4_runtime_requires

%description
Overview:
kio-gallery3 implements a kio-slave, a protocol for KDEs I/O system. It
makes the content of a remote gallery3 server available inside the local
file management. That means you can access all items contained in the
gallery entries using standard file dialogs like 'File open' or 'Save File'.
You even can use a file browser to add, remove or open entries. In general
you can use such a slave wherever you can use an URL inside KDE to get
access to the gallery3 content. There are no temporary file generated.

Author:
Christian Reiner <kio-gallery3@christian-reiner.info>


%prep
%setup -q

%build
  %cmake_kde4 -d build
  %make_jobs

%install
  cd build
  %makeinstall
  cd ..
  %kde_post_install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc AUTHORS CHANGELOG
%_kde4_modules/kio_gallery3.so
%_kde_share_dir/services/gallery3.protocol
%_kde_share_dir/services/gallery3s.protocol

%changelog
* ??? ??? ?? ???? Christian Reiner: version 0.1.4
- fix for a few minor memory leaks
- some code optimizations
* Tue Nov 22 2011 Christian Reiner: version 0.1.3
- internal code simplifications as preparation for future extensions
* Mon Nov 21 2011 Christian Reiner: version 0.1.2
- introduction of d-pointer usage for kde coding compliance
* Sun Nov 20 2011 Christian Reiner: version 0.1.1
- fixed access rights visualization for items
* Tue Nov 15 2011 Christian Reiner: version 0.1
- initial (and certainly more'n buggy) release
- publication on kde-apps.org
