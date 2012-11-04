#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#define OPENDIR opendir
#define DIRENT dirent
#define READDIR readdir
#define CLOSEDIR closedir
#define MAKEDIR mkdir
#define SLEEP usleep
#define STAT stat

#define DATA_DIRECTORY "shared/misc/gngeo/ROMS/"
#define VERSION "1.0.0"

#define HAVE_SCANDIR
