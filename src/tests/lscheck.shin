#!/bin/sh
# category file load/save test
PATH=/bin:/usr/bin
DBACL=$TESTBIN/dbacl
ICHECK=$TESTBIN/icheck

LC_ALL="C" # this test assumes C locale
export LC_ALL

prerequisite_command() {
    type $2 2>&1 > /dev/null
    if [ 0 -ne $? ]; then
        echo "$1: $2 not found, test will be skipped"
        exit 77
    fi
}

prerequisite_command $0 grep
prerequisite_command $0 awk
prerequisite_command $0 sort
prerequisite_command $0 diff

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

echo "The quick brown fox jumped over the lazy dog" \
    | $DBACL -l dummy -L uniform -d \
    | grep -v '^#' \
    | awk '{print $1, $4}' \
    | sort -k 2 \
    > "$DBACL_PATH/out1"

$ICHECK -d -u "$DBACL_PATH/dummy" \
    | grep -v '^#' \
    | awk '{print $1, $2}' \
    | sort -k 2 \
    > "$DBACL_PATH/out2"

diff "$DBACL_PATH/out1" "$DBACL_PATH/out2"

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT