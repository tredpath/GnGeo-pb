/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 * 
 *  This program is free software; you can redistribute it and/or modify  
 *  it under the terms of the GNU General Public License as published by   
 *  the Free Software Foundation; either version 2 of the License, or    
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "SDL.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "streams.h"
#include "ym2610/2610intf.h"
#include "font.h"
#include "fileio.h"
#include "video.h"
#include "screen.h"
#include "emu.h"
#include "sound.h"
#include "messages.h"
#include "memory.h"
#include "debug.h"
#include "blitter.h"
#include "effect.h"
#include "conf.h"
#include "transpack.h"
#include "gngeo_icon.h"
#include "driver.h"

#ifdef USE_GUI
#include "gui_interf.h"
#endif


void calculate_hotkey_bitmasks()
{
    int *p;
    int i, j, mask;
    char *p1_key_list[] = { "p1hotkey0", "p1hotkey1", "p1hotkey2", "p1hotkey3" };
    char *p2_key_list[] = { "p2hotkey0", "p2hotkey1", "p2hotkey2", "p2hotkey3" };

    for ( i = 0; i < 4; i++ ) {
	p=CF_ARRAY(cf_get_item_by_name(p1_key_list[i]));
	for ( mask = 0, j = 0; j < 4; j++ ) mask |= p[j];
	conf.p1_hotkey[i] = mask;
    }

    for ( i = 0; i < 4; i++ ) {
	p=CF_ARRAY(cf_get_item_by_name(p2_key_list[i]));
	for ( mask = 0, j = 0; j < 4; j++ ) mask |= p[j];
	conf.p2_hotkey[i] = mask;
    }

}

void init_joystick(void) 
{
    //    int invert_joy=CF_BOOL(cf_get_item_by_name("invertjoy"));
    int i;
    int joyindex[2];
    int lastinit=-1;
    joyindex[0]=CF_VAL(cf_get_item_by_name("p1joydev"));
    joyindex[1]=CF_VAL(cf_get_item_by_name("p2joydev"));

    if (!CF_BOOL(cf_get_item_by_name("joystick")))
	return;

    SDL_JoystickEventState(SDL_ENABLE);

    conf.nb_joy = SDL_NumJoysticks();
    /* on ne gere que les deux premiers joysticks */
    /*
      if (conf.nb_joy > 2)
      conf.nb_joy = 2;

      if (invert_joy && conf.nb_joy < 2) {
      invert_joy = 0;
      CF_BOOL(cf_get_item_by_name("invertjoy"))=SDL_FALSE;
      }
    */
 
    for (i=0;i<2;i++) {
	if (lastinit!=joyindex[i]) {
	    lastinit=joyindex[i];
	    conf.joy[i] = SDL_JoystickOpen(joyindex[i]);
	    if (conf.joy[i] && joyindex[i]<conf.nb_joy) {
		    joy_numaxes[i] = SDL_JoystickNumAxes(conf.joy[i]);
		    printf("joy %s, axe:%d, button:%d\n",
			   SDL_JoystickName(i),
			   // HAT SUPPORT SDL_JoystickNumAxes(conf.joy[i]),
			   joy_numaxes[i] + (SDL_JoystickNumHats(conf.joy[i]) * 2),
			   SDL_JoystickNumButtons(conf.joy[i]));
		    joy_button[i] =	(Uint8 *) malloc(SDL_JoystickNumButtons(conf.joy[i]));
		    // joy_axe[i] = (Uint32 *) malloc(SDL_JoystickNumAxes(conf.joy[i]) * sizeof(Sint32));
		    joy_axe[i] = (Uint32 *) malloc((joy_numaxes[i] + (SDL_JoystickNumHats(conf.joy[i]) * 2)) * sizeof(Uint32));
		    memset(joy_button[i], 0, SDL_JoystickNumButtons(conf.joy[i]));
		    // memset(joy_axe[i], 0, SDL_JoystickNumAxes(conf.joy[i]) * sizeof(Sint32));
		    memset(joy_axe[i], 0, (joy_numaxes[i] + (SDL_JoystickNumHats(conf.joy[i]) * 2)) * sizeof(Uint32));
	    }
	} else {
	    printf("Joystick number %d used for Player1, skip..\n",joyindex[i]);
	}
    }
    conf.p2_joy = CF_ARRAY(cf_get_item_by_name("p2joy"));
    conf.p1_joy = CF_ARRAY(cf_get_item_by_name("p1joy"));
    /*
      for (i = 0; i < conf.nb_joy; i++) {
      if (invert_joy)
      conf.joy[i] = SDL_JoystickOpen(1 - i);
      else
      conf.joy[i] = SDL_JoystickOpen(i);
      printf("joy %s, axe:%d, button:%d\n",
      SDL_JoystickName(i),
      SDL_JoystickNumAxes(conf.joy[i]),
      SDL_JoystickNumButtons(conf.joy[i]));
      joy_button[i] =	(Uint8 *) malloc(SDL_JoystickNumButtons(conf.joy[i]));
      joy_axe[i] = (Uint32 *) malloc(SDL_JoystickNumAxes(conf.joy[i]) * sizeof(INT32));
      memset(joy_button[i], 0, SDL_JoystickNumButtons(conf.joy[i]));
      memset(joy_axe[i], 0, SDL_JoystickNumAxes(conf.joy[i]) * sizeof(INT32));
	
      }
      if (invert_joy) {
      conf.p2_joy = CF_ARRAY(cf_get_item_by_name("p1joy"));
      conf.p1_joy = CF_ARRAY(cf_get_item_by_name("p2joy"));
      } else {
      conf.p2_joy = CF_ARRAY(cf_get_item_by_name("p2joy"));
      conf.p1_joy = CF_ARRAY(cf_get_item_by_name("p1joy"));
      }
    */
}

void sdl_set_title(char *name) {
    char *title;
    if (name) {
	title = malloc(strlen("Gngeo : ")+strlen(name)+1);
	sprintf(title,"Gngeo : %s",name);
	SDL_WM_SetCaption(title, NULL);
    } else {
	SDL_WM_SetCaption("Gngeo", NULL);
    }
}

void init_sdl(void /*char *rom_name*/)
{
    Uint32 sdl_flag = 0;
    //char title[32] = "Gngeo : ";
    int i;
    int surface_type = SDL_SWSURFACE;//(conf.hw_surface ? SDL_HWSURFACE : SDL_SWSURFACE);
    char *nomouse = getenv("SDL_NOMOUSE");
    SDL_Surface *icon;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);

    atexit(SDL_Quit);

#ifdef DEBUG_VIDEO
    screen = SDL_SetVideoMode(512 + 32, 512 + 32, 16, sdl_flag);
    buffer = SDL_CreateRGBSurface(surface_type, 512 + 32, 512 + 32, 16, 0xF800,
				  0x7E0, 0x1F, 0);
#else

    if (screen_init() == SDL_FALSE) {
	printf("Screen initialization failed.\n");
	exit(-1);
    }
    buffer = SDL_CreateRGBSurface(surface_type, 352, 256, 16, 0xF800, 0x7E0,
				  0x1F, 0);
#endif

    fontbuf = SDL_CreateRGBSurfaceFrom(font_image.pixel_data, font_image.width, font_image.height
				       , 24, font_image.width * 3, 0xFF0000, 0xFF00, 0xFF, 0);
    SDL_SetColorKey(fontbuf,SDL_SRCCOLORKEY,SDL_MapRGB(fontbuf->format,0xFF,0,0xFF));
    fontbuf=SDL_DisplayFormat(fontbuf);
    icon = SDL_CreateRGBSurfaceFrom(gngeo_icon.pixel_data, gngeo_icon.width,
				    gngeo_icon.height, gngeo_icon.bytes_per_pixel*8,
				    gngeo_icon.width * gngeo_icon.bytes_per_pixel,
				    0xFF, 0xFF00, 0xFF0000, 0);
    
    /*
      strncat(title, rom_name, 8);
      SDL_WM_SetCaption(title, NULL);
    */
    SDL_WM_SetIcon(icon,NULL);

    calculate_hotkey_bitmasks();    
    init_joystick();
    /* init key mapping */
    conf.p1_key=CF_ARRAY(cf_get_item_by_name("p1key"));
    conf.p2_key=CF_ARRAY(cf_get_item_by_name("p2key"));

    if (nomouse == NULL)
	SDL_ShowCursor(0);
}

