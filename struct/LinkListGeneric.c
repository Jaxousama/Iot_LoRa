#include <stdio.h>
#include <stdlib.h>
#include "LinkListGeneric.h"



int isEmptyLinklist(List* l){
    if(l->head == NULL){
        return 1;
    }
    return 0;
}

void printLinklist(List* l){
    if(isEmptyLinklist(l)){
        return;
    }

    Cell* current = l->head;

    int index = 0;

    while(current != NULL){
        printf("Index : %d\n",index);
        l->printfunc(current->data);
        current= current->next;
        index++;
    }
}

List* initLinklist(FuncFree freefct, FuncCompare cmpfct, FunPrint printfct){
    List* l = malloc(sizeof(List));

    l->freefunc = freefct;
    l->cmpfunc = cmpfct;
    l->printfunc = printfct;
    l->head = NULL;
    return l;
}

Cell* initCell(void* data){
    Cell* c = malloc(sizeof(Cell));
    c->data = data;
    c->next = NULL;
    return c;
}

void freeList(List* l){
    Cell* current = l->head;
    Cell* next = current->next;
    while(current != NULL){
        if(l->freefunc!=NULL){
            l->freefunc(current->data);
        }
        free(current);
        current = next;
        next = next->next;
    }
    free(l);
}


int addHead(List* l,void* data){
    Cell* c = initCell(data);

    if(isEmptyLinklist(l)){
        l->head = c;
        return 1;
    }
    
    c->next = l->head;
    l->head = c;
    return 1;
}

int addLast(List* l,void* data){
    Cell* c = initCell(data);

    if(isEmptyLinklist(l)){
        l->head = c;
        return 1;
    }

    Cell* current = l->head;
    Cell* next = l->head->next;

    while(next != NULL){
        current = next;
        next = next->next;
    }
    current->next = c;
    return 1;
}

int removeHead(List *l){
    if(isEmptyLinklist(l)){
        return 0;
    }
    if(l->head->next == NULL){
        l->freefunc(l->head->data);
        free(l->head);
        l->head = NULL;
        return 1;
    }
    Cell* next = l->head->next;
    l->freefunc(l->head->data);
    free(l->head);
    l->head = next;
    return 1;

}

int removeLast(List *l){
    if(isEmptyLinklist(l)){
        return 0;
    }
    if(l->head->next == NULL){
        l->freefunc(l->head->data);
        free(l->head);
        l->head = NULL;
        return 1;
    }

    Cell* current = l->head;
    Cell* next = l->head->next;

    while(next->next != NULL){
        current = next;
        next = next->next;
    }

    current->next = NULL;
    l->freefunc(next->data);
    free(next);
    return 1;
}

int removeIndex(List* l, int index){
    if(index<0){
        return 0;
    }
    if(isEmptyLinklist(l)){
        return 0;
    }
    if(index == 0){
        return removeHead(l);
    }

    Cell* current = l->head;
    Cell* next = l->head->next;

    for(int i = 0;i<index-1;i++){
        current = next;
        next = next->next;
    }

    current->next = next->next;
    l->freefunc(next->data);
    free(next);
    return 1;
}

int removeFirstOccurence(List* l, void* data){
    if(isEmptyLinklist(l)){
        return -1;
    }
    if(data == NULL){
        return -1;
    }

    int index = findElem(l,data);

    removeIndex(l,index);

    return 1;
}

/*
find_Elem(List* l,void* data)
return the index of data if data is in the list 
args:
-List* l -> the list where we search for data
-void* data -> data search in the list
return:
index -> the index of the data
-1 -> if data was not found
-2 -> if data == NULL
*/

int findElem(List* l,void* data){
    if(isEmptyLinklist(l)){
        return 0;
    }
    if(data == NULL){
        return -2;
    }
    
    Cell* current = l->head;
    int index = 0;
    while(!(l->cmpfunc(current->data,data))){
        current = current->next;
        if(current==NULL){
            return -1;
        }
        index++;
    }
    
    return index;
}

Cell* returnElem(List* l,void* data){

    if(isEmptyLinklist(l)){
        return 0;
    }
    if(data == NULL){
        return -2;
    }
    
    Cell* current = l->head;
    int index = 0;
    while(!(l->cmpfunc(current->data,data))){
        current = current->next;
        if(current==NULL){
            return NULL;
        }
        index++;
    }

    return current;    
}




//TEST


typedef struct _test{
    int x;
    int y;
}test;

test* init_teststruct(int x, int y){
    test* t = malloc(sizeof(test));
    t->x = x;
    t->y = y;
    return t;
}


/*
void print_test(void* t){
    printf("%d\n",*(int*)t);
}

int compare_test(void* aa, void *bb){
    
}


int main(){
    List* l;
    int t = 7;
    int test = 8;
    l=init_list(NULL,(FuncCompare)compare_test,(FunPrint)print_test);
    add_Head(l,&t);
    add_Head(l,&test);
    print_LinkList(l);
}
*/