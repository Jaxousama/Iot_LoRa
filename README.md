# Build 
## Compiler

```bash
make -j 4 flash
```

## Usage

Rejoindre le terminal de la Board avec tio

`Help` pour afficher les commandes

utilisation de base pour vérifier la communication entre 2 cartes :

* On receving board :
```
> setup 125 12 5
> channel set 868000000
> listen
```

* On seending board :
```
> setup 125 12 5
> channel set 868000000
> send * Coucou tout le monde
```

* On receiving board you should see :
```
User@*:count,3:Coucou tout le monde
```

# Features

- Envoi de message privé, pour tous ou à un channels au formats LoRaMesh
- Abonnement à des channels
- Afficher la liste des channels
- Afficher la liste des utilisateurs avec lesquels on discute
- Fait transiter les messages possédant un TTL
- Affichage des derniers messages reçus avec Status Pending, Pas transiter ou - Transiter

# Commands

Commande que l'on a rajoutée utilisable sur la board :
- send : Permet d'envoyer un message dans un salon `usage: send <target channel> <payload>`
- mp : Permet d'envoyer un message privé à une target `usage : mp <target> <payload>`
- subscribe : Permet de s'abonner à un channel `usage: subscribe <channel name>`
- unsubscribe : Permet de retirer son abonnement à un channel `usage: unsubscribe <channel name>`
- channel_list : Permet d'afficher la liste de channel abonnés
- msg_list : Permet d'afficher la liste des messages reçus
- threshold : Permet d'afficher le Threshold pour le SNR `usage: threshold SNR`
- test_msg : Permet de tester la réception de message
- change_pseudo : Permet de changer de pseudo `usage: change_pseudo pseudo`
