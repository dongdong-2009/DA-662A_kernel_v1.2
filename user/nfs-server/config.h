/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define if using alloca.c.  */
/* #undef C_ALLOCA */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
/* #undef CRAY_STACKSEG_END */

/* Define to the type of elements in the array set by `getgroups'.
   Usually this is either `int' or `gid_t'.  */
#define GETGROUPS_T gid_t

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you have alloca, as a function or macro.  */
#define HAVE_ALLOCA 1

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
#define HAVE_ALLOCA_H 1

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if your struct stat has st_blksize.  */
#define HAVE_ST_BLKSIZE 1

/* Define if your struct stat has st_blocks.  */
#define HAVE_ST_BLOCKS 1

/* Define if your struct stat has st_rdev.  */
#define HAVE_ST_RDEV 1

/* Define if utime(file, NULL) sets file's timestamp to the present.  */
/* #undef HAVE_UTIME_NULL */

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if major, minor, and makedev are declared in <mkdev.h>.  */
/* #undef MAJOR_IN_MKDEV */

/* Define if major, minor, and makedev are declared in <sysmacros.h>.  */
/* #undef MAJOR_IN_SYSMACROS */

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown
 */
/* #undef STACK_DIRECTION */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* XXX */
/* #undef CRAY_STACKSEG_END */

/* Define if your rpcgen has the -C option to generate ANSI C.  */
#define HAVE_RPCGEN_C 1

/* Define if your rpcgen has the -I option to generate servers
   that can be started from a port monitor like inetd or the listener.  */
#define HAVE_RPCGEN_I 1

/* Define if your rpc/xdr.h declares the xdrproc_t type. */
#define HAVE_XDRPROC_T 1

/* Define if your system uses the OSF/1 method of getting the mount list.  */
/* #undef MOUNTED_GETFSSTAT */

/* Define if your system uses the SysVr4 method of getting the mount list.  */
/* #undef MOUNTED_GETMNTENT2 */

/* Define if your system uses the AIX method of getting the mount list.  */
/* #undef MOUNTED_VMOUNT */

/* Define if your system uses the SysVr3 method of getting the mount list.  */
/* #undef MOUNTED_FREAD_FSTYP */

/* Define if your system uses the BSD4.3 method of getting the mount list.  */
#define MOUNTED_GETMNTENT1 1

/* Define if your system uses the BSD4.4 method of getting the mount list.  */
/* #undef MOUNTED_GETMNTINFO */

/* Define if your system uses the Ultrix method of getting the mount list.  */
/* #undef MOUNTED_GETMNT */

/* Define if your system uses the OSF/1 method of getting fs usage.  */
#define STATFS_OSF1 1

/* Define if your system uses the SysVr2 method of getting fs usage.  */
/* #undef STAT_READ */

/* Define if your system uses the BSD4.3 method of getting fs usage.  */
/* #undef STAT_STATFS2_BSIZE */

/* Define if your system uses the BSD4.4 method of getting fs usage.  */
/* #undef STAT_STATFS2_FSIZE */

/* Define if your system uses the Ultrix method of getting fs usage.  */
/* #undef STAT_STATFS2_FS_DATA */

/* Define if your system uses the SysVr3 method of getting fs usage.  */
/* #undef STAT_STATFS4 */

/* Define if your system uses the SysVr4 method of getting fs usage.  */
/* #undef STAT_STATVFS */

/* Define if you're using libwrap.a to protect ugidd, and libwrap.a
 * gives you `undefined symbol: deny_severity' when linking.		*/
#define HAVE_LIBWRAP_BUG 1

/* Define if your system has BSD-style signals (i.e. the handler is
 * reinstalled automatically).						*/
#define HAVE_BSD_SIGNALS 1

/* Define if your system defines authdes_getucred in rpc/auth_des.h */
#define HAVE_AUTHDES_GETUCRED_DECL 1

/* Define if your setfsuid rejects negative uids */
#define HAVE_BROKEN_SETFSUID 1

/* Define these if sys/types.h doesn't */
/* #undef dev_t */
/* #undef ino_t */

/* Sizes of various types */
#define SIZEOF_DEV_T 8
#define SIZEOF_INO_T 4
#define SIZEOF_UID_T 4
#define SIZEOF_GID_T 4
#define SIZEOF_UNSIGNED_SHORT 2
#define SIZEOF_UNSIGNED_INT 4
#define SIZEOF_UNSIGNED_LONG 4

/* Define if you have the authdes_getucred function.  */
#define HAVE_AUTHDES_GETUCRED 1

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the getdtablesize function.  */
#define HAVE_GETDTABLESIZE 1

/* Define if you have the innetgr function.  */
#define HAVE_INNETGR 1

/* Define if you have the lchown function.  */
#define HAVE_LCHOWN 1

/* Define if you have the quotactl function.  */
#define HAVE_QUOTACTL 1

/* Define if you have the seteuid function.  */
#define HAVE_SETEUID 1

/* Define if you have the setfsgid function.  */
#define HAVE_SETFSGID 1

/* Define if you have the setfsuid function.  */
#define HAVE_SETFSUID 1

/* Define if you have the setgroups function.  */
#define HAVE_SETGROUPS 1

/* Define if you have the setreuid function.  */
#define HAVE_SETREUID 1

/* Define if you have the setsid function.  */
#define HAVE_SETSID 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/fsuid.h> header file.  */
#define HAVE_SYS_FSUID_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <syslog.h> header file.  */
#define HAVE_SYSLOG_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <utime.h> header file.  */
#define HAVE_UTIME_H 1

/* Define if you have the crypt library (-lcrypt).  */
#define HAVE_LIBCRYPT 1

/* Define if you have the nsl library (-lnsl).  */
#define HAVE_LIBNSL 1

/* Define if you have the nys library (-lnys).  */
/* #undef HAVE_LIBNYS */

/* Define if you have the rpc library (-lrpc).  */
/* #undef HAVE_LIBRPC */

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */
