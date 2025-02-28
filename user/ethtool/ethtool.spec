Name		: ethtool
Version		: 1.8
Release		: 1
Group		: Utilities

Summary		: A tool for setting ethernet parameters

Copyright	: GPL
URL		: http://sourceforge.net/projects/gkernel/

Buildroot	: %{_tmppath}/%{name}-%{version}
Source		: %{name}-%{version}.tar.gz


%description
Ethtool is a small utility to get and set values from your your ethernet 
controllers.  Not all ethernet drivers support ethtool, but it is getting 
better.  If your ethernet driver doesn't support it, ask the maintainer to 
write support - it's not hard!


%prep
%setup -q


%build
CFLAGS="${RPM_OPT_FLAGS}" ./configure --prefix=/usr --mandir=%{_mandir}
make


%install
make install DESTDIR=${RPM_BUILD_ROOT}


%files
%defattr(-,root,root)
/usr/sbin/ethtool
%{_mandir}/man8/ethtool.8*
%doc AUTHORS COPYING INSTALL NEWS README ChangeLog


%changelog
* Sat Jul 19 2003  Scott Feldman  <scott.feldman@intel.com>
- ethtool.8, ethtool.c, ethtool-copy.h:
  Add support for TSO get/set.  Corresponds to NETIF_F_TSO.
  Extended -k|K option to included tso, and changed meaning from
  just "checksum/sg" to more general "offload".  Now covers Rx/Tx
  csum, SG, and TSO.
* Thu May 28 2003  Ganesh Venkatesan  <ganesh.venkatesan@intel.com>
- ethtool-copy.h: new definitions for 10GbE
* Thu May 28 2003  Scott Feldman  <scott.feldman@intel.com>
- ethtool.c: Add ethtool -E to write EEPROM byte.
- e100.c: Added MDI/MDI-X status to register dump.
* Thu May 28 2003   Reeja John  <reeja.john@amd.com>
- amd8111e.c: new file, support for AMD-8111e NICs
- ethtool.c: properly set ecmd.advertising
* Sat Mar 29 2003   OGAWA Hirofumi  <hirofumi@mail.parknet.co.jp>
- realtek.c: clean up chip enumeration, support additional chips
* Fri Mar 28 2003   Jeb Cramer  <cramerj@intel.com>
- e1000.c: Update supported devices (82541 & 82547).  Add bus type,
  speed and width to register dump printout.
- ethtool.c (show_usage): Add -S to printout of supported commands.
* Tue Jan 32 2003   Jeff Garzik  <jgarzik@pobox.com>
- natsemi.c (PRINT_INTR, __print_intr): Decompose PRINT_INTR
  macro into macro abuse and function call portions.  Move the
  actual function body to new static functoin __print_intr.
  This eliminates the annoying build warning :)
* Thu Jan 16 2003   Jeb Cramer  <jeb.j.cramer@intel.com>
- ethtool.c (do_regs, dump_eeprom): Fix memory leaks on failed
  operations.  Add error handling of dump_regs().  Modify printout of
  eeprom dump to accomodate larger eeproms.
- e1000.c: Update supported devices.  Add error conditions for
  unsupported devices.
* Mon Oct 21 2002   Ben Collins  <bcollins@debian.org>
- ethtool.c: Add new parameters to -e, for raw EEPROM output, and
  offset and length options.
- natsemi.c (natsemi_dump_eeprom): Show correct offset using new
  offset feature above.
- tg3.c: New file, implements tg3_dump_eeprom.
- Makefile.am: Add it to the build sources.
- ethtool-util.h: Prototype tg3_dump_eeprom.
- ethtool.8: Document new -e options.
* Thu Oct 17 2002   Tim Hockin  <thockin@sun.com>
- ethtool.c: make calls to strtol() use base 0
* Wed Sep 18 2002   Scott Feldman  <scott.feldman@intel.com>
- ethtool.c (dump_regs): call e100_dump_regs if e100
- e100.c: new file
- ethtool-util.h: prototype e100_dump_regs
* Thu Jun 20 2002   Jeff Garzik  <jgarzik@mandrakesoft.com>
- ethtool.8: document new -S stats dump argument
- configure.in, NEWS: release version 1.6
* Fri Jun 14 2002   Jeff Garzik  <jgarzik@mandrakesoft.com>
- realtek.c (realtek_dump_regs): dump legacy 8139 registers
- ethtool.c (do_gstats, doit, parse_cmdline):
  support dumping of NIC-specific statistics
