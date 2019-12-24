
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <zconf.h>
#include "list.h"

void List_insert(Node* pos, void *ele) {///在pos之后插入一个新的元素
    Node *temp = malloc(sizeof(Node));
    temp->element = ele;
    temp->next = pos->next;
    pos->next->pre = temp;
    pos->next = temp;
    temp->pre = pos;
}

void List_insert_node(Node* pos, Node *ele) {///在pos之后插入一个新的元素
    if(ele->next && ele->pre) {
        Node *elenext = ele->next;
        Node *elepre = ele->pre;
        elepre->next = elenext;
        elenext->pre = elepre;
    }
    ele->next = pos->next;
    pos->next->pre = ele;
    pos->next = ele;
    ele->pre = pos;
}

void List_push_back(List*list,void*ele){
    sem_wait(&list->lock);
    List_insert(list->ori.pre,ele);
    sem_post(&list->lock);
}


void List_push_front(List*list,void*ele){
    sem_wait(&list->lock);
    List_insert(&list->ori,ele);
    sem_post(&list->lock);
}

void List_shift(List*list,Node*it){
    sem_wait(&list->lock);
    List_insert_node(&list->ori,it);
    sem_post(&list->lock);
}

Node* List_erase(Node* it) {///not thread safe
    if (it->next == it) {
        return it;
    }
    Node*next =it->next;
    next->pre = it->pre;
    it->pre->next = next;
    free(it);
    return next;
}

Node *List_begin(List*list){
    return list->ori.next;
}


Node *List_end(List*list){
    return &list->ori;
}

Node *List_cat_begin(List*list){
    sem_wait(&list->lock);
    if(List_begin(list)==List_end(list)){
        sem_post(&list->lock);
        return NULL;
    }
    Node*ret = List_begin(list);
    ret->pre->next = ret->next;
    ret->next->pre = &list->ori;
    ret->next = ret->pre = NULL;
    sem_post(&list->lock);
    return ret;
}

Node *List_cat_node(List *list,Node*ret){
    sem_wait(&list->lock);
    if(List_begin(list)==List_end(list)){
        sem_post(&list->lock);
        return NULL;
    }
    ret->pre->next = ret->next;
    ret->next->pre = ret->pre;
    ret->next = ret->pre = NULL;
    sem_post(&list->lock);
    return ret;
}

Node *List_cat_last(List *list){

    sem_wait(&list->lock);
    if(List_begin(list)==List_end(list)){

        sem_post(&list->lock);
        return NULL;
    }
    Node*ret = list->ori.pre;
    ret->pre->next = ret->next;
    ret->next->pre = ret->pre;
    ret->next = ret->pre = NULL;
    sem_post(&list->lock);
    return ret;
}

void *List_pull_back(List*list){
    void *ret = NULL;
    sem_wait(&list->lock);
    if(list->ori.pre!=&list->ori){///not empty
        ret = list->ori.pre->element;
        List_erase(list->ori.pre);
    }
    sem_post(&list->lock);
    return ret;
}

void *List_pull_front(List*list){
    void *ret = NULL;
    sem_wait(&list->lock);
    if(list->ori.pre!=&list->ori){///not empty
        ret = list->ori.next->element;
        List_erase(list->ori.next);
    }
    sem_post(&list->lock);
    return ret;
}


void *List_back(List*list){
    void *ret = NULL;
    sem_wait(&list->lock);
    if(list->ori.pre!=&list->ori){///not empty
        ret = list->ori.pre->element;
    }
    sem_post(&list->lock);
    return ret;
}

void *List_front(List*list){
    void *ret = NULL;
    sem_wait(&list->lock);
    if(list->ori.pre!=&list->ori){///not empty
        ret = list->ori.next->element;
    }
    sem_post(&list->lock);
    return ret;
}

void List_clear(List*list){
    while (list->ori.next!=&list->ori){
        free(List_pull_front(list));
    }
}


void List_init(List*list){
    sem_init(&list->lock,getpid(),1);
    list->ori.next = list->ori.pre = &list->ori;
    list->ori.element = NULL;///none in head node

    list->begin = List_begin;
    list->end = List_end;
    list->push_back=List_push_back;
    list->push_front=List_push_front;

    list->back = List_back;
    list->front = List_front;
    list->pull_back = List_pull_back;
    list->pull_front = List_pull_front;

    list->erase = List_erase;
    list->clear = List_clear;
    list->insert = List_insert;
    list->shift = List_shift;

    list->last_added = List_begin;
    list->victim = List_back;
    list->cat_begin = List_cat_begin;
    list->cat_last =List_cat_last;

    list->cat_node = List_cat_node;
}
