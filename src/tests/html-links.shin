#!/bin/sh
# test -T html:links switch
PATH=/bin:/usr/bin
DBACL=$TESTBIN/dbacl

prerequisite_command() {
    type $2 2>&1 > /dev/null
    if [ 0 -ne $? ]; then
        echo "$1: $2 not found, test will be skipped"
        exit 77
    fi
}

prerequisite_command $0 diff
prerequisite_command $0 tr

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

cat ${sourcedir}/sample.spam-2 \
    | $DBACL -R -D -T html -T html:links \
    | tr -s '[ \t\r\n]' \
    > "$DBACL_PATH/out"

diff ${sourcedir}/verify.html-links "$DBACL_PATH/out"

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT