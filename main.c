/*
 * Copyright (C) 2016 Unwired Devices <info@unwds.com>
 *               2017 Inria Chile
 *               2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 * @file
 * @brief       Test application for SX127X modem driver
 *
 * @author      Eugene P. <ep@unwds.com>
 * @author      José Ignacio Alamos <jose.alamos@inria.cl>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @}
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "struct/LinkListGeneric.h"
#include "struct/Fifo.h"

#include "thread.h"
#include "shell.h"
#include "event/deferred_callback.h"
#include "event/thread.h"

#include "net/netdev.h"
#include "net/netdev/lora.h"
#include "net/lora.h"

#include "board.h"

#include "sx127x_internal.h"
#include "sx127x_params.h"
#include "sx127x_netdev.h"

#include "fmt.h"

#define SX127X_LORA_MSG_QUEUE   (16U)
#ifndef SX127X_STACKSIZE
#define SX127X_STACKSIZE        (THREAD_STACKSIZE_DEFAULT)
#endif

                            // definition des varibale globale

#define MSG_TYPE_ISR            (0x3456)
#define  MAX_USER_NAME 4    // taille max du nom d'utilisateur
#define MAX_USER 20         // nombre max d'utilisateur dans la liste des utilisateurs connus
#define MAX_CHANNEL 10      // nombre max de channel dans la liste des channel connu
#define TTL 3               // nombre de fois que le message doit etre renvoyer
#define RELAY_DELAY_MS 5000U// delai avant de renvoyer un message
#define RELAY_JOB_SLOTS 4   // nombre de message pouvant etre en attente de renvoi

static char stack[SX127X_STACKSIZE];
static kernel_pid_t _recv_pid;

static char message[32];
static sx127x_t sx127x;

static uint8_t number_user = 0; // nombre d'utilisateur dans la liste des utilisateurs connus

static int SNR_threshold = 50;  

static List* list_user;     // liste des utilisateurs connus

static char** list_channel; // liste des channel connus
static Fifo* fifo_msg;  // fifo des message deja vu pour eviter de les traiter plusieurs fois et de les renvoyer a l'infini

char pseudo[5] = "Jaxu\0";  // pseudo du client
uint32_t msg_conter = 0;    // compteur des message envoyer

                            // definition des structure

// definition de la structure d'une tache de renvoi
typedef struct {
    event_deferred_callback_t deferred;
    char message[32];
    bool busy;
} relay_job_t;

static relay_job_t relay_jobs[RELAY_JOB_SLOTS];

// definition de la structure d'un utilisateur avec son nom et son numero de sequence du message le plus recent qu'il a envoye
typedef struct user_info{
    int num;
    char* username;
}User;

                            // fonction de traitement des utilisateurs

// fonction de comparaison de deux utilisateur pour la liste des utilisateurs connus
int compare_user(User* user1,char* user2){

    if (strcmp(user1->username , user2) == 0){
        return 1;
    }
    return 0;
}

// fonction d'affichage d'un utilisateur pour la liste des utilisateurs connus
void print_user(User* user){
    printf("name : %s, num : %d\n",user->username,user->num);
}

// fonction d'affichage de la liste des utilisateurs connus
int print_list_user(int argc, char** argv){
    (void)argc;
    (void)argv;
    printLinklist(list_user);
    return 1;
}

// libere la memoire allouee pour un utilisateur
void free_user(User* user){
    free(user->username);
}

                            // fonction fournisse pour la configuration
int lora_setup_cmd(int argc, char **argv)
{

    if (argc < 4) {
        puts("usage: setup "
             "<bandwidth (125, 250, 500)> "
             "<spreading factor (7..12)> "
             "<code rate (5..8)>");
        return -1;
    }

    /* Check bandwidth value */
    int bw = atoi(argv[1]);
    uint8_t lora_bw;

    switch (bw) {
    case 125:
        puts("setup: setting 125KHz bandwidth");
        lora_bw = LORA_BW_125_KHZ;
        break;

    case 250:
        puts("setup: setting 250KHz bandwidth");
        lora_bw = LORA_BW_250_KHZ;
        break;

    case 500:
        puts("setup: setting 500KHz bandwidth");
        lora_bw = LORA_BW_500_KHZ;
        break;

    default:
        puts("[Error] setup: invalid bandwidth value given, "
             "only 125, 250 or 500 allowed.");
        return -1;
    }

    /* Check spreading factor value */
    uint8_t lora_sf = atoi(argv[2]);

    if (lora_sf < 7 || lora_sf > 12) {
        puts("[Error] setup: invalid spreading factor value given");
        return -1;
    }

    /* Check coding rate value */
    int cr = atoi(argv[3]);

    if (cr < 5 || cr > 8) {
        puts("[Error ]setup: invalid coding rate value given");
        return -1;
    }
    uint8_t lora_cr = (uint8_t)(cr - 4);

    /* Configure radio device */
    netdev_t *netdev = &sx127x.netdev;

    netdev->driver->set(netdev, NETOPT_BANDWIDTH,
                        &lora_bw, sizeof(lora_bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR,
                        &lora_sf, sizeof(lora_sf));
    netdev->driver->set(netdev, NETOPT_CODING_RATE,
                        &lora_cr, sizeof(lora_cr));

    puts("[Info] setup: configuration set with success");

    return 0;
}

int random_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    netdev_t *netdev = &sx127x.netdev;
    uint32_t rand;

    netdev->driver->get(netdev, NETOPT_RANDOM, &rand, sizeof(rand));
    printf("random: number from sx127x: %u\n",
           (unsigned int)rand);

    /* reinit the transceiver to default values */
    sx127x_init_radio_settings(&sx127x);

    return 0;
}

int register_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: register <get | set>");
        return -1;
    }

    if (strstr(argv[1], "get") != NULL) {
        if (argc < 3) {
            puts("usage: register get <all | allinline | regnum>");
            return -1;
        }

        if (strcmp(argv[2], "all") == 0) {
            puts("- listing all registers -");
            uint8_t reg = 0, data = 0;
            /* Listing registers map */
            puts("Reg   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
            for (unsigned i = 0; i <= 7; i++) {
                printf("0x%02X ", i << 4);

                for (unsigned j = 0; j <= 15; j++, reg++) {
                    data = sx127x_reg_read(&sx127x, reg);
                    printf("%02X ", data);
                }
                puts("");
            }
            puts("-done-");
            return 0;
        }
        else if (strcmp(argv[2], "allinline") == 0) {
            puts("- listing all registers in one line -");
            /* Listing registers map */
            for (uint16_t reg = 0; reg < 256; reg++) {
                printf("%02X ", sx127x_reg_read(&sx127x, (uint8_t)reg));
            }
            puts("- done -");
            return 0;
        }
        else {
            long int num = 0;
            /* Register number in hex */
            if (strstr(argv[2], "0x") != NULL) {
                num = strtol(argv[2], NULL, 16);
            }
            else {
                num = atoi(argv[2]);
            }

            if (num >= 0 && num <= 255) {
                printf("[regs] 0x%02X = 0x%02X\n",
                       (uint8_t)num,
                       sx127x_reg_read(&sx127x, (uint8_t)num));
            }
            else {
                puts("regs: invalid register number specified");
                return -1;
            }
        }
    }
    else if (strstr(argv[1], "set") != NULL) {
        if (argc < 4) {
            puts("usage: register set <regnum> <value>");
            return -1;
        }

        long num, val;

        /* Register number in hex */
        if (strstr(argv[2], "0x") != NULL) {
            num = strtol(argv[2], NULL, 16);
        }
        else {
            num = atoi(argv[2]);
        }

        /* Register value in hex */
        if (strstr(argv[3], "0x") != NULL) {
            val = strtol(argv[3], NULL, 16);
        }
        else {
            val = atoi(argv[3]);
        }

        sx127x_reg_write(&sx127x, (uint8_t)num, (uint8_t)val);
    }
    else {
        puts("usage: register get <all | allinline | regnum>");
        return -1;
    }

    return 0;
}

                            // fonction d'envois de message

