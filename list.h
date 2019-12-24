#ifndef WEBSERVER_LIST_H
#define WEBSERVER_LIST_H
#include <semaphore.h>
#include <zconf.h>

typedef struct Node{
    void *element;
    struct Node*pre,*next;
    unsigned int loc;
}Node;
typedef struct List List;
struct List{
    Node ori;
    sem_t lock;
    void (*insert)(Node* pos, void *ele);

    void (*push_back)(List*list,void*ele);

    void (*push_front)(List*list,void*ele);

    Node* (*erase)(Node* it);

    Node *(*begin)(List*list);


    Node *(*end)(List*list);

    void (*shift)(List*list,Node*it);///shift node it to list's begin

    void *(*pull_back)(List*list);

    void *(*pull_front)(List*list);

    void *(*back)(List*list);

    Node *(*last_added)(List*list);

    Node *(*cat_begin)(List*list);
    Node *(*cat_last)(List*list);

    Node *(*cat_node)(List *list,Node*a);
    void *(*victim)(List*list);

    void *(*front)(List*list);

    void (*clear)(List*list);
};

void List_init(List*list);

#endif
