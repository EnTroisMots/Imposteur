#include "client.h"

int init_network(const char* ip, const char* port, client_data_t* data) {
    struct sockaddr_in serv_addr;

    data->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (data->fd < 0) {
        error("socket");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        error("inet_pton");
        return -1;
    }

    if (connect(data->fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error("connect");
        return -1;
    }

    printf("Connecté au serveur %s:%s\n", ip, port);
    return 0;
}

void send_message(int fd, const char* msg) {
    if (send(fd, msg, strlen(msg), 0) < 0) {
        perror("send");
    }
}