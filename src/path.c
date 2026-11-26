/* path.c - a file's path on this platform (dgk/path.h). */
#include "dgk/path.h"
#include "dgk/base.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *dirs[2] = { ".", "." };

void dgk_path_dir(int kind, const char *dir)
{
    if (kind == DGK_DATA || kind == DGK_USER)
        dirs[kind] = dir && *dir ? dir : ".";
}

const char *dgk_path(int kind, const char *name)
{
    char *p;
#ifdef DGK_DOS
    DGK_UNUSED(kind);
    p = malloc(strlen(name) + 1);
    if (p)
        strcpy(p, name);
#else
    const char *env = getenv(kind == DGK_USER ? "DGK_USER_DIR" : "DGK_DATA_DIR");
    const char *dir = env && *env ? env : dirs[kind == DGK_USER];
    size_t n = strlen(dir), i;
    p = malloc(n + 1 + strlen(name) + 1);
    if (p) {
        strcpy(p, dir);
        p[n] = '/';
        for (i = 0; name[i]; i++)
            p[n + 1 + i] = kind == DGK_USER ? (char)tolower((unsigned char)name[i]) : name[i];
        p[n + 1 + i] = 0;
        if (!strcmp(dir, "."))                  /* (the current directory: the name alone) */
            memmove(p, p + 2, strlen(p + 2) + 1);
    }
#endif
    return p ? p : name;
}
