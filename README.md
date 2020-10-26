#!/bin/bash

# to verify USB connection use BEFORE connect:
```bash
dmesg -w
...
[246153.808532] usb 1-4: new full-speed USB device number 16 using xhci_hcd
[246153.961692] usb 1-4: New USB device found, idVendor=07cf, idProduct=6803, bcdDevice= 1.00
[246153.961695] usb 1-4: New USB device strings: Mfr=1, Product=2, SerialNumber=0
[246153.961697] usb 1-4: Product: CASIO USB-MIDI
[246153.961698] usb 1-4: Manufacturer: CASIO

```

# after USB connection, is expected to be created a path em  `/sys/devices/...`
```bash
find /sys/devices -name product | xargs grep CASIO
/sys/devices/pci0000:00/0000:00:14.0/usb1/1-3/product:CASIO USB-MIDI
```

# Debug USB
https://wiki.kubuntu.org/Kernel/Debugging/USB

# to play a midi use timidity
# to read MIDI Keyboard input connect USB and try
```bash
$ aplaymidi -l
 Port    Client name                      Port name
 14:0    Midi Through                     Midi Through Port-0
 20:0    CASIO USB-MIDI                   CASIO USB-MIDI MIDI 1

$ aseqdump --port 20:0
Waiting for data. Press Ctrl+C to end.
Source  Event                  Ch  Data
 20:0   Note on                 0, note 81, velocity 43
 20:0   Note on                 0, note 76, velocity 46
 20:0   Note on                 0, note 81, velocity 66
 20:0   Note on                 0, note 76, velocity 69
 20:0   Note off                0, note 81, velocity 64
 20:0   Note off                0, note 76, velocity 64
 20:0   Note on                 0, note 78, velocity 57
 20:0   Note off                0, note 78, velocity 64
...
```

# to send midi files to play in a MIDI keyboard
```bash
$ aplaymidi --port 20:0 src/casio-lk-250/musicope/static/songs/Ramin_Djawadi_-_Westworld_Theme.mid
```
or
```bash
$ pmidi --port 20:0 src/casio-lk-250/musicope/static/songs/Ramin_Djawadi_-_Westworld_Theme.mid
```

# MIDI Keyboard Setup for CASIO LK-250
- Press FUNCTION buttom
- Press `>` button bellow LED screen until
   - Settings MIDIInNavigate - Turn Listen
   - Settings MIDIInNab R Ch == 1
   - Settings MIDIInNab L Ch == 1


# References
https://github.com/igor-liferenko/pcm/blob/master/playpcm.w
https://unix.stackexchange.com/questions/13732/generating-random-noise-for-fun-in-dev-snd
https://stackoverflow.com/questions/6672743/prevent-strace-from-abbreviating-arguments
https://www-uxsup.csx.cam.ac.uk/pub/doc/suse/suse9.0/userguide-9.0/ch18s09.html
https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
https://support.casio.com/storage/en/manual/pdf/EN/008/LKS250_usersguide_B_EN.pdf
- Instruction from official manual in last page "MIDI Implementation Chart"
https://sfxpt.wordpress.com/2015/02/02/how-to-play-midi-files-under-ubuntu-linux/
https://learn.sparkfun.com/tutorials/midi-tutorial/all
http://people.bu.edu/bkulis/projects/music/index.html?ref=hackernoon.com
https://www.recordingblogs.com/wiki/midi-note-on-message
https://www.recordingblogs.com/wiki/midi-tick
https://www.recordingblogs.com/wiki/header-chunk-of-a-midi-file


https://github.com/oldrich-s/musicope
   - musicope does not connect to midi, in fact only browser read the file, not backend.
https://www.synthesiagame.com/

https://www.reddit.com/r/rocksmith/comments/4jwpd3/rocksmithlike_software_for_keyboard/

https://github.com/hiben/MIDISpy/wiki
   - java code to sky midi traffic (sniffer)

## Color reference
https://chroma-notes.com


## Read MIDI files with perl
https://www.fourmilab.ch/webtools/midicsv/


# epoll sample
https://epollwait.blogspot.com/2010/11/linux-sockets-example.html

# Download midi-files
http://www.piano-midi.de/midi_files.htm
https://hiphopmakers.com/best-free-midi-files-midi-songs
