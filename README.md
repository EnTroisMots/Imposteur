# Jeu Réseau — Serveur & Client

Ce projet consiste en un serveur multijoueur et un client réseau permettant à plusieurs joueurs de participer à un jeu via TCP/IP.

---

## 🖥️ Lancement du serveur

### Compilation

```bash
gcc serveur.c -o serveur
```

### Exécution

```bash
./serveur [-p PORT] [-j NB_JOUEURS] [-r NB_TOURS] [-t TIMING_PLAY] [-T TIMING_CHOICE]
```

### Options

- `-p PORT` : port d'écoute du serveur (par défaut : 12345)
- `-j NB_JOUEURS` : nombre de joueurs requis pour commencer la partie
- `-r NB_TOURS` : nombre de tours à jouer
- `-t TIMING_PLAY` : temps (en secondes) accordé pour jouer un tour
- `-T TIMING_CHOICE` : temps (en secondes) accordé pour faire un choix

---

## 🎮 Lancement du client

### Compilation

```bash
make
```

### Exécution

```bash
./client -s IP [-p PORT]
```

### Options

- `-s IP` : adresse IP du serveur
- `-p PORT` : port du serveur (par défaut : 12345)

---

## ✅ Exemple

### Serveur

```bash
./serveur -p 8080 -j 3 -r 5 -t 10 -T 5
```

### Client

```bash
./client -s 127.0.0.1 -p 8080
```

---

## 💡 Remarques

- Le serveur attend que tous les joueurs soient connectés avant de lancer la partie.
- Assurez-vous que le port utilisé est ouvert et non bloqué par un pare-feu.

## 📦 Dépendances
Le client utilise la bibliothèque SDL2 ainsi que SDL2_ttf pour l'affichage graphique. Avant de compiler, vous devez vous assurer que ces bibliothèques sont installées sur votre système.

## 🔧 Sous Ubuntu
sudo apt update
sudo apt install libsdl2-dev libsdl2-ttf-dev