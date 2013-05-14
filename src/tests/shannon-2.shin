#!/bin/sh
# test average shannon entropy calculation for multiple documents
# we test with -w 2 switch (type -p only counts 2-grams for the shannon entropy)
# (verification is more accurate with identical mails)

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
prerequisite_command $0 tr

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

(echo ; echo "From - -" ; cat ${sourcedir}/sample.spam-1 ; \
 echo ; echo "From - -" ; cat ${sourcedir}/sample.spam-1 ; \
 echo ; echo "From - -" ; cat ${sourcedir}/sample.spam-1 ; \
 echo ; echo "From - -" ; cat ${sourcedir}/sample.spam-1 ; ) \
    > "$DBACL_PATH/mbox"

cat "$DBACL_PATH/mbox" \
    | $DBACL -l dummy -T email -X -d -w 2 \
    | grep '# shannon' \
    | awk '{print $3/log(2)}' \
    | tr '\n' ' ' \
    > "$DBACL_PATH/out1"

cat "$DBACL_PATH/mbox" \
    | formail -s $DBACL -c dummy -vX \
    | awk '{d += $3; e += $5} END{print (d/4), (e/4)}' \
    | tr '\n' ' ' \
    > "$DBACL_PATH/out2"

echo "`cat \"$DBACL_PATH/out1\"` `cat \"$DBACL_PATH/out2\"`" \
    | awk '
function abs(x) { return (x >= 0) ? x : -x }
{
    # must invert exit value
    exit !( abs($1 - $3) < (0.15/2.0) * ($1 + $3) ) 
}'

RESULT=$?
rm -rf "$DBACL_PATH"

exit $RESULT