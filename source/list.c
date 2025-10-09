#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "list.h"
#include "util.h"

List *list_new(int init_size) {
    List *list = (List *)malloc(sizeof(List));
    if (list == NULL) return NULL;
    list->_array = (void **)malloc(sizeof(void*) * init_size);
    if (!list->_array) 
    {
        free(list);
        return NULL;
    }
    list->size = 0;
    list->allocated_size = init_size;
    return list;
}

void list_destroy(List **list) {
    if (list == NULL || *list == NULL)
        return; 
    {
        free((*list)->_array);
        free(*list);
    }

    *list = NULL;
}

bool int_list_append(List *list, intptr_t value) 
{
    return list_append(list, (void*)value);
}

bool list_append(List *list, void *value)
{
    if (list->size >= list->allocated_size) 
    {
        int new_size = list->allocated_size * 2;
        void **new_arr = (void **)realloc(list->_array, sizeof(void*) * new_size);
        if (new_arr == NULL) 
            return false;
        list->_array = new_arr;
        list->allocated_size = new_size;
    }

    list->_array[list->size++] = value;
    return true;
}

bool list_remove_by_idx(List *list, int index) {
    if (index < 0 || index >= list->size) 
        return false;
    for (int i = index; i < list->size - 1; ++i) 
    {
        list->_array[i] = list->_array[i + 1];
    }
    list->size--;
    return true;
}

bool list_remove_by_value(List *list, void* value)
{
    for (int i = 0; i < list->size; i++)
    {
        if (list->_array[i] == value)
        {
            return list_remove_by_idx(list, i);
        }
    }

    return false;
}

bool int_list_remove_by_value(List *list, intptr_t value)
{
    return list_remove_by_value(list, (void*)value);
}

void* list_get(List *list, int index)
{
    if (index < 0 || index >= list->size) 
        return NULL;
    return list->_array[index];
}

intptr_t int_list_get(List *list, int index) 
{
    return (intptr_t)list_get(list, index);
}

int list_get_size(List *list)
{
    if (list == NULL)
    {
        return UNDEFINED;
    }
    return list->size;
}

bool list_append_new(ListNode* p_a, ListNode* p_b)
{
    return false;
}

#include "pool.h"
int list_push_front(ListHead *H, int elem_idx)
{
    LinkNode *N = POOL_GET(LinkNode);
    N->elem_idx = elem_idx;
    N->prev = -1;
    N->next = H->head;
    int n = POOL_IDX(LinkNode, N); // Get the index of of the node in the pool
    if (H->head != -1)
    {
        POOL_AT(LinkNode, H->head)->prev = n; // Get the object at the index H->head
    }
    H->head = n;
    return n;
}

void list_remove(ListHead *H, LinkNode *N)
{
    LinkNode* p_prev_node = NULL;
    LinkNode* p_next_node = NULL;

    if(N->prev >= 0)
    {
        p_prev_node = POOL_AT(LinkNode, N->prev);
    }

    if(N->next >= 0)
    {
        p_next_node = POOL_AT(LinkNode, N->next);
    }

    if(p_prev_node && !p_next_node) // end of list
    {
        p_prev_node->next = -1;
    }
    else if(p_prev_node && p_next_node) // somewhere in between
    {
        p_prev_node->next = POOL_IDX(LinkNode, p_next_node);
        p_next_node->prev = POOL_IDX(LinkNode, p_prev_node);
    }
    else if(p_next_node && !p_prev_node) // beginning of list
    {
        p_next_node->prev = -1;
        H->head = POOL_IDX(LinkNode, p_next_node);
    }
    else if(!p_prev_node && !p_next_node)
    {
        H->head = -1;
    }

    POOL_FREE(LinkNode, N);
}

void list_remove_idx(ListHead* H, int elem_idx) {
    int len = 0;
    LinkNode* current_node = (H->head >= 0) ?
        POOL_AT(LinkNode, H->head) :
        NULL;
    while (current_node != NULL)
    {
        if(elem_idx == len++)
        {
            list_remove(H, current_node);
            return;
        };
        current_node = (current_node->next >= 0) ?
                        POOL_AT(LinkNode, current_node->next) :
                        NULL;
    }
}

// usage: list_push_front(&jokers, POOL_IDX(Joker, myjoker));

int list_size_new(ListHead H)
{
    int len = 0;
    LinkNode* current_node = (H.head >= 0) ?
        POOL_AT(LinkNode, H.head) :
        NULL;
    while (current_node != NULL)
    {
        len++;
        current_node = (current_node->next >= 0) ?
                        POOL_AT(LinkNode, current_node->next) :
                        NULL;
    }
    return len;
}

int list_get_new(ListHead H, int elem_idx)
{
    int len = 0;
    LinkNode* current_node = (H.head >= 0) ?
        POOL_AT(LinkNode, H.head) :
        NULL;
    while (current_node != NULL)
    {
        if(elem_idx == len++) return current_node->elem_idx;
        current_node = (current_node->next >= 0) ?
                        POOL_AT(LinkNode, current_node->next) :
                        NULL;
    }
    return -1;
}