// envois d'un message priver, '*' si destiner a tous
int mp_cmd(int argc, char **argv)
{
    if (argc <= 2) {
        puts("usage: send <target> <payload>");
        return -1;
    }

    // calcul de la taille effective du message a envoyer en concatenant les arguments de la commande
    size_t payload_len = 0;
    for (int i = 2; i < argc; i++) {
        payload_len += strlen(argv[i]);
        if (i < (argc - 1)) {
            payload_len++;
        }
    }

    // alloue de la memoire pour le message a envoyer et le construit en concatenant les arguments de la commande
    char *msg_content = malloc(payload_len + 1);
    if (msg_content == NULL) {
        puts("no memory");
        return -1;
    }

    // construit le message en concatenant les arguments de la commande
    size_t tmp_size = 0;
    for (int i = 2; i < argc; i++) {
        size_t arg_len = strlen(argv[i]);
        memcpy(msg_content + tmp_size, argv[i], arg_len);
        tmp_size += arg_len;
        if (i < (argc - 1)) {
            msg_content[tmp_size++] = ' ';
        }
    }
    msg_content[tmp_size] = '\0';

    // calcule la taille total du message et l'alloue en memoire
    size_t size_msg = strlen(pseudo) + strlen(argv[1]) + payload_len + sizeof(TTL) + 10;
    char *msg = malloc(size_msg);
    if (msg == NULL) {
        free(msg_content);
        puts("no memory");
        return -1;
    }

    // cree le message a la forme "pseudo@target:num,ttl:message"
    sprintf(msg, "%s@%s:%lu,%u:%s", pseudo, argv[1], msg_conter++, TTL, msg_content);
    printf("sending \"%s\" payload (%u bytes)\n",
           msg, (unsigned)strlen(msg) + 1);

    // envois le message
    iolist_t iolist = {
        .iol_base = msg,
        .iol_len = (strlen(msg) + 1)
    };

    netdev_t *netdev = &sx127x.netdev;

    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }
    free(msg);
    free(msg_content);

    return 0;
}

// envois d'un message dans un channel
int send_cmd(int argc, char **argv)
{
    if (argc <= 2) {
        puts("usage: send <target> <payload>");
        return -1;
    }

    // calcul de la taille effective du message a envoyer en concatenant les arguments de la commande
    size_t payload_len = 0;
    for (int i = 2; i < argc; i++) {
        payload_len += strlen(argv[i]);
        if (i < (argc - 1)) {
            payload_len++;
        }
    }

    // alloue de la memoire pour le message a envoyer et le construit en concatenant les arguments de la commande
    char *msg_content = malloc(payload_len + 1);
    if (msg_content == NULL) {
        puts("no memory");
        return -1;
    }

    // construit le message en concatenant les arguments de la commande
    size_t tmp_size = 0;
    for (int i = 2; i < argc; i++) {
        size_t arg_len = strlen(argv[i]);
        memcpy(msg_content + tmp_size, argv[i], arg_len);
        tmp_size += arg_len;
        if (i < (argc - 1)) {
            msg_content[tmp_size++] = ' ';
        }
    }
    msg_content[tmp_size] = '\0';

    // calcule la taille total du message et l'alloue en memoire
    size_t size_msg = strlen(pseudo) + strlen(argv[1]) + payload_len + sizeof(TTL) + 10;
    char *msg = malloc(size_msg);
    if (msg == NULL) {
        free(msg_content);
        puts("no memory");
        return -1;
    }

    // cree le message a la forme "pseudo#target:num,ttl:message"
    sprintf(msg, "%s#%s:%lu,%u:%s", pseudo, argv[1], msg_conter++, TTL, msg_content);
    printf("sending \"%s\" payload (%u bytes)\n",
           msg, (unsigned)strlen(msg) + 1);

    // envois le message
    iolist_t iolist = {
        .iol_base = msg,
        .iol_len = (strlen(msg) + 1)
    };

    netdev_t *netdev = &sx127x.netdev;

    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }
    free(msg);
    free(msg_content);


    return 0;
}

                            // fonction fournisse initialement
int listen_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    netdev_t *netdev = &sx127x.netdev;
    /* Switch to continuous listen mode */
    const netopt_enable_t single = false;

    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 0;

    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

    /* Switch to RX state */
    netopt_state_t state = NETOPT_STATE_RX;

    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));

    printf("Listen mode set\n");

    return 0;
}

int syncword_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: syncword <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint8_t syncword;

    if (strstr(argv[1], "get") != NULL) {
        netdev->driver->get(netdev, NETOPT_SYNCWORD, &syncword,
                            sizeof(syncword));
        printf("Syncword: 0x%02x\n", syncword);
        return 0;
    }

    if (strstr(argv[1], "set") != NULL) {
        if (argc < 3) {
            puts("usage: syncword set <syncword>");
            return -1;
        }
        syncword = fmt_hex_byte(argv[2]);
        netdev->driver->set(netdev, NETOPT_SYNCWORD, &syncword,
                            sizeof(syncword));
        printf("Syncword set to %02x\n", syncword);
    }
    else {
        puts("usage: syncword <get|set>");
        return -1;
    }

    return 0;
}
int channel_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: channel <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint32_t chan;

    if (strstr(argv[1], "get") != NULL) {
        netdev->driver->get(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        printf("Channel: %i\n", (int)chan);
        return 0;
    }

    if (strstr(argv[1], "set") != NULL) {
        if (argc < 3) {
            puts("usage: channel set <channel>");
            return -1;
        }
        chan = atoi(argv[2]);
        netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        printf("New channel set\n");
    }
    else {
        puts("usage: channel <get|set>");
        return -1;
    }

    return 0;
}

int rx_timeout_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: channel <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint16_t rx_timeout;

    if (strstr(argv[1], "set") != NULL) {
        if (argc < 3) {
            puts("usage: rx_timeout set <rx_timeout>");
            return -1;
        }
        rx_timeout = atoi(argv[2]);
        netdev->driver->set(netdev, NETOPT_RX_SYMBOL_TIMEOUT, &rx_timeout,
                            sizeof(rx_timeout));
        printf("rx_timeout set to %i\n", rx_timeout);
    }
    else {
        puts("usage: rx_timeout set");
        return -1;
    }

    return 0;
}

int reset_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    netdev_t *netdev = &sx127x.netdev;

    puts("resetting sx127x...");
    netopt_state_t state = NETOPT_STATE_RESET;

    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(netopt_state_t));
    return 0;
}

static void _set_opt(netdev_t *netdev, netopt_t opt, bool val, char *str_help)
{
    netopt_enable_t en = val ? NETOPT_ENABLE : NETOPT_DISABLE;

    netdev->driver->set(netdev, opt, &en, sizeof(en));
    printf("Successfully ");
    if (val) {
        printf("enabled ");
    }
    else {
        printf("disabled ");
    }
    printf("%s\n", str_help);
}

int crc_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0) {
        printf("usage: %s set <1|0>\n", argv[0]);
        return 1;
    }

    int tmp = atoi(argv[2]);

    _set_opt(netdev, NETOPT_INTEGRITY_CHECK, tmp, "CRC check");
    return 0;
}

int implicit_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0) {
        printf("usage: %s set <1|0>\n", argv[0]);
        return 1;
    }

    int tmp = atoi(argv[2]);

    _set_opt(netdev, NETOPT_FIXED_HEADER, tmp, "implicit header");
    return 0;
}

int payload_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0) {
        printf("usage: %s set <payload length>\n", argv[0]);
        return 1;
    }

    uint16_t tmp = atoi(argv[2]);

    netdev->driver->set(netdev, NETOPT_PDU_SIZE, &tmp, sizeof(tmp));
    printf("Successfully set payload to %i\n", tmp);
    return 0;
}
                            // fonction de renvoi de message

