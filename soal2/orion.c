#include "arena.h"

EterionData *sys_data;
int mq_id, sem_lock_id;

void acquire_mutex() { struct sembuf s = {0,-1,0}; semop(sem_lock_id, &s, 1); }
void release_mutex() { struct sembuf s = {0,1,0}; semop(sem_lock_id, &s, 1); }

void push_log_entry(char *text) {
    if(sys_data->log_index < 5) {
        strcpy(sys_data->combat_logs[sys_data->log_index++], text);
    } else {
        for(int i=0; i<4; i++) strcpy(sys_data->combat_logs[i], sys_data->combat_logs[i+1]);
        strcpy(sys_data->combat_logs[4], text);
    }
}

void end_match(int victorious_side) {
    sys_data->is_fighting = 0;
    int f1 = sys_data->fighter1_idx;
    int f2 = sys_data->fighter2_idx;
    
    char foe1[50], foe2[50];
    strcpy(foe1, f2 == -2 ? "Wild Beast" : sys_data->fighters[f2].account_name);
    strcpy(foe2, sys_data->fighters[f1].account_name);

    if(victorious_side == 1) {
        sys_data->fighters[f1].experience += 50; sys_data->fighters[f1].money += 120;
        int rec = sys_data->fighters[f1].total_records;
        if(rec < 10) { strcpy(sys_data->fighters[f1].records[rec].enemy_name, foe1); strcpy(sys_data->fighters[f1].records[rec].status_win, "WIN"); sys_data->fighters[f1].records[rec].gained_xp = 50; sys_data->fighters[f1].records[rec].gained_gold = 120; sys_data->fighters[f1].total_records++; }
    } else {
        sys_data->fighters[f1].experience += 15; sys_data->fighters[f1].money += 30;
        int rec = sys_data->fighters[f1].total_records;
        if(rec < 10) { strcpy(sys_data->fighters[f1].records[rec].enemy_name, foe1); strcpy(sys_data->fighters[f1].records[rec].status_win, "LOSS"); sys_data->fighters[f1].records[rec].gained_xp = 15; sys_data->fighters[f1].records[rec].gained_gold = 30; sys_data->fighters[f1].total_records++; }
    }

    if(f2 >= 0) {
        if(victorious_side == 2) {
            sys_data->fighters[f2].experience += 50; sys_data->fighters[f2].money += 120;
            int rec = sys_data->fighters[f2].total_records;
            if(rec < 10) { strcpy(sys_data->fighters[f2].records[rec].enemy_name, foe2); strcpy(sys_data->fighters[f2].records[rec].status_win, "WIN"); sys_data->fighters[f2].records[rec].gained_xp = 50; sys_data->fighters[f2].records[rec].gained_gold = 120; sys_data->fighters[f2].total_records++; }
        } else {
            sys_data->fighters[f2].experience += 15; sys_data->fighters[f2].money += 30;
            int rec = sys_data->fighters[f2].total_records;
            if(rec < 10) { strcpy(sys_data->fighters[f2].records[rec].enemy_name, foe2); strcpy(sys_data->fighters[f2].records[rec].status_win, "LOSS"); sys_data->fighters[f2].records[rec].gained_xp = 15; sys_data->fighters[f2].records[rec].gained_gold = 30; sys_data->fighters[f2].total_records++; }
        }
    }
}

