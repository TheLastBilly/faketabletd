#ifndef FAKETABLETD_INI_H__
#define FAKETABLETD_INI_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INI_BUFFER_SIZE 10

#define INI_TYPE_INT    0
#define INI_TYPE_FLOAT  1
#define INI_TYPE_STRING 2

struct ini_item_t
{
    int         type;
    char        label[20];
    union
    {
        int     integer;
        float   floating;
        char    string[20];

        void    *_generic;
    };

    void        *_data;
};

extern struct ini_item_t ini_items_[INI_BUFFER_SIZE];

#define ini_get_items()             (ini_items_)
#define ini_get_item(index, type)   (*((type*)ini_get_item_(index)))

void ini_clear_items();

int ini_register_item(int index, int type, const char *label);
void *ini_get_item_(int index);
int ini_clear_item(int index);

int ini_parse_file(const char *file_path);

#endif