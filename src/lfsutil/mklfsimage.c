/*  mklfsimage -- Create a LittleFS by waking a directory tree
    Written 2021 by Eric Olson

    Modified by Memotech-Bill to allow specifying filesystem size.
    October 17, 2024.

    This is free software released under the exact same terms as
    stated in license.txt for the Basic interpreter.  */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "lfsmcu.h"

static char* imagefn = "filesystem.lfs";
static char* dprefix = "/";

static void help() {
    printf("Usage: mklfsimage [options] <directories>...\n"
           "where\n"
           "\t -o s      -- Write image to file s       (%s)\n"
           "\t -p s      -- Specify directory prefix s  (%s)\n"
           "\t -s n[K|M] -- Specify filesystem size     (%dK)\n"
           "\t -h        -- Print this help message\n",
        imagefn, dprefix, ROOT_SIZE / 1024);
    exit(0);
}

lfs_t lfs_root;
lfs_bbc_t lfs_root_context;
static struct lfs_config lfs_root_cfg = {.context = &lfs_root_context,
                                         .read = lfs_bbc_read,
                                         .prog = lfs_bbc_prog,
                                         .erase = lfs_bbc_erase,
                                         .sync = lfs_bbc_sync,
                                         .read_size = 1,
                                         .prog_size = 256,
                                         .block_size = 4096,
                                         .block_count = ROOT_SIZE / 4096,
                                         .cache_size = 256,
                                         .lookahead_size = 32,
                                         .block_cycles = 256};

static char cbuf[4096];
static void docopy(char* fn, char* dest) {
    FILE* fp = fopen(fn, "rb");
    if (!fp) {
        fprintf(stderr, "Unable to open %s for read!\n", fn);
        exit(1);
    }
    lfs_file_t file;
    char path[260];
    sprintf(path, "%s%s", dest, fn);
    if (lfs_file_open(&lfs_root, &file, path, LFS_O_CREAT | LFS_O_RDWR) < 0) {
        fprintf(stderr, "Can't create %s in %s!\n", path, imagefn);
        exit(1);
    }
    for (;;) {
        int r = fread(cbuf, 1, sizeof(cbuf), fp);
        if (r == 0)
            break;
        int s = lfs_file_write(&lfs_root, &file, cbuf, r);
        if (s != r) {
            fprintf(stderr, "Wrote %d of %d bytes of %s!\n", s, r, path);
            exit(1);
        }
    }
    if (lfs_file_close(&lfs_root, &file) < 0) {
        fprintf(stderr, "Unable to close %s in %s!\n", path, imagefn);
        exit(1);
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "Unable to close %s!\n", fn);
        exit(1);
    }
}
static void mkpath(char* path) {
    char travel[260], *tp = travel;
    bzero(travel, sizeof(travel));
    for (;;) {
        if (*path == 0)
            break;
        if (*path == '/' && tp > travel) {
            // ignore errors when path exists
            lfs_mkdir(&lfs_root, travel);
        }
        *tp++ = *path++;
    }
}

static void dowork(char* dirfn, char* curpath) {
    printf("Adding files in %s to %s...\n", dirfn, imagefn);
    DIR* dirp = opendir(dirfn);
    if (dirp == 0) {
        fprintf(stderr, "Unable to to open directory %s!\n", dirfn);
        exit(1);
    }
    if (chdir(dirfn) < 0) {
        fprintf(stderr, "Unable to change to directory %s!\n", dirfn);
        exit(1);
    }
    mkpath(curpath);
    for (;;) {
        struct dirent* entp = readdir(dirp);
        if (entp == 0)
            break;
        if (entp->d_type == DT_REG) {
            printf("\t%s->%s%s\n", entp->d_name, curpath, entp->d_name);
            docopy(entp->d_name, curpath);
        } else if (entp->d_type == DT_DIR) {
            if (entp->d_name[0] == '.')
                continue;
            char newpath[260];
            sprintf(newpath, "%s%s/", curpath, entp->d_name);
            dowork(entp->d_name, newpath);
        }
    }
    if (chdir("..") < 0) {
        fprintf(stderr, "Unable to leave directory %s!\n", dirfn);
        exit(1);
    }
}

uint32_t parse_size (const char *ps)
    {
    uint32_t addr = 0;
    while (*ps)
        {
        if ((*ps >= '0') && (*ps <= '9')) addr = 10 * addr + *ps - '0';
        else break;
        ++ps;
        }
    if ((*ps == 'K') || (*ps == 'k')) {addr *= 1024; ++ps;}
    else if ((*ps == 'M') || (*ps == 'm')) {addr *= 1024 * 1024; ++ps;}
    if ((*ps) || (addr == 0))
        {
        fprintf (stderr, "Invalid filesystem size\n");
        exit (1);
        }
    return  addr;
    }

int main(int argc, char* argv[]) {
    printf("mklfsimage--Make a LittleFS image from a directory tree\n"
           "Version 1.0 written August 2, 2021 by Eric Olson\n"
           "Version 1.1 written October 17, 2024 by Memotech-Bill\n\n");
    int opt;
    uint32_t root_size = ROOT_SIZE;
    while ((opt = getopt(argc, argv, "o:p:s:h")) != -1) {
        switch (opt) {
        case 'o':
            imagefn = optarg;
            break;
        case 'p':
            dprefix = optarg;
            break;
        case 's':
            root_size = parse_size (optarg);
            break;
        default:
            help();
        }
    }
    if (optind >= argc)
        help();
    char curpath[260];
    if (dprefix[0] == 0)
        strcpy(curpath, "/");
    else {
        char* p = dprefix;
        while (*p)
            p++;
        p--;
        sprintf(curpath, "%s%s%s", dprefix[0] == '/' ? "" : "/", dprefix, *p == '/' ? "" : "/");
    }
    struct lfs_bbc_config lfs_bbc_cfg = {.buffer = malloc(root_size)};
    lfs_root_cfg.block_count = root_size / lfs_root_cfg.block_size;
    lfs_bbc_createcfg(&lfs_root_cfg, &lfs_bbc_cfg);
    {
        FILE* root_fp = fopen(imagefn, "rb");
        if (root_fp) {
            fread(lfs_bbc_cfg.buffer, root_size, 1, root_fp);
            fclose(root_fp);
        } else {
            printf("Formatting filesystem...%dKB\n", root_size / 1024);
            memset (lfs_bbc_cfg.buffer, 0xFF, root_size);
            lfs_format(&lfs_root, &lfs_root_cfg);
        }
    }
    FILE* root_fp = fopen(imagefn, "wb");
    if (!root_fp) {
        fprintf(stderr, "Error opening %s for write!\n", imagefn);
        exit(1);
    }
    int lfs_err = lfs_mount(&lfs_root, &lfs_root_cfg);
    if (lfs_err) {
        lfs_format(&lfs_root, &lfs_root_cfg);
        lfs_err = lfs_mount(&lfs_root, &lfs_root_cfg);
        if (lfs_err) {
            fprintf(stderr, "Unable to format littlefs\n");
            exit(1);
        }
    }
    for (int i = optind; i < argc; i++) {
        dowork(argv[i], curpath);
    }
    fwrite(lfs_root_context.buffer, root_size, 1, root_fp);
    fclose(root_fp);
    return 0;
}
