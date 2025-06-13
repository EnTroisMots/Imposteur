#include "client.h"

// Variables globales
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
SDL_Color text_color = {COLOR_TEXT};
SDL_Color accent_color = {COLOR_ACCENT};

int main(int argc, char* argv[]) {
    char* server_ip = NULL;
    char* port = DEFAULT_PORT;  // Valeur par défaut

    if (argc < 3 || argc > 5) {
        printf("Usage: %s -s <ip> -p <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Analyse des arguments
    int opt;
    while ((opt = getopt(argc, argv, "s:p:")) != -1) {
        switch (opt) {
            case 's':
                server_ip = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            default:
        }
    }

    // Initialisation SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        error("SDL_Init failed");
    }

    if (TTF_Init() < 0) {
        error("TTF_Init failed");
    }

    window = SDL_CreateWindow("Imposter Game Client",
                            SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED,
                            800, 600,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        error("SDL_CreateWindow failed");
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        error("SDL_CreateRenderer failed");
    }

    font = TTF_OpenFont("arial.ttf", 24);
    if (!font) {
        font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSans.ttf", 24);
        if (!font) {
            error("TTF_OpenFont failed");
        }
    }

    // Initialisation des données client
    client_data_t data;
    memset(&data, 0, sizeof(data));
    data.state = STATE_LOGIN;
    data.player_count = 0;
    data.expected_players = 0;
    window_size_t window_size = {800, 600};
    
    // Initialiser les tableaux de joueurs
    for (int i = 0; i < MAX_CLIENTS; i++) {
        memset(data.players[i], 0, sizeof(data.players[i]));
        data.player_word_count[i] = 0;
        data.player_order[i] = 0;
        for (int j = 0; j < 10; j++) {
            memset(data.player_words[i][j], 0, sizeof(data.player_words[i][j]));
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        data.player_scores[i] = 0;
        data.player_points_gained[i] = 0;
    }

    // Initialisation réseau
    if (init_network(server_ip, port, &data) != 0) {
        return EXIT_FAILURE;
    }

    // Activer la saisie de texte SDL
    SDL_StartTextInput();

    int running = 1;
    fd_set readfds;

    while (running) {
        // Vérifier les données du serveur
        FD_ZERO(&readfds);
        FD_SET(data.fd, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms

        int activity = select(data.fd + 1, &readfds, NULL, NULL, &tv);

        if (activity > 0 && FD_ISSET(data.fd, &readfds)) {
            char buffer[BUFFER_SIZE];
            ssize_t n = read(data.fd, buffer, sizeof(buffer) - 1);

            if (n <= 0) {
                printf("Serveur déconnecté\n");
                break;
            }

            buffer[n] = '\0';
            
            // Traiter chaque ligne reçue
            char* line = strtok(buffer, "\n");
            while (line != NULL) {
                handle_server_message(&data, line);
                line = strtok(NULL, "\n");
            }
        }

        // Traitement des événements SDL
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        window_size.width = event.window.data1;
                        window_size.height = event.window.data2;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        handle_mouse_click(&data, event.button.x, event.button.y, window_size);
                    }
                    break;
                case SDL_TEXTINPUT:
                    handle_text_input(&data, event.text.text);
                    break;
                case SDL_KEYDOWN:
                    handle_key_press(&data, event.key.keysym.sym);
                    break;
                case SDL_USEREVENT:
                    // Événement de rafraîchissement (timer)
                    break;
            }
        }

        render_screen(&data, window_size);
        SDL_Delay(33); // ~30 FPS
    }

    if (data.timer_id) {
        SDL_RemoveTimer(data.timer_id);
    }

    SDL_StopTextInput();
    close(data.fd);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return EXIT_SUCCESS;
}

void error(const char* msg) {
    fprintf(stderr, "Erreur: %s\n", msg);
    exit(EXIT_FAILURE);
}