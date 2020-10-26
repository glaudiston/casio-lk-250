#!/bin/bash

# This script is for PoC
# The final code should be in C

# Dependences
# - bash
# - coreutils (find,grep,cat,tr,cut,mkfifo,rm,mv)
# - sed
# - alsatools (aplaymidi,aseqdump) for send data to keyboard
# - xargs
# - websocketd (www.websocketd.com) for the web interface

TMP_DIR=tmp
[ -e /dev/shm ] && TMP_DIR=/dev/shm/casio-lk-250

# on exit, remove temp folder and kill any subproccess like cat in status pipe_wait, etc.
cleanup(){
	j=$(jobs -p)
	[ -n "$j" ] && {
		echo "Killing pending jobs $j";
		xargs kill <<<${j};
	}
	rm -fr ${TMP_DIR};
	echo "CleanUp done."
}
trap cleanup ERR EXIT

MIDI_FILE="$1"
if [ "$MIDI_FILE" == "" ]; then
	MIDI_FILE=./ref/musicope/static/songs/Ramin_Djawadi_-_Westworld_Theme.mid
fi;
echo Opening MIDI [$MIDI_FILE]
echo "Connect USB between your casion and the note"
echo "Configure you casio keyboard:"
echo "- Press \`>\` button bellow LED screen until"
echo "   - Settings MIDIInNavigate: Both Hand Off"
echo "      - MIDIInNavigate: Listen - play midi input"
echo "   - Settings MIDIInNab R Ch == 1"
echo "   - Settings MIDIInNab L Ch == 1"
echo

echo -n "Detecting MIDI Device..."
DEVICE=$(find /sys/devices -name product | xargs grep "CASIO")
PRODUCT=$(echo "$DEVICE" | sed 's|.*/product:||g')

if [ "${DEVICE}" == "" ]; then
	echo "Unable to detect a CASIO MIDI Keyboard" >&2;
	echo "Please check:
        - USB connection
	- Power the keyboard ON";
	exit 1;
fi;

echo "$PRODUCT";
echo -n "Detecting MIDI port..."
MIDI_PORT=$(aplaymidi -l | grep "$PRODUCT" | tr -s " " | cut -d " " -f2)
echo "$MIDI_PORT";


mkdir -p "$TMP_DIR"

ACTIVE_NOTES_FILE=$TMP_DIR/active-notes
RELEASED_NOTES_FILE=$TMP_DIR/released-notes
NOTES_TO_OFF_FILE=${TMP_DIR}/notes-to-off
DEBUG=${TMP_DIR}/debug.log

function update_active_notes() {
	echo $@ > $ACTIVE_NOTES_FILE
}

function load_active_notes() {
	cat "$ACTIVE_NOTES_FILE" 2>/dev/null
}

function update_release_notes() {
	echo $@ > $RELEASED_NOTES_FILE
}

function load_release_notes() {
	cat "$RELEASED_NOTES_FILE" 2>/dev/null
}

function set_notes_off() {
	mv ${RELEASED_NOTES_FILE} $NOTES_TO_OFF_FILE
}

function get_notes_to_off() {
	cat $NOTES_TO_OFF_FILE 2>/dev/null
}

function transponse(){
	NOTE="$1";
	while [ $NOTE -lt 36 ];
	do
		echo "Transponsing unavailable note $NOTE on CASIO LK-250 by 12 semitune" >&2;
		NOTE=$(( NOTE + 12))
	done;
	while [ $NOTE -gt 96 ];
	do
		echo "Transponsing unavailable note $NOTE on CASIO LK-250 by 12 semitune" >&2;
		NOTE=$(( NOTE - 12))
	done;
	echo $NOTE;
}

