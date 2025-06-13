#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <errno.h>

#define BUFFER_SIZE 2048
#define MAX_LOGIN_LENGTH 16
#define MIN_LOGIN_LENGTH 3
#define MAX_CLIENTS 10
#define MAX_MESSAGE_LENGTH 2048
#define MAX_CMD_LENGTH 2048
#define MAX_WORD_DISPLAY_LENGTH 50
#define DEFAULT_PORT "5000"

#define COLOR_BACKGROUND 0x1E, 0x1E, 0x1E, 0xFF
#define COLOR_TEXT 0xFF, 0xFF, 0xFF, 0xFF
#define COLOR_BUTTON 0x2E, 0x2E, 0x2E, 0xFF
#define COLOR_BUTTON_HOVER 0x3E, 0x3E, 0x3E, 0xFF
#define COLOR_INPUT 0x2A, 0x2A, 0x2A, 0xFF
#define COLOR_ACCENT 0x42, 0x85, 0xF4, 0xFF

typedef enum {
    STATE_LOGIN,
    STATE_WAITING,
    STATE_PLAY,
    STATE_CHOICE,
    STATE_RESULTS
} client_state_t;

typedef struct {
    int width;
    int height;
} window_size_t;

typedef struct {
    int fd;
    char login[MAX_LOGIN_LENGTH + 1];
    int logged_in;
    int score;
    int points_gained;
    char current_word[BUFFER_SIZE];
    char said_word[BUFFER_SIZE];
    char accused_login[MAX_LOGIN_LENGTH + 1];
    int is_imposter;
    client_state_t state;
    char message[BUFFER_SIZE];
    char players[MAX_CLIENTS][MAX_LOGIN_LENGTH + 1];
    int player_order[MAX_CLIENTS];
    char player_words[MAX_CLIENTS][10][BUFFER_SIZE];
    int player_word_count[MAX_CLIENTS];
    int player_count;
    int expected_players;
    int current_round;
    int total_rounds;
    int play_timeout;
    int choice_timeout;
    int timer;
    SDL_TimerID timer_id;
    int player_scores[MAX_CLIENTS];
    int player_points_gained[MAX_CLIENTS];
} client_data_t;

// Variables globales
extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern TTF_Font* font;
extern SDL_Color text_color;
extern SDL_Color accent_color;

// Fonctions utilitaires
void error(const char* msg);
void send_message(int fd, const char* msg);

// Fonctions de rendu
void render_text(const char* text, float x_percent, float y_percent, int centered, window_size_t window_size);
void render_button(const char* text, float x_percent, float y_percent, float w_percent, float h_percent, int hover, window_size_t window_size);
void render_input_field(const char* text, float x_percent, float y_percent, float w_percent, float h_percent, int active, window_size_t window_size);
void render_screen(client_data_t* data, window_size_t window_size);

// Gestion des états
void handle_server_message(client_data_t* data, const char* msg);
void reorganize_players(client_data_t* data);

// Gestion des entrées
void handle_mouse_click(client_data_t* data, int x, int y, window_size_t window_size);
void handle_text_input(client_data_t* data, const char* text);
void handle_key_press(client_data_t* data, SDL_Keycode key);

// Initialisation réseau
int init_network(const char* ip, const char* port, client_data_t* data);

#endif