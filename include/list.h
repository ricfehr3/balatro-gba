#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct List
{
    void** _array;
    int size;
    int allocated_size;
} List;

typedef struct ListNode
{
    int next; // -1, end
    int prev; // -1, beginning
} ListNode;

List *list_new(int init_size);
void list_destroy(List **list);
bool list_append(List *list, void *value);
bool list_remove_by_idx(List *list, int index);
void* list_get(List *list, int index);
int list_get_size(List *list);
bool list_remove_by_value(List *list, void *value);

bool int_list_append(List *list, intptr_t value);
intptr_t int_list_get(List *list, int index);
bool int_list_remove_by_value(List *list, intptr_t value);

bool list_append_new(ListNode* p_a, ListNode* p_b);

typedef struct
{
    int prev;       // link-node idx, or -1 if at tail of list
    int next;       // link-node idx, or -1 if at head of list
    int elem_idx;   // index into T_pool
} LinkNode;

typedef struct
{
    int head; // link-node idx or -1, or -1 if nothing in list
} ListHead;

int list_push_front(ListHead *H, int elem_idx);
int list_get_new(ListHead H, int elem_idx);
void list_remove(ListHead *H, LinkNode *N);
void list_remove_idx(ListHead* H, int elem_idx);
int list_size_new(ListHead H);

#endif
