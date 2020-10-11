#!/bin/bash

# This script is for PoC
# The final code should be in C

MIDI_FILE=./ref/musicope/static/songs/Ramin_Djawadi_-_Westworld_Theme.mid
echo "Connect USB between your casion and the note"
echo "Configure you casio keyboard:"
echo "- Press \`>\` button bellow LED screen until"
echo "   - Settings MIDIInNavigate - Turn Off"
echo "      - Listen play midi input"
echo "      - Off - Only leds"
echo "   - Settings MIDIInNab R Ch == 1"
echo "   - Settings MIDIInNab L Ch == 1"
echo
DEVICE=$(find /sys/devices -name product | xargs grep "CASIO")
PRODUCT=$(echo "$DEVICE" | sed 's|.*/product:||g')
MIDI_PORT=$(aplaymidi -l | grep "$PRODUCT" | tr -s " " | cut -d " " -f2)

mkdir -p .note-pipe

# open a channel to send led to keyboard
function pmidi_control() {
	PMIDI_PIPE=.note-pipe/pmidi
	COMMAND="$1"
	if [ "$COMMAND" == "START" ]; then
		rm -f $PMIDI_PIPE
		mkfifo $PMIDI_PIPE
		# create a midi file and play it with this note
		pmidi --port ${MIDI_PORT} <( {
		<( {
			cat $PMIDI_PIPE
			echo "0, 0, Header, 1, 2, 960";
			echo "1, 1, Start_track";
			echo "1, 1, End_track";
			echo "2, 0, Start_track";
			echo "2, 0, Note_on_c, 0, $NOTE, $VELOCITY";
			echo "2, 20, Note_off_c, 0, $NOTE, 0";
		} | ref/midicsv-1.1/csvmidi );
	fi;
	if [ "$COMMAND" == "TIP_ON" ]; then
	fi;
	if [ "$COMMAND" == "STOP" ]; then
		echo "2, 21, End_track" > $PMIDI_PIPE
		echo "0, 0, End_of_file" > $PMIDI_PIPE
		rm -f $PMIDI_PIPE
	fi;
}

pmidi_control "START"

function wait_user_note_action()
{
	if [ $NOTE -lt 36 -o $NOTE -gt 96 ]; then
		echo "Ignoring unavailable note $NOTE on CASIO LK-250";
		return;
	fi;
	ACTION="$1"
	NOTE="$2"
	VELOCITY="$3"
	NOTE_PIPE=.note-pipe/$NOTE
	if [ ! -p $NOTE_PIPE ]; then
		rm -fr $NOTE_PIPE
		mkfifo $NOTE_PIPE
	fi;
	pmidi_control "TIP_ON" "$ACTION" "$NOTE" "$VELOCITY";
	WAITING=$(cat $NOTE_PIPE)
	pmidi_control "TIP_OFF" "$ACTION" "$NOTE" "$VELOCITY";
};

# MIDI Keyboard input
aseqdump --port $MIDI_PORT | while read MIDI_RESP ;
do
	[[ "$MIDI_RESP" =~ "Waiting for data. Press Ctrl+C to end." ]] && continue;
	[[ "$MIDI_RESP" =~ "Source" ]] && continue;
	NOTE=$(echo $MIDI_RESP | cut -d ' ' -f 6 | tr -d ,);
	echo $MIDI_RESP >> debug
	[ "$NOTE" == "" ] && continue;
	NOTE_PIPE=.note-pipe/$NOTE
	if [ ! -p $NOTE_PIPE ]; then
		rm -fr $NOTE_PIPE
		mkfifo $NOTE_PIPE
	fi;
	VELOCITY=$(echo $MIDI_RESP | cut -d ' ' -f 8);
	echo $VELOCITY > $NOTE_PIPE &
done &

ref/midicsv-1.1/midicsv "$MIDI_FILE" | while read MIDI_RECORD;
do
	F1=$(echo "$MIDI_RECORD" | cut -d, -f1)
	F2=$(echo "$MIDI_RECORD" | cut -d, -f2)
	F3=$(echo "$MIDI_RECORD" | cut -d, -f3)
	F4=$(echo "$MIDI_RECORD" | cut -d, -f4-)
	ACTION="${F3:1}"

	if [ "$F1" == "1" ]; then
		if [ "$ACTION" == "Title_t" ]; then
			TITLE="$F4";
		fi;
	fi;
	if [ "$F1" == "2" ]; then
		if [ "$F3" == " Title_t" ]; then
			INSTRUMENT="$F4";
		fi;
		TIME=$(( $F2 ))
	fi;
	echo $TITLE - $INSTRUMENT - [$MIDI_RECORD] - [$TIME][$ACTION][$TYPE][$F3]
	if [ "$ACTION" == "Note_on_c" ];then
		# Wait user to press this note
		NOTE=$(( $(echo "$MIDI_RECORD" | cut -d, -f5) ))
		VELOCITY=$(( $(echo "$MIDI_RECORD" | cut -d, -f6) ))
		wait_user_note_action "PRESS" "$NOTE" "$VELOCITY"
	fi;
	if [ "$ACTION" == "Note_off_c" ];then
		# Wait user to press this note
		NOTE=$(( $(echo "$MIDI_RECORD" | cut -d, -f5) ))
		wait_user_note_action "RELEASE" "$NOTE"
	fi;
	pmidi_control "STOP";
done

wait
