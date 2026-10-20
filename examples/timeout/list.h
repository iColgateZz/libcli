#ifndef TIMEOUT_LIST_H
#define TIMEOUT_LIST_H

#include <stdbool.h>
#include <stddef.h>

#define LIST_DEFINE(Type, Name)                                                                                        \
    typedef struct {                                                                                                   \
        Type *items;                                                                                                   \
        size_t size;                                                                                                   \
        size_t capacity;                                                                                               \
    } Name

#define list_append(list, item)                                                                                        \
    ((list)->size < (list)->capacity ? ((list)->items[(list)->size++] = (item), true) : false)

#endif
