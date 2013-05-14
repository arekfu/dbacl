#!/bin/sh
# verify that empirical divergence is zero for a single email.
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
prerequisite_command $0 awk
prerequisite_command $0 tr

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

cat ${sourcedir}/sample.spam-2 \
    | $DBACL -0 -l dummy -X -d -T email \
    | grep '# shannon' \
    | awk '{print $5/log(2)}' \
    | tr '\n' ' ' \
    > "$DBACL_PATH/out1"

cat ${sourcedir}/sample.spam-2 \
    | $DBACL -c dummy -vX \
    | awk '{print $3, $5}' \
    > "$DBACL_PATH/out2"

echo "`cat \"$DBACL_PATH/out1\"` `cat \"$DBACL_PATH/out2\"`" \
    | awk '
function abs(x) { return (x >= 0) ? x : -x }
{
    # error in divergence can be up to 15% of shannon entropy estimate
    # must invert exit value
    exit !( abs($1 - $2) < (0.15) * ($3) ) 
}'

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT