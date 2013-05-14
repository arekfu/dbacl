#!/bin/bash
# This script is a simple chess engine for dbacl and XBoard.
# It is based on the essay 
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
#
# Revision history:
# 
# 28/06/2005 (Laird Breyer) Initial release.
#

DBACL=./dbacl/src/dbacl
SAN=./SAN/SAN_SRC/san
TMP=.
DCE=dce.$$

SANOK="no"
PGN="$TMP/$DCE.current.pgn"
GAMELINE="$TMP/$DCE.current.gameline"
SCORES="$TMP/$DCE.current.scores"
CAPTURES="$SCORES.cap"
ENGINEMOVE="$TMP/$DCE.current.emove"
SANOUT="$TMP/$DCE.current.stdout"
SANERR="$TMP/$DCE.current.stderr"

RANPLAY="no"
SIDE="black"
MOVENOW="no"
FORCEMODE="no"
CATFILE="./BlackWinDraw"

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

	SIDE="black"
	FORCEMODE="no"
	MOVENOW="no"
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

function randomizer() {
	# assume stdin is sorted and $2 is the score
	awk '
{
  if( cf == 0 ) {
    cf = $2;
  }
  score[NR] = $2 - cf;
  line[NR] = $0;
}

END{
  srand();
  while(1) {
    x = int(rand() * NR) + 1;
    t = -log(rand());
    if( log(2) * score[x] < t ) {
      print line[x];
      break;
    }
  }
}
'
}

function echo_best_move() {
	# assume $1 is sorted already
	if [ "$RANPLAY" = "yes" ]; then
		cat $1 \
		| randomizer \
		| sed -e 's/^.* //'
	else 
		cat "$1" \
		| head -1 \
		| sed -e 's/^.* //'
	fi
}

function combine_halfmoves() {
	sed -e 's/$/ Z/' -e 's/ \([^ ]* *[^ ]\)/_\1/g' -e 's/[ ]*Z$//'
}

function split_fullmove() {
	sed -e 's/.*_//'
}

function get_captures() {
	cat "$SCORES" \
	| grep '^.* [^ ]*x[^ _]*$' \
	> "$CAPTURES"

	# we rearrange the captures in order of least important piece to 
	# most important piece
	grep -v '[NBRQK][^_ ]*$' "$CAPTURES" > "$CAPTURES.new"
	grep 'N[^_ ]*$' "$CAPTURES" >> "$CAPTURES.new"
	grep 'B[^_ ]*$' "$CAPTURES" >> "$CAPTURES.new"
	grep 'R[^_ ]*$' "$CAPTURES" >> "$CAPTURES.new"
	grep 'Q[^_ ]*$' "$CAPTURES" >> "$CAPTURES.new"
	# king is too valuable to force captures
	# grep 'K[^_ ]*$' "$CAPTURES" >> "$CAPTURES.new"
	mv "$CAPTURES.new" "$CAPTURES"

	N=`cat $CAPTURES| wc -l`

	if [ "$N" = "0" -o "$N" = "1" ]; then
		# don't recommend capture
		return 1
	fi
	# tell engine to make a capturing move
	return 0
}

function do_engine_move() {
	if exec_san "enum 1"; then

		echo_gameline > "$GAMELINE"

		# make all completions and let dbacl decide
		echo_completions \
		| combine_halfmoves \
		| "$DBACL" -n -c "$CATFILE" -f 1 \
		| sort -k 2 -n \
		> "$SCORES"
		
		if [ "`cat $SCORES| wc -l`" = "0" ]; then
			# no moves left, game over!
			# the gameline contains the result
			echo "`cat $GAMELINE | sed 's/^.* //'` {Play again?}"
			return 0
		else
			# pick best scoring move
			echo_best_move "$SCORES" \
			| split_fullmove \
			> "$ENGINEMOVE"
	
			if get_captures; then
				echo_best_move "$CAPTURES" \
				| split_fullmove \
				> "$ENGINEMOVE"
			fi

			if exec_san "`cat $ENGINEMOVE`"; then
				echo "move `cat $ENGINEMOVE`"
				return 0
			fi
		fi

	fi
	return 1
}

function get_side_to_play() {
	N=`echo_gameline | wc -w`
	if [ "`expr $N % 2`" = "0" ]; then
		SIDE="white"
	else
		SIDE="black"
	fi
}

while read cmd; do
	case "$cmd" in
	xboard)
		echo "feature san=1 done=1"
		;;
	"accepted san")
		SANOK="yes"		
		;;
	random)
		RANPLAY="yes"
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
	force)
		FORCEMODE="yes"
		;;
	white)
		SIDE="black"
		;;
	black)
		SIDE="white"
		;;
	go)
		get_side_to_play
		FORCEMODE="no"
		MOVENOW="yes"
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
	if [ "${FORCEMODE}${MOVENOW}" = "noyes" ]; then
		if do_engine_move; then
			MOVENOW="no"
		else
			echo "Error (engine blew up): kaboom"
			exit 1
		fi
	fi
done

# don't forget to clean up
rm -f $DCE.current.*