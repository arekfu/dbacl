#!/bin/sh
# verify that reservoir sample scores are correct.
PATH=/bin:/usr/bin
DBACL=$TESTBIN/dbacl

prerequisite_command() {
    type $2 2>&1 > /dev/null
    if [ 0 -ne $? ]; then
        echo "$1: $2 not found, test will be skipped"
        exit 77
    fi
}

prerequisite_command $0 formail
prerequisite_command $0 grep
prerequisite_command $0 awk

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

(cat ${sourcedir}/sample.spam-8 ; echo ; \
 cat ${sourcedir}/sample.spam-9 ; echo ; \
 cat ${sourcedir}/sample.spam-7 ; echo ; \
 cat ${sourcedir}/sample.spam-3 ) \
    > "$DBACL_PATH/mbox"

cat "$DBACL_PATH/mbox" \
    | $DBACL -l dummy -X -v -T email 2>&1 \
    | grep '^reservoir' \
    > "$DBACL_PATH/out1"

cat "$DBACL_PATH/mbox" \
    | formail -s $DBACL -c dummy -vX \
    > "$DBACL_PATH/out2"

paste "$DBACL_PATH/out1" "$DBACL_PATH/out2" \
    | awk '
function abs(x) { return (x >= 0) ? x : -x }
{
    if( abs($3 - $10) > 0.1 ) {
	print; exit 1
    }
}'

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT