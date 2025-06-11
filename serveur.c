// gcc serveur.c -o serveur

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>

#define PORT 5000
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_LOGIN_LENGTH 16
#define MIN_LOGIN_LENGTH 3
#define DELAY_BETWEEN_LOGINS 1 // en secondes
#define GAME_RESTART_DELAY 10  // délai de relancement automatique en secondes

typedef enum {
    STATE_WAITING_PLAYERS,
    STATE_GAME_STARTED,
    STATE_WAITING_PLAY,
    STATE_WAITING_CHOICE,
    STATE_ROUND_ENDED,
    STATE_VOTING_PHASE,
    STATE_GAME_FINISHED    // nouvel état pour gérer la fin de partie
} game_state_t;

typedef struct client {
    int fd;
    int id;
    char login[MAX_LOGIN_LENGTH + 1];
    int logged_in;
    int score;
    int points_gained;
    char current_word[BUFFER_SIZE];
    char said_word[BUFFER_SIZE];
    char accused_login[MAX_LOGIN_LENGTH + 1];
    int is_imposter;
    struct sockaddr_in addr;
    struct client* next;
} client_t;

typedef struct {
    char* word;
    char* imposter_word;
} round_words_t;

// Liste des mots déjà utilisés
typedef struct used_word {
    char word[BUFFER_SIZE];
    struct used_word* next;
} used_word_t;

client_t* clients = NULL;
used_word_t* used_words = NULL;
int client_counter = 0;
int expected_players = 3;
int total_rounds = 3;
int current_round = 0;
int play_timeout = 30;
int choice_timeout = 60;
game_state_t game_state = STATE_WAITING_PLAYERS;
time_t game_end_time = 0;  // timestamp de fin de partie
void broadcast_message(const char* msg);
time_t current_timeout_start = 0;
int timeout_active = 0;
client_t* timeout_client = NULL;
int current_word_pair_index = 0;

round_words_t round_words[] = {
    {"Pomme", "Orange"},
    {"Chien", "Chat"},
    {"Lion", "Tigre"},
    {"Voiture", "Camion"},
    {"Bateau", "Navire"},
    {"Avion", "Hélicoptère"},
    {"Train", "Métro"},
    {"Vélo", "Moto"},
    {"Maison", "Appartement"},
    {"École", "Université"},
    {"Hôpital", "Clinique"},
    {"Restaurant", "Cafétéria"},
    {"Pizza", "Burger"},
    {"Pâtes", "Riz"},
    {"Salade", "Soupe"},
    {"Biere", "Vin"},
    {"Café", "Thé"},
    {"Eau", "Jus"},
    {"Football", "Rugby"},
    {"Basket", "Volley"},
    {"Tennis", "Badminton"},
    {"Piano", "Guitare"},
    {"Violon", "Violoncelle"},
    {"Trompette", "Saxophone"},
    {"Peinture", "Dessin"},
    {"Photo", "Vidéo"},
    {"Livre", "Magazine"},
    {"Journal", "Blog"},
    {"Film", "Série"},
    {"Acteur", "Actrice"},
    {"Chanteur", "Musicien"},
    {"Danseur", "Acrobate"},
    {"Docteur", "Infirmier"},
    {"Policier", "Gendarme"},
    {"Pompier", "Secouriste"},
    {"Astronaute", "Pilote"},
    {"Roi", "Reine"},
    {"Président", "Ministre"},
    {"Été", "Hiver"},
    {"Printemps", "Automne"},
    {"Soleil", "Lune"},
    {"Étoile", "Planète"},
    {"Montagne", "Colline"},
    {"Rivière", "Fleuve"},
    {"Lac", "Océan"},
    {"Forêt", "Jungle"},
    {"Désert", "Savane"},
    {"Neige", "Glace"},
    {"Pluie", "Orage"}
};

#define NUM_WORD_PAIRS (sizeof(round_words) / sizeof(round_words[0]))

int count_logged_in_players(void);
void assign_words_to_players(void);
void start_new_round(void);
void handle_play_command(client_t* client, const char* word);
void handle_choice_command(client_t* client, const char* accused_login);
void handle_login_command(client_t* client, const char* login);
void send_message(int fd, const char* msg);

void error(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void add_used_word(const char* word) {
    used_word_t* new_word = malloc(sizeof(used_word_t));
    if (!new_word) error("malloc");
    strncpy(new_word->word, word, BUFFER_SIZE-1);
    new_word->next = used_words;
    used_words = new_word;
}

int is_word_used(const char* word) {
    for (used_word_t* w = used_words; w; w = w->next) {
        if (strcasecmp(w->word, word) == 0) {
            return 1;
        }
    }
    return 0;
}

void clear_used_words() {
    used_word_t* current = used_words;
    while (current != NULL) {
        used_word_t* next = current->next;
        free(current);
        current = next;
    }
    used_words = NULL;
}

void start_timeout(client_t* client) {
    current_timeout_start = time(NULL);
    timeout_active = 1;
    timeout_client = client;
}

void stop_timeout() {
    timeout_active = 0;
    timeout_client = NULL;
    current_timeout_start = 0;
}

void handle_timeout_expiry() {
    if (game_state == STATE_WAITING_PLAY && timeout_client) {
        // Timeout pendant la phase de jeu - passer au joueur suivant
        printf("Play timeout expired for player '%s'\n", timeout_client->login);
        
        // Envoyer un message d'alerte
        char alert_msg[BUFFER_SIZE];
        snprintf(alert_msg, sizeof(alert_msg), 
                "/info ALERT:Temps écoulé pour %s, passage au joueur suivant\n", 
                timeout_client->login);
        broadcast_message(alert_msg);
        
        // Marquer ce joueur comme ayant "dit" un mot vide (pour qu'il soit ignoré)
        strcpy(timeout_client->said_word, "Trop_lent");

        char say_msg[BUFFER_SIZE + MAX_LOGIN_LENGTH + 20];
        snprintf(say_msg, sizeof(say_msg), "/info SAY:%s:%s\n", timeout_client->login, timeout_client->said_word);
        broadcast_message(say_msg);
        
        // Vérifier si tous les joueurs ont joué
        int all_played = 1;
        for (client_t* c = clients; c; c = c->next) {
            if (c->logged_in && strlen(c->said_word) == 0) {
                all_played = 0;
                break;
            }
        }
        
        if (all_played) {
            // Passer au round suivant
            game_state = STATE_ROUND_ENDED;
            stop_timeout();
            sleep(2);
            start_new_round();
        } else {
            // Passer au joueur suivant
            client_t* next_player = timeout_client->next;
            while (next_player && (!next_player->logged_in || strlen(next_player->said_word) > 0)) {
                next_player = next_player->next;
            }
            if (!next_player) {
                next_player = clients;
                while (next_player && (!next_player->logged_in || strlen(next_player->said_word) > 0)) {
                    next_player = next_player->next;
                }
            }
            
            if (next_player) {
                char wait_msg[BUFFER_SIZE];
                snprintf(wait_msg, sizeof(wait_msg), "/info WAIT:%s:PLAY\n", next_player->login);
                broadcast_message(wait_msg);
                
                char play_msg[BUFFER_SIZE];
                snprintf(play_msg, sizeof(play_msg), "/play %d\n", play_timeout);
                send_message(next_player->fd, play_msg);
                
                // Mettre à jour le timeout pour le joueur suivant
                start_timeout(next_player);
            } else {
                // Aucun joueur valide trouvé, désactiver le timeout
                stop_timeout();
            }
        }
        
    } else if (game_state == STATE_VOTING_PHASE) {
        // Timeout pendant la phase de vote - terminer le jeu
        printf("Choice timeout expired during voting phase\n");
        
        char alert_msg[BUFFER_SIZE];
        snprintf(alert_msg, sizeof(alert_msg), 
                "/info ALERT:Temps de vote écoulé, fin de partie\n");
        broadcast_message(alert_msg);
        
        // Marquer la fin de partie
        game_state = STATE_GAME_FINISHED;
        game_end_time = time(NULL);
        
        // Envoyer les résultats avec les scores actuels
        char result_msg[BUFFER_SIZE] = "/info RESULT:";
        for (client_t* c = clients; c; c = c->next) {
            if (c->logged_in) {
                char player_result[100];
                snprintf(player_result, sizeof(player_result), "%s:%d+0:", c->login, c->score);
                strcat(result_msg, player_result);
            }
        }
        result_msg[strlen(result_msg)-1] = '\0';
        strcat(result_msg, "\n");
        broadcast_message(result_msg);
        
        printf("Game finished due to timeout. Auto-restart scheduled in %d seconds.\n", GAME_RESTART_DELAY);

        stop_timeout();
    }
}

void check_timeout() {
    if (timeout_active && current_timeout_start > 0) {
        time_t current_time = time(NULL);
        time_t elapsed = current_time - current_timeout_start;
        
        if (game_state == STATE_WAITING_PLAY && elapsed >= play_timeout) {
            handle_timeout_expiry();
        } else if (game_state == STATE_VOTING_PHASE && elapsed >= choice_timeout) {
            handle_timeout_expiry();
        }
    }
}

void reset_game_state() {
    // Remet à zéro les scores et états des joueurs
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in) {
            c->points_gained = 0;
            c->is_imposter = 0;
            memset(c->current_word, 0, sizeof(c->current_word));
            memset(c->said_word, 0, sizeof(c->said_word));
            memset(c->accused_login, 0, sizeof(c->accused_login));
        }
    }
    
    // Remet à zéro l'état du jeu
    current_round = 0;
    game_state = STATE_WAITING_PLAYERS;
    game_end_time = 0;
    clear_used_words();
    
    printf("Game state reset. Waiting for players to restart...\n");
}

void check_disconnection_restart() {
    int logged_in = count_logged_in_players();
    
    // Si on est en cours de jeu et qu'on passe en dessous du nombre de joueurs requis
    if (game_state != STATE_WAITING_PLAYERS && game_state != STATE_GAME_FINISHED && 
        logged_in < expected_players) {
        
        printf("Not enough players (%d/%d), resetting game...\n", logged_in, expected_players);
        
        // Envoyer un message d'info aux joueurs
        char info_msg[BUFFER_SIZE];
        snprintf(info_msg, sizeof(info_msg), 
                "/info ALERT:Partie annulée - Pas assez de joueurs (%d/%d)\n", 
                logged_in, expected_players);
        broadcast_message(info_msg);
        
        // Réinitialiser le jeu
        reset_game_state();
        
        // Envoyer le message de login pour permettre aux joueurs de se reconnecter
        char login_msg[BUFFER_SIZE];
        snprintf(login_msg, sizeof(login_msg), "/ret LOGIN:000\n");
        broadcast_message(login_msg);
    }
}

void check_auto_restart() {
    if (game_state == STATE_GAME_FINISHED && game_end_time > 0) {
        time_t current_time = time(NULL);
        if (current_time - game_end_time >= GAME_RESTART_DELAY) {
            // Vérifier si on a encore assez de joueurs connectés
            int logged_in = count_logged_in_players();
            if (logged_in == expected_players) {
                printf("Auto-restarting game after %d seconds...\n", GAME_RESTART_DELAY);
                
                // Redémarrer le jeu directement sans message
                reset_game_state();
                game_state = STATE_GAME_STARTED;
                assign_words_to_players();
                start_new_round();
            } else {
                // Pas assez de joueurs, remettre en attente
                reset_game_state();
                char wait_msg[BUFFER_SIZE];
                snprintf(wait_msg, sizeof(wait_msg), "/ret LOGIN:000\n");
                broadcast_message(wait_msg);
            }
        }
    }
}

int is_valid_login(const char* login) {
    if (strlen(login) < MIN_LOGIN_LENGTH || strlen(login) > MAX_LOGIN_LENGTH) {
        return 0;
    }
    
    for (int i = 0; login[i]; i++) {
        if (login[i] == ':') {
            return 0;
        }
        if (!isalnum(login[i]) && login[i] != '_' && login[i] != '-' && login[i] != ' ') {
            return 0;
        }
    }
    return 1;
}

int is_login_available(const char* login) {
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in && strcmp(c->login, login) == 0) {
            return 0;
        }
    }
    return 1;
}

