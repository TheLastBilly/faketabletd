#ifndef FAKETABLETD_INI_H__
#define FAKETABLETD_INI_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define INI_BUFFER_SIZE 20
#define INI_STRING_SIZE 20

#define INI_TYPE_INT    0
#define INI_TYPE_FLOAT  1
#define INI_TYPE_STRING 2

struct ini_item_t
{
    int         type;
    char        label[INI_STRING_SIZE];
    union
    {
        long    integer;
        float   floating;
        char    string[INI_STRING_SIZE];

        void    *_generic;
    };

    void        *_data;
};

extern struct ini_item_t ini_items_[INI_BUFFER_SIZE];

#define ini_get_items()             (ini_items_)
#define ini_get_item(index, type)   (*((type*)ini_get_item_(index)))

void ini_clear_items();
int ini_clear_item(int index);

int ini_register_item(int index, int type, const char *label);
void *ini_get_item_(int index);
bool ini_item_is_populated(int index);

int ini_parse_file(const char *file_path);

#endif