Puzzle collection v1.0 by stubbscroll, released in August 2012.
Program released under GNU general public license v3.

Web homes (all pages are identical):
http://www.stubbscroll.com/puzzle.html
http://www.pvv.org/~spaans/puzzle.html
http://www.pvv.ntnu.no/~spaans/puzzle.html

This is a collection of logic puzzle games. There are currently thirteen games
implemented (Nurikabe, Akari (Light up), Heyawake, Hitori, Picross (Nonogram),
Hashiwokakero, Masyu, Slitherlink, Yajilin, Minesweeper, Kuromaso, Mortal Coil
and Numberlink), including a menu system with score tracking. The program
should run on all desktop platforms capable of running a gcc C compiler and
the SDl library. It is not yet tested on Mac systems.

Future plans include (in no specific order):
- resizable numbers and images for the puzzles
- achievements
- in-game instructions and help
- more puzzle types
- more puzzles
- autogeneration of puzzles
- music

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
"puzzle <filename>"; scores will not be saved in this case. The puzzle files
are placed in the puzzles/ directory by default.

If the program crashes or behaves strangely, check the files "error.txt" or
"log.txt" if they exist, as they could contain error messages.

==== Configuration ===========================================================

The colours, the control schemes, line thicknesses and a few other things can
be changed in the file "puzzle.ini". See the comments in that file for more
detailed information.

==== Controls for the menu ===================================================

Use the mouse to click on buttons, select puzzles and drag the scrollbar.
Double-click a puzzle name or click "play" to play a puzzle. The window
can be resized (also during gameplay); small window sizes can lead to
unexpected behaviour. Alternatively, use these keyboard controls:

up/down/pageup/pagedown/home/end - navigate the puzzle list
ENTER/p                          - play the currently selected puzzle
s                                - scan for new puzzles and add them to list
ESC/q                            - quit

Beware that the menu screen requires a minimum width (around 800 pixels). If
the width is too small, the screen turns red and the message "Please resize
window". This message can also appear after you finish a puzzle where the
screen has been resized.

==== In-game controls that are common for all games =========================

Use the mouse to colour cell black or white, draw edges, place mines or lamps
etc. For most games, cells start out as unfilled (grey), which is not the same
as a white cell.

left mouse button                 - fill in cell
right mouse button                - fill in cell in another way
middle mouse button               - erase cell
click mouse button and drag mouse - create edge between two cells

The game will in some cases colour the empty cells:
green  - cell is filled in correctly
yellow - it is trivial to fill in the cell correctly
red    - cell is filled in wrongly according to the game's rules
white  - none of the above, or it cannot be easily determined, or the game
         has chosen to not give this information

The keyboard cannot be used for playing the games, but the following commands
can be performed:

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

==== How to play - Nurikabe ==================================================

For the full rules, please consult:
http://nikoli.com/en/puzzles/nurikabe/rule.html
http://en.wikipedia.org/wiki/Nurikabe

The purpose is to colour each unfilled cell either black or white. Each region
of white cells sharing a border must contain exactly one number indicating the
number of white cells in the region. All black cells must belong to one
connected region, and there can be no 2x2 region where all cells are black.

Default controls:
- left mouse button: colour the cell black
- right mouse button: colour the cell white
- middle mouse button: erase cell

==== How to play - Akari (Light up) ==========================================

For the full rules, please consult:
http://nikoli.com/en/puzzles/bijutsukan/rule.html
http://en.wikipedia.org/wiki/Light_Up

Each cell should either contain a light bulb, or be lit up by at least one
light bulb. Two light bulbs cannot shine on each other. Wall cells with a
number should have the indicated number of light bulbs among its four
neighbouring cells.

Default controls:
- left mouse button: place light bulb in cell
- right mouse button: mark cell as empty (no light bulb)
- middle mouse button: erase cell

==== How to play - Heyawake ==================================================

For the full rules, please consult:
http://nikoli.com/en/puzzles/heyawake/rule.html
http://en.wikipedia.org/wiki/Heyawake

The board is divided into rooms separated by thick lines. A room may contain
a number; these rooms should have the indicated number of black cells. Two
cells sharing a border cannot both be black. All white cells must belong to
one connected region. A contiguous vertical or horizontal line of white cells
cannot span more than two regions.

Default controls:
- left mouse button: colour the cell black
- right mouse button: colour the cell white
- middle mouse button: erase cell

==== How to play - Hitori ====================================================

For the full rules, please consult:
http://nikoli.com/en/puzzles/hitori/rule.html
http://en.wikipedia.org/wiki/Hitori

Colour each cell white or black. Two black cells cannot be neighbours, and
all white cells must belong to one connected regions. In a given row or
column all empty cells must contain different numbers.

Default controls:
- left mouse button: colour the cell black
- right mouse button: colour the cell white
- middle mouse button: erase cell

==== How to play - Picross ===================================================

For the full rules, please consult:
http://en.wikipedia.org/wiki/Nonogram

Colour each cell white or black so that they satisfy the hints given above
and to the left of the grid. A hint consists of a sequence of numbers; each
number represent an unbroken line of black cells. There should be at least one
white cell between two neighbouring unbroken lines of black cells.

