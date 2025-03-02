#ifndef LFSWRAP_H
#define LFSWRAP_H

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_FAT
#include <../fatfs/ff.h>
#else
#define DIR void
#endif
#define NAME_MAX 256
struct dirent
    {
    char d_name[NAME_MAX+1];
    };

extern char *myrealpath(const char *restrict p, char *restrict r);
extern int mychdir(const char *p);
extern int mymkdir(const char *p, mode_t m);
extern int myrmdir(const char *p);
extern int mychmod(const char *p, mode_t m);
extern char *mygetcwd(char *b, size_t s);
extern int myusleep(useconds_t usec);
extern unsigned int mysleep(unsigned int seconds);
extern long myftell(FILE *fp);
extern int myfseek(FILE *fp, long offset, int whence);
extern int myfextent(FILE *fp, long offset);
extern FILE *myfopen(const char *pathname, char *mode);
extern int myfclose(FILE *fp);
extern size_t myfread(void *ptr, size_t size, size_t nmemb, FILE *fp);
extern size_t myfwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp);
extern DIR *myopendir(const char *name);
extern struct dirent *myreaddir(DIR *dirp);
extern int myclosedir(DIR *dirp);
extern int myrename (const char *old, const char *new);
extern int mount(char *psMsg);

#define realpath myrealpath
#define chdir mychdir
#define mkdir mymkdir
#define rmdir myrmdir
#define remove myrmdir
#define chmod mychmod
#define getcwd mygetcwd
#define usleep myusleep
#define sleep mysleep
#define fclose myfclose
#define fopen myfopen
#define fread myfread
#define fwrite myfwrite
#define fseek myfseek
#define ftell myftell
#define fextent myfextent
#define opendir myopendir
#define readdir myreaddir
#define closedir myclosedir
#define rename myrename

#endif
