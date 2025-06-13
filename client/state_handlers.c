#include "client.h"

Uint32 timer_callback(Uint32 interval, void* param) {
    client_data_t* data = (client_data_t*)param;
    if (data->timer > 0) {
        data->timer--;
        SDL_Event event;
        event.type = SDL_USEREVENT;
        SDL_PushEvent(&event);
    }
    return interval;
}

void reorganize_players(client_data_t* data) {
    int empty_slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (data->player_order[i] == 0) {
            empty_slot = i;
        }
        else if (empty_slot != -1) {
            strncpy(data->players[empty_slot], data->players[i], MAX_LOGIN_LENGTH);
            data->players[empty_slot][MAX_LOGIN_LENGTH] = '\0';
            data->player_order[empty_slot] = 1;
            
            memset(data->players[i], 0, sizeof(data->players[i]));
            data->player_order[i] = 0;
            
            for (int j = 0; j < 10; j++) {
                strncpy(data->player_words[empty_slot][j], data->player_words[i][j], BUFFER_SIZE);
                memset(data->player_words[i][j], 0, sizeof(data->player_words[i][j]));
            }
            data->player_word_count[empty_slot] = data->player_word_count[i];
            data->player_word_count[i] = 0;
            
            empty_slot = i;
        }
    }
    
    data->player_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (data->player_order[i]) {
            data->player_count++;
        }
    }
}

