dnl aclocal.m4 -- custom autoconf macros for various purposes
dnl Updated for Autoconf v2
dnl
dnl ******** save/restore stuff **********
define(AC_KNFSD_SAVE,
  [AC_LANG_SAVE
   save_LDFLAGS=$LDFLAGS
   save_CFLAGS=$CFLAGS
   save_CXXFLAGS=$CXXFLAGS
   save_LIBS=$LIBS
])dnl
define(AC_KNFSD_RESTORE,
  [LDFLAGS=$save_LDFLAGS
   CFLAGS=$save_CFLAGS
   CXXFLAGS=$save_CXXFLAGS
   LIBS=$save_LIBS
   AC_LANG_RESTORE
])dnl
dnl *********** GNU libc 2 ***************
define(AC_GNULIBC,
  [AC_MSG_CHECKING(for GNU libc2)
  AC_CACHE_VAL(knfsd_cv_glibc2,
  [AC_TRY_CPP([
      #include <features.h>
      #if !defined(__GLIBC__)
      # error Nope
      #endif], knfsd_cv_glibc2=yes, knfsd_cv_glibc2=no)])
  AC_MSG_RESULT($knfsd_cv_glibc2)
  if test $knfsd_cv_glibc2 = yes; then
    CFLAGS="$CFLAGS -D_GNU_SOURCE"
    CXXFLAGS="$CXXFLAGS -D_GNU_SOURCE"
  fi
]) dnl
dnl
dnl ************* egcs *******************
define(AC_PROG_EGCS,
  [AC_MSG_CHECKING(for egcs)
  AC_CACHE_VAL(knfsd_cv_prog_EGCS,
  [case `$CC --version 2>/dev/null` in
   egcs*)
	knfsd_cv_prog_EGCS=yes;;
   *)
	knfsd_cv_prog_EGCS=no;;
   esac
  ])
  AC_MSG_RESULT($knfsd_cv_prog_EGCS)
  test $knfsd_cv_prog_EGCS = yes && AC_DEFINE(HAVE_EGCS)
]) dnl
dnl *********** sizeof(dev_t) **************
dnl ** We have to kludge this rather than use AC_CHECK_SIZEOF because
dnl ** we have to include sys/types.h. Ugh.
define(AC_DEV_T_SIZE,
  [AC_MSG_CHECKING(size of dev_t)
   AC_CACHE_VAL(ac_cv_sizeof_dev_t,
   [AC_TRY_LINK(
    [#include <stdio.h>
     #include <sys/types.h>
     main()
     {
      FILE *f=fopen("conftestval", "w");
      if (!f) exit(1);
      fprintf(f, "%d\n", sizeof(dev_t));
      exit(0);
    }], ac_cv_sizeof_dev_t=`cat conftestval`, ac_cv_sizeof_dev_t=0)])
    AC_MSG_RESULT($ac_cv_sizeof_dev_t)
    AC_DEFINE(SIZEOF_DEV_T,$ac_cv_sizeof_dev_t)
  ])
dnl *********** sizeof(xxx_t) **************
dnl ** Overwrite the AC_CHECK_SIZEOF macro as we must include sys/types.h
define([AC_CHECK_SIZEOF],
  [changequote(<<, >>)dnl
   define(<<AC_TYPE_NAME>>,translit(sizeof_$1, [a-z *], [A-Z_P]))dnl
   define(<<AC_CV_NAME>>, translit(ac_cv_sizeof_$1, [ *], [_p]))dnl
   changequote([, ])dnl
   AC_MSG_CHECKING(size of $1)
   AC_CACHE_VAL(AC_CV_NAME,
   [AC_TRY_RUN(
    [#include <stdio.h>
     #include <sys/types.h>
     main()
     {
      FILE *f=fopen("conftestval", "w");
      if (!f) exit(1);
      fprintf(f, "%d\n", sizeof($1));
      exit(0);
    }], AC_CV_NAME=`cat conftestval`, AC_CV_NAME=0)])
    AC_MSG_RESULT($AC_CV_NAME)
    AC_DEFINE_UNQUOTED(AC_TYPE_NAME,$AC_CV_NAME)
    undefine([AC_TYPE_NAME])dnl
    undefine([AC_CV_NAME])dnl
  ])
dnl *********** BSD vs. POSIX signal handling **************
define([AC_BSD_SIGNALS],
  [AC_MSG_CHECKING(for BSD signal semantics)
  AC_CACHE_VAL(knfsd_cv_bsd_signals,
    [AC_TRY_RUN([
	#include <signal.h>
	#include <unistd.h>
	#include <sys/wait.h>

	static int counter = 0;
	static RETSIGTYPE handler(int num) { counter++; }

	int main()
	{
		int	s;
		if ((s = fork()) < 0) return 1;
		if (s != 0) {
			if (wait(&s) < 0) return 1;
			return WIFSIGNALED(s)? 1 : 0;
		}

		signal(SIGHUP, handler);
		kill(getpid(), SIGHUP); kill(getpid(), SIGHUP);
		return (counter == 2)? 0 : 1;
	}
    ], knfsd_cv_bsd_signals=yes, knfsd_cv_bsd_signals=no)]) dnl
    AC_MSG_RESULT($knfsd_cv_bsd_signals)
    test $knfsd_cv_bsd_signals = yes && AC_DEFINE(HAVE_BSD_SIGNALS)
])dnl
dnl *********** the tcp wrapper library ***************
define(AC_TCP_WRAPPER,
  [AC_MSG_CHECKING(for the tcp wrapper library)
  AC_CACHE_VAL(knfsd_cv_tcp_wrapper,
  [old_LIBS="$LIBS"
   LIBS="$LIBS -lwrap $LIBNSL"
   AC_TRY_LINK([
      int deny_severity = 0;
      int allow_severity = 0;],
      [return hosts_ctl ("nfsd", "", "")],
      knfsd_cv_tcp_wrapper=yes, knfsd_cv_tcp_wrapper=no)
   LIBS="$old_LIBS"])
  AC_MSG_RESULT($knfsd_cv_tcp_wrapper)
  if test "$knfsd_cv_tcp_wrapper" = yes; then
    CFLAGS="$CFLAGS -DHAVE_TCP_WRAPPER"
    CXXFLAGS="$CXXFLAGS -DHAVE_TCP_WRAPPER"
    LIBWRAP="-lwrap"
  fi
]) dnl
