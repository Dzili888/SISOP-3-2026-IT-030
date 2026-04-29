#include "protocol.h"

typedef struct {
    int fd;
    char name[50];
    int is_admin;
    int active;
} Client;

Client clients[MAX_CLIENTS];
time_t start_time;

void get_timestamp(char *buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
}

void write_log(const char *role, const char *msg) {
    FILE *f = fopen("history.log", "a");
    if (!f) return;
    char ts[50];
    get_timestamp(ts);
    fprintf(f, "[%s] [%s] [%s]\n", ts, role, msg);
    fclose(f);
    printf("[%s] [%s] [%s]\n", ts, role, msg); 
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].active = 0;
    
    start_time = time(NULL);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    write_log("System", "SERVER ONLINE");

    fd_set readfds;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                FD_SET(clients[i].fd, &readfds);
                if (clients[i].fd > max_sd) max_sd = clients[i].fd;
            }
        }

        select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket;
            int addrlen = sizeof(address);
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
                Packet p;
                recv(new_socket, &p, sizeof(p), 0);

                int name_exists = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].active && strcmp(clients[i].name, p.name) == 0 && strcmp(p.name, ADMIN_NAME) != 0) {
                        name_exists = 1;
                        break;
                    }
                }

                Packet resp;
                resp.type = TYPE_SYS_MSG;
                if (name_exists) {
                    sprintf(resp.payload, "[System] The identity '%s' is already synchronized in The Wired.", p.name);
                    send(new_socket, &resp, sizeof(resp), 0);
                    close(new_socket);
                } else {
                    int is_admin = 0;
                    if (p.type == TYPE_AUTH_ADMIN && strcmp(p.payload, ADMIN_PASS) == 0) {
                        is_admin = 1;
                        sprintf(resp.payload, "[System] Authentication Successful. Granted Admin privileges.");
                    } else if (p.type == TYPE_AUTH_ADMIN) {
                        sprintf(resp.payload, "[System] Authentication Failed.");
                        send(new_socket, &resp, sizeof(resp), 0);
                        close(new_socket);
                        continue;
                    } else {
                        sprintf(resp.payload, "--- Welcome to The Wired, %s ---", p.name);
                    }

                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (!clients[i].active) {
                            clients[i].fd = new_socket;
                            strcpy(clients[i].name, p.name);
                            clients[i].is_admin = is_admin;
                            clients[i].active = 1;
                            break;
                        }
                    }
                    send(new_socket, &resp, sizeof(resp), 0);
                    
                    char log_msg[100];
                    sprintf(log_msg, "User '%s' connected", p.name);
                    write_log("System", log_msg);
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && FD_ISSET(clients[i].fd, &readfds)) {
                Packet p;
                int valread = recv(clients[i].fd, &p, sizeof(p), 0);

                if (valread == 0 || p.type == TYPE_EXIT) {
                    char log_msg[100];
                    sprintf(log_msg, "User '%s' disconnected", clients[i].name);
                    write_log("System", log_msg);
                    close(clients[i].fd);
                    clients[i].active = 0;
                } 
                else if (p.type == TYPE_CHAT) {
                    char log_msg[1200];
                    sprintf(log_msg, "[%s]: %s", clients[i].name, p.payload);
                    write_log("User", log_msg);

                    Packet broadcast;
                    broadcast.type = TYPE_SYS_MSG;
                    sprintf(broadcast.payload, "[%s]: %s", clients[i].name, p.payload);

                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].active && clients[j].fd != clients[i].fd && !clients[j].is_admin) {
                            send(clients[j].fd, &broadcast, sizeof(broadcast), 0);
                        }
                    }
                }
                else if (p.type == TYPE_CMD && clients[i].is_admin) {
                    int cmd = atoi(p.payload);
                    Packet resp;
                    resp.type = TYPE_SYS_MSG;

                    if (cmd == 1) {
                        write_log("Admin", "RPC_GET_USERS");
                        int count = 0;
                        for(int j=0; j<MAX_CLIENTS; j++) if(clients[j].active && !clients[j].is_admin) count++;
                        sprintf(resp.payload, "[System] Active Entities: %d", count);
                        send(clients[i].fd, &resp, sizeof(resp), 0);
                    } 
                    else if (cmd == 2) {
                        write_log("Admin", "RPC_GET_UPTIME");
                        time_t now = time(NULL);
                        sprintf(resp.payload, "[System] Server Uptime: %ld seconds", now - start_time);
                        send(clients[i].fd, &resp, sizeof(resp), 0);
                    } 
                    else if (cmd == 3) {
                        write_log("Admin", "RPC_SHUTDOWN");
                        write_log("System", "EMERGENCY SHUTDOWN INITIATED");
                        sprintf(resp.payload, "[System] Disconnecting from The Wired...");
                        
                        for(int j=0; j<MAX_CLIENTS; j++) {
                            if(clients[j].active) {
                                send(clients[j].fd, &resp, sizeof(resp), 0);
                            }
                        }
                        sleep(1); // Jeda agar klien sempat terima pesan sebelum server mati
                        exit(0);
                    }
                }
            }
        }
    }
    return 0;
}
