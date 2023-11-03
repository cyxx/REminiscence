
REminiscence README
Release version: 0.5.2
-------------------------------------------------------------------------------


About:
------

REminiscence is a re-implementation of the engine used in the game Flashback
made by Delphine Software and released in 1992. More informations about the
game can be found at [1], [2] and [3].


Data Files:
-----------

You will need the original files of the DOS (floppy or CD), Amiga, Macintosh
or PC98 release.

For the Macintosh release, the resource fork must be dumped as a file named
'FLASHBACK.BIN' (MacBinary) or 'FLASHBACK.RSRC' (AppleDouble).

For speech with in-game dialogues, you need to copy the 'VOICE.VCE' file
from the SegaCD version.

The music/ directory of the Amiga version or the .mod fileset from
unexotica [4] can be used as an alternative to the MIDI tracks from the
DOS version. Simply copy the files to the DATA directory and set the
'use_prf_music' option to false in the configuration file.


Running:
--------

By default, the engine loads the game data files from the 'DATA' directory,
as the original game executable did. The savestates are saved in the current
directory.

These paths can be changed using command line switches:

    Usage: rs [OPTIONS]...
    --datapath=PATH   Path to data files (default 'DATA')
    --savepath=PATH   Path to save files (default '.')
    --levelnum=NUM    Level to start from (default '0')
    --fullscreen      Fullscreen display
    --widescreen=MODE Widescreen display (adjacent,mirror,blur,cdi,none)
    --scaler=NAME@X   Graphics scaler (default 'scale@3')
    --language=LANG   Language (fr,en,de,sp,it,jp)
    --autosave        Save game state automatically
    --mididriver=MIDI Driver (adlib, mt32)

The scaler option specifies the algorithm used to smoothen the image and the
scaling factor. External scalers are also supported, the suffix shall be used
as the name. Eg. If you have scaler_xbr.dll, you can pass '--scaler xbr@2'
to use that algorithm with a doubled window size (512x448).

The widescreen option accepts the modes below:

    adjacent   draw left and right rooms bitmap
    mirror     mirror the current room bitmap
    blur       blur and stretch the current room bitmap
    cdi        use bitmaps from the CD-i release ('flashp?.bob' files)

In-game keys:

    Arrow Keys        move Conrad
    Enter             use the current inventory object
    Shift             talk / use / run / shoot
    Escape            display options
    Backspace / Tab   display inventory / skip cutscene
    Alt Enter         toggle windowed / fullscreen mode
    Alt + and -       increase or decrease game screen scaler factor
    Alt S             take screenshot
    Ctrl G            toggle auto zoom (DOS version only)
    Ctrl S            save game state
    Ctrl L            load game state
    Ctrl R            rewind game state buffer (requires --autosave)
    Ctrl + and -      change game state slot
    Function Keys     change game screen scaler


Credits:
--------

Delphine Software, obviously, for making another great game.
Yaz0r, Pixel and gawd for sharing information they gathered on the game.


Contact:
--------

Gregory Montoir, cyx@users.sourceforge.net


URLs:
-----

[1] http://www.mobygames.com/game/flashback-the-quest-for-identity
[2] http://en.wikipedia.org/wiki/Flashback:_The_Quest_for_Identity
[3] http://ramal.free.fr/fb_en.htm
[4] https://www.exotica.org.uk/wiki/Flashback
