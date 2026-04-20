#ifndef fifo_h
#define fifo_h

#define MAX_FIFO_SIZE 10

typedef enum resend_stat {
    RESEND_OK,
    RESEND_NOT_OK,
    RESEND_DELAYED
} resend_stat_t;

typedef struct fifo{
    char data[MAX_FIFO_SIZE][32];
    int head;
    int tail;
    resend_stat_t resend_stat[MAX_FIFO_SIZE];
}Fifo;


Fifo* initFifo(void);

int deleteFifo(Fifo* fifo);

int isEmptyFifo(Fifo* fifo); //renvois 1 si le fifo est vide, 0 sinon

int isFullFifo(Fifo* fifo); //renvois 1 si le fifo est plein, 0 sinon

int pushFifo(Fifo* fifo, char* data); //renvois 1 si push reussi, 0 sinon

int popFifo(Fifo* fifo, char* data); //renvois 1 si pop reussi, 0 sinon, data est rempli avec la valeur popé 

int isInFifo(Fifo* fifo, char* data); //verifie si data est dans le fifo, sans compter le ttl. renvois 1 si data est dans le fifo, 0 sinon

int indexInFifo(Fifo* fifo, char* data); //verifie si data est dans le fifo, sans compter le ttl. renvois l'index de data dans le fifo si data est dans le fifo, -1 sinon

void printFifo(Fifo* fifo); //affiche le fifo

int changeResendStat(Fifo* fifo, char* data, resend_stat_t stat); //change le statut de resend de data dans le fifo. renvois 1 si data est dans le fifo et que le statut a été changé, 0 sinon
#endif