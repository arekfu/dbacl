#!/bin/sh
# test basic maildir style parsing
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
prerequisite_command $0 cut

DBACL_PATH="`pwd`/`basename $0 .sh`_`date +"%Y%m%dT%H%M%S"`"
export DBACL_PATH

mkdir "$DBACL_PATH"

$DBACL -l dummy -T email ${sourcedir}/sample.spam-1 ${sourcedir}/sample.spam-2 \
    > "$DBACL_PATH/out"
NUM=`head -3 "$DBACL_PATH/dummy" | grep '# hash_size' | cut -d ' ' -f 9`

rm -rf "$DBACL_PATH"

test x$NUM = x"2"