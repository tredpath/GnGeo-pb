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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __QNXNTO__
#include <psptypes.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include <alloca.h>
#include "unzip.h"
#include "driver.h"
#include "conf.h"
#include "pbar.h"
#include "memory.h"
#include "neocrypt.h"

static LIST *driver_list=NULL;

static SDL_bool goto_next_driver(FILE * f)
{
    char buf[512];
    char game[10], type[10];
    long pos;

    while (!feof(f)) {
	pos=ftell(f);
	fgets(buf, 510, f);
	if (sscanf(buf, "game %10s %10s", game, type) >= 2) {
	    fseek(f,pos,SEEK_SET);
	    return SDL_TRUE;
	}
    }
    return SDL_FALSE;
}

static int get_sec_by_name(char *section) {
    static char *sec[SEC_MAX]={"CPU","SFIX","SM1","SOUND1","SOUND2","GFX"};
    int i;
    for(i=0;i<SEC_MAX;i++) {
	if (strcmp(sec[i],section)==0)
	    return i;
    }
    /*printf("Unknow section %s\n",section);*/
    return -1;
}

static int get_romtype_by_name(char *type) {
    static char *rt[ROMTYPE_MAX]={"MGD2","MVS","MVS_CMC42","MVS_CMC50"};
    int i;
    for(i=0;i<ROMTYPE_MAX;i++) {
	if (strcmp(rt[i],type)==0)
	    return i;
    }
    printf("Unknown rom type %s\n",type);
    return -1;
}

static void add_driver_section(Uint32 s, SECTION *sec,FILE *f) {
    char buf[512], a[64], b[64];
    Uint32 size, offset;
    sec->size=s;
    sec->item=NULL;
    //  printf("SECTION size=%x\n",sec->size);
    while (1) {
        SECTION_ITEM *item;
        fgets(buf, 511, f);
        if (strcmp(buf, "END\n") == 0)
            break;
        sscanf(buf, "%s %x %x %s\n", a, &offset, &size, b);
        item=malloc(sizeof(SECTION_ITEM));
        item->filename=strdup(a);
        item->begin=offset;
        item->size=size;
        item->type=(strcmp(b,"ALTERNATE")==0?LD_ALTERNATE:LD_NORM);
        sec->item=list_append(sec->item,item);
	//    printf("%s %x %x %d\n",item->filename,item->begin,item->size,item->type);
    }
}

/* add driver define in f */
static void add_driver(FILE *f)
{
    DRIVER *dr;
    char buf[512], a[64], b[64];
    Uint32 s, size, offset;
    int sec;
    char game[10], type[10];
    char *t;

    dr=malloc(sizeof(DRIVER));

    /* TODO: driver creation */
    fgets(buf, 510, f);
    sscanf(buf, "game %10s %10s", game, type);
    dr->name=strdup(game);
    dr->rom_type=get_romtype_by_name(type);
    dr->special_bios=0;
    dr->banksw_type=BANKSW_NORMAL;
    t = strchr(buf, '"');
    if (t) {
	char *e;
	t+=1;
	e=strchr(t, '"');
	if (e) {
	    e[0]=0;
	    dr->longname=strdup(t);
	} else
	    dr->longname=NULL;
    }
    else dr->longname=NULL;

        printf("Add %8s | %s \n",dr->name,dr->longname);

    while (1) {
	fgets(buf, 511, f);
	if (strcmp(buf, "END\n") == 0)
	    break;
	sscanf(buf, "%s %x\n", a, &s);
	sec=get_sec_by_name(a);
	if (sec==-1) {
	    int b=0;
	    if (strcmp(a,"BIOS")==0) { add_driver_section(s,&(dr->bios), f); dr->special_bios=1;}
	    else if (strcmp(a,"XOR")==0) { dr->xor=s; }
	    else if (strcmp(a,"BANKSWITCH")==0) {
		dr->banksw_type=s;
		if(s==BANKSW_SCRAMBLE) {
		    /* not implemented yet */
		    fgets(buf,511,f);
		    sscanf(buf,"%x",&dr->banksw_addr);
		    fgets(buf, 511, f);
		    sscanf(buf, "%d %d %d %d %d %d\n",
			   (int *) &dr->banksw_unscramble[0],
			   (int *) &dr->banksw_unscramble[1],
			   (int *) &dr->banksw_unscramble[2],
			   (int *) &dr->banksw_unscramble[3],
			   (int *) &dr->banksw_unscramble[4],
			   (int *) &dr->banksw_unscramble[5]);
		    while (1) {
			fgets(buf, 511, f);
			if (strcmp(buf, "END\n") == 0)
			    break;
			sscanf(buf,"%x\n", &dr->banksw_off[b]);
			b++;
		    }
		}
	    } else {
		printf("Unknown section %s\n",a);
		return;
	    }
	} else add_driver_section(s,&(dr->section[sec]),f);
    }


    driver_list=list_prepend(driver_list,dr);
}

