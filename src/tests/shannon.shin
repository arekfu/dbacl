#!/bin/sh
# test basic dbacl -l switch
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

echo "The quick brown fox jumped over the lazy dog" \
    | $DBACL -l dummy -X -d \
    | grep '# shannon 2.043' \
    > /dev/null

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT