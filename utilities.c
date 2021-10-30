#include "utilities.h"

bool path_exits(const char *path)
{
    return access(path, F_OK) == 0;
}

bool path_is_dir(const char *path)
{
    struct stat _s = (struct stat){};
    return stat(path, &_s) == 0 && _s.st_mode & S_IFDIR;
}

const char *check_paths(const char *paths[], int len)
{
    while(len-- > 0)
        if(path_exits(paths[len])) return paths[len];
    return NULL;
}