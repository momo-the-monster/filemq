#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.61)
#
# The version number is extracted from include/fmq.h using
# the version.sh script. Hence, it should be updated there.
# The version in git should reflect the *next* version planned.
#
AC_INIT([clab],[m4_esyscmd([./version.sh fmq])],[zeromq-dev@lists.zeromq.org])

AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(config)
AC_CONFIG_HEADERS([src/platform.h])
AM_INIT_AUTOMAKE(tar-ustar)

# This defines PACKAGE_VERSION_... in src/platform.h
PV_MAJOR=`echo $PACKAGE_VERSION | cut -d . -f 1`
PV_MINOR=`echo $PACKAGE_VERSION | cut -d . -f 2`
PV_PATCH=`echo $PACKAGE_VERSION | cut -d . -f 3`
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MAJOR],[$PV_MAJOR],
    [FMQ major version])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MINOR],[$PV_MINOR],
    [FMQ minor version])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_PATCH],[$PV_PATCH],
    [FMQ patchlevel])
# This lets us use PACKAGE_VERSION in Makefiles
AC_SUBST(PACKAGE_VERSION)

#
# Libtool -version-info (ABI version)
#
# Currently 1.0.0 ("stable"). Don't change this unless you
# know exactly what you're doing and have read and understand
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
#
# libfmq -version-info
LTVER="1:0:0"
AC_SUBST(LTVER)

# Capture c flags
FMQ_ORIG_CFLAGS="${CFLAGS:-none}"

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_SED
AC_PROG_AWK

# Checks for libraries.
AC_ARG_WITH(gcov, [AS_HELP_STRING([--with-gcov=yes/no],
                  [With GCC Code Coverage reporting.])],
                  [FMQ_GCOV="$withval"])

if test "x${FMQ_GCOV}" == "xyes"; then
    CFLAGS="-O0 -g -fprofile-arcs -ftest-coverage"

    if test "x${FMQ_ORIG_CFLAGS}" != "xnone"; then
        CFLAGS="${CFLAGS} ${FMQ_ORIG_CFLAGS}"
    fi
fi

# libzmq integration
AC_ARG_WITH([libzmq],
            [AS_HELP_STRING([--with-libzmq],
                            [Specify libzmq prefix])],
            [czmq_search_libzmq="yes"],
            [])

if test "x$czmq_search_libzmq" = "xyes"; then
    if test -r "${with_libzmq}/include/zmq.h"; then
        CFLAGS="-I${with_libzmq}/include ${CFLAGS}"
        LDFLAGS="-L${with_libzmq}/lib ${LDFLAGS}"
    else
        AC_MSG_ERROR([${with_libzmq}/include/zmq.h not found. Please check libzmq prefix])
    fi
fi

AC_CHECK_LIB(zmq, zmq_init, ,[AC_MSG_ERROR([cannot link with -lzmq, install libzmq.])])

AC_MSG_CHECKING([whether libzmq installation works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <zmq.h>],
                                [zmq_init(1);])
               ],
               [AC_MSG_RESULT([yes])],
               [AC_MSG_ERROR([no. Please specify libzmq installation prefix using --with-libzmq])])

# libczmq integration
AC_ARG_WITH([libczmq],
            [AS_HELP_STRING([--with-libczmq],
                            [Specify libczmq prefix])],
            [fmq_search_libczmq="yes"],
            [])

if test "x$fmq_search_libczmq" = "xyes"; then
    if test -r "${with_libczmq}/include/czmq.h"; then
        CFLAGS="-I${with_libczmq}/include ${CFLAGS}"
        LDFLAGS="-L${with_libczmq}/lib ${LDFLAGS}"
    else
        AC_MSG_ERROR([${with_libczmq}/include/czmq.h not found. Please check libczmq prefix])
    fi
fi

AC_CHECK_LIB(czmq, zctx_new, ,[AC_MSG_ERROR([cannot link with -lczmq, install libczmq.])])

AC_MSG_CHECKING([whether libczmq installation works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <czmq.h>],
                                [zctx_new ();])
               ],
               [AC_MSG_RESULT([yes])],
               [AC_MSG_ERROR([no. Please specify libczmq installation prefix using --with-libczmq])])

# Other libraries
AC_CHECK_LIB(crypto, SHA1_Init, ,[AC_MSG_ERROR([cannot link with -lcrypto, install libssl-dev.])])

#Whether we are on mingw or not.
fmq_on_mingw32="no"

# Host speciffic checks
AC_CANONICAL_HOST

# Determine whether or not documentation should be built and installed.
fmq_build_doc="yes"
fmq_install_man="yes"

