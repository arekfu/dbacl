#!/bin/sh
# test basic dbacl -a switch
PATH=/bin:/usr/bin
DBACL=$TESTBIN/dbacl

prerequisite_command() {
    type $2 2>&1 > /dev/null
    if [ 0 -ne $? ]; then
        echo "$1: $2 not found, test will be skipped"
        exit 77
    fi
}

prerequisite_command $0 grep

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

(echo "From -" ; cat ${sourcedir}/sample.spam-1 ; echo ; echo "From -" ; cat ${sourcedir}/sample.spam-2) \
    | $DBACL -0 -l dummy -T email
head -3 $DBACL_PATH/dummy \
    | grep '^# hash' \
    > $DBACL_PATH/out1

(echo "From -" ; cat ${sourcedir}/sample.spam-1) \
    | $DBACL -0 -l dummy -T email -o dummy.onl
(echo "From -" ; cat ${sourcedir}/sample.spam-2) \
    | $DBACL -0 -l dummy -T email -o dummy.onl
head -3 $DBACL_PATH/dummy \
    | grep '^# hash' \
    > $DBACL_PATH/out2

test x"`cat $DBACL_PATH/out1`" = x"`cat $DBACL_PATH/out2`"

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT