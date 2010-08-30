// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// DESCRIPTION:
//	DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
//
//-----------------------------------------------------------------------------


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "deh_main.h"
#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "doomfeatures.h"
#include "sounds.h"

#include "d_iwad.h"

#include "z_zone.h"
#include "w_wad.h"
#include "w_merge.h"
#include "s_sound.h"
#include "v_video.h"

#include "f_finale.h"
#include "f_wipe.h"

#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_menu.h"
#include "p_saveg.h"

#include "i_endoom.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "i_swap.h"

#include "g_game.h"

#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "net_client.h"
#include "net_dedicated.h"
#include "net_query.h"

#include "p_setup.h"
#include "r_local.h"


#include "d_main.h"

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop (void);

// Location where savegames are stored

char *          savegamedir;

// location of IWAD and WAD files

char *          iwadfile;


boolean		devparm;	// started game with -devparm
boolean         nomonsters;	// checkparm of -nomonsters
boolean         respawnparm;	// checkparm of -respawn
boolean         fastparm;	// checkparm of -fast

boolean		singletics = false; // debug flag to cancel adaptiveness


//extern int soundVolume;
//extern  int	sfxVolume;
//extern  int	musicVolume;

extern  boolean	inhelpscreens;

skill_t		startskill;
int             startepisode;
int		startmap;
boolean		autostart;
int             startloadgame;

FILE*		debugfile;

boolean		advancedemo;

// Store demo, do not accept any inputs
// haleyjd [STRIFE] Unused.
//boolean         storedemo;


char		wadfile[1024];		// primary wad file
char		mapdir[1024];           // directory of development maps

int             show_endoom = 1;


void D_CheckNetGame (void);
void D_ProcessEvents (void);
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo (void);


//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents (void)
{
    event_t*	ev;

    // haleyjd 08/22/2010: [STRIFE] there is no such thing as a "store demo" 
    // version of Strife

    // IF STORE DEMO, DO NOT ACCEPT INPUT
    //if (storedemo)
    //    return;

    while ((ev = D_PopEvent()) != NULL)
    {
        if (M_Responder (ev))
            continue;               // menu ate the event
        G_Responder (ev);
    }
}




//
// D_Display
//  draw current display, possibly wiping it from the previous
//
// wipegamestate can be set to -1 to force a wipe on the next draw
//
// haleyjd 08/23/10: [STRIFE]:
// * Changes to eliminate intermission and change timing of screenwipe
//
gamestate_t     wipegamestate = GS_DEMOSCREEN;
extern  boolean setsizeneeded;
extern  int             showMessages;
void R_ExecuteSetViewSize (void);

void D_Display (void)
{
    static  boolean             viewactivestate = false;
    static  boolean             menuactivestate = false;
    static  boolean             inhelpscreensstate = false;
    static  boolean             fullscreen = false;
    static  gamestate_t         oldgamestate = -1;
    static  int                 borderdrawcount;
    int                         nowtime;
    int                         tics;
    int                         wipestart;
    int                         y;
    boolean                     done;
    boolean                     wipe;
    boolean                     redrawsbar;

    if (nodrawers)
        return;                    // for comparative timing / profiling

    redrawsbar = false;
    
    // change the view size if needed
    if (setsizeneeded)
    {
        R_ExecuteSetViewSize ();
        oldgamestate = -1;                      // force background redraw
        borderdrawcount = 3;
    }

    // save the current screen if about to wipe
    if (gamestate != wipegamestate)
    {
        wipe = true;
        wipe_StartScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
    }
    else
        wipe = false;

    if (gamestate == GS_LEVEL && gametic)
        HU_Erase();

    // do buffered drawing
    switch (gamestate)
    {
    case GS_LEVEL:
        if (!gametic)
            break;
        if (automapactive)
            AM_Drawer ();
        if (wipe || (viewheight != 200 && fullscreen) )
            redrawsbar = true;
        // haleyjd 08/29/10: [STRIFE] Always redraw sbar if menu is/was active
        if (menuactivestate || (inhelpscreensstate && !inhelpscreens))
            redrawsbar = true;              // just put away the help screen
        ST_Drawer (viewheight == 200, redrawsbar );
        fullscreen = viewheight == 200;
        break;
      
     // haleyjd 08/23/2010: [STRIFE] No intermission
     /*
     case GS_INTERMISSION:
         WI_Drawer ();
         break;
     */
    case GS_FINALE:
        F_Drawer ();
        break;

    case GS_DEMOSCREEN:
        D_PageDrawer ();
        break;
    }
    
    // draw buffered stuff to screen
    I_UpdateNoBlit ();

    // draw the view directly
    if (gamestate == GS_LEVEL && !automapactive && gametic)
        R_RenderPlayerView (&players[displayplayer]);

    if (gamestate == GS_LEVEL && gametic)
    {
        HU_Drawer ();
        // STRIFE-TODO: ST_DrawMore, unknown variable dword_861C8
        /*
        if(ST_DrawMore()) 
            dword_861C8 = 1;
        else if(dword_861C8)
        {
            dword_861C8 = 0;
            menuactivestate = 1;
        }
        */
    }

    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
        I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));

    // see if the border needs to be initially drawn
    if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
    {
        viewactivestate = false;        // view was not active
        R_FillBackScreen ();    // draw the pattern into the back screen
    }

    // see if the border needs to be updated to the screen
    if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != 320)
    {
        if (menuactive || menuactivestate || !viewactivestate)
        {
            borderdrawcount = 3;
            // STRIFE-FIXME / TODO: Unknown variable
            // dword_861C8 = 0; 
        }
        if (borderdrawcount)
        {
            R_DrawViewBorder ();    // erase old menu stuff
            borderdrawcount--;
        }

    }

    if (testcontrols)
    {
        // Box showing current mouse speed

        G_DrawMouseSpeedBox();
    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    // draw pause pic
    if (paused)
    {
        if (automapactive)
            y = 4;
        else
            y = viewwindowy+4;
        V_DrawPatchDirect(viewwindowx + (scaledviewwidth - 68) / 2, y,
                          W_CacheLumpName (DEH_String("M_PAUSE"), PU_CACHE));
    }


    // menus go directly to the screen
    M_Drawer ();          // menu is drawn even on top of everything
    NetUpdate ();         // send out any new accumulation


    // normal update
    if (!wipe)
    {
        I_FinishUpdate ();              // page flip or blit buffer
        return;
    }
    
    // wipe update
    wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);

    wipestart = I_GetTime () - 1;

    do
    {
        do
        {
            nowtime = I_GetTime ();
            tics = nowtime - wipestart;
            I_Sleep(1);
        } while (tics < 3); // haleyjd 08/23/2010: [STRIFE] Changed from == 0 to < 3

        // haleyjd 08/26/10: [STRIFE] Changed to use ColorXForm wipe.
        wipestart = nowtime;
        done = wipe_ScreenWipe(wipe_ColorXForm
                               , 0, 0, SCREENWIDTH, SCREENHEIGHT, tics);
        I_UpdateNoBlit ();
        M_Drawer ();                            // menu is drawn even on top of wipes
        I_FinishUpdate ();                      // page flip or blit buffer
    } while (!done);
}

//
// Add configuration file variable bindings.
//

void D_BindVariables(void)
{
    int i;

    M_ApplyPlatformDefaults();

    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();

    M_BindBaseControls();
    M_BindWeaponControls();
    M_BindMapControls();
    M_BindMenuControls();

#ifdef FEATURE_MULTIPLAYER
    NET_BindVariables();
#endif

    // haleyjd 08/29/10: [STRIFE]
    // * Added voice volume
    // * Added back flat
    M_BindVariable("mouse_sensitivity",      &mouseSensitivity);
    M_BindVariable("sfx_volume",             &sfxVolume);
    M_BindVariable("music_volume",           &musicVolume);
    M_BindVariable("voice_volume",           &voiceVolume); 
    M_BindVariable("show_messages",          &showMessages);
    M_BindVariable("screenblocks",           &screenblocks);
    M_BindVariable("detaillevel",            &detailLevel);
    M_BindVariable("snd_channels",           &snd_channels);
    M_BindVariable("vanilla_savegame_limit", &vanilla_savegame_limit);
    M_BindVariable("vanilla_demo_limit",     &vanilla_demo_limit);
    M_BindVariable("show_endoom",            &show_endoom);
    M_BindVariable("back_flat",              &back_flat);

    // Multiplayer chat macros

    for (i=0; i<10; ++i)
    {
        char buf[12];

        sprintf(buf, "chatmacro%i", i);
        M_BindVariable(buf, &chat_macros[i]);
    }
}

//
// D_GrabMouseCallback
//
// Called to determine whether to grab the mouse pointer
//

boolean D_GrabMouseCallback(void)
{
    // Drone players don't need mouse focus

    if (drone)
        return false;

    // when menu is active or game is paused, release the mouse 
 
    if (menuactive || paused)
        return false;

    // only grab mouse when playing levels (but not demos)

    return (gamestate == GS_LEVEL) && !demoplayback;
}

//
//  D_DoomLoop
//
//  haleyjd 08/23/10: [STRIFE] Verified unmodified.
//
extern  boolean         demorecording;

void D_DoomLoop (void)
{
    if (demorecording)
	G_BeginRecording ();
		
    if (M_CheckParm ("-debugfile"))
    {
	char    filename[20];
	sprintf (filename,"debug%i.txt",consoleplayer);
	printf ("debug output to: %s\n",filename);
	debugfile = fopen (filename,"w");
    }

    TryRunTics();

    I_SetWindowTitle(gamedescription);
    I_InitGraphics();
    I_EnableLoadingDisk();
    I_SetGrabMouseCallback(D_GrabMouseCallback);

    V_RestoreBuffer();
    R_ExecuteSetViewSize();

    D_StartGameLoop();

    if (testcontrols)
    {
        wipegamestate = gamestate;
    }

    while (1)
    {
	// frame syncronous IO operations
	I_StartFrame ();                
	
	// process one or more tics
	if (singletics)
	{
	    I_StartTic ();
	    D_ProcessEvents ();
	    G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
	    if (advancedemo)
		D_DoAdvanceDemo ();
	    M_Ticker ();
	    G_Ticker ();
	    gametic++;
	    maketic++;
	}
	else
	{
	    TryRunTics (); // will run at least one tic
	}
		
	S_UpdateSounds (players[consoleplayer].mo);// move positional sounds

	// Update display, next frame, with current state.
        if (screenvisible)
            D_Display ();
    }
}



//
//  DEMO LOOP
//
int             demosequence;
int             pagetic;
char                    *pagename;


//
// D_PageTicker
// Handles timing for warped projection
//
// haleyjd 08/22/2010: [STRIFE] verified unmodified
//
void D_PageTicker (void)
{
    if (--pagetic < 0)
	D_AdvanceDemo ();
}



//
// D_PageDrawer
//
// haleyjd 08/22/2010: [STRIFE] verified unmodified
//
void D_PageDrawer (void)
{
    V_DrawPatch (0, 0, W_CacheLumpName(pagename, PU_CACHE));
}


//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
// haleyjd 08/22/2010: [STRIFE] verified unmodified
//
void D_AdvanceDemo (void)
{
    advancedemo = true;
}


//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//
void D_DoAdvanceDemo (void)
{
    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;               // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    if (gamemode == retail && gameversion != exe_chex)
      demosequence = (demosequence+1)%7;
    else
      demosequence = (demosequence+1)%6;
    
    switch (demosequence)
    {
      case 0:
	if ( gamemode == commercial )
	    pagetic = TICRATE * 11;
	else
	    pagetic = 170;
	gamestate = GS_DEMOSCREEN;
	pagename = DEH_String("TITLEPIC");
	if ( gamemode == commercial )
	  S_StartMusic(mus_dm2ttl);
	else
	  S_StartMusic (mus_intro);
	break;
      case 1:
	G_DeferedPlayDemo(DEH_String("demo1"));
	break;
      case 2:
	pagetic = 200;
	gamestate = GS_DEMOSCREEN;
	pagename = DEH_String("CREDIT");
	break;
      case 3:
	G_DeferedPlayDemo(DEH_String("demo2"));
	break;
      case 4:
	gamestate = GS_DEMOSCREEN;
	if ( gamemode == commercial)
	{
	    pagetic = TICRATE * 11;
	    pagename = DEH_String("TITLEPIC");
	    S_StartMusic(mus_dm2ttl);
	}
	else
	{
	    pagetic = 200;

	    if ( gamemode == retail )
	      pagename = DEH_String("CREDIT");
	    else
	      pagename = DEH_String("HELP2");
	}
	break;
      case 5:
	G_DeferedPlayDemo(DEH_String("demo3"));
	break;
        // THE DEFINITIVE DOOM Special Edition demo
      case 6:
	G_DeferedPlayDemo(DEH_String("demo4"));
	break;
    }
}



//
// D_StartTitle
//
void D_StartTitle (void)
{
    // STRIFE-FIXME: some poorly understood changes are pending here
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo ();
}

// Strings for dehacked replacements of the startup banner
//
// These are from the original source: some of them are perhaps
// not used in any dehacked patches

static char *banners[] = 
{
    // doom1.wad
    "                            "
    "DOOM Shareware Startup v%i.%i"
    "                           ",
    // doom.wad
    "                            "
    "DOOM Registered Startup v%i.%i"
    "                           ",
    // Registered DOOM uses this
    "                          "
    "DOOM System Startup v%i.%i"
    "                          ",
    // doom.wad (Ultimate DOOM)
    "                         "
    "The Ultimate DOOM Startup v%i.%i"
    "                        ",
    // doom2.wad
    "                         "
    "DOOM 2: Hell on Earth v%i.%i"
    "                           ",
    // tnt.wad
    "                     "
    "DOOM 2: TNT - Evilution v%i.%i"
    "                           ",
    // plutonia.wad
    "                   "
    "DOOM 2: Plutonia Experiment v%i.%i"
    "                           ",
};

//
// Get game name: if the startup banner has been replaced, use that.
// Otherwise, use the name given
// 

static char *GetGameName(char *gamename)
{
    size_t i;
    char *deh_sub;
    
    for (i=0; i<arrlen(banners); ++i)
    {
        // Has the banner been replaced?

        deh_sub = DEH_String(banners[i]);
        
        if (deh_sub != banners[i])
        {
            // Has been replaced
            // We need to expand via printf to include the Doom version 
            // number
            // We also need to cut off spaces to get the basic name

            gamename = Z_Malloc(strlen(deh_sub) + 10, PU_STATIC, 0);
            sprintf(gamename, deh_sub, DOOM_VERSION / 100, DOOM_VERSION % 100);

            while (gamename[0] != '\0' && isspace(gamename[0]))
                strcpy(gamename, gamename+1);

            while (gamename[0] != '\0' && isspace(gamename[strlen(gamename)-1]))
                gamename[strlen(gamename) - 1] = '\0';
            
            return gamename;
        }
    }

    return gamename;
}

//
// Find out what version of Doom is playing.
//

void D_IdentifyVersion(void)
{
    // gamemission is set up by the D_FindIWAD function.  But if 
    // we specify '-iwad', we have to identify using 
    // IdentifyIWADByName.  However, if the iwad does not match
    // any known IWAD name, we may have a dilemma.  Try to 
    // identify by its contents.

    if (gamemission == none)
    {
        unsigned int i;

        for (i=0; i<numlumps; ++i)
        {
            if (!strncasecmp(lumpinfo[i].name, "MAP01", 8))
            {
                gamemission = doom2;
                break;
            } 
            else if (!strncasecmp(lumpinfo[i].name, "E1M1", 8))
            {
                gamemission = doom;
                break;
            }
        }

        if (gamemission == none)
        {
            // Still no idea.  I don't think this is going to work.

            I_Error("Unknown or invalid IWAD file.");
        }
    }

    // Make sure gamemode is set up correctly

    if (gamemission == doom)
    {
        // Doom 1.  But which version?

        if (W_CheckNumForName("E4M1") > 0)
        {
            // Ultimate Doom

            gamemode = retail;
        } 
        else if (W_CheckNumForName("E3M1") > 0)
        {
            gamemode = registered;
        }
        else
        {
            gamemode = shareware;
        }
    }
    else
    {
        // Doom 2 of some kind.

        gamemode = commercial;
    }
}

#if 0
//
// DoTimeBomb
//
// haleyjd 08/23/2010: [STRIFE] New function
// Code with no xrefs; probably left over from a private alpha or beta.
// Translated here because it explains what the SERIAL lump was meant to do.
//
void DoTimeBomb(void)
{
    dosdate_t date;
    char *serial;
    int serialnum;
    int serial_year;
    int serial_month;

    serial = W_CacheLumpName("serial", PU_CACHE);
    serialnum = atoi(serial);

    // Rogue, much like Governor Mourel, were lousy liars. These deceptive
    // error messages are pretty low :P
    dos_getdate(&date);
    if(date.year > 1996 || date.day > 15 && date.month > 4)
        I_Error("Data error! Corrupted WAD File!");
    serial_year = serialnum / 10000;
    serial_month = serialnum / 100 - 100 * serial_year;
    if(date.year < serial_year || 
       date.day < serialnum - 100 * serial_month - 10000 * serial_year &&
       date.month < serial_month)
       I_Error("Bad wadfile");
}
#endif

// Set the gamedescription string

void D_SetGameDescription(void)
{
    gamedescription = "Unknown";

    if (gamemission == doom)
    {
        // Doom 1.  But which version?

        if (gamemode == retail)
        {
            // Ultimate Doom

            gamedescription = GetGameName("The Ultimate DOOM");
        } 
        else if (gamemode == registered)
        {
            gamedescription = GetGameName("DOOM Registered");
        }
        else if (gamemode == shareware)
        {
            gamedescription = GetGameName("DOOM Shareware");
        }
    }
    else
    {
        // Doom 2 of some kind.  But which mission?

        if (gamemission == doom2)
            gamedescription = GetGameName("DOOM 2: Hell on Earth");
        else if (gamemission == pack_plut)
            gamedescription = GetGameName("DOOM 2: Plutonia Experiment"); 
        else if (gamemission == pack_tnt)
            gamedescription = GetGameName("DOOM 2: TNT - Evilution");
    }
}

static void SetSaveGameDir(char *iwad_filename)
{
    char *sep;
    char *basefile;

    // Extract the base filename
 
    sep = strrchr(iwad_filename, DIR_SEPARATOR);

    if (sep == NULL)
    {
        basefile = iwad_filename;
    }
    else
    {
        basefile = sep + 1;
    }

    // ~/.chocolate-doom/savegames/

    savegamedir = Z_Malloc(strlen(configdir) + 30, PU_STATIC, 0);
    sprintf(savegamedir, "%ssavegames%c", configdir,
                         DIR_SEPARATOR);

    M_MakeDirectory(savegamedir);

    // eg. ~/.chocolate-doom/savegames/doom2.wad/

    sprintf(savegamedir + strlen(savegamedir), "%s%c",
            basefile, DIR_SEPARATOR);

    M_MakeDirectory(savegamedir);
}

// Check if the IWAD file is the Chex Quest IWAD.  
// Returns true if this is chex.wad.
// haleyjd 08/23/10: there is no Chex Quest based on Strife,
// though I must admit that makes an intriguing idea....
/*
static boolean CheckChex(char *iwadname)
{
    char *chex_iwadname = "chex.wad";

    return (strlen(iwadname) > strlen(chex_iwadname)
     && !strcasecmp(iwadname + strlen(iwadname) - strlen(chex_iwadname),
                    chex_iwadname));
}
*/

//      print title for every printed line
char            title[128];


static boolean D_AddFile(char *filename)
{
    wad_file_t *handle;

    printf(" adding %s\n", filename);
    handle = W_AddFile(filename);

    return handle != NULL;
}

// Copyright message banners
// Some dehacked mods replace these.  These are only displayed if they are 
// replaced by dehacked.
// haleyjd 08/22/2010: [STRIFE] altered to match strings from binary
static char *copyright_banners[] =
{
    "===========================================================================\n"
    "ATTENTION:  This version of STRIFE has extra files added to it.\n"
    "        You will not receive technical support for modified games.\n"
    "===========================================================================\n",

    "===========================================================================\n"
    "             This version is NOT SHAREWARE, do not distribute!\n"
    "         Please report software piracy to the SPA: 1-800-388-PIR8\n"
    "===========================================================================\n",

    "===========================================================================\n"
    "                                Shareware!\n"
    "===========================================================================\n"
};

// Prints a message only if it has been modified by dehacked.

void PrintDehackedBanners(void)
{
    size_t i;

    for (i=0; i<arrlen(copyright_banners); ++i)
    {
        char *deh_s;

        deh_s = DEH_String(copyright_banners[i]);

        if (deh_s != copyright_banners[i])
        {
            printf("%s", deh_s);

            // Make sure the modified banner always ends in a newline character.
            // If it doesn't, add a newline.  This fixes av.wad.

            if (deh_s[strlen(deh_s) - 1] != '\n')
            {
                printf("\n");
            }
        }
    }
}

static struct 
{
    char *description;
    char *cmdline;
    GameVersion_t version;
} gameversions[] = {
    {"Doom 1.9",             "1.9",        exe_doom_1_9},
    {"Ultimate Doom",        "ultimate",   exe_ultimate},
    {"Final Doom",           "final",      exe_final},
    {"Chex Quest",           "chex",       exe_chex},
    { NULL,                  NULL,         0},
};

// Initialize the game version

static void InitGameVersion(void)
{
    int p;
    int i;

    //! 
    // @arg <version>
    // @category compat
    //
    // Emulate a specific version of Doom.  Valid values are "1.9",
    // "ultimate" and "final".
    //

    p = M_CheckParm("-gameversion");

    if (p > 0)
    {
        for (i=0; gameversions[i].description != NULL; ++i)
        {
            if (!strcmp(myargv[p+1], gameversions[i].cmdline))
            {
                gameversion = gameversions[i].version;
                break;
            }
        }
        
        if (gameversions[i].description == NULL) 
        {
            printf("Supported game versions:\n");

            for (i=0; gameversions[i].description != NULL; ++i)
            {
                printf("\t%s (%s)\n", gameversions[i].cmdline,
                        gameversions[i].description);
            }
            
            I_Error("Unknown game version '%s'", myargv[p+1]);
        }
    }
    else
    {
        // Determine automatically

        // haleyjd 08/23/10: Removed Chex mode, as it is irrelevant to Strife
        if (gamemode == shareware || gamemode == registered)
        {
            // original

            gameversion = exe_doom_1_9;
        }
        else if (gamemode == retail)
        {
            gameversion = exe_ultimate;
        }
        else if (gamemode == commercial)
        {
            if (gamemission == doom2)
            {
                gameversion = exe_doom_1_9;
            }
            else
            {
                // Final Doom: tnt or plutonia

                gameversion = exe_final;
            }
        }
    }
    
    // The original exe does not support retail - 4th episode not supported

    if (gameversion < exe_ultimate && gamemode == retail)
    {
        gamemode = registered;
    }

    // EXEs prior to the Final Doom exes do not support Final Doom.

    if (gameversion < exe_final && gamemode == commercial)
    {
        gamemission = doom2;
    }
}

void PrintGameVersion(void)
{
    int i;

    for (i=0; gameversions[i].description != NULL; ++i)
    {
        if (gameversions[i].version == gameversion)
        {
            printf("Emulating the behavior of the "
                   "'%s' executable.\n", gameversions[i].description);
            break;
        }
    }
}

// Load the Chex Quest dehacked file, if we are in Chex mode.
// haleyjd 08/23/2010: Removed, as irrelevant to Strife.
/*
static void LoadChexDeh(void)
{
    char *chex_deh;

    if (gameversion == exe_chex)
    {
        chex_deh = D_FindWADByName("chex.deh");

        if (chex_deh == NULL)
        {
            I_Error("Unable to find Chex Quest dehacked file (chex.deh).\n"
                    "The dehacked file is required in order to emulate\n"
                    "chex.exe correctly.  It can be found in your nearest\n"
                    "/idgames repository mirror at:\n\n"
                    "   utils/exe_edit/patches/chexdeh.zip");
        }

        if (!DEH_LoadFile(chex_deh))
        {
            I_Error("Failed to load chex.deh needed for emulating chex.exe.");
        }
    }
}
*/

// Function called at exit to display the ENDOOM screen

static void D_Endoom(void)
{
    byte *endoom;

    // Don't show ENDOOM if we have it disabled, or we're running
    // in screensaver or control test mode.

    if (!show_endoom || screensaver_mode || M_CheckParm("-testcontrols") > 0)
    {
        return;
    }

    // haleyjd 08/27/10: [STRIFE] ENDOOM -> ENDSTRF
    endoom = W_CacheLumpName(DEH_String("ENDSTRF"), PU_STATIC);

    I_Endoom(endoom);
}

//=============================================================================
//
// haleyjd: Chocolate Strife Specifics
//
// None of the code in here is from the original executable, but is needed for
// other reasons.

//
// D_PatchClipCallback
//
// haleyjd 08/28/10: Clip patches to the framebuffer without errors.
// Returns false if V_DrawPatch should return without drawing.
//
static boolean D_PatchClipCallback(patch_t *patch, int x, int y)
{
    // note that offsets were already accounted for in V_DrawPatch
    return (x >= 0 && y >= 0 
            && x + SHORT(patch->width) <= SCREENWIDTH 
            && y + SHORT(patch->height) <= SCREENHEIGHT);
}

//
// D_InitChocoStrife
//
// haleyjd 08/28/10: Take care of some Strife-specific initialization
// that is necessitated by Chocolate Doom issues, such as setting global 
// callbacks.
//
static void D_InitChocoStrife(void)
{
    // set the V_DrawPatch clipping callback
    V_SetPatchClipCallback(D_PatchClipCallback);
}

//
// End Chocolate Strife Specifics
//
//=============================================================================