void add_client(int fd, struct sockaddr_in addr) {
    client_t* new_client = malloc(sizeof(client_t));
    if (!new_client) error("malloc");

    new_client->fd = fd;
    new_client->id = client_counter++;
    new_client->logged_in = 0;
    new_client->score = 0;
    new_client->is_imposter = 0;
    memset(new_client->login, 0, sizeof(new_client->login));
    memset(new_client->current_word, 0, sizeof(new_client->current_word));
    memset(new_client->said_word, 0, sizeof(new_client->said_word));
    memset(new_client->accused_login, 0, sizeof(new_client->accused_login));
    new_client->addr = addr;
    new_client->next = clients;
    clients = new_client;

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
    printf("New client %d from %s:%d\n", new_client->id, ip, ntohs(addr.sin_port));
}

void remove_client(int fd) {
    client_t** cur = &clients;
    while (*cur) {
        if ((*cur)->fd == fd) {
            client_t* tmp = *cur;

            // Sauvegarde du nom du joueur s'il était loggé
            char login[MAX_LOGIN_LENGTH + 1];
            int was_logged_in = tmp->logged_in;
            strncpy(login, tmp->login, MAX_LOGIN_LENGTH);
            login[MAX_LOGIN_LENGTH] = '\0';

            *cur = (*cur)->next;
            close(tmp->fd);
            free(tmp);

            // Envoi du message ALERT si le joueur était loggé
            if (was_logged_in) {
                char alert_msg[BUFFER_SIZE];
                snprintf(alert_msg, sizeof(alert_msg), "/info ALERT:Le joueur '%s' a quitté la partie.\n", login);
                broadcast_message(alert_msg);

                check_disconnection_restart();
            }

            return;
        }
        cur = &(*cur)->next;
    }
}

client_t* find_client(int fd) {
    for (client_t* c = clients; c; c = c->next) {
        if (c->fd == fd) return c;
    }
    return NULL;
}

client_t* find_client_by_login(const char* login) {
    for (client_t* c = clients; c; c = c->next) {
        if (strcmp(c->login, login) == 0) return c;
    }
    return NULL;
}

void send_message(int fd, const char* msg) {
    if (send(fd, msg, strlen(msg), 0) < 0) {
        perror("send");
    }
}

void broadcast_message(const char* msg) {
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in) {
            send_message(c->fd, msg);
        }
    }
}

void broadcast_message_except(const char* msg, int except_fd) {
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in && c->fd != except_fd) {
            send_message(c->fd, msg);
        }
    }
}

int count_logged_in_players() {
    int count = 0;
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in) count++;
    }
    return count;
}

void assign_words_to_players() {
    // Sélectionner une paire de mots aléatoire
    current_word_pair_index = rand() % NUM_WORD_PAIRS; // Mettre à jour l'index global
    char* word = round_words[current_word_pair_index].word;
    char* imposter_word = round_words[current_word_pair_index].imposter_word;

    // Sélectionner l'imposteur aléatoirement
    int imposter_index = rand() % expected_players;
    int i = 0;
    
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in) {
            c->is_imposter = (i == imposter_index);
            if (c->is_imposter) {
                strcpy(c->current_word, imposter_word);
            } else {
                strcpy(c->current_word, word);
            }
            memset(c->accused_login, 0, sizeof(c->accused_login));
            i++;
        }
    }

    // Envoyer les mots attribués aux joueurs
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in) {
            char assign_msg[BUFFER_SIZE + 10];
            snprintf(assign_msg, sizeof(assign_msg), "/assign %s\n", c->current_word);
            send_message(c->fd, assign_msg);
        }
    }
}

void start_new_round() {
    current_round++;
    if (current_round > total_rounds) {
        // All rounds completed, start voting phase
        game_state = STATE_VOTING_PHASE;
        
        // Reset accusations for voting
        for (client_t* c = clients; c; c = c->next) {
            memset(c->accused_login, 0, sizeof(c->accused_login));
        }
        
        // Start voting
        char choice_msg[BUFFER_SIZE];
        snprintf(choice_msg, sizeof(choice_msg), "/choice %d\n", choice_timeout);
        
        for (client_t* c = clients; c; c = c->next) {
            if (c->logged_in) {
                send_message(c->fd, choice_msg);
            }
        }

        current_timeout_start = time(NULL);
	    timeout_active = 1;
	    timeout_client = NULL;

        return;
    }

    // Clear said words but keep used words list for the entire game
    for (client_t* c = clients; c; c = c->next) {
        memset(c->said_word, 0, sizeof(c->said_word));
    }

    // Send game info
    char game_info[BUFFER_SIZE];
    snprintf(game_info, sizeof(game_info), "/info GAME:%d/%d:%d:%d:%d\n", 
            current_round, total_rounds, expected_players, play_timeout, choice_timeout);
    broadcast_message(game_info);


    // Start with first player
    client_t* first_player = clients;
    while (first_player && !first_player->logged_in) {
        first_player = first_player->next;
    }

    if (first_player) {
        char wait_msg[BUFFER_SIZE];
        snprintf(wait_msg, sizeof(wait_msg), "/info WAIT:%s:PLAY\n", first_player->login);
        broadcast_message(wait_msg);
        
        char play_msg[BUFFER_SIZE];
        snprintf(play_msg, sizeof(play_msg), "/play %d\n", play_timeout);
        send_message(first_player->fd, play_msg);
        
        game_state = STATE_WAITING_PLAY;
        
        // AJOUTER CETTE LIGNE :
        start_timeout(first_player);
    }
}

void handle_play_command(client_t* client, const char* word) {
    if (game_state != STATE_WAITING_PLAY) {
        send_message(client->fd, "/ret PLAY:202\n");
        return;
    }

    // Check if it's this client's turn
    client_t* current_player = clients;
    while (current_player && 
          (!current_player->logged_in || strlen(current_player->said_word) > 0)) {
        current_player = current_player->next;
    }

    if (!current_player || client->fd != current_player->fd) {
        send_message(client->fd, "/ret PLAY:102\n");
        return;
    }

    // Check if word is the same as assigned
    if (strcasecmp(word, client->current_word) == 0) {
        send_message(client->fd, "/ret PLAY:104\n");
        return;
    }

    // Check if word was already said in this game
    if (strcmp(word, "Trop_lent") != 0 && is_word_used(word)) {
        send_message(client->fd, "/ret PLAY:103\n");
        return;
    }

    // Si on arrive ici, la commande /play est valide
    send_message(client->fd, "/ret PLAY:000\n");

    stop_timeout();

    strncpy(client->said_word, word, BUFFER_SIZE-1);

    if (strcmp(word, "   ") != 0) {
    	add_used_word(word); // Add to global used words list
    }
    
    // Broadcast that this player said this word
    char say_msg[BUFFER_SIZE + MAX_LOGIN_LENGTH + 20];
    snprintf(say_msg, sizeof(say_msg), "/info SAY:%s:%s\n", client->login, client->said_word);
    broadcast_message(say_msg);

    // Check if all players have played
    int all_played = 1;
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in && strlen(c->said_word) == 0) {
            all_played = 0;
            break;
        }
    }

    if (all_played) {
        // Move to next round
        game_state = STATE_ROUND_ENDED;
        
        // Wait a bit before starting next round
        sleep(2);
        start_new_round();
    } else {
        // Next player's turn
        client_t* next_player = current_player->next;
        while (next_player && !next_player->logged_in) {
            next_player = next_player->next;
        }
        if (!next_player) next_player = clients;

        char wait_msg[BUFFER_SIZE];
        snprintf(wait_msg, sizeof(wait_msg), "/info WAIT:%s:PLAY\n", next_player->login);
        broadcast_message(wait_msg);
        
        char play_msg[BUFFER_SIZE];
        snprintf(play_msg, sizeof(play_msg), "/play %d\n", play_timeout);
        send_message(next_player->fd, play_msg);

        start_timeout(next_player);
    }
}

void handle_choice_command(client_t* client, const char* accused_login) {
    if (game_state != STATE_VOTING_PHASE) {
        send_message(client->fd, "/ret CHOICE:202\n");
        return;
    }

    // Check if accusing self
    if (strcmp(accused_login, client->login) == 0) {
        send_message(client->fd, "/ret CHOICE:105\n");
        return;
    }

    // Check if accused player exists
    client_t* accused = find_client_by_login(accused_login);
    if (!accused) {
        send_message(client->fd, "/ret CHOICE:106\n");
        return;
    }

    strncpy(client->accused_login, accused_login, MAX_LOGIN_LENGTH);

    if (timeout_active) {
        stop_timeout();
    }
    
    // Broadcast the accusation
    char choice_msg[BUFFER_SIZE];
    snprintf(choice_msg, sizeof(choice_msg), "/info CHOICE:%s:%s\n", client->login, accused_login);
    broadcast_message(choice_msg);

    // Check if all players have chosen
    int all_chosen = 1;
    for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in && strlen(c->accused_login) == 0) {
            all_chosen = 0;
            break;
        }
    }

    if (all_chosen) {
        // End game and determine final results
        
        // Find who was imposter most often
        int max_imposter_count = 0;
        client_t* likely_imposter = NULL;
        
        for (client_t* c = clients; c; c = c->next) {
            if (c->logged_in) {
                int accusation_count = 0;
                for (client_t* accuser = clients; accuser; accuser = accuser->next) {
                    if (accuser->logged_in && strcmp(accuser->accused_login, c->login) == 0) {
                        accusation_count++;
                    }
                }
                
                if (accusation_count > max_imposter_count) {
                    max_imposter_count = accusation_count;
                    likely_imposter = c;
                }
            }
        }
        
        // Calculate points gained in this round (1 if correctly accused imposter, 0 otherwise)
        for (client_t* c = clients; c; c = c->next) {
            if (c->logged_in) {
                int points_gained = 0;
                if (likely_imposter && strcmp(c->accused_login, likely_imposter->login) == 0) {
                    points_gained = 1;
                    c->score += points_gained;
                }
                c->points_gained = points_gained; // Store temporarily for result message
            }
        }
        
        // Send final results in the requested format
        char result_msg[BUFFER_SIZE] = "/info RESULT:";
        for (client_t* c = clients; c; c = c->next) {
            if (c->logged_in) {
                char player_result[100];
                snprintf(player_result, sizeof(player_result), "%s:%d+%d:", c->login, c->score - c->points_gained, c->points_gained);
                strcat(result_msg, player_result);
            }
        }
        // Remove last ':' and add newline
        result_msg[strlen(result_msg)-1] = '\0';
        strcat(result_msg, "\n");
        broadcast_message(result_msg);
        
        // Announce likely imposter and correct words
        if (likely_imposter) {
	        char answer_msg[BUFFER_SIZE];
	        snprintf(answer_msg, sizeof(answer_msg), 
	                "/info ANSWER:%s:%s:%s\n", likely_imposter->login, 
	                round_words[current_word_pair_index].imposter_word, 
	                round_words[current_word_pair_index].word);
	        broadcast_message(answer_msg);
	    }
        
        // Marquer la fin de partie et enregistrer l'heure
        game_state = STATE_GAME_FINISHED;
        game_end_time = time(NULL);
        
        printf("Game finished. Auto-restart scheduled in %d seconds.\n", GAME_RESTART_DELAY);
    }
}

void handle_login_command(client_t* client, const char* login) {
    if (client->logged_in) {
        send_message(client->fd, "/ret LOGIN:202\n");
        return;
    }

    if (!is_valid_login(login)) {
        send_message(client->fd, "/ret LOGIN:201\n");
        return;
    }

    if (!is_login_available(login)) {
        send_message(client->fd, "/ret LOGIN:101\n");
        return;
    }

    strncpy(client->login, login, MAX_LOGIN_LENGTH);
    client->logged_in = 1;
    send_message(client->fd, "/ret LOGIN:000\n");


    int tempo = 1;
    // Envoyer la liste des joueurs déjà logués au nouveau client seulement
	for (client_t* c = clients; c; c = c->next) {
        if (c->logged_in && c->fd != client->fd) { // Ne pas envoyer le nouveau client lui-même
            char info_msg[BUFFER_SIZE];
            snprintf(info_msg, sizeof(info_msg), "/info LOGIN:%d/%d:%s\n", 
                    tempo, expected_players, c->login);
            send_message(client->fd, info_msg);
            tempo++;
       }
	}
    tempo = 1;
    
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client->addr.sin_addr), ip, INET_ADDRSTRLEN);
    printf("Client %d (%s:%d) logged in as '%s'\n", 
           client->id, ip, ntohs(client->addr.sin_port), client->login);

    // Envoyer UN SEUL message avec le nouveau joueur seulement
    int logged_in = count_logged_in_players();
    char info_msg[BUFFER_SIZE];
    snprintf(info_msg, sizeof(info_msg), "/info LOGIN:%d/%d:%s\n", 
            logged_in, expected_players, client->login);
    broadcast_message(info_msg);

    // Check if all players are connected and logged in to start game
    if (logged_in == expected_players && game_state == STATE_WAITING_PLAYERS) {
        game_state = STATE_GAME_STARTED;
        assign_words_to_players();
        start_new_round();
    }
}

