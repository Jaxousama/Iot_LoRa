#ifndef LINKLIST_H
#define LINKLIST_H

typedef void(*FuncFree)(void *);
typedef int(*FuncCompare)(void *a, void *b);
typedef void(*FunPrint)(void *);



typedef struct Node{
    void* data;
    struct Node* next;
}Cell;

typedef struct _list{
    Cell* head;
    FuncFree freefunc;
    FuncCompare cmpfunc;
    FunPrint printfunc;
}List;

int isEmptyLinklist(List* l);

void printLinklist(List* l);

List* initLinklist(FuncFree freefct, FuncCompare cmpfct, FunPrint printfct);

Cell* initCell(void* data);

void freeList(List* l);

int addHead(List* l,void* data);

int addLast(List* l,void* data);

int removeHead(List *l);

int removeLast(List *l);

int removeIndex(List* l, int index);

int removeFirstOccurence(List* l, void* data);

int findElem(List* l,void* data);

Cell* returnElem(List* l,void* data);

#endif