// fonction de callback pour le renvoi d'un message
static void _relay_send_cb(void *arg)
{
    // recupere le message a renvoyer et le netdev pour l'envois
    relay_job_t *job = (relay_job_t *)arg;
    netdev_t *netdev = &sx127x.netdev;

    //envois le message
    iolist_t iolist = {
        .iol_base = job->message,
        .iol_len = (strlen(job->message) + 1)
    };

    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }
    else {
        printf("renvois \"%s\" payload (%u bytes)\n",
               job->message, (unsigned)strlen(job->message) + 1);
        changeResendStat(fifo_msg, job->message, RESEND_OK);    // met a jour le statut du message dans la fifo pour eviter de le renvoyer a nouveau si on le recois a nouveau
    }

    job->busy = false;
}

// planifie le renvoi d'un message apres un delai
static void _schedule_relay_send(const char *msg, uint32_t delay_ms)
{
    for (size_t i = 0; i < RELAY_JOB_SLOTS; i++) {
        if (!relay_jobs[i].busy) {
            relay_jobs[i].busy = true;
            strncpy(relay_jobs[i].message, msg, sizeof(relay_jobs[i].message) - 1);
            relay_jobs[i].message[sizeof(relay_jobs[i].message) - 1] = '\0';

            event_deferred_callback_post(&relay_jobs[i].deferred,
                                         EVENT_PRIO_MEDIUM,
                                         ZTIMER_MSEC,
                                         delay_ms,
                                         _relay_send_cb,
                                         &relay_jobs[i]);
            return;
        }
    }

    puts("Relay queue full: drop message");
}

//fonction qui decide si un message doit etre renvoyer ou pas en fonction de son SNR et de son TTL et qui planifie son renvoi si besoin
void lora_mesh_renvois(char* message,int SNR){
    if(SNR > SNR_threshold){    // si le message est de bonne qualite, on ne le renvoie pas (il vien du'une carte proche)
        return;
    }
    //cherche le ttl dans le message
    int index = 0;
    while (message[index] != ':') {//on cherche le debut du numero
        index++;
    }
    index++;
    while(message[index] != ',' ){//on cherche la fin du numero et le debut du ttl
        index++;
        if(message[index] == ':'){
            return;
        }
    }
    index++;
    int cmp = 0;
    while(message[index + cmp] != ':'){//on cherche la fin du ttl et le debut du message
        cmp++;
    }
    // recupere le ttl 
    char* tmp = malloc(sizeof(char) * cmp);
    strncpy(tmp,message + index,cmp);
    int ttl = atoi(tmp);
    free(tmp);

    if(ttl > 0){    // si le ttl > 0 alors on planifie le renvoi du message en decrementant le ttl de 1 dans le message
        //on construit un nouveau message qui a un ttl de 1 de moins que le message recu
        ttl--;
        char ttl_char[11];
        sprintf(ttl_char,"%d",ttl);
        char new_message[32];
        strncpy(new_message,message,index);
        strncpy(new_message + index, ttl_char, strlen(ttl_char));
        strncpy(new_message + index + strlen(ttl_char), message + index + cmp, 32 - index - cmp);
        changeResendStat(fifo_msg,message,RESEND_DELAYED);
        _schedule_relay_send(new_message, RELAY_DELAY_MS);// planifie le renvoi du message avec le ttl decremente de 1 apres un delai de RELAY_DELAY_MS
    }
}