void handle_client_command(int fd, char* buffer) {
    client_t* client = find_client(fd);
    if (!client) return;

    buffer[strcspn(buffer, "\r\n")] = 0;

    if (strncmp(buffer, "/login ", 7) == 0) {
        handle_login_command(client, buffer + 7);
    } else if (strncmp(buffer, "/play ", 6) == 0) {
        handle_play_command(client, buffer + 6);
    } else if (strncmp(buffer, "/choice ", 8) == 0) {
        handle_choice_command(client, buffer + 8);
    } else {
        send_message(fd, "/ret PROTO:201\n");
    }
}

int main(int argc, char* argv[]) {
    int server_fd, port = PORT;
    struct sockaddr_in server_addr;
    srand(time(NULL));

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-j") == 0 && i+1 < argc) {
            expected_players = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) {
            total_rounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) {
            play_timeout = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-T") == 0 && i+1 < argc) {
            choice_timeout = atoi(argv[++i]);
        }
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) error("socket");

    printf("Socket created successfully! (%d)\n", server_fd);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        error("bind");

    printf("Socket bound successfully (port %d)!\n", port);

    if (listen(server_fd, 5) < 0)
        error("listen");

    printf("Socket listening...\n");
    printf("Waiting for %d players...\n", expected_players);

    struct pollfd fds[MAX_CLIENTS + 1];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    for (int i = 1; i <= MAX_CLIENTS; ++i) {
        fds[i].fd = -1;
    }

    const char* server_info = "/info ID:Imposter Server v0.3\n";

    while (1) {
        // Vérifier si un redémarrage automatique est nécessaire
        check_auto_restart();
        check_timeout();
        
        int activity = poll(fds, MAX_CLIENTS + 1, 1000); // timeout de 1 seconde pour vérifier le redémarrage
        if (activity < 0 && errno != EINTR) {
            error("poll");
        }

        if (activity == 0) {
            // Timeout du poll, continuer pour vérifier le redémarrage
            continue;
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int new_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
            if (new_fd < 0) {
                perror("accept");
                continue;
            }

            int slot_available = -1;
            for (int i = 1; i <= MAX_CLIENTS; ++i) {
                if (fds[i].fd < 0) {
                    slot_available = i;
                    break;
                }
            }

            if (slot_available == -1) {
                const char* msg_refuse = "ERROR: Game is full. Try again later.\n";
                send(new_fd, msg_refuse, strlen(msg_refuse), 0);
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);
                printf("Connection refused for %s:%d (game full)\n", ip, ntohs(client_addr.sin_port));

                close(new_fd);
            } else {
                add_client(new_fd, client_addr);
                fds[slot_available].fd = new_fd;
                fds[slot_available].events = POLLIN;
                
                send_message(new_fd, server_info);
                send_message(new_fd, "/login\n");
                
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);
                printf("New client %d (%s:%d) connected\n", client_counter-1, ip, ntohs(client_addr.sin_port));
            }
        }

        for (int i = 1; i <= MAX_CLIENTS; ++i) {
            if (fds[i].fd < 0) continue;
        
            if (fds[i].revents & POLLIN) {
                char buf[BUFFER_SIZE];
                ssize_t n = read(fds[i].fd, buf, sizeof(buf) - 1);

                if (n <= 0) {
                    client_t* cli = find_client(fds[i].fd);
                    if (cli) {
                        if (cli->logged_in) {
                            printf("Client '%s' (id %d) disconnected\n", cli->login, cli->id);
                        } else {
                            printf("Client %d disconnected\n", cli->id);
                        }
                    }

                    remove_client(fds[i].fd);
                    fds[i].fd = -1;
                } else {
                    buf[n] = '\0';
                    client_t* cli = find_client(fds[i].fd);
                    if (cli) {
                        printf("Received from client %d: %s\n", cli->id, buf);
                        handle_client_command(fds[i].fd, buf);
                    }
                }
            }
        }
    }

    return 0;
}