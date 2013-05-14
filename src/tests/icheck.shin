#!/bin/sh
# category file integrity test
PATH=/bin:/usr/bin
DBACL=$TESTBIN/dbacl
ICHECK=$TESTBIN/icheck

LC_ALL="C" # this test assumes C locale
export LC_ALL

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

echo "The quick brown fox jumped over the lazy dog" \
    | $DBACL -l dummy -L uniform

$ICHECK -u "$DBACL_PATH/dummy"

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT