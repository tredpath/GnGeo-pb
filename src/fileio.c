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
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "unzip.h"
#include "memory.h"
#include "video.h"
#include "emu.h"
#include "fileio.h"
#include "neocrypt.h"
#include "screen.h"
#include "conf.h"
#include "pbar.h"
#include "driver.h"
#include "sound.h"

#ifdef USE_GUI
#include "gui/gui.h"
#endif

Uint8 *current_buf;
//extern Uint8 fix_buffer[0x20000];
char *rom_file;

char *file_basename(char *filename) {
    char *t;
    t=strrchr(filename,'/');
    if (t) return t+1;
    return filename;
}

/* check if dir_name exist. Create it if not */
SDL_bool check_dir(char *dir_name)
{
    DIR *d;

    if (!(d = OPENDIR(dir_name)) && (errno == ENOENT)) {
    	MAKEDIR(dir_name, 0755);
	return SDL_FALSE;
    }
    return SDL_TRUE;
}

/* return a char* to $HOME/.gngeo/ 
   DO NOT free it!
*/
char *get_gngeo_dir(void) {
    static char *filename=NULL;
    int len = strlen(getenv("HOME")) + strlen("/.gngeo/") + 1;
    int i;
    if (!filename) {
	filename=malloc(len*sizeof(char));
	sprintf(filename,"%s/.gngeo/",getenv("HOME"));
    }
    check_dir(filename);
    //printf("get_gngeo_dir %s\n",filename);
    return filename;
}

void open_nvram(char *name)
{
    char *filename;
    char *gngeo_dir=get_gngeo_dir();
    FILE *f;
    int len =strlen(name) + strlen(gngeo_dir) + 4; /* ".nv\0" => 4 */
    //strlen(name) + strlen(getenv("HOME")) + strlen("/.gngeo/") + 4;
//    printf("Open nvram %s\n",name);
    filename = (char *) alloca(len);
    sprintf(filename,"%s%s.nv",gngeo_dir,name);
    
    if ((f = fopen(filename, "rb")) == 0)
	return;
    fread(memory.sram, 1, 0x10000, f);
    fclose(f);
}

void save_nvram(char *name)
{
    char *filename;
    char *gngeo_dir=get_gngeo_dir();
    FILE *f;
    int len = strlen(name) + strlen(gngeo_dir) + 4; /* ".nv\0" => 4 */
    //strlen(name) + strlen(getenv("HOME")) + strlen("/.gngeo/") + 4;
    int i;
//    printf("Save nvram %s\n",name);
    for (i = 0xffff; i >= 0; i--) {
	if (memory.sram[i] != 0)
	    break;
    }

    filename = (char *) alloca(len);
    sprintf(filename,"%s%s.nv",gngeo_dir,name);

    f = fopen(filename, "wb");
    fwrite(memory.sram, 1, 0x10000, f);
    fclose(f);
}

void free_game_memory(void) {

    /* clean up memory */
    free(memory.cpu);memory.cpu=NULL;
    
    free(memory.sm1);memory.sm1=NULL;
    free(memory.sfix_game);memory.sfix_game=NULL;
    if (memory.sound1!=memory.sound2) 
	free(memory.sound2);
    memory.sound2=NULL;
    free(memory.sound1);memory.sound1=NULL;
    free(memory.gfx);memory.gfx=NULL;
    free(memory.pen_usage);memory.pen_usage=NULL;

}

SDL_bool init_game(char *rom_name) {
    DRIVER *dr;
    char *drconf,*gpath;
    dr=dr_get_by_name(rom_name);
    if (!dr) {
#ifdef USE_GUI
	gui_error_box(20,80,264,60,
		      "Error!","No valid romset found for\n%s\n",
		      file_basename(rom_name));
#else
	printf("No valid romset found for %s\n",rom_name);
#endif
	return SDL_FALSE;
    }

    if (conf.game!=NULL) {
	save_nvram(conf.game);
	if (conf.sound) {
	    close_sdl_audio();
	    YM2610_sh_stop();
	    streams_sh_stop();
	}
	free_game_memory();
    }


    /* open transpack if need */
    trans_pack_open(CF_STR(cf_get_item_by_name("transpack")));

    //open_rom(rom_name);
    if (dr_load_game(dr,rom_name)==SDL_FALSE) {
#ifdef USE_GUI
	gui_error_box(20,80,264,60,
		      "Error!","Couldn't load\n%s\n",
		      file_basename(rom_name));
#else
	printf("Can't load %s\n",rom_name);
#endif
	return SDL_FALSE;
    }
    /* per game config */
    gpath=get_gngeo_dir();
    drconf=alloca(strlen(gpath)+strlen(dr->name)+strlen(".cf")+1);
    sprintf(drconf,"%s%s.cf",gpath,dr->name);
    cf_open_file(drconf);
    open_bios();
    open_nvram(conf.game);
    init_sdl();
    sdl_set_title(conf.game);
    init_neo(conf.game);
    if (conf.sound) 
	init_sdl_audio();
    return SDL_TRUE;
}

