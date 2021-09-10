#include <stdbool.h>
#include <errno.h>

#include "ini.h"
#include "utilities.h"

#define CHECK_IF_OUT_OF_BOUNDS(index_, rtn_)                        \
{                                                                   \
    if(index_ >= sizeof(ini_items_)/sizeof(struct ini_item_t))      \
    {                                                               \
        __ERROR("index out of bounds");                             \
        return rtn_;                                                \
    }                                                               \
}
#define BAD_TYPE_ERROR()                                            \
{                                                                   \
    __ERROR("unspecified type");                                    \
    return -1;                                                      \
}
#define APPLY_TO_STATIC_STRING(ssrc_, dsrc_)                        \
{                                                                   \
    uint s_ = MIN(GET_LEN(ssrc_), strlen(dsrc_));                   \
    memcpy(ssrc_, dsrc_, s_);                                       \
}

struct ini_item_t ini_items_[] = {};

void ini_clear_items()
{
    memset(ini_items_, 0, INI_BUFFER_SIZE * sizeof(struct ini_item_t));
}

int ini_register_item(int index, int type, const char *label)
{
    struct ini_item_t *item = NULL;

    CHECK_IF_OUT_OF_BOUNDS(index, EINVAL);
    if(label == NULL)
    {
        __ERROR("cannot register item with empty label");
        return EINVAL;
    }
    
    item = &ini_items_[index];
    
    switch (type)
    {
    case INI_TYPE_INT:
    case INI_TYPE_FLOAT:
    case INI_TYPE_STRING:
        item->type = type;
        break;
    
    default:
        BAD_TYPE_ERROR();
    }

    APPLY_TO_STATIC_STRING(item->label, label);
    return 0;
}
void *ini_get_item_(int index)
{
    CHECK_IF_OUT_OF_BOUNDS(index, NULL);

    switch (ini_items_[index].type)
    {
    case INI_TYPE_INT:
        return (void *)&ini_items_[index].integer;
    case INI_TYPE_FLOAT:
        return (void *)&ini_items_[index].floating;
    case INI_TYPE_STRING:
        return (void *)&ini_items_[index].string;
    default:
        return NULL;
        break;
    }

    return NULL;
}
int ini_clear_item(int index)
{
    CHECK_IF_OUT_OF_BOUNDS(index, -1);
    
    memset(&ini_items_[index], 0, sizeof(struct ini_item_t));
    return 0;
}

static int parse_entry(const char *label, const char *value)
{
    struct ini_item_t *item = NULL;

    for(uint i = 0; i < GET_LEN(ini_items_); i++)
    {
        item = &ini_items_[i];
        if(strcmp(label, item->label) == 0) break;
        item = NULL;
    }

    if(item == NULL)
        return -1;

    switch (item->type)
    {
    case INI_TYPE_INT:
        item->integer = strtol(value, NULL, 10);
        break;
    case INI_TYPE_FLOAT:
        item->integer = strtof(value, NULL);
        break;
    case INI_TYPE_STRING:
        APPLY_TO_STATIC_STRING(item->string, value);
        break;
    default:
        return -1;
        break;
    }

    return 0;
}

int ini_parse_file(const char *file_path)
{
    FILE *fp = NULL;
    char line[100] = {0};
    char *label = NULL, *value = NULL;
    int start = 0, end = 0, line_number = 0;

    if(file_path == NULL)
    {
        __ERROR("cannot register item with empty label");
        return EINVAL;
    }

    if((fp = fopen(file_path, "r")) == NULL)
    {
        __ERROR("cannot open \"%s\"", file_path);
        return ENOENT;
    }

#define FIND_COMPONENT()                                                \
    while(start < sizeof(line))                                         \
        if(line[start] != ' ') break;                                   \
        else start++;                                                   \
    end = start;                                                        \
    do                                                                  \
    {                                                                   \
        end++;                                                          \
        if(line[end] == ' ')                                            \
        {                                                               \
            line[end] = '\0';                                           \
            continue;                                                   \
        }                                                               \
        if(line[end] == '\n' || line[end] == '=')                       \
        {                                                               \
            line[end] = '\0';                                           \
            break;                                                      \
        }                                                               \
    } while(end < sizeof(line));                                        \
                                                                        \
    if(end >= sizeof(line))                                             \
    {                                                                   \
        __WARNING("invalid entry %s:%d",file_path, line_number);        \
        continue;                                                       \
    }

    while(fgets(line, sizeof(line), fp))
    {
        // Get label
        FIND_COMPONENT();
        label = &line[start];
        start = end +1;

        // Get value
        FIND_COMPONENT();
        value = &line[start];
        start = end = 0;

        parse_entry(label, value);

        line_number++;
    }

#undef FIND_COMPONENT
    fclose(fp);
    return 0;
}