//
// D_DoomMain
//
void D_DoomMain (void)
{
    int             p;
    char            file[256];
    char            demolumpname[9];

    I_AtExit(D_Endoom, false);

    M_FindResponseFile ();

    // print banner

    I_PrintBanner(PACKAGE_STRING);

    printf (DEH_String("Z_Init: Init zone memory allocation daemon. \n"));
    Z_Init ();

#ifdef FEATURE_MULTIPLAYER
    //!
    // @category net
    //
    // Start a dedicated server, routing packets but not participating
    // in the game itself.
    //

    if (M_CheckParm("-dedicated") > 0)
    {
        printf("Dedicated server mode.\n");
        NET_DedicatedServer();

        // Never returns
    }

    //!
    // @arg <address>
    // @category net
    //
    // Query the status of the server running on the given IP
    // address.
    //

    p = M_CheckParm("-query");

    if (p > 0)
    {
        NET_QueryAddress(myargv[p+1]);
    }

    //!
    // @category net
    //
    // Search the local LAN for running servers.
    //

    if (M_CheckParm("-search"))
        NET_LANQuery();

#endif
            
#ifdef FEATURE_DEHACKED
    printf("DEH_Init: Init Dehacked support.\n");
    DEH_Init();
#endif

    iwadfile = D_FindIWAD(IWAD_MASK_DOOM, &gamemission);

    // None found?

    if (iwadfile == NULL)
    {
        I_Error("Game mode indeterminate.  No IWAD file was found.  Try\n"
                "specifying one with the '-iwad' command line parameter.\n");
    }

    modifiedgame = false;

    //!
    // @vanilla
    //
    // Disable monsters.
    //
	
    nomonsters = M_CheckParm ("-nomonsters");

    //!
    // @vanilla
    //
    // Monsters respawn after being killed.
    //

    respawnparm = M_CheckParm ("-respawn");

    //!
    // @vanilla
    //
    // Monsters move faster.
    //

    fastparm = M_CheckParm ("-fast");

    //! 
    // @vanilla
    //
    // Developer mode.  F1 saves a screenshot in the current working
    // directory.
    //

    devparm = M_CheckParm ("-devparm");

    I_DisplayFPSDots(devparm);

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch game.
    //

    if (M_CheckParm ("-deathmatch"))
	deathmatch = 1;

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch 2.0 game.  Weapons do not stay in place and
    // all items respawn after 30 seconds.
    //

    if (M_CheckParm ("-altdeath"))
	deathmatch = 2;

    if (devparm)
	printf(DEH_String(D_DEVSTR));
    
    // find which dir to use for config files

#ifdef _WIN32

    //!
    // @platform windows
    // @vanilla
    //
    // Save configuration data and savegames in c:\doomdata,
    // allowing play from CD.
    //

    if (M_CheckParm("-cdrom") > 0)
    {
        printf(D_CDROM);

        // haleyjd 08/22/2010: [STRIFE] Use strife.cd folder for -cdrom
        M_SetConfigDir("c:\\strife.cd\\");
    }
    else
#endif
    {
        // Auto-detect the configuration dir.

        M_SetConfigDir(NULL);
    }
    
    //!
    // @arg <x>
    // @vanilla
    //
    // Turbo mode.  The player's speed is multiplied by x%.  If unspecified,
    // x defaults to 200.  Values are rounded up to 10 and down to 400.
    //

    if ( (p=M_CheckParm ("-turbo")) )
    {
	int     scale = 200;
	extern int forwardmove[2];
	extern int sidemove[2];
	
	if (p<myargc-1)
	    scale = atoi (myargv[p+1]);
	if (scale < 10)
	    scale = 10;
	if (scale > 400)
	    scale = 400;
	printf (DEH_String("turbo scale: %i%%\n"),scale);
	forwardmove[0] = forwardmove[0]*scale/100;
	forwardmove[1] = forwardmove[1]*scale/100;
	sidemove[0] = sidemove[0]*scale/100;
	sidemove[1] = sidemove[1]*scale/100;
    }
    
    // init subsystems
    printf(DEH_String("V_Init: allocate screens.\n"));
    V_Init();

    // Load configuration files before initialising other subsystems.
    // haleyjd 08/22/2010: [STRIFE] - use strife.cfg
    printf(DEH_String("M_LoadDefaults: Load system defaults.\n"));
    M_SetConfigFilenames("default.cfg", PROGRAM_PREFIX "strife.cfg");
    D_BindVariables();
    M_LoadDefaults();

    // Save configuration at exit.
    I_AtExit(M_SaveDefaults, false);

    printf (DEH_String("W_Init: Init WADfiles.\n"));
    D_AddFile(iwadfile);

#ifdef FEATURE_WAD_MERGE

    // Merged PWADs are loaded first, because they are supposed to be 
    // modified IWADs.

    //!
    // @arg <files>
    // @category mod
    //
    // Simulates the behavior of deutex's -merge option, merging a PWAD
    // into the main IWAD.  Multiple files may be specified.
    //

    p = M_CheckParm("-merge");

    if (p > 0)
    {
        for (p = p + 1; p<myargc && myargv[p][0] != '-'; ++p)
        {
            char *filename;

            filename = D_TryFindWADByName(myargv[p]);

            printf(" merging %s\n", filename);
            W_MergeFile(filename);
        }
    }

    // NWT-style merging:

    // NWT's -merge option:

    //!
    // @arg <files>
    // @category mod
    //
    // Simulates the behavior of NWT's -merge option.  Multiple files
    // may be specified.

    p = M_CheckParm("-nwtmerge");

    if (p > 0)
    {
        for (p = p + 1; p<myargc && myargv[p][0] != '-'; ++p)
        {
            char *filename;

            filename = D_TryFindWADByName(myargv[p]);

            printf(" performing NWT-style merge of %s\n", filename);
            W_NWTDashMerge(filename);
        }
    }
    
    // Add flats

    //!
    // @arg <files>
    // @category mod
    //
    // Simulates the behavior of NWT's -af option, merging flats into
    // the main IWAD directory.  Multiple files may be specified.
    //

    p = M_CheckParm("-af");

    if (p > 0)
    {
        for (p = p + 1; p<myargc && myargv[p][0] != '-'; ++p)
        {
            char *filename;

            filename = D_TryFindWADByName(myargv[p]);

            printf(" merging flats from %s\n", filename);
            W_NWTMergeFile(filename, W_NWT_MERGE_FLATS);
        }
    }

    //!
    // @arg <files>
    // @category mod
    //
    // Simulates the behavior of NWT's -as option, merging sprites
    // into the main IWAD directory.  Multiple files may be specified.
    //

    p = M_CheckParm("-as");

    if (p > 0)
    {
        for (p = p + 1; p<myargc && myargv[p][0] != '-'; ++p)
        {
            char *filename;

            filename = D_TryFindWADByName(myargv[p]);

            printf(" merging sprites from %s\n", filename);
            W_NWTMergeFile(filename, W_NWT_MERGE_SPRITES);
        }
    }

    //!
    // @arg <files>
    // @category mod
    //
    // Equivalent to "-af <files> -as <files>".
    //

    p = M_CheckParm("-aa");

    if (p > 0)
    {
        for (p = p + 1; p<myargc && myargv[p][0] != '-'; ++p)
        {
            char *filename;

            filename = D_TryFindWADByName(myargv[p]);

            printf(" merging sprites and flats from %s\n", filename);
            W_NWTMergeFile(filename, W_NWT_MERGE_SPRITES | W_NWT_MERGE_FLATS);
        }
    }

#endif

    //!
    // @arg <files>
    // @vanilla
    //
    // Load the specified PWAD files.
    //

    p = M_CheckParm ("-file");
    if (p)
    {
	// the parms after p are wadfile/lump names,
	// until end of parms or another - preceded parm
	modifiedgame = true;            // homebrew levels
	while (++p != myargc && myargv[p][0] != '-')
        {
            char *filename;

            filename = D_TryFindWADByName(myargv[p]);

	    D_AddFile(filename);
        }
    }

    // Debug:
//    W_PrintDirectory();

    // add any files specified on the command line with -file wadfile
    // to the wad list
    //
    // convenience hack to allow -wart e m to add a wad file
    // prepend a tilde to the filename so wadfile will be reloadable
    p = M_CheckParm ("-wart");
    if (p)
    {
	myargv[p][4] = 'p';     // big hack, change to -warp

	// Map name handling.
	switch (gamemode )
	{
	  case shareware:
	  case retail:
	  case registered:
	    sprintf (file,"~"DEVMAPS"E%cM%c.wad",
		     myargv[p+1][0], myargv[p+2][0]);
	    printf("Warping to Episode %s, Map %s.\n",
		   myargv[p+1],myargv[p+2]);
	    break;
	    
	  case commercial:
	  default:
	    p = atoi (myargv[p+1]);
	    if (p<10)
	      sprintf (file,"~"DEVMAPS"cdata/map0%i.wad", p);
	    else
	      sprintf (file,"~"DEVMAPS"cdata/map%i.wad", p);
	    break;
	}
	D_AddFile (file);
    }

    //!
    // @arg <demo>
    // @category demo
    // @vanilla
    //
    // Play back the demo named demo.lmp.
    //

    p = M_CheckParm ("-playdemo");

    if (!p)
    {
        //!
        // @arg <demo>
        // @category demo
        // @vanilla
        //
        // Play back the demo named demo.lmp, determining the framerate
        // of the screen.
        //
	p = M_CheckParm ("-timedemo");

    }

    if (p && p < myargc-1)
    {
        if (!strcasecmp(myargv[p+1] + strlen(myargv[p+1]) - 4, ".lmp"))
        {
            strcpy(file, myargv[p + 1]);
        }
        else
        {
	    sprintf (file,"%s.lmp", myargv[p+1]);
        }

	if (D_AddFile (file))
        {
            strncpy(demolumpname, lumpinfo[numlumps - 1].name, 8);
            demolumpname[8] = '\0';

            printf("Playing demo %s.\n", file);
        }
        else
        {
            // If file failed to load, still continue trying to play
            // the demo in the same way as Vanilla Doom.  This makes
            // tricks like "-playdemo demo1" possible.

            strncpy(demolumpname, myargv[p + 1], 8);
            demolumpname[8] = '\0';
        }

    }

    I_AtExit((atexit_func_t) G_CheckDemoStatus, true);

    // Generate the WAD hash table.  Speed things up a bit.

    W_GenerateHashTable();
    
    D_IdentifyVersion();
    InitGameVersion();
    //LoadChexDeh(); - haleyjd: removed, as irrelevant to Strife
    D_SetGameDescription();
    SetSaveGameDir(iwadfile);

    // Check for -file in shareware
    if (0 /*modifiedgame*/) // STRIFE-FIXME: DISABLED FOR TESTING
    {
        // These are the lumps that will be checked in IWAD,
        // if any one is not present, execution will be aborted.
        // haleyjd 08/22/2010: [STRIFE] Check for Strife lumps.
        char name[3][8]=
        {
            "map23", "map30", "ROB3E1"
        };
        int i;

        // haleyjd 08/22/2010: [STRIFE] Changed string to match binary
        // STRIFE-FIXME: Needs to test isdemoversion variable
        if ( gamemode == shareware)
            I_Error(DEH_String("\nYou cannot -file with the demo "
                               "version. You must buy the real game!"));

        // Check for fake IWAD with right name,
        // but w/o all the lumps of the registered version. 
        // STRIFE-FIXME: Needs to test isregistered variable
        if (gamemode == registered)
            for (i = 0; i < 3; i++)
                if (W_CheckNumForName(name[i])<0)
                    I_Error(DEH_String("\nThis is not the registered version."));
    }
    
    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    //!
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    p = M_CheckParm ("-skill");

    if (p && p < myargc-1)
    {
	startskill = myargv[p+1][0]-'1';
	autostart = true;
    }

    //!
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    p = M_CheckParm ("-episode");

    if (p && p < myargc-1)
    {
	startepisode = myargv[p+1][0]-'0';
	startmap = 1;
	autostart = true;
    }
	
    timelimit = 0;

    //! 
    // @arg <n>
    // @category net
    // @vanilla
    //
    // For multiplayer games: exit each level after n minutes.
    //

    p = M_CheckParm ("-timer");

    if (p && p < myargc-1 && deathmatch)
    {
	timelimit = atoi(myargv[p+1]);
	printf("timer: %i\n", timelimit);
    }

    //!
    // @category net
    // @vanilla
    //
    // Austin Virtual Gaming: end levels after 20 minutes.
    //

    p = M_CheckParm ("-avg");

    if (p && p < myargc-1 && deathmatch)
    {
	printf(DEH_String("Austin Virtual Gaming: Levels will end "
			  "after 20 minutes\n"));
	timelimit = 20;
    }

    //!
    // @arg [<x> <y> | <xy>]
    // @vanilla
    //
    // Start a game immediately, warping to ExMy (Doom 1) or MAPxy
    // (Doom 2)
    //

    p = M_CheckParm ("-warp");

    if (p && p < myargc-1)
    {
        if (gamemode == commercial)
            startmap = atoi (myargv[p+1]);
        else
        {
            startepisode = myargv[p+1][0]-'0';

            if (p + 2 < myargc)
            {
                startmap = myargv[p+2][0]-'0';
            }
            else
            {
                startmap = 1;
            }
        }
        autostart = true;
    }

    // Undocumented:
    // Invoked by setup to test the controls.

    p = M_CheckParm("-testcontrols");

    if (p > 0)
    {
        startepisode = 1;
        startmap = 1;
        autostart = true;
        testcontrols = true;
    }

    // Check for load game parameter
    // We do this here and save the slot number, so that the network code
    // can override it or send the load slot to other players.

    //!
    // @arg <s>
    // @vanilla
    //
    // Load the game in slot s.
    //

    p = M_CheckParm ("-loadgame");
    
    if (p && p < myargc-1)
    {
        startloadgame = atoi(myargv[p+1]);
    }
    else
    {
        // Not loading a game
        startloadgame = -1;
    }

    if (W_CheckNumForName("SS_START") >= 0
     || W_CheckNumForName("FF_END") >= 0)
    {
        I_PrintDivider();
        printf(" WARNING: The loaded WAD file contains modified sprites or\n"
               " floor textures.  You may want to use the '-merge' command\n"
               " line option instead of '-file'.\n");
    }

    I_PrintStartupBanner(gamedescription);
    PrintDehackedBanners();

    // haleyjd 08/28/10: Init Choco Strife stuff.
    D_InitChocoStrife();

    printf (DEH_String("M_Init: Init miscellaneous info.\n"));
    M_Init ();

    // haleyjd 08/22/2010: [STRIFE] Modified string to match binary
    printf (DEH_String("R_Init: Loading Graphics - "));
    R_Init ();

    printf (DEH_String("\nP_Init: Init Playloop state.\n"));
    P_Init ();

    printf (DEH_String("I_Init: Setting up machine state.\n"));
    I_CheckIsScreensaver();
    I_InitTimer();
    I_InitJoystick();

#ifdef FEATURE_MULTIPLAYER
    printf ("NET_Init: Init network subsystem.\n");
    NET_Init ();
#endif

    printf (DEH_String("S_Init: Setting up sound.\n"));
    S_Init (sfxVolume * 8, musicVolume * 8);

    printf (DEH_String("D_CheckNetGame: Checking network game status.\n"));
    D_CheckNetGame ();

    PrintGameVersion();

    printf (DEH_String("HU_Init: Setting up heads up display.\n"));
    HU_Init ();

    printf (DEH_String("ST_Init: Init status bar.\n"));
    ST_Init ();

    // If Doom II without a MAP01 lump, this is a store demo.  
    // Moved this here so that MAP01 isn't constantly looked up
    // in the main loop.
    // haleyjd 08/23/2010: [STRIFE] There is no storedemo version of Strife
    /*
    if (gamemode == commercial && W_CheckNumForName("map01") < 0)
        storedemo = true;
    */

    //!
    // @arg <x>
    // @category demo
    // @vanilla
    //
    // Record a demo named x.lmp.
    //

    p = M_CheckParm ("-record");

    if (p && p < myargc-1)
    {
	G_RecordDemo (myargv[p+1]);
	autostart = true;
    }

    p = M_CheckParm ("-playdemo");
    if (p && p < myargc-1)
    {
	singledemo = true;              // quit after one demo
	G_DeferedPlayDemo (demolumpname);
	D_DoomLoop ();  // never returns
    }
	
    p = M_CheckParm ("-timedemo");
    if (p && p < myargc-1)
    {
	G_TimeDemo (demolumpname);
	D_DoomLoop ();  // never returns
    }
	
    if (startloadgame >= 0)
    {
        strcpy(file, P_SaveGameFile(startloadgame));
	G_LoadGame (file);
    }
	
    if (gameaction != ga_loadgame )
    {
	if (autostart || netgame)
	    G_InitNew (startskill, startepisode, startmap);
	else
	    D_StartTitle ();                // start up intro loop
    }

    D_DoomLoop ();  // never returns
}
