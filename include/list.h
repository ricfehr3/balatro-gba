/** @file list.h
 *
 *  @brief A doubly-linked list
 *
 *  List Implementation
 *  ===================
 *
 *  - This @ref List operates as a linked list @ref ListNodes. It operates as a regular doubly-linked list
 *  but doesn't allocate memory and rather gets @ref ListNodes from a pool.
 */ 
#ifndef LIST_H
#define LIST_H

#include <stdbool.h>

typedef struct ListNode ListNode;

/**
 * @brief A single entry in a @ref List
 */
struct ListNode
{
    /**
     * @brief The previous @ref ListNode in the associated @ref List, NULL if at the `head` of the list
     */
    ListNode* prev;

    /**
     * @brief The next @ref ListNode in the associated @ref List, NULL if at the `tail` of the list
     */
    ListNode* next;

    /**
     * @brief Pointer to generic data stored in this node
     */
    void* data;
};

/**
 * @brief A doubly-linked list
 */
typedef struct
{
    /**
     * @brief The first entry in the list
     */
    ListNode* head;

    /**
     * @brief The last entry in the list
     */
    ListNode* tail;
} List;

/**
 * @brief An iterator into a list
 */
typedef struct
{
    /**
     * @brief A pointer to the @ref List this is iterating through
     */
    const List* list;

    /**
     * @brief The current node in the list
     */
    ListNode* current_node;
} ListItr;

/**
 * Create a new list.
 *
 * @return A @ref List with head and tail reset.
 */
List list_new(void);

/**
 * Destroy a list.
 *
 * Go through the list and free each node and set the `head` and `tail` to `NULL`.
 * Note, it doesn't "free" the data at the node.
 *
 * @param list pointer to a @ref List to destroy
 */
void list_destroy(List* list);

/**
 * Check if a list is empty
 *
 * @param list pointer to a @ref List
 *
 * @return `true` if the `list` is empty, `false` otherwise.
 */
bool list_is_empty(const List* list);

/**
 * Prepend an entry to the `head` of a @ref list
 *
 * @param list pointer to a @ref List
 * @param data pointer to data to put into the @ref List
 */
void list_push_front(List* list, void* data);

/**
 * Append an entry to the `tail` of a @ref list
 *
 * @param list pointer to a @ref List
 * @param data pointer to data to put into the @ref List
 */
void list_push_back(List* list, void* data);

/**
 * Remove a node from a list.
 *
 * Remove a @ref ListNode from a @ref List. There are no checks to ensure that the
 * passed `node` is actually part of the passed `list`. Handle with care.
 * This is used with the @ref ListItr specifically. 
 *
 * @param list pointer to a @ref List
 * @param node pointer to a @ref ListNode 
 */
void list_remove_node(List *list, ListNode *node);

/**
 * Get a List's node at it's nth index
 *
 * @param list pointer to a @ref List
 * @param n index of the desired @ref ListNode in the list
 *
 * @return a pointer to the data at the nth @ref ListNode, or NULL if out-of-bounds
 */
void* list_get_at_idx(const List *list, int n);

/**
 * Remove a List's node at it's nth index
 *
 * @param list pointer to a @ref List
 * @param n index of the desired @ref ListNode in the list
 *
 * @return `true` if successfully removed, `false` if out-of-bounds
 */
bool list_remove_at_idx(List *list, int n);

/**
 * Get the number of elements in a @ref List
 *
 * @param list pointer to a @ref List
 *
 * @return The number of elements in the list
 */
int list_get_len(List list);

/**
 * Create a new @ref ListItr
 *
 * @param list pointer to a @ref List
 *
 * @return A new @ref ListItr
 */
ListItr list_itr_new(const List* list);

/**
 * Get the next entry in a @ref ListItr
 *
 * @param itr pointer to the @ref ListItr
 *
 * @return A pointer to the next @ref ListNode if valid, otherwise return NULL.
 */
ListNode* list_itr_next(ListItr* itr);

#endif