* Fri Jun 14 2002   Jeff Garzik  <jgarzik@mandrakesoft.com>
- realtek.c (realtek_dump_regs): dump RTL8139C+ registers
* Fri Jun 14 2002   Jeff Garzik  <jgarzik@mandrakesoft.com>
- realtek.c: new file, dumps RealTek RTL8169 PCI NIC's registers
- Makefile.am, ethtool.c, ethtool-util.h: use it
* Tue Jun 11 2002   Jeff Garzik  <jgarzik@mandrakesoft.com>
- NEWS: list new commands added recently
- ethtool.c (do_gcoalesce, do_scoalesce, dump_coalesce): new
  (parse_cmdline, doit): handle get/set coalesce parameters (-c,-C)
  (do_[gs]*): convert to use table-driven cmd line parsing
- ethtool.8: document -c and -C
* Tue Jun 11 2002   Jeff Garzik  <jgarzik@mandrakesoft.com>
- ethtool.c (do_gring, do_sring, dump_ring,
  parse_ring_cmdline): new functions
  (parse_cmdline, doit): handle get/set ring parameters (-g,-G)
  (do_spause): fix off-by-one bugs
- ethtool.8: document -g and -G
* Tue Jun 11 2002   Jeff Garzik  <jgarzik@mandrakesoft.com>
- ethtool.c (do_gpause, do_spause, dump_pause,
  parse_pause_cmdline): new functions
  (parse_cmdline, doit): handle get/set pause parameters (-a,-A)
- ethtool.8: document -a, -A, -e, and -p
* 2002-05-22  Chris Leech <christopher.leech@intel.com>
  Scott Feldman <scott.feldman@intel.com>
- ethtool-copy.h: add support for ETHTOOL_PHYS_ID function.
- ethtool.c: add support for ETHTOOL_PHYS_ID function, add
  support for e1000 reg dump.
- Makefile.am: add e1000.c
- e1000.c: reg dump support for Intel(R) PRO/1000 adapters.
- ethtool-util.h: add e1000 reg dump support.
* 2002-05-11  Eli Kupermann  <eli.kupermann@intel.com>
- ethtool.c (do_test): add support for online/offline test modes
  Elsewhere: document "-t" arg usage, and handle usage
* 2002-05-11  Jes Sorensen  <jes@wildopensource.com>
- ethtool.c (dump_ecmd): If unknown value is
  encountered in speed, duplex, or port ETHTOOL_GSET
  return data, print the numeric value returned.
* 2002-05-01  Eli Kupermann  <eli.kupermann@intel.com>
- ethtool.8: document new -t test option
* 2002-05-01  Christoph Hellwig  <hch@lst.de>
- Makefile.am (dist-hook): Use $(top-srcdir) for refering to sources.
* 2002-04-29  Christoph Hellwig  <hch@lst.de>
- Makefile.am (SUBDIRS): Remove.
  (RPMSRCS): Likewise.
  (TMPDIR): Likewise.
  (rpm): Likewise.
  (EXTRA_DIST): Add ethtool.spec.in.
  (dist-hook): New rule.  Create rpm specfile.
- configure.in (AC_OUTPUT): Add ethtool.spec.
- ethtool.spec.in: New file.  Rpm specfile template.
- redhat/ethtool.spec.in: Removed.
- redhat/Makefile.am: Removed.
* Wed Mar 20 2002   Jeff Garzik  <jgarzik@mandrakesoft.com>
- ethtool-copy.h: Merge coalescing param, ring
  param, and pause param ioctl structs from kernel 2.5.7.
  Merge ethtool_test changes fromkernel 2.5.7.
- ethtool: Update for ethtool_test cleanups.
* Wed Mar 20 2002   Eli Kupermann  <eli.kupermann@intel.com>
- ethtool.c: (do_test): new function
  Elsewhere: add support for 'perform test' function,
  via a new "-t" arg, by calling do_test.
* Sun Mar  3 2002   Brad Hards  <bhards@bigpond.net.au>
- ethtool.c (parse_cmdline): Support "usb"
  as well as "eth" network interfaces.  USB networking
  uses a different prefix.
* Fri Feb  8 2002  "Noam, Amir" <amir.noam@intel.com>,
  "Kupermann, Eli" <eli.kupermann@intel.com>
- ethtool.c (dump_advertised): new function.
  (dump_ecmd): Call it.
  Elsewhere: reformat code.
