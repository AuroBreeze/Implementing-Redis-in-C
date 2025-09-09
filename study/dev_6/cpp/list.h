#pragma once

#include <cstddef>

struct DList{
    DList* prev = nullptr;
    DList* next = nullptr;
};

inline void dlist_init(DList* node){
    node->prev = node->next = node;
}

inline bool dlist_empty(DList* node){
    return node->next == node;
}

inline void dlist_detach(DList* node){
    DList* prev = node->prev;
    DList* next = node->next;
    prev->next = next;
    next->prev = prev;
}

// insert node to the list before target
inline void dlist_insert_before(DList* target, DList* rookie){
    DList* prev = target->prev;
    prev->next = rookie;
    rookie->next = target;
    rookie->prev = prev;
    target->prev = rookie;
}



