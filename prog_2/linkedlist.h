#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct LLNode {
    struct LLNode *prev, *next;
    void* data;
} LLNode;

LLNode* ll_append(LLNode*, void*);

void ll_remove(LLNode* list, void (*rm_data)(void*));

void ll_delete(LLNode* list, void (*rm_data)(void*));

LLNode* ll_sremove(LLNode* list, void* s_term, int (*match)(void*, void*), void (*rm_data)(void*));
 
void* ll_search(LLNode* list, void* s_term, int (*match)(void*, void*));

void* ll_get_stream(LLNode* list);

void* ll_get_next_entry(void** stream);
 
#endif