int main(int argc, char *argv[])
{
    char *rom_name;
    int nopt;
    char *gpath;
    DRIVER *dr;
    Uint8 gui_res,gngeo_quit=0;
    char *country;
    char *system;
    /* faut bien le mettre quelque part */

#ifdef __QNXNTO__
    mkdir("shared/misc/gngeo/", 0777);
    mkdir("shared/misc/gngeo/ROMS/", 0777);
    mkdir("shared/misc/gngeo/ROMS/romrc.d/", 0777);
    mkdir("shared/misc/gngeo/ROMS/shots/", 0777);
    mkdir("shared/misc/gngeo/bios/", 0777);
    mkdir("shared/misc/gngeo/save/", 0777);
#endif

    cf_init(); /* must be the first thing to do */
    cf_open_file(NULL);
    cf_init_cmd_line();
    //nopt=cf_get_non_opt_index(argc,argv);
    rom_name=cf_parse_cmd_line(argc,argv); /* First pass */
    /*
      if (nopt<argc)
      rom_name=argv[nopt];
      else
      rom_name=NULL;
    */
    dr_load_driver(CF_STR(cf_get_item_by_name("romrc")));
    dr_load_driver_dir(CF_STR(cf_get_item_by_name("romrcdir")));
    //printf("romrc = %s\n",CF_STR(cf_get_item_by_name("romrc")));

    /* cache some frequently used conf item */
    conf.sound=CF_BOOL(cf_get_item_by_name("sound"));
    conf.sample_rate=CF_BOOL(cf_get_item_by_name("samplerate"));
    conf.debug=CF_BOOL(cf_get_item_by_name("debug"));
    conf.raster=CF_BOOL(cf_get_item_by_name("raster"));
    conf.pal=CF_BOOL(cf_get_item_by_name("pal"));
    country=CF_STR(cf_get_item_by_name("country"));
    system=CF_STR(cf_get_item_by_name("system"));
    if (!strcmp(system,"home")) {
        conf.system=SYS_HOME;
    } else {
        conf.system=SYS_ARCADE;
    }
    if (!strcmp(country,"japan")) {
        conf.country=CTY_JAPAN;
    } else if (!strcmp(country,"usa")) {
        conf.country=CTY_USA;
    } else if (!strcmp(country,"asia")) {
        conf.country=CTY_ASIA;
    } else {
        conf.country=CTY_EUROPE;
    }
    

    if (conf.debug) conf.sound=0;

    /* print effect/blitter list if asked by user */
    if (!strcmp(CF_STR(cf_get_item_by_name("effect")),"help")) {
	print_effect_list();
	exit(0);
    }
    if (!strcmp(CF_STR(cf_get_item_by_name("blitter")),"help")) {
	print_blitter_list();
	exit(0);
    }

#if defined(USE_GUI)
    init_sdl();
    sdl_set_title(NULL);
    init_gngeo_gui();
    if (!rom_name) {
    	TCO_invisible = 1;
		while(!main_gngeo_gui()) {
			TCO_invisible = 0;
			if (conf.game==NULL) break; /* no game was loaded, quit */
			if (conf.debug)
				debug_loop();
			else
				main_loop();
			if (shutdown)
				break;
	    	TCO_invisible = 1;
		}
		if (conf.sound) close_sdl_audio();
    } else {
		init_game(rom_name);
		do {
			if (conf.debug)
				debug_loop();
			else
				main_loop();
		} while(!main_gngeo_gui());
    }
#else

    if (!rom_name) {
#ifdef __QNXNTO__
    	rom_name = (char*)malloc(128 * sizeof(char));
    	if (!AutoLoadRom(rom_name))
    	{
    		free(rom_name);
    		cf_print_help();
    		exit(0);
    	}
#else
		cf_print_help();
		exit(0);
#endif
    }
    init_game(rom_name);
    if (conf.debug)
	debug_loop();
    else
	main_loop();

#endif

    save_nvram(conf.game);
    return 0;
}
