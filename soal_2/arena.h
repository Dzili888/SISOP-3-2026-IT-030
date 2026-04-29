#ifndef ARENA_H
#define ARENA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

#define SHM_KEY 0x1234
#define MSG_KEY 0x5678
#define SEM_KEY 0x9012
#define MAX_USERS 20

typedef struct {
    char opponent[50];
    char result[10];
    int xp_gain;
    int gold_gain;
} MatchHistory;

typedef struct {
    char uname[50];
    char pass[50];
    int gold;
    int xp;
    int weapon_dmg;
    int is_online;
    MatchHistory history[10];
    int hist_count;
} Player;

typedef struct {
    Player p[MAX_USERS];
    int user_count;
    int queue_idx;
    
    int battle_active;
    int p1_idx;
    int p2_idx; 
    int hp1, max_hp1;
    int hp2, max_hp2;
    
    char logs[5][100]; 
    int log_cnt;
} SharedData;

typedef struct {
    long type;
    pid_t pid;
    int cmd; 
    char uname[50];
    char pass[50];
    int val;
} MsgReq;

typedef struct {
    long type;
    int status;
    char text[100];
} MsgRes;

#endif