# open a channel to send led to keyboard
function pmidi_control() {
	PMIDI_PIPE=$TMP_DIR/pmidi;
	COMMAND="$1";
	if [ "$COMMAND" == "START" ]; then
		rm -f $PMIDI_PIPE;
		mkfifo $PMIDI_PIPE;
		# create a midi file and play it with this note
		while [ -p "$PMIDI_PIPE" ];
			do aplaymidi -d 0 --port ${MIDI_PORT} <( {
				echo "0, 0, Header, 1, 1, 960";
				echo "1, 1, Start_track";
				ACTIVE_NOTES=($(load_active_notes))
				let t=0;
				TO=0;
				if [ ${#ACTIVE_NOTES[@]} -gt 4 ]; then
					TO=30;
				fi
				for (( i=0; i<${#ACTIVE_NOTES[@]}; i++ ));
				do
					for (( j=0; j<i/4; j++ )); do
						# ON Casio LK-250, we can have only 4 led on at time
						read -d= N <<< ${ACTIVE_NOTES[$((j*i + i))]}
						echo "1, $t, Note_off_c, 0, $N, 0";
						let t+=${TO};
					done
					read -d= NOTE <<< ${ACTIVE_NOTES[$i]}
					VELOCITY=$(echo ${ACTIVE_NOTES[$i]} | cut -d= -f2)
					echo "1, $t, Note_on_c, 0, $NOTE, $VELOCITY";
					let t+=${TO};
				done;

				RELEASED_NOTES=($(get_notes_to_off))
				for NOTE in ${RELEASED_NOTES[@]}; do
					echo "1, $t, Note_off_c, 0, $NOTE, 0";
				done;
				echo "1, $t, End_track";
				echo "0, 0, End_of_file";
			} | ref/midicsv-1.1/csvmidi; );
		done &
	fi;
	if [ "$COMMAND" == "TIP_ON" ]; then
		ACTION="$2";
		NOTE="$3";
		VELOCITY="$4";

		if [ ${VELOCITY:=0} == 0 ]; then
			ACTION="RELEASE";
		fi;
		if [ "$ACTION" == "PRESS" ]; then
			# add this not to active notes map
			read NOTE_LAST_ACTION 2>/dev/null < $TMP_DIR/note-last-action
			ACTIVE_NOTES=($(load_active_notes));
			if [ "${NOTE_LAST_ACTION}" == "RELEASE" ]; then
				# Turn off released notes unil now
				echo "WAITING USER INPUT FOR [$ACTION] [${ACTIVE_NOTES[@]}]"
				# WE NEED TO WAIT ALL THE ACTIVE_NOTES
				WAIT_LIST=()
				for ENTRY in ${ACTIVE_NOTES[@]}; do
					read -d= N <<< $ENTRY
					cat $(get_note_pipe $N) > /dev/null &
					WAIT_LIST=(${WAIT_LIST[@]} $!)
				done
				echo "WAITING PIDS ${WAIT_LIST[@]}" >> $DEBUG
				wait ${WAIT_LIST[@]};
				set_notes_off
				ACTIVE_NOTES=()
			fi;
			ACTIVE_NOTES=(${ACTIVE_NOTES[@]} ${NOTE}=${VELOCITY});
			update_active_notes "${ACTIVE_NOTES[@]}";
		fi;
		if [ "$ACTION" == "RELEASE" ]; then
			RELEASE_NOTES=($(load_release_notes) ${NOTE});
			update_release_notes "${RELEASE_NOTES[@]}";
		fi;
		echo "$ACTION" > $TMP_DIR/note-last-action
	fi;
	if [ "$COMMAND" == "STOP" ]; then
		rm -f $PMIDI_PIPE
	fi;
}

pmidi_control "START"

function get_note_pipe(){
	NOTE=$1
	NOTE_PIPE=$TMP_DIR/${NOTE:=0}
	if [ ! -p $NOTE_PIPE ]; then
		rm -fr $NOTE_PIPE
		mkfifo $NOTE_PIPE
	fi;
	echo $NOTE_PIPE
}

function wait_user_note_action()
{
	ACTION="$1"
	NOTE="$2"
	VELOCITY="$3"
	# echo "${ACTION} $NOTE $VELOCITY" > $TMP_DIR/ws &
	pmidi_control "TIP_ON" "$ACTION" "$NOTE" "$VELOCITY";
};

# MIDI Keyboard input
aseqdump --port $MIDI_PORT | while read MIDI_RESP ;
do
	[[ "$MIDI_RESP" =~ "Waiting for data. Press Ctrl+C to end." ]] && continue; # UI msg
	[[ "$MIDI_RESP" =~ "Source" ]] && continue; # data headers
	ACTION=$(echo $MIDI_RESP | cut -d ' ' -f 2,3 );
	NOTE=$(echo $MIDI_RESP | cut -d ' ' -f 6 | tr -d ,);
	echo $MIDI_RESP >> $DEBUG
	[ "$NOTE" == "" ] && continue;
	VELOCITY=$(echo $MIDI_RESP | cut -d ' ' -f 8);
	if [ "$ACTION" == "Note on" -a ${VELOCITY:=0} -gt 0 ]; then
		# Are we listening this note ?
		ACTIVE_NOTES=($(load_active_notes));
		for (( i=0; i<${#ACTIVE_NOTES[@]}; i++ )); do
			read -d= N <<< ${ACTIVE_NOTES[i]};
			if [ "$N" == "$NOTE" ]; then
				echo $VELOCITY > $(get_note_pipe $NOTE) &
				break;
			fi;
		done
	fi;
done &

# mkfifo $TMP_DIR/ws
# websocketd --port 8888 --staticdir static ./ws.sh &>> $DEBUG &

while read MIDI_RECORD;
do
	F1=$(echo "$MIDI_RECORD" | cut -d, -f1)
	F2=$(echo "$MIDI_RECORD" | cut -d, -f2)
	F3=$(echo "$MIDI_RECORD" | cut -d, -f3)
	F4=$(echo "$MIDI_RECORD" | cut -d, -f4-)
	ACTION=${F3:1}

	if [ "$F1" == "1" ]; then
		if [ "$ACTION" == "Title_t" ]; then
			TITLE="$F4";
			echo "Playing $TITLE";
		fi;
	fi;
	if [ "$F1" == "2" ]; then
		if [ "$ACTION" == "Title_t" ]; then
			INSTRUMENT="$F4";
			echo " on $INSTRUMENT";
		fi;
		TIME=$(( $F2 ))
	fi;
	echo MIDI FILE: $TITLE - $INSTRUMENT - [$MIDI_RECORD] - [$TIME][$ACTION][$TYPE][$F3] >> $DEBUG
	if [[ "$ACTION" =~ End_of_file ]]; then
		echo "$ACTION"
		exit 0;
	fi;
	if [ "$ACTION" == "Note_on_c" ];then
		# Wait user to press this note
		NOTE=$( transponse $(echo "$MIDI_RECORD" | cut -d, -f5) )
		VELOCITY=$(( $(echo "$MIDI_RECORD" | cut -d, -f6) ))
		wait_user_note_action "PRESS" "$NOTE" "${VELOCITY:=0}"
	fi;
	if [ "$ACTION" == "Note_off_c" ];then
		# Wait user to press this note
		NOTE=$( transponse $(echo "$MIDI_RECORD" | cut -d, -f5) )
		wait_user_note_action "RELEASE" "$NOTE"
	fi;
	#if [ "$F1" == "2" -a "$ACTION" == "End_track" ]; then
	#	exit 0;
	#fi;
done < <(ref/midicsv-1.1/midicsv "$MIDI_FILE")
pmidi_control "STOP";
cleanup

wait
