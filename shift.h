//
// Created by devilinchina on 12/15/19.
//

#ifndef WEBSERVER_SHIFT_H
#define WEBSERVER_SHIFT_H


#include "heap.h"

typedef struct ShiftParameter{
    void *Store;
    int (*Cmp)(const void *,const void*);
    double (*change)(double);
}ShiftParameter;

typedef struct StoreNode{
    char name[32];
    int times;
    unsigned int lastRefTime;

    double Key;
}StoreNode;

typedef struct LRUQueue{
    List ls;
    sem_t lock,outLock;
    int len;

    Node *(*last_added)(struct LRUQueue*Q);
    void *(*victim)(struct LRUQueue *Q);
    Node*(*AddFirst)(struct LRUQueue *Q,StoreNode *a);
    void (*ChangePriority)(struct LRUQueue *Q,Node *loc);
    void (*PopLast)(struct LRUQueue*Q);
}LRUQueue;

void LRUQueue_Init(ShiftParameter*Q);
#define MQUEUE 2
#define MQUEUELEVE 2
#define EXPIRETIME 50000
#define MAXINLRUTIMES 100
typedef struct MQueue MQueue;
struct MQueue{
    List ls;
    LRUQueue LRU[MQUEUE];
    sem_t lock,outLock;
    int len;
    unsigned int currentTime;
    Node*(*AddFirst)(struct MQueue *Q,StoreNode *a);
    void (*ChangePriority)(struct MQueue *Q,Node *loc);
    void (*PopLast)(struct MQueue*Q);
};

void MQueue_Init(ShiftParameter *Q);


typedef struct LFUHeap LFUHeap;
struct LFUHeap{
    Heap ls;
    sem_t lock,outLock;
    int len;
    double (*changeFunc)(double);
    Node*(*AddFirst)(LFUHeap *Q,StoreNode *a);
    void (*ChangePriority)(LFUHeap *Q,Node *loc);
    void (*PopLast)(LFUHeap*Q);
};


void LFUHeap_Init(ShiftParameter *Q);


typedef struct ARCQH ARCQH;
struct ARCQH{
    List ls;
    Heap hs;
    sem_t lock,outLock;
    int len;
    int listLen;
    Node*(*AddFirst)(ARCQH *Q,StoreNode *a);
    void (*ChangePriority)(ARCQH *Q,Node *loc);
    void (*PopLast)(ARCQH*Q);
    double (*changeFunc)(double);
};



void ARCQH_Init(ShiftParameter *Q);


#endif //WEBSERVER_SHIFT_H