int main() {
    int mem_seg = shmget(MEM_ID, sizeof(EterionData), IPC_CREAT|0666);
    sys_data = shmat(mem_seg, NULL, 0);
    mq_id = msgget(QUEUE_ID, IPC_CREAT|0666);
    sem_lock_id = semget(MUTEX_ID, 1, IPC_CREAT|0666);
    semctl(sem_lock_id, 0, SETVAL, 1);
    
    sys_data->total_fighters = 0; sys_data->is_fighting = 0; sys_data->waiting_player_idx = -1;
    printf(">> Orion is Online [PID: %d]\n", getpid());

    RequestPacket req; ResponsePacket res;
    time_t last_ai_hit = 0;

    while(1) {
        if(msgrcv(mq_id, &req, sizeof(req)-sizeof(long), 1, IPC_NOWAIT) >= 0) {
            res.msg_type = req.sender_pid; res.success_flag = 0; strcpy(res.info_text, "");
            acquire_mutex();
            
            int active_idx = -1;
            for(int i=0; i<sys_data->total_fighters; i++) 
                if(strcmp(sys_data->fighters[i].account_name, req.username) == 0) active_idx = i;

            if(req.action_id == 1) { 
                if(active_idx == -1) {
                    strcpy(sys_data->fighters[sys_data->total_fighters].account_name, req.username);
                    strcpy(sys_data->fighters[sys_data->total_fighters].account_pass, req.password);
                    sys_data->fighters[sys_data->total_fighters].money = 150;
                    sys_data->fighters[sys_data->total_fighters].experience = 0;
                    sys_data->fighters[sys_data->total_fighters].total_records = 0;
                    sys_data->fighters[sys_data->total_fighters].weapon_power = 0;
                    sys_data->total_fighters++;
                    res.success_flag = 1; strcpy(res.info_text, "Registration successful!");
                } else strcpy(res.info_text, "Fighter already exists!");
            } else if(req.action_id == 2) { 
                if(active_idx != -1 && strcmp(sys_data->fighters[active_idx].account_pass, req.password) == 0) {
                    sys_data->fighters[active_idx].is_active = 1;
                    res.success_flag = 1; strcpy(res.info_text, "Welcome to Eterion!");
                } else strcpy(res.info_text, "Authentication failed!");
            } else if(req.action_id == 3 && active_idx != -1) { 
                int price = 0, power = 0;
                if(req.payload==1){price=100; power=5;} else if(req.payload==2){price=300; power=15;}
                else if(req.payload==3){price=600; power=30;} else if(req.payload==4){price=1500; power=60;}
                else if(req.payload==5){price=5000; power=150;}

                if(sys_data->fighters[active_idx].money >= price) {
                    sys_data->fighters[active_idx].money -= price;
                    if(power > sys_data->fighters[active_idx].weapon_power) sys_data->fighters[active_idx].weapon_power = power;
                    res.success_flag = 1; strcpy(res.info_text, "Weapon equipped!");
                } else strcpy(res.info_text, "Not enough Gold!");
            } else if(req.action_id == 4 && active_idx != -1) { 
                if(sys_data->waiting_player_idx == -1) {
                    sys_data->waiting_player_idx = active_idx; res.success_flag = 0; 
                } else {
                    sys_data->fighter1_idx = sys_data->waiting_player_idx; sys_data->fighter2_idx = active_idx;
                    sys_data->f1_max_hp = sys_data->f1_hp = 100 + (sys_data->fighters[sys_data->fighter1_idx].experience/10);
                    sys_data->f2_max_hp = sys_data->f2_hp = 100 + (sys_data->fighters[active_idx].experience/10);
                    sys_data->is_fighting = 1; sys_data->log_index = 0; sys_data->waiting_player_idx = -1;
                    
                    for(int i=0; i<5; i++) strcpy(sys_data->combat_logs[i], "");
                    push_log_entry("PvP Match Commenced!"); res.success_flag = 1;
                }
            } else if(req.action_id == 5 && active_idx != -1) { 
                sys_data->waiting_player_idx = -1;
                sys_data->fighter1_idx = active_idx; sys_data->fighter2_idx = -2;
                sys_data->f1_max_hp = sys_data->f1_hp = 100 + (sys_data->fighters[active_idx].experience/10);
                sys_data->f2_max_hp = sys_data->f2_hp = 100; 
                sys_data->is_fighting = 1; sys_data->log_index = 0;
                for(int i=0; i<5; i++) strcpy(sys_data->combat_logs[i], "");
                push_log_entry("A Wild Beast appears!"); res.success_flag = 1;
            } else if((req.action_id == 6 || req.action_id == 7) && active_idx != -1 && sys_data->is_fighting) { 
                int base_dmg = 10 + (sys_data->fighters[active_idx].experience/50);
                if(req.action_id == 7) base_dmg = (base_dmg + sys_data->fighters[active_idx].weapon_power) * 3;
                else base_dmg += sys_data->fighters[active_idx].weapon_power; 
                
                int is_f1 = (sys_data->fighter1_idx == active_idx);
                if(is_f1) sys_data->f2_hp -= base_dmg; else sys_data->f1_hp -= base_dmg;
                
                char l[100]; sprintf(l, "> %s hits for %d damage!", req.username, base_dmg); push_log_entry(l);
                
                if(sys_data->f1_hp <= 0) end_match(2);
                else if(sys_data->f2_hp <= 0) end_match(1);
            } else if(req.action_id == 8 && active_idx != -1) { 
                sys_data->fighters[active_idx].is_active = 0;
            }
            
            release_mutex();
            msgsnd(mq_id, &res, sizeof(res)-sizeof(long), 0);
        }

        time_t clock_now = time(NULL);
        if(sys_data->is_fighting && sys_data->fighter2_idx == -2 && (clock_now - last_ai_hit >= 2)) {
            acquire_mutex();
            if(sys_data->is_fighting && sys_data->f1_hp > 0) {
                int ai_hit = 10 + (rand() % 5);
                sys_data->f1_hp -= ai_hit;
                char l[100]; sprintf(l, "> Wild Beast strikes back for %d damage!", ai_hit); push_log_entry(l);
                if(sys_data->f1_hp <= 0) end_match(2);
            }
            release_mutex();
            last_ai_hit = clock_now;
        }
        usleep(50000); 
    }
}
