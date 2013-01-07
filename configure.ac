dnl Template file for configure.
AC_INIT([dvdcopy], [0.1])
AM_INIT_AUTOMAKE([foreign])

# AC_CONFIG_MACRO_DIRS([/usr/share/aclocal])

AC_CHECK_HEADER(dvdread/dvd_reader.h)
AC_CHECK_LIB(dvdread, DVDOpenFile)
AC_CHECK_HEADER(getopt.h)

AC_PROG_CXX
AC_LANG([C++])

AX_CXX_COMPILE_STDCXX_11()


	
AC_OUTPUT([Makefile])
