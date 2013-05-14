#!/bin/sh
# test Japanese parsing
# this should work in the C locale
#
# you can also check out the tokens directly in a "kterm -km sjis".
#
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

cat $DOCDIR/japanese.txt \
    | $DBACL -l dummy -e cef -j
head -3 "$DBACL_PATH/dummy" \
    | grep '# hash_size 15 features 8614 unique_features 543 documents 0' \
    > /dev/null

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT