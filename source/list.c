#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "list.h"
#include "pool.h"


ListHead list_new(void)
{
    ListHead head = { .head = -1, .tail = -1};
    return head;
}

bool list_empty(ListHead H)
{
    return H.head >= 0;
}

int list_push_front(ListHead *p_list, int elem_idx)
{
    ListNode *p_node = POOL_GET(ListNode);
    int n = POOL_IDX(ListNode, p_node); // Get the index of of the node in the pool

    p_node->elem_idx = elem_idx;
    p_node->prev = -1;
    p_node->next = p_list->head;

    if (list_empty(*p_list))
    {
        p_list->tail = n;
    }
    else
    {
        POOL_AT(ListNode, p_list->head)->prev = n;
    }

    p_list->head = n;

    return n;
}

int list_push_back(ListHead *p_list, int elem_idx)
{
    ListNode *p_node = POOL_GET(ListNode);
    int n = POOL_IDX(ListNode, p_node); // Get the index of of the node in the pool
    p_node->elem_idx = elem_idx;
    p_node->prev = p_list->tail;
    p_node->next = -1;

    if (list_empty(*p_list))
    {
        p_list->head = n;
    }
    else
    {
        POOL_AT(ListNode, p_list->tail)->next = n;
    }

    p_list->tail = n;

    return n;
}

void list_remove_node(ListHead *p_list, ListNode *p_node)
{
    ListNode* p_prev_node = NULL;
    ListNode* p_next_node = NULL;

    if(p_node->prev >= 0)
    {
        p_prev_node = POOL_AT(ListNode, p_node->prev);
    }

    if(p_node->next >= 0)
    {
        p_next_node = POOL_AT(ListNode, p_node->next);
    }


    if(p_prev_node && !p_next_node) // end of list
    {
        p_prev_node->next = -1;
        p_list->tail = POOL_IDX(ListNode, p_prev_node);
    }
    else if(p_prev_node && p_next_node) // somewhere in between
    {
        p_prev_node->next = POOL_IDX(ListNode, p_next_node);
        p_next_node->prev = POOL_IDX(ListNode, p_prev_node);
    }
    else if(p_next_node && !p_prev_node) // beginning of list
    {
        p_next_node->prev = -1;
        p_list->head = POOL_IDX(ListNode, p_next_node);
    }
    else if(!p_prev_node && !p_next_node) // only element in list
    {
        p_list->head = -1;
        p_list->tail = -1;
    }

    POOL_FREE(ListNode, p_node);
}

ListItr list_itr_new(ListHead* p_list)
{
    ListItr itr =
    {
        .p_list = p_list,
        .p_current_node = NULL,
    };

    return itr;
}

ListNode* list_itr_next(ListItr* p_itr)
{
    ListNode* ln = p_itr->p_current_node;
    if(!p_itr->p_current_node) { return NULL; };
    return ln->next >= 0 ? POOL_AT(ListNode, ln->next) : NULL;
}

void list_remove_at_idx(ListHead* p_list, int elem_idx)
{
    int len = 0;
    /*
    int len = 0;

    ListItr itr = list_itr_new(p_list);

    ListNode* ln = list_itr_next(&itr);
    while(ln) {
        if(elem_idx == len++)
        {
            list_remove_node(p_list, ln);
            return;
        }
        ln = list_itr_next(&itr);
    }
    */

    ListNode* current_node = (p_list->head >= 0) ?
        POOL_AT(ListNode, p_list->head) :
        NULL;

    while (current_node != NULL)
    {
        if(elem_idx == len++)
        {
            list_remove_node(p_list, current_node);
            return;
        };
        current_node = (current_node->next >= 0) ?
                        POOL_AT(ListNode, current_node->next) :
                        NULL;
    }
}

int list_get_len(ListHead list)
{
    int len = 0;

    ListNode* current_node = (list.head >= 0) ?
        POOL_AT(ListNode, list.head) :
        NULL;

    while (current_node != NULL)
    {
        len++;
        current_node = (current_node->next >= 0) ?
                        POOL_AT(ListNode, current_node->next) :
                        NULL;
    }
    return len;
}

int list_get_at_idx(ListHead list, int elem_idx)
{
    int len = 0;
    ListNode* current_node = (list.head >= 0) ?
        POOL_AT(ListNode, list.head) :
        NULL;
    while (current_node != NULL)
    {
        if(elem_idx == len++) return current_node->elem_idx;
        current_node = (current_node->next >= 0) ?
                        POOL_AT(ListNode, current_node->next) :
                        NULL;
    }
    return -1;
}