// fonction de callback pour les evenement du netdev, elle traite les evenement de reception de message et de fin d'emission de message
static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR) {
        msg_t msg;

        msg.type = MSG_TYPE_ISR;
        msg.content.ptr = dev;

        if (msg_send(&msg, _recv_pid) <= 0) {
            puts("gnrc_netdev: possibly lost interrupt.");
        }
    }
    else {
        size_t len;
        netdev_lora_rx_info_t packet_info;
        switch (event) {
        case NETDEV_EVENT_RX_STARTED:
            puts("Data reception started");
            break;

        case NETDEV_EVENT_RX_COMPLETE:
            len = dev->driver->recv(dev, NULL, 0, 0);
            dev->driver->recv(dev, message, len, &packet_info);

            // verfifie si le message na pas deja etais recu
            if(!isInFifo(fifo_msg,message)){
                pushFifo(fifo_msg, message);
                lora_mesh_renvois(message,(int)packet_info.snr);    // decide si le message doit etre renvoyer
            }

            // debut du traitement du message
            //cree un utilisateur et recupere sont pseudo dans le message
            User* user = malloc(sizeof(User));
            user->username = malloc(sizeof(char) * MAX_USER_NAME + 1);
            strncpy(user->username,message,MAX_USER_NAME);
            user->username[MAX_USER_NAME] = '\0';
            int index = 0;
            for(size_t i = MAX_USER_NAME; i < len; i++){
                if(message[i] == ':'){
                    index = i + 1;
                    break;
                }
            }

            // verifie si le message est un message priver ou un message de channel
            if(message[MAX_USER_NAME] == '@'){  // message pour un utilisateur
                if(message[MAX_USER_NAME + 1]=='*'){    // message pour tous les utilisateur, on ne verifie pas le pseudo du destinataire
                }
                else{
                    // verfie si le message nous est destiner
                    char recv_name[MAX_USER_NAME + 1];
                    strncpy(recv_name,message + MAX_USER_NAME + 1 ,MAX_USER_NAME);
                    recv_name[MAX_USER_NAME] = '\0';
                    if(strcmp(recv_name,pseudo) != 0){  // si il ne nous est pas destiner, on ne le traite pas
                        return;
                    } 
                }
            }else{  // message pour un channel
                // recupere le nom du channel dans le message
                int size = index - 6;
                char name_channel[size + 1];
                strncpy(name_channel,message + 5,size);
                name_channel[size] = '\0';
                for(int i = 0;i<MAX_CHANNEL;i++){   // verifie si on est abonner a ce channel, si on ne l'est pas, on ne traite pas le message
                    if(strcmp(name_channel,list_channel[i])==0){
                        break;
                    }
                    if(i == MAX_CHANNEL - 1){
                        return;
                    }
                }
            }

            // recupere le numero du message
            int cmp = 0;
            for(size_t i = index; i < len; i++){
                if(message[i] == ':'){
                    break;
                }
                if(message[i] == ','){
                    break;
                }
                cmp++;
            }
            char* tmp = malloc(sizeof(char) * cmp);
            strncpy(tmp,message + index,cmp);
            user->num = atoi(tmp);
            free(tmp);
            int i;
            // on verifie si l'utilisateur qui a envoye le message est deja dans la liste des utilisateur connu
            if((i = findElem(list_user,user->username)) != -1){ // si on le conais deja on le reinsert en tete pour qu'on enleve le plus ancien si la liste est pleine
                removeIndex(list_user,i);
                number_user--;
            }
            if(number_user == 3){   // cas de la liste pleine
                removeLast(list_user);
            }
            addHead(list_user,user);
            number_user++;
            // printf(
            //     "{Payload: \"%s\" (%d bytes), RSSI: %i, SNR: %i, TOA: %" PRIu32 "}\n",
            //     message, (int)len,
            //     packet_info.rssi, (int)packet_info.snr,
            //     sx127x_get_time_on_air((const sx127x_t *)dev, len));
            printf("%s\n",message);
            break;

        case NETDEV_EVENT_TX_COMPLETE:
            sx127x_set_sleep(&sx127x);
            puts("Transmission completed");
            break;

        case NETDEV_EVENT_CAD_DONE:
            break;

        case NETDEV_EVENT_TX_TIMEOUT:
            sx127x_set_sleep(&sx127x);
            break;

        default:
            printf("Unexpected netdev event received: %d\n", event);
            break;
        }
    }
}

                            // fonction fournisse

