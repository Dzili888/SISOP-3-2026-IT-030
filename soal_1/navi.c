#include "protocol.h"

int sock;
int is_admin = 0;

void *listen_server(void *arg) {
    Packet p;
    while (recv(sock, &p, sizeof(p), 0) > 0) {
        if (p.type == TYPE_SYS_MSG) {
            if (!is_admin) printf("\r\033[K");
            
            printf("%s\n", p.payload);
            
            if (strstr(p.payload, "Disconnecting") != NULL || strstr(p.payload, "SHUTDOWN") != NULL) {
                exit(0);
            }
            
            if (!is_admin) {
                printf("> ");
                fflush(stdout);
            }
        }
    }
    
    printf("\n[System] Server connection lost. Exiting...\n");
    exit(0);
    return NULL;
}

int main() {
    struct sockaddr_in serv_addr;
    char name[50];

    printf("Enter your name: ");
    fgets(name, 50, stdin);
    name[strcspn(name, "\n")] = 0;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    Packet init_packet;
    strcpy(init_packet.name, name);

    if (strcmp(name, ADMIN_NAME) == 0) {
        is_admin = 1;
        init_packet.type = TYPE_AUTH_ADMIN;
        printf("Enter Password: ");
        fgets(init_packet.payload, BUFFER_SIZE, stdin);
        init_packet.payload[strcspn(init_packet.payload, "\n")] = 0;
    } else {
        init_packet.type = TYPE_CONNECT;
    }

    send(sock, &init_packet, sizeof(init_packet), 0);

    Packet resp;
    recv(sock, &resp, sizeof(resp), 0);
    printf("%s\n", resp.payload);

    if (strstr(resp.payload, "already synchronized") != NULL || strstr(resp.payload, "Failed") != NULL) {
        close(sock);
        return 0;
    }

    pthread_t listen_thread;
    pthread_create(&listen_thread, NULL, listen_server, NULL);

    while (1) {
        if (is_admin) {
            printf("\n=== THE KNIGHTS CONSOLE ===\n");
            printf("1. Check Active Entites (Users)\n");
            printf("2. Check Server Uptime\n");
            printf("3. Execute Emergency Shutdown\n");
            printf("4. Disconnect\n");
            printf("Command >> ");
            
            int cmd;
            if (scanf("%d", &cmd) != 1) break;
            
            if (cmd == 4) {
                Packet p; p.type = TYPE_EXIT;
                send(sock, &p, sizeof(p), 0);
                printf("[System] Disconnecting from The Wired...\n");
                break;
            } else {
                Packet p; p.type = TYPE_CMD;
                sprintf(p.payload, "%d", cmd);
                send(sock, &p, sizeof(p), 0);
                sleep(1);
            }
        } else {
            printf("> ");
            char input[BUFFER_SIZE];
            fgets(input, BUFFER_SIZE, stdin);
            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "/exit") == 0) {
                Packet p; p.type = TYPE_EXIT;
                send(sock, &p, sizeof(p), 0);
                printf("[System] Disconnecting from The Wired...\n");
                break;
            }
            if (strlen(input) > 0) {
                Packet p; p.type = TYPE_CHAT;
                strcpy(p.payload, input);
                send(sock, &p, sizeof(p), 0);
            }
        }
    }

    close(sock);
    return 0;
}
