Puzzle collection v0.0 by Ruben Spaans, released in July 2011.
Released under GPL v3.0.

Web homes:
http://www.stubbscroll.com/puzzle.html
http://www.pvv.org/~spaans/puzzle.html
http://www.pvv.ntnu.no/~spaans/puzzle.html

This is a collection of logic puzzle games. There are currently five games
implemented (Nurikabe, Akari (Light up), Heyawake, Hitori and Picross),
and the interface is rather unfinished. The program should run on all
desktop platforms capable of running a gcc C compiler and the SDl library,
but it is currently only tested on Windows systems.

The puzzle collection also includes a program that downloads puzzles from
the Nikoli site.

Future plans include:
- adding a menu system for selecting puzzles
- rendering the numbers in a nice way
- show more info on the screen (timer and possibly other info)
- adding more puzzle types
- auto-generation of puzzle instances
- refactoring the code: more source files for each game type (gui, board/rules
  manager, solver)
- expanding the documentation

==== How to build the program ================================================

If you are using win32, you can run the supplied executable (puzzle.exe). If
not, run make. If that doesn't work, check if the correct library names are
used. win32 users who want to change stuff can recompile with make.bat
(assuming you have mingw-gcc with SDL).

Warning, the makefile is not tested.

==== How to run the program ==================================================

From the command line, navigate to the directory where the program resides.
Run with 'puzzle puzzles\file' (in windows) or './puzzle puzzles/file' (in
unix) where file is the file name of the puzzle you want to run. Supplied
puzzles reside in the puzzles/ directory.

==== How to play - Nurikabe ==================================================

For the rules, check http://nikoli.com/en/puzzles/nurikabe/rule.html and
http://en.wikipedia.org/wiki/Nurikabe .

Controls (mouse):
- left mouse button: black cell (water (stream) or wall)
- right mouse button: white cell (island)
- middle mouse button: erase cell
- drag corners with mouse to resize the game window

Controls (keyboard):
- Backspace: undo one move (including hint moves)
- V: show connectedness status of walls (water): each disconnected region is
     shown in different colours
- H: hint, the game fills in one cell correctly
- J: superhint, the game solves as much as it can
- Escape: quit the game

An island can have the following colours:
- white: incomplete
- yellow: complete, need to be enclosed by black cells
- red: island consists of too many cells

The control scheme, the colours and a few other settings can be changed in
the file puzzle.ini.

==== How to play - Akari =====================================================

TODO

==== How to play - Heyawake ==================================================

TODO

==== How to play - Hitori ====================================================

TODO

==== How to play - Picross ===================================================

TODO

==== grab ====================================================================

To compile the grabber, you need the curl library:

http://curl.haxx.se/download.html

To run a 32-bit version of the program in 64-bit windows, you might need a
32-bit copy of the libsasl.dll file in the program's current directory.
