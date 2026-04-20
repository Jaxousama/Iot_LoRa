#include "Fifo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Fifo* initFifo(void){
    Fifo* fifo = (Fifo*)malloc(sizeof(Fifo));
    fifo->head = 0;
    fifo->tail = 0;
    return fifo;
}

int deleteFifo(Fifo* fifo){
    free(fifo);
    return 1;
}

int isEmptyFifo(Fifo* fifo){
    return fifo->head == fifo->tail;
}

int isFullFifo(Fifo* fifo){
    return (fifo->tail + 1) % MAX_FIFO_SIZE == fifo->head;
}

int pushFifo(Fifo* fifo, char* data){
    if(isFullFifo(fifo)){
        popFifo(fifo, NULL);
    }
    strncpy(fifo->data[fifo->tail], data, 32);
    fifo->resend_stat[fifo->tail] = RESEND_NOT_OK;
    fifo->tail = (fifo->tail + 1) % MAX_FIFO_SIZE;
    return 1;
}

int popFifo(Fifo* fifo, char* data){
    if(isEmptyFifo(fifo)){
        return 0;
    }
    strncpy(data, fifo->data[fifo->head], 32);
    fifo->head = (fifo->head + 1) % MAX_FIFO_SIZE;
    return 1;
}

int isInFifo(Fifo* fifo, char* data){   //split le msg en 2 et verfie si les 2 sont ok
    if(isEmptyFifo(fifo)){
        return 0;
    }
    int index = fifo->head;
    char prefix[32];
    char suffix[32];
    int i = 0;
    while (i < 32 && data[i] != ',') {
        prefix[i] = data[i];
        i++;
    }
    prefix[i] = '\0';  // Null-terminate the prefix
    i++;  // Skip the comma
    while (i < 32 && data[i] != ':') { // Skip the ttl
        i++;
    }
    int j = 0;
    while (i < 32 && data[i] != '\0') {
        suffix[j] = data[i];
        i++;
        j++;
    }
    suffix[j] = '\0';  // Null-terminate the suffix
    char prefix_fifo[32];
    char suffix_fifo[32];
    while(index != fifo->tail){
        int k = 0;
        while (k < 32 && fifo->data[index][k] != ',') {
            prefix_fifo[k] = fifo->data[index][k];
            k++;
        }
        prefix_fifo[k] = '\0';  // Null-terminate the prefix
        k++;  // Skip the comma
        while (k < 32 && fifo->data[index][k] != ':') { // Skip the ttl
            k++;
        }
        int l = 0;
        while (k < 32 && fifo->data[index][k] != '\0') {
            suffix_fifo[l] = fifo->data[index][k];
            k++;
            l++;
        }
        suffix_fifo[l] = '\0';  // Null-terminate the suffix
        if(strncmp(prefix_fifo, prefix, 32) == 0 && strncmp(suffix_fifo, suffix, 32) == 0){
            return 1;
        }
        index = (index + 1) % MAX_FIFO_SIZE;
    }
    return 0;
}

int indexInFifo(Fifo* fifo, char* data){
    if(isEmptyFifo(fifo)){
        return -1;
    }
    int index = fifo->head;
    char prefix[32];
    char suffix[32];
    int i = 0;
    while (i < 32 && data[i] != ',') {
        prefix[i] = data[i];
        i++;
    }
    prefix[i] = '\0';  // Null-terminate the prefix
    i++;  // Skip the comma
    while (i < 32 && data[i] != ':') { // Skip the ttl
        i++;
    }
    int j = 0;
    while (i < 32 && data[i] != '\0') {
        suffix[j] = data[i];
        i++;
        j++;
    }
    suffix[j] = '\0';  // Null-terminate the suffix
    char prefix_fifo[32];
    char suffix_fifo[32];
    while(index != fifo->tail){
        int k = 0;
        while (k < 32 && fifo->data[index][k] != ',') {
            prefix_fifo[k] = fifo->data[index][k];
            k++;
        }
        prefix_fifo[k] = '\0';  // Null-terminate the prefix
        k++;  // Skip the comma
        while (k < 32 && fifo->data[index][k] != ':') { // Skip the ttl
            k++;
        }
        int l = 0;
        while (k < 32 && fifo->data[index][k] != '\0') {
            suffix_fifo[l] = fifo->data[index][k];
            k++;
            l++;
        }
        suffix_fifo[l] = '\0';  // Null-terminate the suffix
        if(strncmp(prefix_fifo, prefix, 32) == 0 && strncmp(suffix_fifo, suffix, 32) == 0){
            return index;
        }
        index = (index + 1) % MAX_FIFO_SIZE;
    }
    return -1;
}

char* resend_stat_str[] = {
    "RESEND_OK",
    "RESEND_NOT_OK",
    "RESEND_DELAYED"
};

void printFifo(Fifo* fifo){
    int index = fifo->head;
    while(index != fifo->tail){
        printf("%s : %s\n", fifo->data[index], resend_stat_str[fifo->resend_stat[index]]);
        index = (index + 1) % MAX_FIFO_SIZE;
    } 
}

int changeResendStat(Fifo* fifo, char* data, resend_stat_t stat){
    if(isEmptyFifo(fifo)){
        return 0;
    }
    int index;
    if((index = indexInFifo(fifo,data)) != -1){
        fifo->resend_stat[index] = stat;
        return 1;
    }
    return 0;
}