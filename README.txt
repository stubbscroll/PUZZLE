Puzzle collection v1.0 by Ruben Spaans, released in August 2012.
Released under GPL v3.0.

Web homes:
http://www.stubbscroll.com/puzzle.html (alias address)
http://www.pvv.org/~spaans/puzzle.html
http://www.pvv.ntnu.no/~spaans/puzzle.html (alias address)

This is a collection of logic puzzle games. There are currently eleven games
implemented (Nurikabe, Akari (Light up), Heyawake, Hitori, Picross,
Hashiwokakero, Masyu, Slitherlink, Yajilin, Minesweeper and Kuromaso)
including a menu system with score tracking. The program should run on all
desktop platforms capable of running a gcc C compiler and the SDl library, but
it is not yet tested on Mac systems.

Future plans include:
- adding achievements
- using resizable numbers and images for the puzzles
- adding in-game instructions and help
- adding more puzzle types
- adding more puzzles
- autogeneration of puzzles

==== How to build the program ================================================

If you are using win32, you can run the supplied executable (puzzle.exe). If
not, run make. If that doesn't work, check if the correct library names are
used. win32 users who want to change stuff can download the source package
and recompile with make.bat (assuming that the user has mingw-gcc with
SDL).

==== How to run the program ==================================================

Run the file "puzzle.exe" (windows) or "./puzzle" (non-windows). You will then
enter the menu system.

It is also possible to bypass the menu and play individual puzzles with
"puzzle filename"; scores will not be saved in this case. The puzzle files
are placed in the puzzles/ directory by default.

==== Controls for the menu ===================================================

Use the mouse to click on buttons, select puzzles and drag the scrollbar.
Double-click a puzzle name or click "play" to play a puzzle. Alternatively,
use these keyboard controls:

up/down/pageup/pagedown/home/end - navigate in puzzle list
ENTER/p                          - play the currently selected puzzle
s                                - scan for new puzzles and add them to list
ESC/q                            - quit

==== In-game controls that are common for all games =========================

ESC       - quit to menu (this will delete all progress for this session with
            no confirmation prompt)
h         - hint, the computer fills in one cell correctly (or warns you that
            the current board is in an unsolvable state). This function will
	    disable scoring for this puzzle until it is restarted.
j         - superhint, the computer attemps to solve the entire puzzle (no
            scoring, naturally). This will solve all but the hardest puzzles
	    in full.
v         - show connected components in different colours (not applicable for
            all games)
backspace - undo the last move

left mouse button                 - fill in cell
right mouse button                - fill in cell in another way
middle mouse button               - erase cell
click mouse button and drag mouse - create edge between two cells

There are no keyboard controls for the games.

Use the mouse to colour cell black or white, draw edges, place mines or lamps
etc. For most games, cells start out as unfilled (grey), which is not the same
as a white cell.

The game will in some cases colour the empty cells:
green  - cell is filled in correctly
yellow - it is trivial to fill in the cell correctly
red    - cell is filled in wrongly according to the game's rules
white  - none of the above, or it cannot be easily determined, or the game
         has chosen to not give this information

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

==== How to play - Hashiwokakero =============================================

TODO

==== How to play - Slitherlink ===============================================

TODO

==== How to play - Masyu =====================================================

TODO

==== How to play - Yajilin ===================================================

TODO

==== How to play - Kuromasu ==================================================

TODO

==== How to play - Minesweeper ===============================================

TODO

==== Credits =================================================================

Programming by Ruben Spaans

Puzzles by:
TODO long list goes here

