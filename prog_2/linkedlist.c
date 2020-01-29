#include <malloc.h>

#include "linkedlist.h"

LLNode* ll_append(LLNode* list, void* data) {
    LLNode* node;

    node = (LLNode*)malloc(sizeof(LLNode));

    node->data = data;
    node->prev = NULL;
    node->next = list;

    if (list != NULL)
        list->prev = node;

    return node;
}

// removes a link (will destroy head of list upon removal)
void ll_remove(LLNode* list, void (*rm_data)(void*)) {
    if (list == NULL)
        return;

    (*rm_data)(list->data);

    if (list->prev != NULL)
        list->prev->next = list->next;
    
    if (list->next != NULL)
        list->next->prev = list->prev;

    free(list);
}

LLNode* ll_sremove(LLNode* list, void* s_term, int (*match)(void*, void*), void (*rm_data)(void*)) {
    LLNode* head = NULL;

    if (list == NULL)
        return NULL;
    
    if (match(s_term, list->data)) {
        if (list->prev == NULL)
            head = list->next;

        ll_remove(list, (*rm_data));
        
        return head;
    }

    ll_sremove(list->next, s_term, (*match), (*rm_data));
    
    return list;
}

// deletes from list to end
void ll_delete(LLNode* list, void (*rm_data)(void*)) {
    if (list == NULL)
        return;

    ll_delete(list->next, (*rm_data));

    (*rm_data)(list->data);

    if (list->prev != NULL)
        list->prev->next = NULL;

    free(list);
}

// searches for s_term using match function
void* ll_search(LLNode* list, void* s_term, int (*match)(void*, void*)) {
   if (list == NULL)
        return NULL;

    if ((*match)(s_term, list->data))
        return list->data;

    return ll_search(list->next, s_term, (*match));
}

void* ll_get_stream(LLNode* list) {
    return (void*)list;
}

void* ll_get_next_entry(void** stream) {
    void* data;
    LLNode* list = (LLNode*)(*stream);

    if (list == NULL)
        return NULL;

    data = list->data;
    *stream = list->next;

    return data;
}
