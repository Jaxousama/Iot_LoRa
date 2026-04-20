#include "Fifo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// initialise le fifo
Fifo* initFifo(void){
    Fifo* fifo = (Fifo*)malloc(sizeof(Fifo));
    fifo->head = 0;
    fifo->tail = 0;
    return fifo;
}

// supprime le fifo
int deleteFifo(Fifo* fifo){
    free(fifo);
    return 1;
}

// test si le fifo est vide, renvois 1 si le fifo est vide, 0 sinon
int isEmptyFifo(Fifo* fifo){
    return fifo->head == fifo->tail;
}

// test si le fifo est plein, renvois 1 si le fifo est plein, 0 sinon
int isFullFifo(Fifo* fifo){
    return (fifo->tail + 1) % MAX_FIFO_SIZE == fifo->head;
}

// ajoute un element au fifo, renvois 1 si push reussi, 0 sinon
int pushFifo(Fifo* fifo, char* data){
    if(isFullFifo(fifo)){
        popFifo(fifo, NULL);
    }
    strncpy(fifo->data[fifo->tail], data, 32);
    fifo->resend_stat[fifo->tail] = RESEND_NOT_OK;
    fifo->tail = (fifo->tail + 1) % MAX_FIFO_SIZE;
    return 1;
}

// retire un element du fifo, renvois 1 si pop reussi, 0 sinon, data est rempli avec la valeur popé
int popFifo(Fifo* fifo, char* data){
    if(isEmptyFifo(fifo)){
        return 0;
    }
    strncpy(data, fifo->data[fifo->head], 32);
    fifo->head = (fifo->head + 1) % MAX_FIFO_SIZE;
    return 1;
}

// verifie si data est dans le fifo, sans compter le ttl. renvois 1 si data est dans le fifo, 0 sinon
int isInFifo(Fifo* fifo, char* data){   //split le msg en 2 et verfie si les 2 sont ok
    //verifie si le fifo est vide
    if(isEmptyFifo(fifo)){
        return 0;
    }

    //decoupe le message en 2 partie, la partie avant le ',' et la partie apres le ':', pour ne pas prendre en compte le ttl
    int index = fifo->head;
    char prefix[32];
    char suffix[32];
    int i = 0;
    while (i < 32 && data[i] != ',') {
        prefix[i] = data[i];
        i++;
    }
    prefix[i] = '\0';  
    i++;  
    while (i < 32 && data[i] != ':') { 
        i++;
    }
    int j = 0;
    while (i < 32 && data[i] != '\0') {
        suffix[j] = data[i];
        i++;
        j++;
    }
    suffix[j] = '\0';  

    // boucle sur les message en memoire et verifie si le prefix et le suffix sont les meme que ceux de data
    char prefix_fifo[32];
    char suffix_fifo[32];
    while(index != fifo->tail){
        int k = 0;
        while (k < 32 && fifo->data[index][k] != ',') {
            prefix_fifo[k] = fifo->data[index][k];
            k++;
        }
        prefix_fifo[k] = '\0';  
        k++; 
        while (k < 32 && fifo->data[index][k] != ':') {
            k++;
        }
        int l = 0;
        while (k < 32 && fifo->data[index][k] != '\0') {
            suffix_fifo[l] = fifo->data[index][k];
            k++;
            l++;
        }
        suffix_fifo[l] = '\0';  
        if(strncmp(prefix_fifo, prefix, 32) == 0 && strncmp(suffix_fifo, suffix, 32) == 0){
            return 1;
        }
        index = (index + 1) % MAX_FIFO_SIZE;
    }
    return 0;
}

// verifie si data est dans le fifo, sans compter le ttl. renvois l'index de data dans le fifo si data est dans le fifo, -1 sinon
int indexInFifo(Fifo* fifo, char* data){
    // verifie si le fifo est vide
    if(isEmptyFifo(fifo)){
        return -1;
    }

    // decoupe le message en 2 partie, la partie avant le ',' et la partie apres le ':', pour ne pas prendre en compte le ttl
    int index = fifo->head;
    char prefix[32];
    char suffix[32];
    int i = 0;
    while (i < 32 && data[i] != ',') {
        prefix[i] = data[i];
        i++;
    }
    prefix[i] = '\0';  
    i++;  
    while (i < 32 && data[i] != ':') { 
        i++;
    }
    int j = 0;
    while (i < 32 && data[i] != '\0') {
        suffix[j] = data[i];
        i++;
        j++;
    }
    suffix[j] = '\0';

    // boucle sur les message en memoire et verifie si le prefix et le suffix sont les meme que ceux de data, si oui renvois l'index du message dans le fifo
    char prefix_fifo[32];
    char suffix_fifo[32];
    while(index != fifo->tail){
        int k = 0;
        while (k < 32 && fifo->data[index][k] != ',') {
            prefix_fifo[k] = fifo->data[index][k];
            k++;
        }
        prefix_fifo[k] = '\0'; 
        k++;  
        while (k < 32 && fifo->data[index][k] != ':') { 
            k++;
        }
        int l = 0;
        while (k < 32 && fifo->data[index][k] != '\0') {
            suffix_fifo[l] = fifo->data[index][k];
            k++;
            l++;
        }
        suffix_fifo[l] = '\0'; 
        if(strncmp(prefix_fifo, prefix, 32) == 0 && strncmp(suffix_fifo, suffix, 32) == 0){
            return index;
        }
        index = (index + 1) % MAX_FIFO_SIZE;
    }
    return -1;
}

// tableau de string pour les etats de resend, pour l'affichage du fifo
char* resend_stat_str[] = {
    "RESEND_OK",
    "RESEND_NOT_OK",
    "RESEND_DELAYED"
};

// affiche le fifo
void printFifo(Fifo* fifo){
    int index = fifo->head;
    while(index != fifo->tail){
        printf("%s : %s\n", fifo->data[index], resend_stat_str[fifo->resend_stat[index]]); // affichage du style "message : statut de resend"
        index = (index + 1) % MAX_FIFO_SIZE;
    } 
}

// change le statut de resend de data dans le fifo. renvois 1 si data est dans le fifo et que le statut a été changé, 0 sinon
int changeResendStat(Fifo* fifo, char* data, resend_stat_t stat){
    // verifie si le fifo est vide
    if(isEmptyFifo(fifo)){
        return 0;
    }
    int index;
    if((index = indexInFifo(fifo,data)) != -1){ // cherche l'index de data dans le fifo, si data est dans le fifo, change son statut de resend
        fifo->resend_stat[index] = stat;    // change le statut de resend de data dans le fifo
        return 1;
    }
    return 0;
}