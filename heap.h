//
// Created by devilinchina on 12/18/19.
//

#ifndef WEBSERVER_HEAP_H
#define WEBSERVER_HEAP_H
#define DEBUG_ONE printf("bef");fflush(stdout);
#define DEBUG_TWO printf("aft\n");fflush(stdout);

#include "list.h"
#include <stdlib.h>
#define HEAPSIZE 25000
#define MAXHEAPSIZE (HEAPSIZE+500)
#define LRUBUFSIZE 5000
#define LRUMAXTIMES 1
typedef struct Heap Heap;


struct Heap{
    Node **store;
    Node *lastAdd;
    sem_t lock,adjustLock;
    int (*Compare)(const void*a,const void*b);
    void *top;

    void (*push)(Heap*it,void *element);
    void (*push_node)(Heap*it,Node *a);
    void*(*pull)(Heap*it);
    Node* (*pull_node)(Heap*it);



    Node *(*last_added)(Heap*heap);
    void *(*victim)(Heap *heap);
    void (*adjust)(Heap*it,Node*locs);
    void (*destroy)(Heap*it,void (*func)(void*));

    unsigned int len;
};

void Heap_Init(Heap*heap,int (*Cmp)(const void*a,const void*b));
#endif //WEBSERVER_HEAP_H
