dnl @synopsis AX_FUNC_POSIX_MEMALIGN
dnl 
dnl Some versions of posix_memalign (notably glibc 2.2.5) incorrectly
dnl apply their power-of-two check to the size argument, not the alignment
dnl argument.  AX_FUNC_POSIX_MEMALIGN defines HAVE_POSIX_MEMALIGN if the
dnl power-of-two check is correctly applied to the alignment argument.
dnl 
dnl @version $Id: $
dnl @author Scott Pakin <pakin@uiuc.edu>
dnl
AC_DEFUN([AX_FUNC_POSIX_MEMALIGN],
[AC_CACHE_CHECK([for working posix_memalign],
  [ax_cv_func_posix_memalign_works],
  [AC_TRY_RUN([
#include <stdlib.h>

int
main ()
{
  void *buffer;

  /* Some versions of glibc incorrectly perform the alignment check on
   * the size word. */
  exit (posix_memalign (&buffer, sizeof(void *), 123) != 0);
}
    ],
    [ax_cv_func_posix_memalign_works=yes],
    [ax_cv_func_posix_memalign_works=no],
    [ax_cv_func_posix_memalign_works=no])])
if test "$ax_cv_func_posix_memalign_works" = "yes" ; then
  AC_DEFINE([HAVE_POSIX_MEMALIGN], [1],
    [Define to 1 if `posix_memalign' works.])
fi
])
