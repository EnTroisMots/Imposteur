#include "client.h"

void handle_mouse_click(client_data_t* data, int x, int y, window_size_t window_size) {
    switch (data->state) {
        case STATE_LOGIN: {
            // Convertir les coordonnées du clic en pourcentages relatifs
            float mouse_x_percent = (float)x / window_size.width;
            float mouse_y_percent = (float)y / window_size.height;

            // Bouton CONNEXION à (0.375f, 0.416f, 0.25f, 0.083f)
            float btn_x_min = 0.375f; // Début du bouton en X
            float btn_x_max = 0.375f + 0.25f; // Fin du bouton en X
            float btn_y_min = 0.416f; // Début du bouton en Y
            float btn_y_max = 0.416f + 0.083f; // Fin du bouton en Y

            if (mouse_x_percent >= btn_x_min && mouse_x_percent <= btn_x_max &&
                mouse_y_percent >= btn_y_min && mouse_y_percent <= btn_y_max) {
                if (strlen(data->login) >= MIN_LOGIN_LENGTH) {
                    char cmd[BUFFER_SIZE];
                    snprintf(cmd, sizeof(cmd), "/login %s\n", data->login);
                    send_message(data->fd, cmd);
                    printf("Tentative de connexion avec le pseudo: %s\n", data->login);
                } else {
                    strcpy(data->message, "Pseudo trop court (min 3 caractères)");
                }
            }
            break;
        }
        case STATE_PLAY: {
            // Convertir les coordonnées du clic en pourcentages relatifs
            float mouse_x_percent = (float)x / window_size.width;
            float mouse_y_percent = (float)y / window_size.height;

            // Bouton VALIDER à (0.375f, 0.466f, 0.25f, 0.083f)
            float btn_x_min = 0.375f; // Début du bouton en X
            float btn_x_max = 0.375f + 0.25f; // Fin du bouton en X
            float btn_y_min = 0.466f; // Début du bouton en Y
            float btn_y_max = 0.466f + 0.083f; // Fin du bouton en Y

            if (mouse_x_percent >= btn_x_min && mouse_x_percent <= btn_x_max &&
                mouse_y_percent >= btn_y_min && mouse_y_percent <= btn_y_max) {
                if (strlen(data->said_word) > 0) {
                    char cmd[BUFFER_SIZE];
                    if (strlen(data->said_word) < BUFFER_SIZE - 10) { // Réserve l'espace pour "/play \n"
                        int max_word_len = sizeof(cmd) - 8; // Réserve l'espace pour "/play \n" et le terminateur null
                        int word_len = strnlen(data->said_word, max_word_len);
                        snprintf(cmd, sizeof(cmd), "/play %.*s\n", word_len, data->said_word);
                        send_message(data->fd, cmd);
                        memset(data->said_word, 0, sizeof(data->said_word));
                    } else {
                        strcpy(data->message, "Mot trop long");
                    }
                }
            }
            break;
        }
        case STATE_CHOICE: {
            float mouse_x_percent = (float)x / window_size.width;
            float mouse_y_percent = (float)y / window_size.height;
            
            // Coordonnées des boutons de vote (relatives)
            float btn_x_min = 0.625f;  // Début du bouton en X (62.5%)
            float btn_x_max = 0.875f;  // Fin du bouton en X (87.5%)
            float btn_height = 0.066f; // Hauteur d'un bouton (6.6%)
            float first_btn_y = 0.183f; // Position Y du premier bouton (18.3%)
            
            for (int i = 0; i < data->player_count; i++) {
                float btn_y_min = first_btn_y + i * 0.083f; // Espacement de 8.3% entre les boutons
                float btn_y_max = btn_y_min + btn_height;
                
                if (mouse_x_percent >= btn_x_min && mouse_x_percent <= btn_x_max &&
                    mouse_y_percent >= btn_y_min && mouse_y_percent <= btn_y_max) {
                    if (strcmp(data->players[i], data->login) != 0) {
                        char cmd[BUFFER_SIZE];
                        snprintf(cmd, sizeof(cmd), "/choice %s\n", data->players[i]);
                        send_message(data->fd, cmd);
                    } else {
                        strcpy(data->message, "Vous ne pouvez pas vous accuser vous-même");
                    }
                    break;
                }
            }
            break;
        }
        default:
            break;
    }
}

void handle_text_input(client_data_t* data, const char* text) {
    switch (data->state) {
        case STATE_LOGIN:
            if (strlen(data->login) + strlen(text) < MAX_LOGIN_LENGTH) {
                strcat(data->login, text);
            }
            break;
        case STATE_PLAY:
            if (strlen(data->said_word) + strlen(text) < BUFFER_SIZE - 1) {
                strcat(data->said_word, text);
            }
            break;
        case STATE_WAITING:
        case STATE_CHOICE:
        case STATE_RESULTS:
            break;
    }
}

void handle_key_press(client_data_t* data, SDL_Keycode key) {
    switch (data->state) {
        case STATE_LOGIN:
            if (key == SDLK_BACKSPACE && strlen(data->login) > 0) {
                data->login[strlen(data->login) - 1] = '\0';
            } else if (key == SDLK_RETURN) {
                if (strlen(data->login) >= MIN_LOGIN_LENGTH) {
                    char cmd[BUFFER_SIZE];
                    snprintf(cmd, sizeof(cmd), "/login %s\n", data->login);
                    send_message(data->fd, cmd);
                    printf("Tentative de connexion avec le pseudo: %s\n", data->login);
                }
            }
            break;
        case STATE_PLAY:
            if (key == SDLK_BACKSPACE && strlen(data->said_word) > 0) {
                data->said_word[strlen(data->said_word) - 1] = '\0';
            } else if (key == SDLK_RETURN) {
                if (strlen(data->said_word) > 0) {
                    char cmd[BUFFER_SIZE];
                    // Fix: Ensure command fits in buffer
                    if (strlen(data->said_word) < BUFFER_SIZE - 10) { // Reserve space for "/play \n"
                        int max_word_len = sizeof(cmd) - 8; // Reserve space for "/play \n" and null terminator
                        int word_len = strnlen(data->said_word, max_word_len);
                        snprintf(cmd, sizeof(cmd), "/play %.*s\n", word_len, data->said_word);
                        send_message(data->fd, cmd);
                        memset(data->said_word, 0, sizeof(data->said_word));
                    } else {
                        strcpy(data->message, "Mot trop long");
                    }
                }
            }
            break;
        case STATE_WAITING:
        case STATE_CHOICE:
        case STATE_RESULTS:
            break;
    }
}