void *_recv_thread(void *arg)
{
    (void)arg;

    static msg_t _msg_q[SX127X_LORA_MSG_QUEUE];

    msg_init_queue(_msg_q, SX127X_LORA_MSG_QUEUE);

    while (1) {
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR) {
            netdev_t *dev = msg.content.ptr;
            dev->driver->isr(dev);
        }
        else {
            puts("Unexpected msg type");
        }
    }
}


int init_sx1272_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
	    sx127x.params = sx127x_params[0];
	    netdev_t *netdev = &sx127x.netdev;

	    netdev->driver = &sx127x_driver;

        netdev->event_callback = _event_cb;

//        printf("%8x\n", (unsigned int)netdev->driver);
//        printf("%8x\n", (unsigned int)netdev->driver->init);

	    if (netdev->driver->init(netdev) < 0) {
	        puts("Failed to initialize SX127x device, exiting");
	        return 1;
	    }

	    _recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
	                              THREAD_CREATE_STACKTEST, _recv_thread, NULL,
	                              "recv_thread");

	    if (_recv_pid <= KERNEL_PID_UNDEF) {
	        puts("Creation of receiver thread failed");
	        return 1;
	    }
        puts("5");

        list_user = initLinklist((FuncFree)free_user, (FuncCompare)compare_user, (FunPrint)print_user);
        list_channel = malloc(sizeof(char*) * 10);
        for(int i = 0;i< 10;i++){
            list_channel[i] = malloc(sizeof(char)*10);
            list_channel[i][0] = '\0';
        }

        fifo_msg = initFifo();
        return 0;
}

                            // fonction de la gestion des channel

// abonnement a un channel, on ajoute le nom du channel a la liste des channel auquel on est abonner
int subscribe_cmd(int argc, char** argv){
    if(argc < 2){
        return 1;
    }
    //recupere le nom du channel a abonner
    char* name_channel = argv[1];
    int index = -1;
    for(int i = 0;i<MAX_CHANNEL;i++){   // verifie si on est deja abonner a ce channel
        if(strcmp(name_channel,list_channel[i])==0){
            printf("Already sub\n");
            return 1;
        }
        if(list_channel[i][0] == '\0'){
            index = i;
        }
    }
    if(index == -1){    // cas de la liste pleine
        printf("Chanel list full\n");
        return 1;
    }
    strcpy(list_channel[index],name_channel);   // insert le nom du channel dans la liste des channel auquel on est abonner
    return 0;
}

// desabonnement d'un channel, on supprime le nom du channel de la liste des channel auquel on est abonner
int unsubscribe_cmd(int argc, char** argv){
    if(argc < 2){
        return 1;
    }
    // recupere le nom du channel a desabonner
    char* name_channel = argv[1];
    for(int i = 0;i<MAX_CHANNEL;i++){
        if(strcmp(name_channel,list_channel[i]) == 0){
            list_channel[i][0] = '\0';  // supprime le nom du channel de la liste des channel auquel on est abonner en mettant le premier caractere a '\0'
            return 0;
        }
    }
    printf("Chanel not found\n");
    return 1;
}

// affiche la liste des channel auquel on est abonner
int print_channel_cmd(int argc, char** argv){
    if(argc < 1){
        return 1;
    }
    (void)argv[1];
    for(int i = 0;i<MAX_CHANNEL;i++){
        if(strcmp(list_channel[i],"\0") != 0){
            printf("%s\n",list_channel[i]);
        }
    }
    return 0;
}

// affiche la liste des message
int printFifo_cmd(int argc, char** argv){
    if(argc < 1){
        return 1;
    }
    (void)argv[1];
    printFifo(fifo_msg);
    return 0;
}

