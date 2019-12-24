#include "shift.h"
#include <semaphore.h>
#include <stdlib.h>

Node* LRUQueue_AddFirst(LRUQueue *Q,StoreNode *a){
    sem_wait(&Q->lock);
    Q->ls.push_front(&Q->ls,a);
    a->times = 0;
    ++Q->len;
    Node *ret= Q->ls.begin(&Q->ls);
    sem_post(&Q->lock);
    return ret;
}


void LRUQueue_ChangePriority(LRUQueue *Q,Node *loc){
    Q->ls.shift(&Q->ls,loc);
}

void LRUQueue_PopLast(LRUQueue*Q){
    sem_wait(&Q->lock);
    if(Q->len) {
        --Q->len;
        free(Q->ls.pull_back(&Q->ls));
    }
    sem_post(&Q->lock);
}

Node* LRUQueue_last_added(LRUQueue*it){
    return it->ls.last_added(&it->ls);
}


void* LRUQueue_victim(LRUQueue*it){
    return it->ls.victim(&it->ls);
}

void LRUQueue_Init(ShiftParameter *P){
    LRUQueue *Q = P->Store;
    List_init(&Q->ls);
    sem_init(&Q->lock,getpid(),1);
    sem_init(&Q->outLock,getpid(),1);
    Q->len = 0;


    Q->AddFirst = LRUQueue_AddFirst;
    Q->ChangePriority = LRUQueue_ChangePriority;
    Q->PopLast = LRUQueue_PopLast;

    Q->last_added = LRUQueue_last_added;
    Q->victim = LRUQueue_victim;
}




void MQueue_Adjust(MQueue *Q){
    Node * loc ;
    StoreNode *locs;
    int k;
    if(Q->LRU[0].len) {
        loc = Q->LRU[0].ls.end(&Q->LRU[0].ls);
        loc = loc->pre;
        locs = loc->element;
        k = Q->currentTime - locs->lastRefTime;
        if (k >= EXPIRETIME) {
            Q->ls.shift(&Q->ls, loc);
            locs->times = 0;
            locs->lastRefTime = Q->currentTime;
            --Q->LRU[0].len;
        }
    }
    for(int i = 1; i < MQUEUE ; ++i){
        if(Q->LRU[i].len) {
            loc = Q->LRU[0].ls.end(&Q->LRU[i].ls);
            loc = loc->pre;
            locs = loc->element;
            k = Q->currentTime - locs->lastRefTime;
            if (k >= EXPIRETIME) {
                Q->LRU[0].ls.shift(&Q->LRU[i - 1].ls, loc);
                locs->times = (i-1)*MQUEUELEVE+1;
                locs->lastRefTime = Q->currentTime;
                --Q->LRU[i].len;
                ++Q->LRU[i - 1].len;
            }
        }
    }
}


Node* MQueue_AddFirst(MQueue *Q,StoreNode *a){
    sem_wait(&Q->lock);
    Q->ls.push_front(&Q->ls,a);
    ++Q->len;
    a->times = 0;
    Node *ret = Q->ls.begin(&Q->ls);
    a->lastRefTime = Q->currentTime;
    ++Q->currentTime;
    MQueue_Adjust(Q);
    sem_post(&Q->lock);
    return ret;
}

void MQueue_ChangePriority(MQueue *Q,Node *loc){
    sem_wait(&Q->lock);
    StoreNode * locs = loc->element;
    locs->lastRefTime = Q->currentTime;
    ++Q->currentTime;
    if(!locs->times){
        Q->LRU[0].ls.shift(&Q->LRU[0].ls,loc);
        ++Q->LRU[0].len;
        ++locs->times;
    }else {
        int bef = (locs->times - 1) / MQUEUELEVE;
        int cur = (locs->times) / MQUEUELEVE;
        if (cur >= MQUEUE) {
            cur = bef = MQUEUE - 1;
        }
        if (bef != cur) {
            --Q->LRU[bef].len;
            ++Q->LRU[cur].len;
        }
        Q->LRU[cur].ChangePriority(Q->LRU + cur, loc);
        ++locs->times;
    }
    MQueue_Adjust(Q);
    sem_post(&Q->lock);
}

void MQueue_PopLast(MQueue*Q){
    sem_wait(&Q->lock);
    int k = Q->LRU[0].len;
    if(Q->ls.begin(&Q->ls)==Q->ls.end(&Q->ls)){
        for(int i = 0 ; i < MQUEUE ; ++i){
            if(Q->LRU[i].len){
                Q->LRU[i].PopLast(Q->LRU+i);
                --Q->len;
                break;
            }
        }
    }else{
        if(Q->ls.begin(&Q->ls)!=Q->ls.end(&Q->ls)) {
            free(Q->ls.pull_back(&Q->ls));
            --Q->len;
        }
    }
    sem_post(&Q->lock);
}

