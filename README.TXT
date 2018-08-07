
Igor: Objetivo Uikokahonia
--------------------------

Running:
--------

You need the original files of the Spanish CD release :

    IGOR.DAT  61682719 bytes
    IGOR.EXE   9115648 bytes

One extra datafile is also required (present in the igor-*.zip file) :

    igor.bin

Audio tracks can be encoded as 22 Khz Vorbis files :

    track02.ogg - track16.ogg

The game can be started with :

    ./igor <data directory> <part number> <save directory> <language>

- If data directory is empty, current directory is used ('.').
- If part is not specified, it defaults to startup logos (900).
  The game.h file lists what the part numbers map to.

Game state is saved on exit (as 'igor.s00').

Status:
-------

This program is mostly a proof of concept. Since the original game
executable contains both hardcoded logic and assets, the idea was
to extract the datafiles and write a x86 decoder and interpreter.
Only higher level functions (palette, display, inputs) have been
rewritten (these are referred as 'traps' in the source code).

Missing features :
- original save/load screen

Contact:
--------

Gregory Montoir, cyx@users.sourceforge.net