// fonction de test car on a pas d'ami
int test_msg_cmd(int argc, char** argv){
    (void)argc;
    size_t len = (size_t)strlen(argv[1]) + 1;
            char message[32];
            strncpy(message,argv[1],32);
            if(!isInFifo(fifo_msg,message)){
                pushFifo(fifo_msg, message);
                lora_mesh_renvois(message,(int)SNR_threshold - 1);
            }
            User* user = malloc(sizeof(User));
            user->username = malloc(sizeof(char) * MAX_USER_NAME + 1);
            strncpy(user->username,message,MAX_USER_NAME);
            user->username[MAX_USER_NAME] = '\0';
            int index = 0;
            for(size_t i = MAX_USER_NAME; i < len; i++){
                if(message[i] == ':'){
                    index = i + 1;
                    break;
                }
            }
            if(message[MAX_USER_NAME] == '@'){
                if(message[MAX_USER_NAME + 1]=='*'){
                }
                else{
                    char recv_name[MAX_USER_NAME + 1];
                    strncpy(recv_name,message + MAX_USER_NAME + 1 ,MAX_USER_NAME);
                    recv_name[MAX_USER_NAME] = '\0';
                    if(strcmp(recv_name,pseudo) != 0){
                        return 1;
                    } 
                }
            }else{
                int size = index - 6;
                char name_channel[size + 1];
                strncpy(name_channel,message + 5,size);
                name_channel[size] = '\0';
                for(int i = 0;i<MAX_CHANNEL;i++){
                    if(strcmp(name_channel,list_channel[i])==0){
                        break;
                    }
                    if(i == MAX_CHANNEL - 1){
                        return 1;
                    }
                }
            }
            int cmp = 0;
            for(size_t i = index; i < len; i++){
                if(message[i] == ':'){
                    break;
                }
                cmp++;
            }
            char* tmp = malloc(sizeof(char) * cmp);
            strncpy(tmp,message + index,cmp);
            user->num = atoi(tmp);
            int i;
            if((i = findElem(list_user,user->username)) != -1){
                removeIndex(list_user,i);
                number_user--;
            }
            if(number_user == 3){
                removeLast(list_user);
            }
            addHead(list_user,user);
            number_user++;
            // printf(
            //     "{Payload: \"%s\" (%d bytes), RSSI: %i, SNR: %i, TOA: %" PRIu32 "}\n",
            //     message, (int)len,
            //     packet_info.rssi, (int)packet_info.snr,
            //     sx127x_get_time_on_air((const sx127x_t *)dev, len));
            printf("%s\n",message);
            return 0;
}

int threshold_cmd(int argc, char** argv){
    if(argc < 1){
        return 1;
    }

    SNR_threshold = (int)argv[1];
    return 0;
    
}

static const shell_command_t shell_commands[] = {
	{ "init",    "Initialize SX1272",     					init_sx1272_cmd },
	{ "setup",    "Initialize LoRa modulation settings",     lora_setup_cmd },
    { "implicit", "Enable implicit header",                  implicit_cmd },
    { "crc",      "Enable CRC",                              crc_cmd },
    { "payload",  "Set payload length (implicit header)",    payload_cmd },
    { "random",   "Get random number from sx127x",           random_cmd },
    { "syncword", "Get/Set the syncword",                    syncword_cmd },
    { "rx_timeout", "Set the RX timeout",                    rx_timeout_cmd },
    { "channel",  "Get/Set channel frequency (in Hz)",       channel_cmd },
    { "register", "Get/Set value(s) of registers of sx127x", register_cmd },
    { "send",     "Send raw payload string",                 send_cmd },
    { "mp",      "Send raw payload string in a user",        mp_cmd },    
    { "listen",   "Start raw payload listener",              listen_cmd },
    { "reset",    "Reset the sx127x device",                 reset_cmd },
    { "user_list", "Show user list",                         print_list_user},
    { "subscribe", "Join a channel" ,                        subscribe_cmd},
    { "unsubscribe", "Unjoin a channel",                     unsubscribe_cmd},
    { "channel_list", "Show all the channel",                print_channel_cmd},
    { "msg_list", "Show all the message in the fifo",        printFifo_cmd},
    { "threshold", "Change SNR threshold",                   threshold_cmd},
    { "test_msg", "Test the message treatment",              test_msg_cmd},
    { NULL, NULL, NULL }
};

int main(void) {

    //init_sx1272_cmd(0,NULL);

    /* start the shell */
    puts("Initialization successful - starting the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}

