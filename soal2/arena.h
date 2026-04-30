#ifndef ETERION_SYSTEM_H
#define ETERION_SYSTEM_H

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

#define MEM_ID 0x2233
#define QUEUE_ID 0x4455
#define MUTEX_ID 0x6677
#define MAX_FIGHTERS 20

typedef struct {
    char enemy_name[50];
    char status_win[10];
    int gained_xp;
    int gained_gold;
} CombatRecord;

typedef struct {
    char account_name[50];
    char account_pass[50];
    int money;
    int experience;
    int weapon_power;
    int is_active;
    CombatRecord records[10];
    int total_records;
} Fighter;

typedef struct {
    Fighter fighters[MAX_FIGHTERS];
    int total_fighters;
    int waiting_player_idx;
    
    int is_fighting;
    int fighter1_idx;
    int fighter2_idx; 
    int f1_hp, f1_max_hp;
    int f2_hp, f2_max_hp;
    
    char combat_logs[5][100]; 
    int log_index;
} EterionData;

typedef struct {
    long msg_type;
    pid_t sender_pid;
    int action_id; 
    char username[50];
    char password[50];
    int payload;
} RequestPacket;

typedef struct {
    long msg_type;
    int success_flag;
    char info_text[100];
} ResponsePacket;

#endif
