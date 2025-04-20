/* List directory of an LFS image */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "lfs.h"
#include "bd/lfs_filebd.h"

typedef struct s_dir_item
    {
    struct s_dir_item*  next;
    char                path[LFS_NAME_MAX+1];
    } DIR_ITEM;


int main (int nArg, const char *psArg[])
    {
    char sPath[512];
    char sBuff[512];
    FILE *fIn;
    FILE *fOut;
    if (nArg < 2)
        {
        fprintf (stderr, "Usage: %s <uf2 file> [<lfs dir> [<export dir>]]\n", psArg[0]);
        }
    fIn = fopen (psArg[1], "rb");
    if (fIn == NULL)
        {
        fprintf (stderr, "Failed to open UF2 file: %s\n", psArg[1]);
        return 1;
        }
    uint32_t head[8];
    fread (head, sizeof (head), 1, fIn);
    printf ("LFS version = %d.%d, sector size = %d, block count = %d\n",
        head[5] >> 16, head[5] & 0xFFFF, head[6], head[7]);
    fclose (fIn);
    
    struct lfs_filebd_config fcfg;
    fcfg.read_size = head[6];
    fcfg.prog_size = head[6];
    fcfg.erase_size = head[6];
    fcfg.erase_count = head[7];

    lfs_filebd_t fbd;

    struct lfs_config lfscfg;
    memset (&lfscfg, 0, sizeof (lfscfg));
    lfscfg.context = (void *) &fbd;
    lfscfg.read = lfs_filebd_read;
    lfscfg.prog = lfs_filebd_prog;
    lfscfg.erase = lfs_filebd_erase;
    lfscfg.sync = lfs_filebd_sync;
    lfscfg.read_size = head[6];
    lfscfg.prog_size = head[6];
    lfscfg.block_size = head[6];
    lfscfg.block_count = head[7];
    lfscfg.block_cycles = -1;
    lfscfg.cache_size = head[6];
    lfscfg.lookahead_size = 32;

    lfs_t lfs;

    lfs_filebd_create (&lfscfg, psArg[1], &fcfg);
    lfs_mount (&lfs, &lfscfg);

    lfs_dir_t dir;
    struct lfs_info info;
    DIR_ITEM* dir_bot = (DIR_ITEM*) malloc (sizeof (DIR_ITEM));
    DIR_ITEM* dir_top = dir_bot;
    if (nArg > 2) strcpy (dir_bot->path, psArg[2]);
    else strcpy (dir_bot->path, "/");
    dir_bot->next = NULL;
    bool bNewDir = true;

    while (dir_bot != NULL)
        {
        printf ("%s\n", dir_bot->path);
        lfs_dir_open (&lfs, &dir, dir_bot->path);
        while (lfs_dir_read (&lfs, &dir, &info) > 0)
            {
            if (info.type == LFS_TYPE_DIR)
                {
                if (info.name[0] != '.')
                    {
                    dir_top->next = (DIR_ITEM*) malloc (sizeof (DIR_ITEM));
                    dir_top = dir_top->next;
                    if (dir_bot->path[1] == '\0') sprintf (dir_top->path, "/%s", info.name);
                    else sprintf (dir_top->path, "%s/%s", dir_bot->path, info.name);
                    dir_top->next = NULL;
                    }
                }
            else
                {
                printf ("%8d %s\n", info.size, info.name);
                if (nArg > 3)
                    {
                    if (bNewDir && (dir_bot->path[1] != '\0'))
                        {
                        sprintf (sPath, "%s%s", psArg[3], dir_bot->path);
                        mkdir (sPath, 0770);
                        bNewDir = false;
                        }
                    if (dir_bot->path[1] == '\0') sprintf (sPath, "/%s", info.name);
                    else sprintf (sPath, "%s/%s", dir_bot->path, info.name);
                    lfs_file_t flfs;
                    lfs_file_open (&lfs, &flfs, sPath, LFS_O_RDONLY);
                    if (dir_bot->path[1] == '\0') sprintf (sPath, "%s/%s", psArg[3], info.name);
                    else sprintf (sPath, "%s/%s/%s", psArg[3], dir_bot->path, info.name);
                    fOut = fopen (sPath, "wb");
                    int nLen;
                    while ((nLen = lfs_file_read (&lfs, &flfs, sBuff, sizeof (sBuff))) > 0)
                        {
                        fwrite (sBuff, 1, nLen, fOut);
                        }
                    fclose (fOut);
                    lfs_file_close (&lfs, &flfs);
                    }
                }
            }
        lfs_dir_close (&lfs, &dir);
        DIR_ITEM* dir_new = dir_bot->next;
        free (dir_bot);
        dir_bot = dir_new;
        bNewDir = true;
        }
    
    lfs_unmount (&lfs);
    lfs_filebd_destroy (&lfscfg);
    
    return 0;
    }
