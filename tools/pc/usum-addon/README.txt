Ultra Sun / Ultra Moon: single-screen battle buttons and X menu (Windows)
=======================================================================

An SDL2.dll for the Windows 3DS program (Pokemon ORAS.exe and Pokemon ORAS (Vulkan).exe) that goes in
front of the program's own SDL2.dll, renamed SDL2_real.dll. Every SDL call goes on to the real one; on top
of that, in Ultra Sun and Ultra Moon's one-screen layout (Settings > Display > Layout: One screen), the
add-on draws real single-screen battle buttons and a cleaner X menu, both made live from the game's own
bottom screen. In every other game's folder it only passes SDL through.

Battle
------
The battle's bottom screen used to be a box in the corner, or pieces cut out of it. With the add-on the
battle gets buttons of its own: clean panels in a thin frame with a soft shadow, each filled with its
button's own picture (FIGHT's flames, POKEMON's and BAG's swirls, RUN's footprints) refitted to the panel
and taken live from the game, with the game's own lettering cut out cleanly and centred on it.

- Commands: FIGHT across the top, BAG / RUN / POKEMON under it, lined up with FIGHT's edges (in the Battle
  Tree, which has no BAG, RUN and POKEMON take half the row each). The quick ball (Y) sits left of the row
  in wild battles; in a double battle's second turn the back button takes RUN's place.
- Weather: a small panel in the top left corner with the game's weather picture; click it for the field.
- Info column: every Pokemon on the field at the right edge above the commands (foes red, yours blue),
  clickable for stat changes and abilities; it scrolls with the mouse wheel in double and multi battles.
- Moves: the game's four move panels stacked at the bottom right; the one under the game's cursor glows
  gold. Z-Power and Mega Evolution sit in a column beside them (Mega in its rainbow colours), Z-Moves take
  the last move's place, and a move's details (its "i") show the game's wavy panel on its own.
- Targets: the four target plates in a 2x2 grid, the chosen one glowing gold. Yes/No questions show the
  game's own bars. While a turn plays out nothing is drawn; the party, bag and summaries keep the
  program's own layouts.
- Back buttons are clean squares with the B badge on a corner. Panels light up under the mouse pointer.

Clicks and taps on the new buttons press the same spot on the game's bottom screen; the keyboard and
controllers work as before.

X menu
------
With Settings > Display > Menus: Laid out, the program shows the X menu at the right of the screen in
pieces of the bottom screen. The add-on draws it anew in the same places:

- The six options stand on their own: each pill and icon is cut out along its own dark outline (no yellow
  background), with its label white on a dark edge and a soft shadow. The cursor arrow and the green "!"
  badges stay on the pills.
- The page dots between L and R (which of the two pages is shown) are left out: L and R sit together,
  centred over the six options, drawn like the program's own keys. When the other page has something new,
  the game's green "!" sits on the key that goes there (R on the first page, L on the second).
- Back and SAVE are clean panels with their B and Y badges. The option under the pointer gets a white rim.

Clicks on the menu reach the game exactly as before (everything is where the program's pieces are).

Settings it works with
----------------------
Both Graphics APIs (Vulkan and OpenGL ES), every Resolution, every Aspect ratio and any window size. When
the picture has bars (4:3, Integer scale, or 16:9 on an ultrawide or 16:10 screen) the battle buttons stay
on the picture. Battle button size scales the new buttons. Each time Ultra Sun or Ultra Moon starts, the
add-on sets Battle buttons to "Whole screen" (it works from the whole bottom screen); nothing else in the
settings changes. A tall (portrait) window makes the program stack the screens, and the add-on leaves
that layout alone, as it does every layout but One screen.

In FullDex's download
---------------------
tools/pc/build_all.sh builds the add-on when it packs the Windows 3DS program (SSR_3DS_WIN_DIR): 3ds\program
gets SDL2.dll (the add-on) and SDL2_real.dll (the program's own). A game's folder is a copy of 3ds\program;
a folder made before the add-on was there, whose SDL2.dll is still the program's own, gets it the next time
FullDex starts that game (frontend/n3ds.h, MakeFolder). A folder whose SDL2.dll is anything else (an add-on
put in by hand, say) keeps it.

To turn it off for a game, put an empty file named  usumfix_off.txt  in the game's folder
(3ds\00040000001B5000 is Ultra Sun, 3ds\00040000001B5100 Ultra Moon): the program's own box and X menu come
back within a second, even mid-battle, and Battle buttons is left as you set it. Delete the file to turn it
on again.

If something looks wrong, an empty file named  usumfix_debug.txt  in the game's folder makes the add-on
write usumfix.log there, with what it saw on the bottom screen.

Notes
-----
- Tested in the Battle Tree (single, double and multi battles, Z-Moves, Mega Evolution) and in a wild
  battle, and the X menu on both its pages, with Vulkan and OpenGL; at Resolution Original (240p), 720p
  and 1080p; with Aspect ratio 16:9, 4:3, Integer scale and Fill screen; in windows from 800x480 to
  2560x1440, including 1920x1200 and 2560x1080.
- The battle lettering is matched against the English version's screens; in another language the commands
  fall back by themselves to cut-out pieces of the bottom screen.
- Windows only: the Linux program has no SDL2.dll to stand in for.

Building
--------
    tools/pc/usum-addon/build.sh OUT_DIR

makes OUT_DIR/SDL2.dll with MinGW-w64's C compiler, glslangValidator (glslang-tools), the Vulkan headers
(libvulkan-dev, or VULKAN_INC) and Python 3. The files:

  usumfix.c      the whole add-on: SDL pass-through (usumfix.def forwards every export to SDL2_real.dll),
                 the OpenGL and Vulkan hooks, the read-back of the bottom screen, the layouts and the
                 shader (OpenGL's GLSL; mkglsl.py makes Vulkan's from it, spv2h.py embeds the SPIR-V).
  usum_masks.h   the battle buttons' lettering and symbols as one-bit coverage, with points to recognise
                 them, traced from pictures of the English game's bottom screen.
  usum_glyphs.h  the letters L and R for the X menu's keys, as distance fields; made by gen_glyphs.py from
                 FullDex's Inter Bold (frontend/fonts/ttf/inter-bold.ttf):
                     python3 tools/pc/usum-addon/gen_glyphs.py > tools/pc/usum-addon/usum_glyphs.h