static void print_driver(void *data) {
    DRIVER *dr=data;
    int i;
    printf("game %8s | %s \n",dr->name,dr->longname);
    for (i=0;i<SEC_MAX;i++) {
	LIST *l;
	printf("Section %d\n",i);
	for(l=dr->section[i].item;l;l=l->next) {
	    SECTION_ITEM *item=l->data;
	    printf("%8s %x %x\n",item->filename,item->begin,item->size);
	}
    }
}

SDL_bool dr_load_driver_dir(char *dirname) {
    DIR *pdir;
    struct stat buf;
    struct DIRENT *pfile;
    if (!(pdir=OPENDIR(dirname))) {
        printf("Couldn't find %s\n",dirname);
        return SDL_FALSE; 
    }
    while(pfile=READDIR(pdir)) {
        char *filename=alloca(strlen(pfile->d_name)+strlen(dirname)+2);
        sprintf(filename,"%s/%s",dirname,pfile->d_name);
        STAT(filename,&buf);
        if (S_ISREG(buf.st_mode)) {
            dr_load_driver(filename);
        }
    }
    CLOSEDIR(pdir);
    return SDL_TRUE;
}

/* load the specified file, and create the driver struct */
SDL_bool dr_load_driver(char *filename) {
    FILE *f;
    LIST *i;


    f=fopen(filename,"r");
    if (!f) {
	printf("Couldn't find %s\n",filename);
	return SDL_FALSE;
    }
    while(goto_next_driver(f)==SDL_TRUE) {
	add_driver(f);
    }

    //list_foreach(driver_list,print_driver);

    fclose(f);
    return SDL_TRUE;
}

void dr_list_all(void);
void dr_list_available(void);

static SDL_bool file_is_zip(char *name) {
    unzFile *gz;
    gz=unzOpen(name);
    if (gz!=NULL) {
	unzClose(gz);
	return SDL_TRUE;
    }
    return SDL_FALSE;
}

static void free_ziplist_item(void *data) {
    free(data);
}

/* check if the driver dr correspond to the zip file pointed by gz 
   (zip_list contain the zip file content)
*/
static SDL_bool check_driver_for_zip(DRIVER *dr,unzFile *gz,LIST *zip_list) {
    int i;
    LIST *l,*zl;

    for (i=0;i<SEC_MAX;i++) {
	//printf("Check section %d\n",i);
	for(l=dr->section[i].item;l;l=l->next) {
	    SECTION_ITEM *item=l->data;
	    if (strcmp(item->filename,"-")!=0) {
		for(zl=zip_list;zl;zl=zl->next) {
		    //printf("Check filename %s %s\n",(char*)zl->data,item->filename);
		    if (strcasecmp(item->filename,(char*)zl->data)==0) {
			//printf("filename %s=%s\n",(char*)zl->data,item->filename);
			break;
		    }
		}
		//printf("Zl %s = %p\n",item->filename,zl);
		if (zl==NULL)
		    return SDL_FALSE;
	    }
	   
	}
    }
    
    return SDL_TRUE;
}

char *get_zip_name(char *name) {
    char *zip;
#ifdef __QNXNTO__
    char *path = "shared/misc/gngeo/ROMS";
#else
    char *path = CF_STR(cf_get_item_by_name("rompath"));
#endif
    if (file_is_zip(name)) {
        zip=malloc(strlen(name)+1);
        strcpy(zip,name);
    } else {

		char* ext = strrchr(name, '.');
		if (ext && stricmp(ext, ".zip") == 0)
		{
			int len = strlen(path) + strlen(name) + 1;
			zip = malloc(len);
			sprintf(zip, "%s/%s", path, name);
		}
		else
		{
			int len = strlen(path) + strlen(name) + 6;
			zip = malloc(len);
			sprintf(zip,"%s/%s.zip",path,name);
		}
    }
    return zip;
}

/* return the correct driver for the zip file zip*/