void handle_server_message(client_data_t* data, const char* msg) {
    printf("Message du serveur: %s\n", msg);

    if (msg == NULL || strlen(msg) == 0) {
        return;
    }

    if (strncmp(msg, "/info LOGIN:", 12) == 0) {
        int position, total;
        char login[MAX_LOGIN_LENGTH + 1];
        if (sscanf(msg + 12, "%d/%d:%16s", &position, &total, login) == 3) {
            data->expected_players = total;
            
            // Mettre à jour l'ordre des joueurs
            if (position > 0 && position <= MAX_CLIENTS) {
                // Vérifier si ce joueur n'est pas déjà dans la liste
                int found = 0;
                for (int i = 0; i < data->player_count; i++) {
                    if (strcmp(data->players[i], login) == 0) {
                        found = 1;
                        break;
                    }
                }
                
                // Ajouter le nouveau joueur s'il n'est pas déjà présent
                if (!found && data->player_count < MAX_CLIENTS) {
                    strncpy(data->players[position-1], login, MAX_LOGIN_LENGTH);
                    data->players[position-1][MAX_LOGIN_LENGTH] = '\0';
                    data->player_order[position-1] = 1; // Marquer cette position comme occupée
                    
                    // Recompter le nombre total de joueurs
                    data->player_count = 0;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (data->player_order[i]) {
                            data->player_count++;
                        }
                    }
                }
                // Si le joueur existe déjà, mettre à jour sa position
                else if (found) {
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (i != position-1 && strcmp(data->players[i], login) == 0) {
                            memset(data->players[i], 0, sizeof(data->players[i]));
                            data->player_order[i] = 0;
                        }
                    }
                    strncpy(data->players[position-1], login, MAX_LOGIN_LENGTH);
                    data->players[position-1][MAX_LOGIN_LENGTH] = '\0';
                    data->player_order[position-1] = 1;
                    
                    // Recompter
                    data->player_count = 0;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (data->player_order[i]) {
                            data->player_count++;
                        }
                    }
                }
            }
        }
    }
    else if (strncmp(msg, "/info WAIT:", 11) == 0) {
        char login[MAX_LOGIN_LENGTH + 1];
        char action[10];
        if (sscanf(msg + 11, "%[^:]:%9s", login, action) == 2) {
            if (strcmp(action, "PLAY") == 0) {
                if (strcmp(login, data->login) == 0) {
                    data->state = STATE_PLAY;
                    data->timer = data->play_timeout;
                    if (data->timer_id) {
                        SDL_RemoveTimer(data->timer_id);
                    }
                    data->timer_id = SDL_AddTimer(1000, timer_callback, data);
                    memset(data->message, 0, sizeof(data->message));
                } else {
                    snprintf(data->message, sizeof(data->message), "En attente que %s joue...", login);
                    data->state = STATE_WAITING;
                }
            }
        }
    }
    else if (strncmp(msg, "/info GAME:", 11) == 0) {
        if (sscanf(msg + 11, "%d/%d:%d:%d:%d",
              &data->current_round, &data->total_rounds,
              &data->expected_players, &data->play_timeout,
              &data->choice_timeout) == 5) {
            printf("Info jeu reçue: round %d/%d, joueurs attendus: %d\n", 
                   data->current_round, data->total_rounds, data->expected_players);
        }
    }
    else if (strncmp(msg, "/assign ", 8) == 0) {
        strncpy(data->current_word, msg + 8, sizeof(data->current_word) - 1);
        data->current_word[sizeof(data->current_word) - 1] = '\0';
        printf("Mot assigné: %s\n", data->current_word);
    }
    else if (strncmp(msg, "/info SAY:", 10) == 0) {
        char login[MAX_LOGIN_LENGTH + 1];
        char word[BUFFER_SIZE];
        if (sscanf(msg + 10, "%[^:]:%s", login, word) == 2) {

            char temp_msg[BUFFER_SIZE];
            snprintf(temp_msg, sizeof(temp_msg), "%s a dit: ", login);
            int remaining = sizeof(data->message) - strlen(temp_msg) - 1;
            if (remaining > 0) {
                strncpy(data->message, temp_msg, sizeof(data->message));
                strncat(data->message, word, remaining);
                data->message[sizeof(data->message)-1] = '\0';
            }

            int max_len = sizeof(data->message) - 10; // Reserve space for " a dit: " and null terminator
            int login_len = strnlen(login, MAX_LOGIN_LENGTH);
            int word_len = strnlen(word, max_len - login_len);
            snprintf(data->message, sizeof(data->message), "%.*s a dit: %.*s", 
                     login_len, login, word_len, word);
            
            // Ajouter le mot à l'historique du joueur
            for (int i = 0; i < data->player_count; i++) {
                if (strcmp(data->players[i], login) == 0) {
                    if (data->player_word_count[i] < 10) { // Max 10 mots par joueur
                        strncpy(data->player_words[i][data->player_word_count[i]], word, BUFFER_SIZE - 1);
                        data->player_words[i][data->player_word_count[i]][BUFFER_SIZE - 1] = '\0';
                        data->player_word_count[i]++;
                    }
                    break;
                }
            }
        }
    }
    else if (strncmp(msg, "/info ALERT:", 12) == 0) {
        // Vérifier si c'est un message de déconnexion
        if (strstr(msg + 12, "a quitté la partie")) {
            // Extraire le nom du joueur qui a quitté
            char* start = strchr(msg + 12, '\'');
            if (start) {
                start++;
                char* end = strchr(start, '\'');
                if (end) {
                    char disconnected_login[MAX_LOGIN_LENGTH + 1];
                    strncpy(disconnected_login, start, end - start);
                    disconnected_login[end - start] = '\0';
                    
                    // Supprimer ce joueur de la liste
                    for (int i = 0; i < data->player_count; i++) {
                        if (strcmp(data->players[i], disconnected_login) == 0) {
                            memset(data->players[i], 0, sizeof(data->players[i]));
                            data->player_order[i] = 0;
                            data->player_word_count[i] = 0;
                            for (int j = 0; j < 10; j++) {
                                memset(data->player_words[i][j], 0, sizeof(data->player_words[i][j]));
                            }
                            break;
                        }
                    }
                    
                    // Réorganiser la liste des joueurs
                    reorganize_players(data);
                }
            }
        }
        // Afficher le message d'alerte
        strncpy(data->message, msg + 12, sizeof(data->message) - 1);
        data->message[sizeof(data->message) - 1] = '\0';
    }
    else if (strncmp(msg, "/choice ", 8) == 0) {
        int timeout;
        if (sscanf(msg + 8, "%d", &timeout) == 1) {
            data->state = STATE_CHOICE;
            data->timer = timeout;
            if (data->timer_id) {
                SDL_RemoveTimer(data->timer_id);
            }
            data->timer_id = SDL_AddTimer(1000, timer_callback, data);
            memset(data->message, 0, sizeof(data->message));
            strcpy(data->message, "Phase de vote - Choisissez l'imposteur !");
            printf("Passage en phase de vote, temps: %d secondes\n", timeout);
        }
    }
    else if (strncmp(msg, "/info CHOICE:", 13) == 0) {
        char login[MAX_LOGIN_LENGTH + 1];
        char accused[MAX_LOGIN_LENGTH + 1];
        if (sscanf(msg + 13, "%[^:]:%s", login, accused) == 2) {
            snprintf(data->message, sizeof(data->message), "%s accuse %s", login, accused);
        }
    }
    else if (strncmp(msg, "/info RESULT:", 13) == 0) {
        data->state = STATE_RESULTS;
        
        // Réinitialiser les scores
        for (int i = 0; i < data->player_count; i++) {
            data->player_scores[i] = 0;
            data->player_points_gained[i] = 0;
        }
        
        /// Parser le message de résultats de manière plus sûre
        const char* ptr = msg + 13;  // Supposons que le préfixe fait 13 caractères
        char login[MAX_LOGIN_LENGTH + 1] = {0};
        int total_score, points_gained;
        int parsed_count = 0;

        // Vérification initiale
        if (msg == NULL || strlen(msg) < 13) {
            fprintf(stderr, "Message invalide\n");
            return;
        }

        while (*ptr != '\0' && parsed_count < data->player_count) {
            // Trouver les séparateurs
            const char* colon = strchr(ptr, ':');
            const char* plus = colon ? strchr(colon + 1, '+') : NULL;
            const char* next_sep = plus ? strchr(plus + 1, ':') : NULL;
            
            // Cas spécial pour le dernier élément (pas de ':' à la fin)
            int is_last_element = (next_sep == NULL);
            
            if (!colon || !plus) break; // Format invalide
            
            // Extraire login (avec protection contre overflow)
            size_t login_len = colon - ptr;
            if (login_len > MAX_LOGIN_LENGTH) login_len = MAX_LOGIN_LENGTH;
            strncpy(login, ptr, login_len);
            login[login_len] = '\0';
            
            // Extraire les scores
            total_score = atoi(colon + 1);
            points_gained = atoi(plus + 1);
            
            // Trouver le joueur correspondant
            for (int i = 0; i < data->player_count; i++) {
                if (strcmp(data->players[i], login) == 0) {
                    data->player_scores[i] = total_score;
                    data->player_points_gained[i] = points_gained;
                    parsed_count++;
                    break;
                }
            }
            
            // Avancer au prochain élément ou terminer
            if (is_last_element) {
                break;
            } else {
                ptr = next_sep + 1;
            }
        }
        // Force refresh
        SDL_Event event;
        event.type = SDL_USEREVENT;
        SDL_PushEvent(&event);
    }
    else if (strncmp(msg, "/info ANSWER:", 13) == 0) {
        char imposter[MAX_LOGIN_LENGTH + 1];
        char imposter_word[BUFFER_SIZE];
        char correct_word[BUFFER_SIZE];
        if (sscanf(msg + 13, "%[^:]:%[^:]:%s", imposter, imposter_word, correct_word) == 3) {
            // Fix: Truncate words if necessary to fit in message buffer
            char safe_imposter_word[50];
            char safe_correct_word[50];
            
            strncpy(safe_imposter_word, imposter_word, sizeof(safe_imposter_word)-1);
            safe_imposter_word[sizeof(safe_imposter_word)-1] = '\0';
            
            strncpy(safe_correct_word, correct_word, sizeof(safe_correct_word)-1);
            safe_correct_word[sizeof(safe_correct_word)-1] = '\0';
            
            snprintf(data->message, sizeof(data->message),
                    "L'imposteur était %s! Mot imposteur: %s, Mot correct: %s",
                    imposter, safe_imposter_word, safe_correct_word);

            // Réinitialiser les mots des joueurs pour la prochaine manche
            memset(data->said_word, 0, sizeof(data->said_word));
            memset(data->current_word, 0, sizeof(data->current_word));
            for (int i = 0; i < data->player_count; i++) {
                memset(data->player_words[i], 0, sizeof(data->player_words[i]));
                data->player_word_count[i] = 0;
            }
        }
        // Force refresh
        SDL_Event event;
        event.type = SDL_USEREVENT;
        SDL_PushEvent(&event);
    }
    else if (strncmp(msg, "/ret LOGIN:", 11) == 0) {
        int code;
        if (sscanf(msg + 11, "%d", &code) == 1) {
            if (code == 0) {  // Succès
                data->logged_in = 1;
                data->state = STATE_WAITING;
                memset(data->message, 0, sizeof(data->message));
                printf("Connexion réussie, passage en état d'attente\n");
            } else if (code == 101) {
                strcpy(data->message, "Ce pseudo est déjà pris");
            } else if (code == 107) {
                strcpy(data->message, "Pseudo invalide (3-16 caractères alphanumériques)");
            }else if (code == 109) {
                strcpy(data->message, "Partie en cours - Veuillez attendre la fin pour rejoindre");
            } else if (code == 202) {
                strcpy(data->message, "Vous êtes déjà connecté");
            }
        }
    }
    else if (strncmp(msg, "/ret PLAY:", 10) == 0) {
        int code;
        if (sscanf(msg + 10, "%d", &code) == 1) {
            if (code == 102) {
                strcpy(data->message, "Ce n'est pas votre tour");
            } else if (code == 103) {
                strcpy(data->message, "Ce mot a déjà été utilisé");
            } else if (code == 104) {
                strcpy(data->message, "Vous ne pouvez pas dire votre mot assigné");
            } else if (code == 108) {
            strcpy(data->message, "Mot invalide (ne doit pas contenir ':')");
            } else if (code == 202) {
                strcpy(data->message, "Action non autorisée dans cet état");
            }
        }
    }
    else if (strncmp(msg, "/ret CHOICE:", 12) == 0) {
        int code;
        if (sscanf(msg + 12, "%d", &code) == 1) {
            if (code == 105) {
                strcpy(data->message, "Vous ne pouvez pas vous accuser vous-même");
            } else if (code == 106) {
                strcpy(data->message, "Joueur introuvable");
            } else if (code == 202) {
                strcpy(data->message, "Action non autorisée dans cet état");
            }
        }
    }
    else if (strncmp(msg, "/login", 6) == 0) {
        data->state = STATE_LOGIN;
        data->logged_in = 0;
        memset(data->login, 0, sizeof(data->login));
        memset(data->message, 0, sizeof(data->message));
    }
}