#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024
#define ADMIN_NAME "The Knights"
#define ADMIN_PASS "protocol7"

// Enum untuk tipe pesan (Protokol Komunikasi)
typedef enum {
    TYPE_CONNECT,
    TYPE_AUTH_ADMIN,
    TYPE_CHAT,
    TYPE_CMD,
    TYPE_EXIT,
    TYPE_SYS_MSG
} MsgType;

// Struct untuk paket yang dikirim bolak-balik
typedef struct {
    MsgType type;
    char name[50];
    char payload[BUFFER_SIZE];
} Packet;

#endif