Default controls:
- left mouse button: colour the cell black
- right mouse button: colour the cell white
- middle mouse button: erase cell

==== How to play - Hashiwokakero =============================================

For the full rules, please consult:
http://nikoli.com/en/puzzles/hashiwokakero/rule.html
http://en.wikipedia.org/wiki/Hashiwokakero

The purpose is to draw edges between each nodes. It is possible to connect two
nodes if they are on the same line (horizontal or vertical) and there are no
obstructions. All nodes must belong to one connected component, and the number
inside the node indicates how manyadjacent edges should have. There cannot be
more than two edges between a pair of nodes.

Controls:
Hold the left mouse button and drag the mouse pointer somewhere along the path
between two nodes to cycle between 0, 1 and 2 edges.

==== How to play - Slitherlink ===============================================

For the full rules, please consult:
http://nikoli.com/en/puzzles/slitherlink/rule.html
http://en.wikipedia.org/wiki/Slitherlink

The purpose is to create a single closed loop made up by edges between dots.
A number must have the indicated number of edges next to it.

Default controls:
- left mouse button: create an edge
- right mouse button: mark x for no edge
- middle mouse button: erase edge/x

==== How to play - Masyu =====================================================

For the full rules, please consult:
http://nikoli.com/en/puzzles/masyu/rule.html
http://en.wikipedia.org/wiki/Masyu

The purpose is to create a single closed loop passing through all white and
black circles. A loop segment goes through two neighbouring cells.

Controls:
Hold the left mouse button and drag the mouse pointer between two neighbouring
cells to toggle between edge and no edge.

==== How to play - Yajilin ===================================================

For the full rules, please consult:
http://nikoli.com/en/puzzles/yajilin/rule.html
http://en.wikipedia.org/wiki/Yajilin

The purpose is to create a single closed loop out of edges between
neighbouring cells. An initial unfilled cell must have either two edges or
it must be black. Two black cells cannot be next to each other. A hint
indicates the exact number of black cells it points to.

Controls:
Hold the left mouse button and drag the mouse pointer between two neighbouring
cells to toggle between edge and no edge. Press the right mouse button to
colour a cell black, and press the middle mouse button to remove a black cell.

==== How to play - Kuromasu ==================================================

For the full rules, please consult:
http://en.wikipedia.org/wiki/Kuromasu

The cells should be marked white or black. Two neighbouring cells cannot both
be black, and all white cells should belong to one connected region. A number
in a cell indicates the total number of white cells it can "see" in all four
directions (including itself).

==== How to play - Minesweeper ===============================================

This is not the same game as the one that's bundled with windows!

Rules for an equivalent game:
http://en.wikipedia.org/wiki/Tentaizu_%28puzzle%29

Very brief description of the rules:
http://rohanrao.blogspot.no/2008/12/rules-of-minesweeper.html

Place mines so that all numbers on the board have the specified number of
mines next to them. On some puzzles the total number of mines places must be
equal to a given number.

Default controls:
- left mouse button: place a mine
- right mouse button: mark a cell as empty
- middle mouse button: erase cell

==== How to play - Mortal coil ===============================================

Instructions taken from hacker.org:
Click to place your starting position. Subsequent clicks move until you hit
something. Try to cover the whole board with your coil.

URL to game: http://www.hacker.org/coil/

Default control:
- left mouse button to place coil (on empty board)
- when coil exists: left mouse button or arrow keys to move in a direction

==== How to play - Numberlink ================================================

Draw lines between similar numbers
TODO

==== Credits =================================================================

Programming by Ruben Spaans
Font by Brian Raiter (from Tile World)
        unknown (CyrillicHelvet_Medium from ufonts.com)

Puzzles by:

Abe Hajime
Adolfo Zanellati
Anuraag Sahay
Brandon McPhail
Christian Steinruecken
Connect4
Daniel Adams
Daniel Rieder 
Dark Murad
Elsbeth Endel
Grant Fikes
Halflength Sleeve
Hermann Kudlich
Hirofumi Fujiwara
Hobbit
Iwa Daigeki
Jak Marshall
James Dow Allen
Jan Wolter
Joachim Vetter
Johan de Ruiter
Johannes Kestler
Komieda
Lars Huttar
Luke Pebody
Maarten L
Mie Kawabata
Mokuani
Momotereu
Nicolas
Nikoli
Nobuyuki Sakamoto
Noriyuki Shimazaki
NyanBaz
OX
Oleg Andrushko
Otto Janko
Palmer Mebane
Red Magician
Robert Hollenstein
Sasayama Takayoshi
TKG
The Zotmeister
Thomas Snyder
Tom Collyer
Toonkigou
Tooru Nakata
Toshio Karino
Vadim Teriokhin
Warai Kamosika
Wilfried Elmenreich
Yilmaz Ekici
Yuichi Saito
affpuzz 
asetonitoriru
mimic
ooya

I have tried to only use puzzles released under a free licence. If you find
any puzzles that I don't have permission to use, please send a mail to
spaans at pvv.org and inform me, and I'll remove it.
