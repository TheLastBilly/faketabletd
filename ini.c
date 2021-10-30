#include <stdbool.h>
#include <errno.h>

#include "ini.h"
#include "utilities.h"

#define CHECK_IF_OUT_OF_BOUNDS(index_, rtn_)                                    \
{                                                                               \
    if(index_ >= sizeof(ini_items_)/sizeof(struct ini_item_t) || index_ < 0)    \
    {                                                                           \
        __ERROR("index out of bounds");                                         \
        return rtn_;                                                            \
    }                                                                           \
}
#define BAD_TYPE_ERROR()                                                        \
{                                                                               \
    __ERROR("unspecified type");                                                \
    return -1;                                                                  \
}
#define APPLY_TO_STATIC_STRING(dsrc_, ssrc_)                                    \
{                                                                               \
    uint s_ = MIN(GET_LEN(dsrc_), strlen(ssrc_));                               \
    memset(dsrc_, 0, sizeof(dsrc_));                                            \
    memcpy(dsrc_, ssrc_, s_);                                                   \
}


struct ini_item_t ini_items_[] = {};

static bool string_is_number(const char *str)
{
    size_t size = strlen(str);
    while(size-- > 0)
    {
        switch (str[size])
        {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '.':
        case ' ':
            continue;
        default:
            return false;
        }
    }

    return true;
}

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
    struct ini_item_t *item = &ini_items_[index];
    
    if(item->_data == NULL)
        return NULL;
    
    switch (item->type)
    {
    case INI_TYPE_INT:
    case INI_TYPE_FLOAT:
        return item->_data;
    case INI_TYPE_STRING:
        return &item->_data;
    default:
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

static bool parse_entry(const char *label, const char *value)
{
    struct ini_item_t *item = NULL;

    for(uint i = 0; i < GET_LEN(ini_items_); i++)
    {
        item = &ini_items_[i];
        if(strcmp(label, item->label) == 0) break;
        item = NULL;
    }

    if(item == NULL)
    {
        __ERROR("invalid label \"%s\"", label);
        return false;
    }
    if(item->type != INI_TYPE_STRING && !string_is_number(value))
    {
        __ERROR("value \"%s\" of label \"%s\" is not a number", value, label);
        return false;
    }

    item->_data = &item->_generic;
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
        __ERROR("unidentified type on label \"%s\"", label);
        return false;
    }

    return true;
}

int ini_parse_file(const char *file_path)
{
    FILE *fp = NULL;
    char line[100] = {0};
    char *label = NULL, *value = NULL;
    int start = 0, end = 0, line_number = 0, ret = 0;

    if(file_path == NULL)
    {
        __ERROR("cannot register item with empty label");
        return -1;
    }

    if((fp = fopen(file_path, "r")) == NULL)
    {
        __ERROR("cannot open \"%s\"", file_path);
        return -1;
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
        __WARNING("invalid entry on %s:%d",file_path, line_number);     \
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

        if(!parse_entry(label, value))
        {
            ret = -1;
            break;
        }
        start = end = 0;
        label = value = NULL;
        
        line_number++;
    }
#undef FIND_COMPONENT

    fclose(fp);
    return ret;
}

bool ini_item_is_populated(int index)
{
    CHECK_IF_OUT_OF_BOUNDS(index, false);
    return ini_items_[index]._data != NULL;
}