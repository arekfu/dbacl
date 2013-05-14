#!/bin/sh
# maxent model sum check (-w 1) must sum to less than 1.
PATH=/bin:/usr/bin
DBACL=$TESTBIN/dbacl

LC_ALL="C" # this test assumes C locale
export LC_ALL

prerequisite_command() {
    type $2 2>&1 > /dev/null
    if [ 0 -ne $? ]; then
        echo "$1: $2 not found, test will be skipped"
        exit 77
    fi
}

prerequisite_command $0 cut
prerequisite_command $0 grep
prerequisite_command $0 awk

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

echo "The quick brown fox jumped over the lazy dog" \
    | $DBACL -l dummy -L uniform -d -q 2 \
    > $DBACL_PATH/dummy.dump

cat $DBACL_PATH/dummy.dump \
    | grep logZ \
    | cut -d ' ' -f 5 \
    > $DBACL_PATH/dummy.logZ

cat $DBACL_PATH/dummy.dump \
    | grep -v '^#' \
    | awk "
BEGIN{ logz=`cat $DBACL_PATH/dummy.logZ` }
{ s += exp(\$1 + \$2 - logz) }
END{ exit(s > 1.0) }"
RESULT=$?

rm -rf "$DBACL_PATH"

exit $RESULT