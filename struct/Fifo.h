#ifndef fifo_h
#define fifo_h

#define MAX_FIFO_SIZE 10

typedef struct fifo{
    char data[MAX_FIFO_SIZE][32];
    int head;
    int tail;
}Fifo;


Fifo* initFifo();

int deleteFifo(Fifo* fifo);

int isEmptyFifo(Fifo* fifo); //renvois 1 si le fifo est vide, 0 sinon

int isFullFifo(Fifo* fifo); //renvois 1 si le fifo est plein, 0 sinon

int pushFifo(Fifo* fifo, char* data); //renvois 1 si push reussi, 0 sinon

int popFifo(Fifo* fifo, char* data); //renvois 1 si pop reussi, 0 sinon, data est rempli avec la valeur popé 

int isInFifo(Fifo* fifo, char* data); //verifie si data est dans le fifo, sans compter le ttl. renvois 1 si data est dans le fifo, 0 sinon

#endif