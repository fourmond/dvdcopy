dnl Template file for configure.
AC_INIT([dvdcopy], [0.1])
AM_INIT_AUTOMAKE([foreign])

# AC_CONFIG_MACRO_DIRS([/usr/share/aclocal])

AC_CHECK_HEADER(dvdread/dvd_reader.h, [], 
                [AC_MSG_ERROR([cannot find dvdread headers, please install libdvdread-dev])])
AC_CHECK_LIB(dvdread, DVDOpenFile,[],
             [AC_MSG_ERROR([cannot link to dvdread])])

AC_CHECK_HEADER(getopt.h)

AC_CHECK_HEADERS(linux/cdrom.h)

AC_CHECK_PROG([PANDOC], [pandoc], [$(which pandoc)])

AC_PROG_CXX
AC_LANG([C++])

AX_CXX_COMPILE_STDCXX_11()

AC_MSG_CHECKING([for C++11 lambda support])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
        auto l = [[]]() {};])
    ],
    AC_MSG_RESULT(ok),
    AC_MSG_RESULT(not available)
    AC_MSG_ERROR([C++11 "lambda" support required; use g++ 4.7 or newer])
)
	
AC_OUTPUT([Makefile])