void MQueue_Init(ShiftParameter *P){
    MQueue *Q = P->Store;
    List_init(&Q->ls);

    for(int i = 0 ; i < MQUEUE ; ++i) {
        P->Store = Q->LRU+i;
        LRUQueue_Init(P);
    }
    sem_init(&Q->lock,getpid(),1);
    sem_init(&Q->outLock,getpid(),1);
    Q->currentTime = 0;
    Q->AddFirst = MQueue_AddFirst;
    Q->ChangePriority = MQueue_ChangePriority;
    Q->PopLast = MQueue_PopLast;
}


void LFUHeap_PopLast(LFUHeap*it){
    sem_wait(&it->lock);
    StoreNode * des=it->ls.pull(&it->ls);
    if(des!=NULL){
        --it->len;
    }
    free(des);
    sem_post(&it->lock);
}


void LFUHeap_ChangePriority(LFUHeap *it,Node *loc){
    sem_wait(&it->lock);
    StoreNode*ele = loc->element;

    ele->Key = it->changeFunc(ele->Key);

    it->ls.adjust(&it->ls,loc);
    sem_post(&it->lock);
}

Node* LFUHeap_AddFirst(LFUHeap*it,StoreNode *a){
    sem_wait(&it->lock);

    it->ls.push(&it->ls,a);

    ++it->len;
    a->Key = 1;
    Node *ret = it->ls.last_added(&it->ls);
    //Show(&it->ls);
    sem_post(&it->lock);
    return ret;
}



void LFUHeap_Init(ShiftParameter *Q){
    LFUHeap * it =Q->Store;
    Heap_Init(&it->ls,Q->Cmp);
    it->len = 0;

    sem_init(&it->lock,getpid(),1);
    sem_init(&it->outLock,getpid(),1);

    it->PopLast = LFUHeap_PopLast;
    it->AddFirst = LFUHeap_AddFirst;

    it->ChangePriority = LFUHeap_ChangePriority;

    it->changeFunc = Q->change;
}

Node*ARCQH_AddFirst(ARCQH *Q,StoreNode *a){
    sem_wait(&Q->lock);
    a->Key = 0;
    Q->ls.push_front(&Q->ls,a);
    a->times = 0;
    ++Q->len;
    ++Q->listLen;
    Node *ret= Q->ls.begin(&Q->ls);
    ret->loc = -1;
   /* while (Q->listLen>LRUBUFSIZE) {
        Node *cats = Q->ls.cat_last(&Q->ls);
        --Q->listLen;
        StoreNode *locs = cats->element;
        locs->Key = 1;
        locs->times = -1;
        Q->hs.push_node(&Q->hs, cats);
    }*/
    sem_post(&Q->lock);
    return ret;
}

void ARCQH_ChangePriority(ARCQH *Q,Node *loc){
    sem_wait(&Q->lock);
    StoreNode *locs = loc->element;
    if(locs->times==-1){
        locs->Key = Q->changeFunc(locs->Key);
        Q->hs.adjust(&Q->hs,loc);
    }else {
        ++locs->times;
        if(locs->times>LRUMAXTIMES){

            Q->ls.cat_node(&Q->ls,loc);
            --Q->listLen;
            locs->Key = 1;
            locs->times = -1;
            Q->hs.push_node(&Q->hs, loc);
        }else {
            Q->ls.shift(&Q->ls, loc);
        }
    }
    sem_post(&Q->lock);
}


void ARCQH_PopLast(ARCQH*Q){
    sem_wait(&Q->lock);
    if(Q->len){
        if(Q->listLen) {
            --Q->listLen;
            free(Q->ls.pull_back(&Q->ls));
            --Q->len;
            if(Q->hs.len){
                Node*cats = Q->hs.pull_node(&Q->hs);
                StoreNode *locs = cats->element;
                locs->times = 0;
                cats->loc = -1;
                Q->ls.shift(&Q->ls,cats);
                ++Q->listLen;
            }
        }else if(Q->hs.len){
            free(Q->hs.pull(&Q->hs));
            --Q->len;
        }

    }
    sem_post(&Q->lock);
}

void ARCQH_Init(ShiftParameter *Q){
    ARCQH *s=Q->Store;
    List_init(&s->ls);
    Heap_Init(&s->hs,Q->Cmp);
    s->len = 0;
    s->listLen = 0;
    s->changeFunc = Q->change;
    sem_init(&s->lock,getpid(),1);
    sem_init(&s->outLock,getpid(),1);
    s->AddFirst = ARCQH_AddFirst;
    s->ChangePriority = ARCQH_ChangePriority;
    s->PopLast=ARCQH_PopLast;
}
