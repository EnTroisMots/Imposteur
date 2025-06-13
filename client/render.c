#include "client.h"

void render_login_screen(client_data_t* data, window_size_t window_size) {
    render_text("IMPOSTER GAME", 0.5f, 0.083f, 1, window_size);
    render_text("Entrez votre pseudo:", 0.5f, 0.25f, 1, window_size);
    render_input_field(data->login, 0.3125f, 0.3f, 0.375f, 0.066f, 1, window_size);
    render_button("CONNEXION", 0.375f, 0.416f, 0.25f, 0.083f, 0, window_size);
    
    if (strlen(data->message) > 0) {
        render_text(data->message, 0.5f, 0.533f, 1, window_size);
    }
}

void render_waiting_screen(client_data_t* data, window_size_t window_size) {
    render_text("EN ATTENTE DES JOUEURS...", 0.5f, 0.083f, 1, window_size);

    char info[100];
    snprintf(info, sizeof(info), "%d/%d joueurs connectés", 
            data->player_count, data->expected_players);
    render_text(info, 0.5f, 0.166f, 1, window_size);

    // Afficher la liste des joueurs connectés avec leurs mots
    render_text("Joueurs connectés:", 0.25f, 0.233f, 0, window_size);
    float  y_offset = 0.283f;
    
    for (int i = 0; i < data->player_count; i++) {
        char player_status[50];
        snprintf(player_status, sizeof(player_status), "✓ %s", data->players[i]);
        render_text(player_status, 0.25f, y_offset, 0, window_size);
        
        // Afficher les mots joués par ce joueur
        if (data->player_word_count[i] > 0) {
            char words_line[BUFFER_SIZE] = "  Mots: ";
            for (int j = 0; j < data->player_word_count[i]; j++) {
                if (j > 0) strcat(words_line, ", ");
                strcat(words_line, data->player_words[i][j]);
            }
            render_text(words_line, 0.25f, y_offset + 0.033f, 0, window_size);
            y_offset += 0.075f;
        } else {
            y_offset += 0.0416f;
        }
    }

    // Afficher les slots en attente
    for (int i = data->player_count; i < data->expected_players; i++) {
        render_text("✗ (en attente)", 0.25f, y_offset, 0, window_size);
        y_offset += 0.0416f;
    }

    if (strlen(data->message) > 0) {
        render_text(data->message, 0.5f, 0.833f, 1, window_size);
    }
}

void render_play_screen(client_data_t* data, window_size_t window_size) {
    char round_info[100];
    snprintf(round_info, sizeof(round_info), "Manche %d/%d", data->current_round, data->total_rounds);
    render_text(round_info, 0.5f, 0.05f, 1, window_size);

    render_text("Votre mot:", 0.5f, 0.133f, 1, window_size);
    render_text(data->current_word, 0.5f, 0.2f, 1, window_size);

    render_text("Proposez un mot différent:", 0.5f, 0.3f, 1, window_size);
    render_input_field(data->said_word, 0.3125f, 0.35f, 0.375f, 0.066f, 1, window_size);

    render_button("VALIDER", 0.375f, 0.466f, 0.25f, 0.083f, 0, window_size);

    char timer[20];
    snprintf(timer, sizeof(timer), "Temps restant: %d", data->timer);
    render_text(timer, 0.5f, 0.583f, 1, window_size);

    render_text("Historique:", 0.75f, 0.133f, 0, window_size);
    float y_offset = 0.183f;

    for (int i = 0; i < data->player_count; i++) {
        render_text(data->players[i], 0.75f, y_offset, 0, window_size);
        y_offset += 0.033f;

        if (data->player_word_count[i] > 0) {
            for (int j = 0; j < data->player_word_count[i]; j++) {
                char word_display[BUFFER_SIZE];
                snprintf(word_display, sizeof(word_display), "  %d. ", j+1);
                strncat(word_display, data->player_words[i][j], sizeof(word_display)-10);
                word_display[sizeof(word_display)-1] = '\0';

                render_text(word_display, 0.75f, y_offset, 0, window_size);
                y_offset += 0.03f;
            }
        } else {
            render_text("  (aucun mot)", 0.75f, y_offset, 0, window_size);
            y_offset += 0.03f;
        }
        y_offset += 0.016f;
    }

    render_text(data->message, 0.5f, 0.666f, 1, window_size);
}