# Check for asciidoc and xmlto and don't build the docs if these are not installed.
AC_CHECK_PROG(fmq_have_asciidoc, asciidoc, yes, no)
AC_CHECK_PROG(fmq_have_xmlto, xmlto, yes, no)
if test "x$fmq_have_asciidoc" = "xno" -o "x$fmq_have_xmlto" = "xno"; then
    fmq_build_doc="no"
    # Tarballs built with 'make dist' ship with prebuilt documentation.
    if ! test -f doc/fmq.7; then
        fmq_install_man="no"
        AC_MSG_WARN([You are building an unreleased version of libfmq and asciidoc or xmlto are not installed.])
        AC_MSG_WARN([Documentation will not be built and manual pages will not be installed.])
    fi
fi
AC_MSG_CHECKING([whether to build documentation])
AC_MSG_RESULT([$fmq_build_doc])
AC_MSG_CHECKING([whether to install manpages])
AC_MSG_RESULT([$fmq_install_man])

# Set some default features required by libfmq code.
CPPFLAGS="-D_REENTRANT -D_THREAD_SAFE $CPPFLAGS"

# OS-specific tests
case "${host_os}" in
    *linux*)
        # Define on Linux to enable all library features
        CPPFLAGS="-D_GNU_SOURCE -DLINUX $CPPFLAGS"
        AC_DEFINE(FMQ_HAVE_LINUX, 1, [Have Linux OS])
        ;;
    *solaris*)
        # Define on Solaris to enable all library features
        CPPFLAGS="-D_PTHREADS $CPPFLAGS"
        AC_DEFINE(FMQ_HAVE_SOLARIS, 1, [Have Solaris OS])
        ;;
    *freebsd*)
        # Define on FreeBSD to enable all library features
        CPPFLAGS="-D__BSD_VISIBLE $CPPFLAGS"
        AC_DEFINE(FMQ_HAVE_FREEBSD, 1, [Have FreeBSD OS])
        ;;
    *darwin*)
        # Define on Darwin to enable all library features
        CPPFLAGS="-D_DARWIN_C_SOURCE $CPPFLAGS"
        AC_DEFINE(FMQ_HAVE_OSX, 1, [Have DarwinOSX OS])
        ;;
    *netbsd*)
        # Define on NetBSD to enable all library features
        CPPFLAGS="-D_NETBSD_SOURCE $CPPFLAGS"
        AC_DEFINE(FMQ_HAVE_NETBSD, 1, [Have NetBSD OS])
        ;;
    *openbsd*)
        # Define on OpenBSD to enable all library features
        CPPFLAGS="-D_BSD_SOURCE $CPPFLAGS"
        AC_DEFINE(FMQ_HAVE_OPENBSD, 1, [Have OpenBSD OS])
        ;;
    *nto-qnx*)
        AC_DEFINE(FMQ_HAVE_QNXNTO, 1, [Have QNX Neutrino OS])
        ;;
    *aix*)
        AC_DEFINE(FMQ_HAVE_AIX, 1, [Have AIX OS])
        ;;
    *hpux*)
        # Define on HP-UX to enable all library features
        CPPFLAGS="-D_POSIX_C_SOURCE=200112L"
        AC_DEFINE(FMQ_HAVE_HPUX, 1, [Have HPUX OS])
        ;;
    *mingw32*)
        AC_DEFINE(FMQ_HAVE_WINDOWS, 1, [Have Windows OS])
        AC_DEFINE(FMQ_HAVE_MINGW32, 1, [Have MinGW32])
        AC_CHECK_HEADERS(windows.h)
        fmq_on_mingw32="yes"
        fmq_install_man="no"
        ;;
    *cygwin*)
        # Define on Cygwin to enable all library features
        CPPFLAGS="-D_GNU_SOURCE $CPPFLAGS"
        AC_DEFINE(FMQ_HAVE_CYGWIN, 1, [Have Cygwin])
        ;;
    *)
        AC_MSG_ERROR([unsupported system: ${host_os}.])
        ;;
esac

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(errno.h arpa/inet.h netinet/tcp.h netinet/in.h stddef.h \
stdlib.h string.h sys/socket.h sys/time.h unistd.h limits.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_HEADER_TIME
AC_TYPE_UINT32_T
AC_C_VOLATILE

# Math libraries
AC_CHECK_LIB([m], [pow])

# These options are GNU compiler specific.
if test "x$GCC" = "xyes"; then
    CPPFLAGS="-pedantic -Werror -Wall -Wno-unused ${CPPFLAGS}"
fi

AM_CONDITIONAL(ON_MINGW, test "x$fmq_on_mingw32" = "xyes")
AM_CONDITIONAL(INSTALL_MAN, test "x$fmq_install_man" = "xyes")
AM_CONDITIONAL(BUILD_DOC, test "x$fmq_build_doc" = "xyes")

# Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(perror gettimeofday memset)

AC_OUTPUT(Makefile scripts/Makefile src/Makefile doc/Makefile src/libfmq.pc)
