#!/bin/bash

# This isn't an ordinary regression test, but instead it runs splint
# on the source files in the src directory. If the user doesn't have
# splint, we do nothing and pretend success.

# splint is very fussy, and prints many spurious errors, which cannot
# all be fixed by editing the dbacl source code. In particular,
# nonstandard header files cause a problem, as do compiler
# idiosyncracies. So it is not a good idea to run the lint check as
# part of the regular regression tests, since it would make dbacl
# uncompilable on some systems.

# There are other issues such as the fact that dbacl uses realloc and
# splint can't deal with this properly. But overall, it's worth using
# it.

PATH=/bin:/usr/bin
SRCDIR=.

prerequisite_command() {
    type $2 2>&1 > /dev/null
    if [ 0 -ne $? ]; then
        echo "$1: $2 not found, test will be skipped"
        exit 77
    fi
}

prerequisite_command $0 splint

# This part is messy. Some source files need special preprocessor symbols,
# so I've chosen to run a separate splint command on each source file. 
# Ideally, this would be generated automatically by analysing the automake
# file, but I have no idea how to do it. Note that splint should only 
# check the dbacl files, not the public domain files included from elsewhere.

splint -f $SRCDIR/splintrc $SRCDIR/dbacl.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/bayesol.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/catfun.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/const.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/dbacl.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/fh.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/fram.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/hmine.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/hparse.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/hypex.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/mailinspect.c \
&& splint -f $SRCDIR/splintrc -DMBW_WIDE $SRCDIR/mbw.c \
&& splint -f $SRCDIR/splintrc -DMBW_MB $SRCDIR/mbw.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/probs.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/rfc2822.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/rfc822.c \
&& splint -f $SRCDIR/splintrc $SRCDIR/util.c