void render_choice_screen(client_data_t* data, window_size_t window_size) {
    render_text("VOTEZ POUR L'IMPOSTEUR", 0.5f, 0.05f, 1, window_size);
    
    // Afficher les joueurs avec leurs mots à gauche
    render_text("Joueurs et leurs mots:", 0.1875f, 0.133f, 0, window_size);
    float y_offset = 0.183f;
    
    for (int i = 0; i < data->player_count; i++) {
        // Nom du joueur
        render_text(data->players[i], 0.1875f, y_offset, 0, window_size);
        y_offset += 0.033f;
        
        // Ses mots
        if (data->player_word_count[i] > 0) {
            for (int j = 0; j < data->player_word_count[i]; j++) {
                char word_display[BUFFER_SIZE];
                // Fix: Add bounds checking
                int remaining = sizeof(word_display) - 5; // Reserve space for "  • "
                snprintf(word_display, sizeof(word_display), "  • ");
                strncat(word_display, data->player_words[i][j], remaining);
                word_display[sizeof(word_display)-1] = '\0'; // Ensure null termination
                
                render_text(word_display, 0.1875f, y_offset, 0, window_size);
                y_offset += 0.03f;
            }
        } else {
            render_text("  (aucun mot)", 0.1875f, y_offset, 0, window_size);
            y_offset += 0.03f;
        }
        y_offset += 0.016f;
    }

    // Boutons de vote à droite
    render_text("Votez:", 0.6875f, 0.133f, 0, window_size);
    for (int i = 0; i < data->player_count; i++) {
        float btn_y = 0.183f + i * 0.083f;
        render_button(data->players[i], 0.625f, btn_y, 0.25f, 0.066f, 0, window_size);
    }

    char timer[20];
    snprintf(timer, sizeof(timer), "Temps restant: %d", data->timer);
    render_text(timer, 0.5f, 0.833f, 1, window_size);

    render_text(data->message, 0.5f, 0.883f, 1, window_size);
}

void render_results_screen(client_data_t* data, window_size_t window_size) {
    render_text("RESULTATS", 0.5f, 0.05f, 1, window_size);

    char round_info[100];
    snprintf(round_info, sizeof(round_info), "Manche %d/%d", data->current_round, data->total_rounds);
    render_text(round_info, 0.5f, 0.1f, 1, window_size);

    // Afficher les scores
    float y = 0.2f;
    for (int i = 0; i < data->player_count; i++) {
        char score[100];
        if (data->player_points_gained[i] > 0) {
            snprintf(score, sizeof(score), "%s: %d points (+%d)", 
                    data->players[i], 
                    data->player_scores[i], 
                    data->player_points_gained[i]);
        } else {
            snprintf(score, sizeof(score), "%s: %d points", 
                    data->players[i], 
                    data->player_scores[i]);
        }
        render_text(score, 0.5f, y, 1, window_size);
        y += 0.05f;
    }

    // Afficher le message de résultat (qui contient l'info sur l'imposteur)
    if (strlen(data->message) > 0) {
        // Diviser le message en lignes si nécessaire
        char* line = strtok(data->message, "\n");
        float msg_y = y + 0.05f;
        while (line != NULL && msg_y < 0.9f) {
            render_text(line, 0.5f, msg_y, 1, window_size);
            msg_y += 0.05f;
            line = strtok(NULL, "\n");
        }
    }
}

void render_text(const char* text, float x_percent, float y_percent, int centered, window_size_t window_size) {
    if (text == NULL || strlen(text) == 0) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, text_color);
    if (!surface) {
        printf("Erreur rendu texte: %s\n", TTF_GetError());
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    int x = (int)(x_percent * window_size.width);
    int y = (int)(y_percent * window_size.height);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    
    if (centered) {
        rect.x = x - surface->w / 2;
        rect.y = y - surface->h / 2;
    }

    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void render_button(const char* text, float x_percent, float y_percent, float w_percent, float h_percent, int hover, window_size_t window_size) {
    int x = (int)(x_percent * window_size.width);
    int y = (int)(y_percent * window_size.height);
    int w = (int)(w_percent * window_size.width);
    int h = (int)(h_percent * window_size.height);

    SDL_Color btn_color = hover ? (SDL_Color){COLOR_BUTTON_HOVER} : (SDL_Color){COLOR_BUTTON};

    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, btn_color.r, btn_color.g, btn_color.b, btn_color.a);
    SDL_RenderFillRect(renderer, &rect);

    render_text(text, (x + w/2.0f) / window_size.width, (y + h/2.0f) / window_size.height, 1, window_size);
}

void render_input_field(const char* text, float x_percent, float y_percent, float w_percent, float h_percent, int active, window_size_t window_size) {
    int x = (int)(x_percent * window_size.width);
    int y = (int)(y_percent * window_size.height);
    int w = (int)(w_percent * window_size.width);
    int h = (int)(h_percent * window_size.height);

    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, COLOR_INPUT);
    SDL_RenderFillRect(renderer, &rect);

    if (active) {
        SDL_SetRenderDrawColor(renderer, accent_color.r, accent_color.g, accent_color.b, accent_color.a);
        SDL_RenderDrawRect(renderer, &rect);
    }

    if (strlen(text) > 0) {
        render_text(text, (x + 10.0f) / window_size.width, (y + h/2.0f - 10) / window_size.height, 0, window_size);
    }
}

void render_screen(client_data_t* data, window_size_t window_size) {
    SDL_SetRenderDrawColor(renderer, COLOR_BACKGROUND);
    SDL_RenderClear(renderer);

    switch (data->state) {
        case STATE_LOGIN:
            render_login_screen(data, window_size);
            break;
        case STATE_WAITING:
            render_waiting_screen(data, window_size);
            break;
        case STATE_PLAY:
            render_play_screen(data, window_size);
            break;
        case STATE_CHOICE:
            render_choice_screen(data, window_size);
            break;
        case STATE_RESULTS:
            render_results_screen(data, window_size);
            break;
    }

    SDL_RenderPresent(renderer);
}