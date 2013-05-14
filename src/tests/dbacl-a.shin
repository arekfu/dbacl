#!/bin/sh
# test basic dbacl -a switch
PATH=/bin:/usr/bin
DBACL=$TESTBIN/dbacl

prerequisite_command() {
    type $2 2>&1 > /dev/null
    if [ 0 -ne $? ]; then
        echo "$1: $2 not found, test will be skipped"
        exit 77
    fi
}

prerequisite_command $0 tr

test 91 -eq `cat ${sourcedir}/sample.spam-1 | $DBACL -R -a -n -v | wc -l`