DRIVER *get_driver_for_zip(char *zip) {
    unzFile *gz;
    int i;
    char zfilename[256];
    char *t;
    LIST *zip_list=NULL,*l,*zl;
    int res;

    /* first, we check if it a zip */
    gz = unzOpen(zip);
    if (gz==NULL) {
	return NULL;
    }
    //printf("Get driver for %s\n",zip);
    /* now, we create a list containing the content of the zip */
    i=0;
    unzGoToFirstFile(gz);
    do {
	unzGetCurrentFileInfo(gz,NULL,zfilename,256,NULL,0,NULL,0);
	//printf("List zip %s\n",zfilename);
	t=strrchr(zfilename,'.');
	if (! ( (strncasecmp(zfilename,"n",1)==0 && strlen(zfilename)<=12 )|| 
		(t && (strcasecmp(t,".rom")==0 || strcasecmp(t,".bin")==0) ) )
	    )
	    
	    i++;
	if (i>10) {
	    //printf("More than 10 file are not rom....\n");
	    /* more than 10 files are not rom.... must not be a valid romset 
	       10 files should be enough */
	    list_erase_all(zip_list,free_ziplist_item);
	    return NULL;
	}
	zip_list=list_prepend(zip_list,strdup(zfilename));
    } while (unzGoToNextFile(gz)!=UNZ_END_OF_LIST_OF_FILE);
    
    /* now we check every driver to see if it match the zip content */
    for (l=driver_list;l;l=l->next) {
	DRIVER *dr=l->data;
	if (check_driver_for_zip(dr,gz,zip_list)==SDL_TRUE) {
	    unzClose(gz);
	    list_erase_all(zip_list,free_ziplist_item);
	    return dr;
	}
    }
		
    list_erase_all(zip_list,free_ziplist_item);
    unzClose(gz);
    /* not match found */
    return NULL;
}

DRIVER *dr_get_by_name(char *name) {
    char *zip=get_zip_name(name);
    DRIVER *dr=get_driver_for_zip(zip);
    free(zip);
    return dr;
}

static int zfread(unzFile * f, void *buffer, int length)
{
    Uint8 *buf = (Uint8*)buffer;
    Uint8 *tempbuf;
    Uint32 totread, r, i;
    int totlength=length;
    totread = 0;
    tempbuf=alloca(4097);

    while (length) {

	r = length;
	if (r > 4096)
	    r = 4096;

	r = unzReadCurrentFile(f, tempbuf, r);
	if (r == 0) {
	    terminate_progress_bar();
	    return totread;
	}
	memcpy(buf, tempbuf, r);

	buf += r;
	totread += r;
	length -= r;
	update_progress_bar(totread,totlength);
    }

    terminate_progress_bar();
    return totread;
}

static int zfread_alternate(unzFile * f, void *buffer, int length, int inc)
{
    Uint8 *buf = buffer;
    Uint8 tempbuf[4096];
    Uint32 totread, r, i;
    int totlength=length;
    totread = 0;
    

    while (length) {

	r = length;
	if (r > 4096)
	    r = 4096;

	r = unzReadCurrentFile(f, tempbuf, r);
	if (r == 0) {
	    terminate_progress_bar();
	    return totread;
	}
	for (i = 0; i < r; i++) {
	    *buf = tempbuf[i];
	    buf += inc;
	}
	totread += r;
	length -= r;
	update_progress_bar(totread,totlength);
    }

    terminate_progress_bar();
    return totread;
}

SDL_bool dr_load_section(unzFile *gz, SECTION s, Uint8 *current_buf) {
    LIST *l;
    for(l=s.item;l;l=l->next) {
	SECTION_ITEM *item=l->data;
    
	if (strcmp(item->filename,"-")!=0) {
	    /* nouveau fichier, on l'ouvre */
	    if (unzLocateFile(gz, item->filename, 2) == UNZ_END_OF_LIST_OF_FILE) {
		unzClose(gz);
		return SDL_FALSE;
	    }
	    if (unzOpenCurrentFile(gz) != UNZ_OK) {
		unzClose(gz);
		return SDL_FALSE;
	    }
	}
	create_progress_bar(item->filename);
	if (item->type==LD_ALTERNATE)
	    zfread_alternate(gz, current_buf + item->begin, item->size, 2);
	else
	    zfread(gz, current_buf + item->begin, item->size);
    }
    return SDL_TRUE;
}