void free_bios_memory(void) {
    free(memory.ram);memory.ram=NULL;
    if (!conf.special_bios)
      free(memory.bios);memory.bios=NULL;
    free(memory.ng_lo);memory.ng_lo=NULL;
    free(memory.sfix_board);memory.sfix_board=NULL;

    free(memory.pal1);memory.pal1=NULL;
    free(memory.pal2);memory.pal2=NULL;
    free(memory.pal_pc1);memory.pal_pc1=NULL;
    free(memory.pal_pc2);memory.pal_pc2=NULL;
}

void open_bios(void)
{
    FILE *f;
    char *romfile;
    char *path = CF_STR(cf_get_item_by_name("rompath"));//conf.rom_path;
    int len = strlen(path) + 15;

    if (conf.game!=NULL) free_bios_memory();

    /* allocation de la ram */
    memory.ram = (Uint8 *) malloc(0x10000);
    memset(memory.ram,0,0x10000);
    memory.sfix_board = (Uint8 *) malloc(0x20000);
    memory.ng_lo = (Uint8 *) malloc(0x10000);

    /* partie video */
    memory.pal1 = (Uint8 *) malloc(0x2000);
    memory.pal2 = (Uint8 *) malloc(0x2000);

    memory.pal_pc1 = (Uint8 *) malloc(0x2000);
    memory.pal_pc2 = (Uint8 *) malloc(0x2000);

    memset(memory.video, 0, 0x20000);

    romfile = (char *) malloc(len);
    memset(romfile, 0, len);
    if (!conf.special_bios) {
      memory.bios = (Uint8 *) malloc(0x20000);
      memory.bios_size=0x20000;
      /* try new bios */
      if (conf.system==SYS_HOME) {
          sprintf(romfile, "%s/aes-bios.bin", path);
      } else {
          if (conf.country==CTY_JAPAN) {
              sprintf(romfile, "%s/vs-bios.rom", path);
          } else if (conf.country==CTY_USA) {
              sprintf(romfile, "%s/usa_2slt.bin", path);
          } else if (conf.country==CTY_ASIA) {
              sprintf(romfile, "%s/asia-s3.rom", path);
          } else {
              sprintf(romfile, "%s/sp-s2.sp1", path);
          }
      }
      f = fopen(romfile, "rb");
      if (f == NULL) {
          printf("Can't find %s\n", romfile);
          exit(1);
      }
      fread(memory.bios, 1, 0x20000, f);
      fclose(f);
    }
    sprintf(romfile, "%s/ng-sfix.rom", path);
    f = fopen(romfile, "rb");
    if (f == NULL) {
	/* try new bios */
	sprintf(romfile, "%s/sfix.sfx", path);
	f = fopen(romfile, "rb");
	if (f == NULL) {
	    printf("Can't find %s\n", romfile);
	    exit(1);
	}
    }
    fread(memory.sfix_board, 1, 0x20000, f);
    fclose(f);

    sprintf(romfile, "%s/ng-lo.rom", path);
    f = fopen(romfile, "rb");
    if (f == NULL) {
	/* try new bios */
	sprintf(romfile, "%s/000-lo.lo", path);
	f = fopen(romfile, "rb");
	if (f == NULL) {
	    printf("Can't find %s\n", romfile);
	    exit(1);
	}
    }
    fread(memory.ng_lo, 1, 0x10000, f);
    fclose(f);


    /* convert bios fix char */
    convert_all_char(memory.sfix_board, 0x20000, memory.fix_board_usage);

    fix_usage = memory.fix_board_usage;
    current_pal = memory.pal1;
    current_fix = memory.sfix_board;
    current_pc_pal = (Uint16 *) memory.pal_pc1;

    free(romfile);
}


