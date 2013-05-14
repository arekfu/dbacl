#!/bin/sh
# verify classification of unknown words 
# because some words have been learned, unknown words
# have a lower probability compared to the purely random case
PATH=/bin:/usr/bin
DBACL=$TESTBIN/dbacl

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

echo "The quick brown fox jumped over the lazy dog" \
    | $DBACL -l dummy -L uniform -q 2

echo "bla bri pto lir lsk wqe" \
    | $DBACL -c dummy -v -R \
    > $DBACL_PATH/verdict

V=`cat $DBACL_PATH/verdict`

RESULT=`test x"$V" = xrandom` 

rm -rf "$DBACL_PATH"

exit $RESULT