void set_bankswitchers(BANKSW_TYPE bt) {
    switch(bt) {
    case BANKSW_NORMAL:
	mem68k_fetch_bksw_byte=mem68k_fetch_bk_normal_byte;
	mem68k_fetch_bksw_word=mem68k_fetch_bk_normal_word;
	mem68k_fetch_bksw_long=mem68k_fetch_bk_normal_long;
	mem68k_store_bksw_byte=mem68k_store_bk_normal_byte;
	mem68k_store_bksw_word=mem68k_store_bk_normal_word;
	mem68k_store_bksw_long=mem68k_store_bk_normal_long;
	break;
    case BANKSW_KOF2003:
	mem68k_fetch_bksw_byte=mem68k_fetch_bk_kof2003_byte;
	mem68k_fetch_bksw_word=mem68k_fetch_bk_kof2003_word;
	mem68k_fetch_bksw_long=mem68k_fetch_bk_kof2003_long;
	mem68k_store_bksw_byte=mem68k_store_bk_kof2003_byte;
	mem68k_store_bksw_word=mem68k_store_bk_kof2003_word;
	mem68k_store_bksw_long=mem68k_store_bk_kof2003_long;
	break;
    }
}
SDL_bool dr_load_game(DRIVER *dr,char *name) {
    unzFile *gz;
    int i;
    LIST *l;
    char *zip=get_zip_name(name);
    gz = unzOpen(zip);
    free(zip);
    if (gz==NULL) {
	return SDL_FALSE;
    }
    if (dr->special_bios) {
	memory.bios=malloc(dr->bios.size);
	memory.bios_size=dr->bios.size;
	if (!dr_load_section(gz,dr->bios,memory.bios)) return SDL_FALSE;
    }
    for (i=0;i<SEC_MAX;i++) {
	int s=dr->section[i].size;
	Uint8 *current_buf=NULL;
	//if (dr->section[i].item==NULL) continue;
	if (s==0) continue;
	//      printf("%p %d \n",dr->section[i].item,i);
	switch (i) {
	case SEC_CPU:
	    memory.cpu = malloc(s);
	    current_buf = memory.cpu;
	    memory.cpu_size = s;
	    break;
	case SEC_SFIX:
	    memory.sfix_game = malloc(s);
	    memory.fix_game_usage = malloc(s >> 5);
	    current_buf = memory.sfix_game;
	    memory.sfix_size = s;
	    break;
	case SEC_SM1:
	    memory.sm1 = malloc(s);
	    current_buf = memory.sm1;
	    memory.sm1_size = s;
	    break;
	case SEC_SOUND1:
	    memory.sound1 = malloc(s);
	    memory.sound1_size = s;
	    current_buf = memory.sound1;
	    break;
	case SEC_SOUND2:
	    memory.sound2 = malloc(s);
	    memory.sound2_size = s;
	    current_buf = memory.sound2;
	    break;
	case SEC_GFX:
	    memory.gfx = malloc(s);
	    //printf("Alloc %x for GFX: %p\n",s,memory.gfx);
	    memory.gfx_size = s;
	    current_buf = memory.gfx;
	    memory.pen_usage = malloc((s >> 7) * sizeof(int));
	    memset(memory.pen_usage, 0, (s >> 7) * sizeof(int));
	    memory.nb_of_tiles = s >> 7;
	    break;
	    /* TODO: Crypted rom */
	default:
	    break;
	}
	if (!dr_load_section(gz,dr->section[i],current_buf)) return SDL_FALSE;
    }
    unzClose(gz);

    /* TODO: Use directly the driver value insteed of recopying them */
    conf.game=dr->name;
    conf.rom_type=dr->rom_type;
    conf.special_bios=dr->special_bios;
    conf.extra_xor=dr->xor;
    set_bankswitchers(dr->banksw_type);
    for(i=0;i<6;i++)
	memory.bksw_unscramble[i]=dr->banksw_unscramble[i];
    for(i=0;i<64;i++)
	memory.bksw_offset[i]=dr->banksw_off[i];


    if (conf.rom_type == MGD2) {
	create_progress_bar("Convert MGD2");
	convert_mgd2_tiles(memory.gfx, memory.gfx_size);
	convert_mgd2_tiles(memory.gfx, memory.gfx_size);
	terminate_progress_bar();
    }
    if (conf.rom_type == MVS_CMC42) {
	create_progress_bar("Decrypt GFX ");
	kof99_neogeo_gfx_decrypt(conf.extra_xor);
	terminate_progress_bar();
    }
    if (conf.rom_type == MVS_CMC50) {
	create_progress_bar("Decrypt GFX ");
	kof2000_neogeo_gfx_decrypt(conf.extra_xor);
	terminate_progress_bar();
    }

    convert_all_char(memory.sfix_game, memory.sfix_size,
		     memory.fix_game_usage);


    if (CF_BOOL(cf_get_item_by_name("convtile"))) {
	create_progress_bar("Convert tile");
	for (i = 0; i < memory.nb_of_tiles; i++) {
	    convert_tile(i);
	    if (i%100==0) update_progress_bar(i,memory.nb_of_tiles);
	}
	terminate_progress_bar();
    }

    if (memory.sound2 == NULL) {
	memory.sound2 = memory.sound1;
	memory.sound2_size = memory.sound1_size;
    }
    //backup neogeo game vectors
    memcpy(memory.game_vector,memory.cpu,0x80);
    printf("                                                                             \r");

    return SDL_TRUE;
}

static int cmp_driver(void *a,void *b) {
    DRIVER *da=a;
    DRIVER *db=b;
    return strcmp(da->name,db->name);
}

void dr_list_all(void) {
    LIST *l;
    LIST *t=NULL;
    for(l=driver_list;l;l=l->next) {
	t=list_insert_sort_unique(t,l->data,cmp_driver);
    }
    for(l=t;l;l=l->next) {
	DRIVER *dr=l->data;
	printf("%-8s : %s\n",dr->name,dr->longname);
    }
}

LIST *driver_get_all(void) {
    return driver_list;
}
