#include "arena.h"

SharedData *shm;
int msgid, semid;

void lock() { struct sembuf s = {0,-1,0}; semop(semid, &s, 1); }
void unlock() { struct sembuf s = {0,1,0}; semop(semid, &s, 1); }

void add_log(char *txt) {
    if(shm->log_cnt < 5) {
        strcpy(shm->logs[shm->log_cnt++], txt);
    } else {
        for(int i=0; i<4; i++) strcpy(shm->logs[i], shm->logs[i+1]);
        strcpy(shm->logs[4], txt);
    }
}

void finish_battle(int win_side) {
    shm->battle_active = 0;
    int p1 = shm->p1_idx;
    int p2 = shm->p2_idx;
    
    char opp1[50], opp2[50];
    strcpy(opp1, p2 == -2 ? "Wild Beast" : shm->p[p2].uname);
    strcpy(opp2, shm->p[p1].uname);

    if(win_side == 1) {
        shm->p[p1].xp += 50; shm->p[p1].gold += 120;
        int hc = shm->p[p1].hist_count;
        if(hc < 10) { strcpy(shm->p[p1].history[hc].opponent, opp1); strcpy(shm->p[p1].history[hc].result, "WIN"); shm->p[p1].history[hc].xp_gain = 50; shm->p[p1].history[hc].gold_gain = 120; shm->p[p1].hist_count++; }
    } else {
        shm->p[p1].xp += 15; shm->p[p1].gold += 30;
        int hc = shm->p[p1].hist_count;
        if(hc < 10) { strcpy(shm->p[p1].history[hc].opponent, opp1); strcpy(shm->p[p1].history[hc].result, "LOSS"); shm->p[p1].history[hc].xp_gain = 15; shm->p[p1].history[hc].gold_gain = 30; shm->p[p1].hist_count++; }
    }

    if(p2 >= 0) {
        if(win_side == 2) {
            shm->p[p2].xp += 50; shm->p[p2].gold += 120;
            int hc = shm->p[p2].hist_count;
            if(hc < 10) { strcpy(shm->p[p2].history[hc].opponent, opp2); strcpy(shm->p[p2].history[hc].result, "WIN"); shm->p[p2].history[hc].xp_gain = 50; shm->p[p2].history[hc].gold_gain = 120; shm->p[p2].hist_count++; }
        } else {
            shm->p[p2].xp += 15; shm->p[p2].gold += 30;
            int hc = shm->p[p2].hist_count;
            if(hc < 10) { strcpy(shm->p[p2].history[hc].opponent, opp2); strcpy(shm->p[p2].history[hc].result, "LOSS"); shm->p[p2].history[hc].xp_gain = 15; shm->p[p2].history[hc].gold_gain = 30; shm->p[p2].hist_count++; }
        }
    }
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT|0666);
    shm = shmat(shmid, NULL, 0);
    msgid = msgget(MSG_KEY, IPC_CREAT|0666);
    semid = semget(SEM_KEY, 1, IPC_CREAT|0666);
    semctl(semid, 0, SETVAL, 1);
    
    shm->user_count = 0; shm->battle_active = 0; shm->queue_idx = -1;
    printf("Orion Ready (PID: %d)\n", getpid());

    MsgReq req; MsgRes res;
    time_t last_bot_atk = 0;

    while(1) {
        if(msgrcv(msgid, &req, sizeof(req)-sizeof(long), 1, IPC_NOWAIT) >= 0) {
            res.type = req.pid; res.status = 0; strcpy(res.text, "");
            lock();
            
            int my_idx = -1;
            for(int i=0; i<shm->user_count; i++) 
                if(strcmp(shm->p[i].uname, req.uname) == 0) my_idx = i;

            if(req.cmd == 1) { 
                if(my_idx == -1) {
                    strcpy(shm->p[shm->user_count].uname, req.uname);
                    strcpy(shm->p[shm->user_count].pass, req.pass);
                    shm->p[shm->user_count].gold = 150;
                    shm->p[shm->user_count].xp = 0;
                    shm->p[shm->user_count].hist_count = 0;
                    shm->p[shm->user_count].weapon_dmg = 0;
                    shm->user_count++;
                    res.status = 1; strcpy(res.text, "Account created!");
                } else strcpy(res.text, "Username exists!");
            } else if(req.cmd == 2) { 
                if(my_idx != -1 && strcmp(shm->p[my_idx].pass, req.pass) == 0) {
                    shm->p[my_idx].is_online = 1;
                    res.status = 1; strcpy(res.text, "Welcome!");
                } else strcpy(res.text, "Invalid login!");
            } else if(req.cmd == 3 && my_idx != -1) { 
                int w_cost = 0, w_dmg = 0;
                if(req.val==1){w_cost=100; w_dmg=5;} else if(req.val==2){w_cost=300; w_dmg=15;}
                else if(req.val==3){w_cost=600; w_dmg=30;} else if(req.val==4){w_cost=1500; w_dmg=60;}
                else if(req.val==5){w_cost=5000; w_dmg=150;}

                if(shm->p[my_idx].gold >= w_cost) {
                    shm->p[my_idx].gold -= w_cost;
                    if(w_dmg > shm->p[my_idx].weapon_dmg) shm->p[my_idx].weapon_dmg = w_dmg;
                    res.status = 1; strcpy(res.text, "Weapon bought!");
                } else strcpy(res.text, "Not enough Gold!");
            } else if(req.cmd == 4 && my_idx != -1) { 
                if(shm->queue_idx == -1) {
                    shm->queue_idx = my_idx; res.status = 0; 
                } else {
                    shm->p1_idx = shm->queue_idx; shm->p2_idx = my_idx;
                    shm->max_hp1 = shm->hp1 = 100 + (shm->p[shm->p1_idx].xp/10);
                    shm->max_hp2 = shm->hp2 = 100 + (shm->p[my_idx].xp/10);
                    shm->battle_active = 1; shm->log_cnt = 0; shm->queue_idx = -1;
                    
                    for(int i=0; i<5; i++) strcpy(shm->logs[i], "");
                    add_log("PvP Battle Started!"); res.status = 1;
                }
            } else if(req.cmd == 5 && my_idx != -1) { 
                shm->queue_idx = -1;
                shm->p1_idx = my_idx; shm->p2_idx = -2;
                shm->max_hp1 = shm->hp1 = 100 + (shm->p[my_idx].xp/10);
                shm->max_hp2 = shm->hp2 = 100; 
                shm->battle_active = 1; shm->log_cnt = 0;
                for(int i=0; i<5; i++) strcpy(shm->logs[i], "");
                add_log("Battle vs Wild Beast Started!"); res.status = 1;
            } else if((req.cmd == 6 || req.cmd == 7) && my_idx != -1 && shm->battle_active) { 
                int dmg = 10 + (shm->p[my_idx].xp/50);
                if(req.cmd == 7) dmg = (dmg + shm->p[my_idx].weapon_dmg) * 3;
                else dmg += shm->p[my_idx].weapon_dmg; 
                
                int is_p1 = (shm->p1_idx == my_idx);
                if(is_p1) shm->hp2 -= dmg; else shm->hp1 -= dmg;
                
                char l[100]; sprintf(l, "> %s hit for %d damage!", req.uname, dmg); add_log(l);
                
                if(shm->hp1 <= 0) finish_battle(2);
                else if(shm->hp2 <= 0) finish_battle(1);
            } else if(req.cmd == 8 && my_idx != -1) { 
                shm->p[my_idx].is_online = 0;
            }
            
            unlock();
            msgsnd(msgid, &res, sizeof(res)-sizeof(long), 0);
        }

        time_t now = time(NULL);
        if(shm->battle_active && shm->p2_idx == -2 && (now - last_bot_atk >= 2)) {
            lock();
            if(shm->battle_active && shm->hp1 > 0) {
                int bot_dmg = 10 + (rand() % 5);
                shm->hp1 -= bot_dmg;
                char l[100]; sprintf(l, "> Wild Beast hit for %d damage!", bot_dmg); add_log(l);
                if(shm->hp1 <= 0) finish_battle(2);
            }
            unlock();
            last_bot_atk = now;
        }
        usleep(50000); 
    }
}
