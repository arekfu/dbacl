#!/bin/bash
# This is the code for dce-1.sh 
# This script functions as an incomplete chess engine for XBoard.
# It is an integral part of the essay
#
# "Can spam filters play chess?" (c) 2005 Laird A. Breyer
# http://www.lbreyer.com/spam_chess.html
# 
# The essay explains the construction of this engine by
# successive modifications of the dce-basic.sh script.
#
# THE dce-*.sh SCRIPTS ARE INCOMPLETE ENGINES AND INTENDED
# ONLY FOR ILLUSTRATION OF POINTS RAISED IN THE ESSAY.
# IF YOU WANT TO TRY OUT YOUR OWN MODIFICATIONS OF THE
# CHESS ENGINE, YOU SHOULD MAKE A COPY OF THE FINAL (dce.sh)
# FILE AND CHANGE THAT COPY.

DBACL=./dbacl/src/dbacl
SAN=./SAN/SAN_SRC/san
TMP=.
DCE=dce-1

SANOK="no"
PGN="$TMP/$DCE.current.pgn"
GAMELINE="$TMP/$DCE.current.gameline"
SCORES="$TMP/$DCE.current.scores"
ENGINEMOVE="$TMP/$DCE.current.emove"
SANOUT="$TMP/$DCE.current.stdout"
SANERR="$TMP/$DCE.current.stderr"

SIDE="black"
MOVENOW="no"
CATFILE="./BlackWinDraw-1"

trap "" SIGTERM
trap "" SIGINT

function exec_san() {
	rm -rf $PGN.new $SANOUT $SANERR
	echo -ne "svop namd nabd napr\nlfer $PGN\n$1\nsfer $PGN.new" \
		| "$SAN" > "$SANOUT" 2> "$SANERR"
	if grep Error "$SANOUT" > /dev/null; then
		echo "Error (illegal move): $cmd"
		return 1
	else 
		mv $PGN.new $PGN
	fi
	return 0
}

function do_reset() {
	rm -f $PGN
	touch $PGN
	exec_san ""
}

function do_move() {
	if [ "$SANOK" != "yes" ]; then
		echo "Illegal move (you must use SAN): $1"
		echo "Error (cannot play): $1"
	fi
	if exec_san "$1"; then
		MOVENOW="yes"
	fi
}

function echo_gameline() {
	cat "$PGN" \
	| sed -e 's/\r//g' \
	| sed -e :a -e '$!N;s/\n\([a-hKQNBRO0-9]\)/ \1/;ta' -e 'P;D' \
	| sed -e 's/^ *//' \
	| grep '^1\.' \
	| sed 's/[0-9]*\.[ ]*//g' \
	| sed 's/ \*.*$//'
}

function echo_completions() {
	# legal moves are in $SANOUT
	cat "$SANOUT" \
	| grep '.* : 1$' \
	| cut -f 1 -d ' ' \
	| while read move; do
		echo "`cat $GAMELINE` $move"
	done
}

function echo_scorefile() {
	cat "$1" \
	| sort -k 2 -n \
	| head -1 \
	| sed -e 's/^.* //'
}

function combine_halfmoves() {
	sed -e 's/$/ Z/' -e 's/ \([^ ]* *[^ ]\)/_\1/g' -e 's/[ ]*Z$//'
}

function split_fullmove() {
	sed -e 's/.*_//'
}

function do_engine_move() {
	if exec_san "enum 1"; then

		echo_gameline > "$GAMELINE"

		# make all completions and let dbacl decide
		echo_completions \
		| combine_halfmoves \
		| "$DBACL" -n -c "$CATFILE" -f 1 \
		> "$SCORES"
		
		if [ `cat "$SCORES"| wc -l` = "0" ]; then
			# no moves left, game over!
			# the gameline contains the result
			echo "`cat $GAMELINE | sed 's/^.* //'` {Play again?}"
			return 0
		else
			# pick best scoring move
			echo_scorefile "$SCORES" \
			| split_fullmove \
			> "$ENGINEMOVE"
	
			if exec_san "`cat $ENGINEMOVE`"; then
				echo "move `cat $ENGINEMOVE`"
				return 0
			fi
		fi

	fi
	return 1
}


while read cmd; do
	case "$cmd" in
	xboard)
		echo "feature san=1 done=1"
		;;
	"accepted san")
		SANOK="yes"		
		;;
	quit)
		exit 0
		;;
	new)
		do_reset
		;;
	variant*)
		echo "Error (only standard chess please): $cmd"
		;;
	[a-h][x1-8]*)
		do_move "$cmd"
		;;
	[KQBNRP][xa-h]*)
		do_move "$cmd"
		;;
	O-O*)
		do_move "$cmd"
		;;
	analyze)
		echo "Error (unknown command): $cmd"
		;;
	*) 
		# ignore other commands
		echo "Error (command not implemented): $cmd"
		;;
	esac
	if [ "$MOVENOW" = "yes" ]; then
		if do_engine_move; then
			MOVENOW="no"
		else
			echo "Error (engine blew up): kaboom"
			exit 1
		fi
	fi
done