* Wed Nov 28 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- configure.in, Makefile.am, redhat/Makefile.am:
  make sure redhat spec is included in dist tarball.
* Tue Nov 27 2001  Tim Hockin  <thockin@sun.com>
- natsemi.c: strings changes
- ethtool.c: print messagelevel as hex (netif_msg_* shows better :)
* Sun Nov 18 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- NEWS: update with recent changes
- ethtool.8: phy address can be used if implemented in the
  driver, so remove "Not used yet" remark.
* Sun Nov 18 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- Makefile.am, de2104x.c, ethtool-util.h, ethtool.c:
  Support register dumps for de2104x driver.
* Tue Nov 13 2001  Tim Hockin  <thockin@sun.com>
- natsemi.c, ethtool.c: use u8 data for ethtool_regs
- ethtool-copy.h: latest from kernel
- natsemi.c, ethtool.c: support ETHTOOL_GEEPROM via -e param
* Mon Nov 12 2001  Tim Hockin  <thockin@sun.com>
- natsemi.c: check version, conditionally print RFCR-indexed data
* Wed Nov 07 2001  Tim Hockin  <thockin@sun.com>
- ethtool.c: print less errors for unsupported ioctl()s
- ethtool.c: warn if all ioctl()s are unsupported or failed
- ethtool.c: change autoneg-restart mechanism to -r (as per jgarzik)
- ethtool.c: check for "eth" in devicename (per jg)
- ethtool.c: remove 'extraneous' braces
* Wed Nov 07 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- ethtool.c, ethtool.8: support bnc port/media
* Tue Nov 06 2001  Tim Hockin  <thockin@sun.com>
- ethtool.c: clean up output for unhandled register dumps
- natsemi.c: finish pretty-printing register dumps
- ethtool.8: document -d option
- various: add copyright info, where applicable
- ethtool.c: be nicer about unsupported ioctl()s where possible
  and be more verbose where nice is not an option.
* Mon Nov 05 2001  Tim Hockin  <thockin@sun.com>
- natsemi.c: first cut at 'pretty-printing' register dumps
* Fri Nov 02 2001  Tim Hockin  <thockin@sun.com>
- ethtool.c: add support for ETHTOOL_GREGS via -d (dump) flag
- ethtool.c: add support for device-specific dumps for known devices
- ethtool.c: make mode-specific handling allocate ifr_data
- Makefile.am: import ChangeLog to rpm specfile
- natsemi.c: added
- ethtool-util.h: added
* Thu Nov 01 2001  Tim Hockin  <thockin@sun.com>
- ethtool.c: add support for ETHTOOL_GLINK in output
- ethtool.c: add support for ETHTOOL_NWAY_RST via 'autoneg restart'
- ethtool.c: add support for ETHTOOL_[GS]MSGLVL via 'msglvl' param
- ethtool.8: add documentation for above
- ethtool-copy.h: updated to sync with kernel
* Fri Oct 26 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- ethtool.8: Update contributors list, home page URL.
- ethtool.8: Much cleanup, no content change.
  Contributed by Andre Majorel.
- ethtool.c: Clean up '-h' usage message.
  Contributed by Andre Majorel.
* Fri Oct 26 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- Configure.in: bump version to 1.4cvs
- Makefile.am: include ethtool-copy.h in list of sources
- ethtool-copy.h:
  Import ethtool.h from kernel 2.4.13.
- ethtool.c:
  Define SIOCETHTOOL if it is missing,
  trim trailing whitespace.
- NEWS: update for these changes
* Wed Sep 19 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- ethtool.c, ethtool-copy.h:
  Import copy of kernel 2.4.10-pre12's ethtool.h.
* Wed Sep 19 2001  Tim Hockin  <thockin@sun.com>
- Makefile.am, redhat/ethtool.spec.in:
  Basic "make rpm" support.
* Wed Sep 19 2001  Tim Hockin  <thockin@sun.com>
- AUTHORS, NEWS, ethtool.8, ethtool.c:
  Wake-on-LAN support.
* Thu May 17 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- configure.in, NEWS, README: Version 1.2 release
- ethtool.c: Support ETHTOOL_GDRVINFO.
- ethtool.8: Document it.
* Fri Mar 20 2001  Jeff Garzik  <jgarzik@mandrakesoft.com>
- Makefile.am, configure.in, autogen.sh, NEWS,
  ChangeLog, AUTHORS, README:
  Add autoconf/automake support.
