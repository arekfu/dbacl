#!/bin/sh
# maxent model symmetry check (-w 2) 2-grams with identical 
# length must have identical reference weights (but lambda weights will differ).
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
prerequisite_command $0 tr
prerequisite_command $0 sort
prerequisite_command $0 wc

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

echo "The quick brown fox jumped over the lazy dog" \
    | $DBACL -l dummy -L uniform -d -q 2 \
    | grep -v '^#' \
    | awk '{print $2}' \
    | sort -u \
    | wc -l \
    > $DBACL_PATH/dummy.count

L=`cat $DBACL_PATH/dummy.count`

RESULT=`test x"$L" = x8`

rm -rf "$DBACL_PATH"

exit $RESULT