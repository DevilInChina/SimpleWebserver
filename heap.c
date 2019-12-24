#include "heap.h"
#include "shift.h"
#include <stdio.h>

typedef unsigned int __Index;
#define LSON(i) (((i)<<1u)|1u)
#define RSON(i) (((i)<<1u)+2u)
#define FATHER(i) (((i)-1u)>>1u)


void CheckFunc(char *info,Heap *it,Node* loc){
    if(loc->loc > it->len){
        printf("%s has pro %d but %u\n",info,it->len,loc->loc);
    }
}


void* Heap_victim(Heap *it){
    //CheckFunc("Vic ",it,it->store[0]);
    return it->store[0]->element;
}



void Up_adjust(Heap*it,Node* locs){

    sem_wait(&it->adjustLock);

    Node *mark = locs;
    __Index len = it->len-1;
    __Index i = locs->loc,j = LSON(i);
    //printf("%d\n",loc);
    while (j<=len){
        if(j+1<=len && (it->Compare(it->store[j]->element,it->store[j+1]->element)<0))++j;
        if((it->Compare(mark->element,it->store[j]->element)>=0))break;
        it->store[i] = it->store[j];
        it->store[i]->loc = i;
        i = j,j=LSON(i);
    }

    it->store[i] = mark;
    it->store[i]->loc = i;
    //CheckFunc("UP adj",it,mark);
    sem_post(&it->adjustLock);
}

void* Heap_pull(Heap*it) {
    sem_wait(&it->lock);
    void *ret = NULL;
    if (it->len) {
        Node *destroy = it->store[0];
        --it->len;
        it->store[0] = it->store[it->len];
        it->store[it->len] = NULL;
        if (it->len) {
            it->store[0]->loc = 0;
            Up_adjust(it, it->store[0]);
        }
        ret = destroy->element;
        free(destroy);
    }
    sem_post(&it->lock);
    return ret;
}

Node *Heap_pull_node(Heap*it){
    sem_wait(&it->lock);
    Node *ret = NULL;
    if(it->len) {
        ret = it->store[0];
        --it->len;
        it->store[0] = it->store[it->len];
        it->store[it->len] = NULL;
        if(it->len) {
            it->store[0]->loc = 0;
            Up_adjust(it, it->store[0]);
        }
    }
    sem_post(&it->lock);
    return ret;
}

void Down_adjust(Heap*it,Node* locs){

    sem_wait(&it->adjustLock);
    __Index loc = locs->loc;
    __Index i = loc, j;

    //CheckFunc("DOWN adj",it,locs);
    Node *des = locs;
    while (i) {
        j = FATHER(i);
        if (it->Compare(it->store[j]->element, des->element)>=0)break;
        it->store[i] = it->store[j];
        it->store[i]->loc = i;
        i = j;
    }
    it->store[i] = des;
    it->store[i]->loc = i;
    sem_post(&it->adjustLock);
}

void Heap_push(Heap*it,void *a) {
    sem_wait(&it->lock);
    it->store[it->len] = malloc(sizeof(Node));
    it->store[it->len]->loc = it->len;
    it->store[it->len]->element = a;
    it->lastAdd = it->store[it->len];
    Down_adjust(it,it->store[it->len]);

    ++it->len;

    sem_post(&it->lock);
}
void Heap_push_node(Heap*it,Node*a){
    sem_wait(&it->lock);
    it->store[it->len] = a;
    it->store[it->len]->loc = it->len;
    it->lastAdd = it->store[it->len];
    Down_adjust(it,it->store[it->len]);
    ++it->len;
    sem_post(&it->lock);
}

void Heap_adjust(Heap *it,Node*locs){
    sem_wait(&it->lock);

    //CheckFunc("ADJ ",it,locs);
    __Index lson = LSON(locs->loc);
    __Index rson = RSON(locs->loc);
    int adjust = 0;
    if(lson < it->len){
        if(it->Compare(locs->element,it->store[lson]->element) > 0){
            adjust = 1;///up adjust
        }else if(rson < it->len){
            if(it->Compare(locs->element,it->store[rson]->element) > 0){
                adjust = 1;///up adjust
            }
        }
    }
    if(!adjust && locs->loc!=0) {
        int father = FATHER(locs->loc);
        if(it->Compare(locs->element,it->store[father]->element) < 0){
            adjust = -1;
        }
    }

    switch (adjust){
        case 1:
            Up_adjust(it,locs);
            break;
        case -1:
            Down_adjust(it,locs);
            break;
        default:
            break;
    }

    sem_post(&it->lock);
}


void Heap_destroy(Heap*it,void (*func)(void*)){
    for(int i =0  ; i < it->len ; ++i){
        func(it->store[i]->element);
        free(it->store[i]);
    }
    free(it->store);
}

Node *Heap_last_added(Heap *it){
  //  CheckFunc("Last added ",it,it->lastAdd);
    return it->lastAdd;
}

void Heap_Init(Heap*heap,int (*Cmp)(const void*a,const void*b)){
    heap->store = (Node**)malloc(MAXHEAPSIZE*sizeof(Node*));
    heap->len = 0;
    sem_init(&heap->lock,getpid(),1);
    sem_init(&heap->adjustLock,getpid(),1);
    heap->last_added = Heap_last_added;
    heap->victim = Heap_victim;

    heap->pull = Heap_pull;
    heap->push = Heap_push;

    heap->push_node = Heap_push_node;
    heap->pull_node = Heap_pull_node;


    heap->destroy = Heap_destroy;
    heap->adjust = Heap_adjust;

    heap->Compare = Cmp;

}