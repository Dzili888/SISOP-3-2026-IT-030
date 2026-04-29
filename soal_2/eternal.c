#include "arena.h"

SharedData *shm;
int msgid, semid;
char me[50];

void lock() { struct sembuf s = {0,-1,0}; semop(semid, &s, 1); }
void unlock() { struct sembuf s = {0,1,0}; semop(semid, &s, 1); }

void set_raw(int enable) {
    struct termios t; tcgetattr(0, &t);
    if(enable) t.c_lflag &= ~(ICANON|ECHO); else t.c_lflag |= (ICANON|ECHO);
    tcsetattr(0, TCSANOW, &t);
}

void* render(void* arg) {
    while(1) {
        lock();
        if(!shm->battle_active) { unlock(); break; }
        int my_hp, my_max, en_hp, en_max, my_xp, wpn;
        char en_name[50];
        
        int p1 = shm->p1_idx; int p2 = shm->p2_idx;
        if(strcmp(shm->p[p1].uname, me) == 0) {
            my_hp = shm->hp1; my_max = shm->max_hp1; en_hp = shm->hp2; en_max = shm->max_hp2;
            strcpy(en_name, p2 == -2 ? "Wild Beast" : shm->p[p2].uname);
            my_xp = shm->p[p1].xp; wpn = shm->p[p1].weapon_dmg;
        } else {
            my_hp = shm->hp2; my_max = shm->max_hp2; en_hp = shm->hp1; en_max = shm->max_hp1;
            strcpy(en_name, shm->p[p1].uname);
            my_xp = shm->p[p2].xp; wpn = shm->p[p2].weapon_dmg;
        }
        
        system("clear");
        printf("=== ARENA ===\n");
        printf("%-15s Lvl %d\n[ %d/%d ]\n\nVS\n\n", en_name, (p2==-2 ? 1 : 1+(shm->p[p2].xp/100)), en_hp, en_max);
        printf("%-15s Lvl %d | Weapon: %s\n[ %d/%d ]\n\n", me, 1+(my_xp/100), wpn > 0 ? "Equipped" : "None", my_hp, my_max);
        
        printf("Combat Log:\n");
        for(int i=0; i<5; i++) printf("%s\n", shm->logs[i]);
        
        printf("\nCD: Atk(1.0s) | Ult(1.0s)\n");
        printf("[A] Attack   [U] Ultimate\n");
        unlock();
        usleep(200000);
    }
    return NULL;
}

void play() {
    MsgReq req; MsgRes res; req.type = 1; req.pid = getpid(); strcpy(req.uname, me);
    while(1) {
        system("clear");
        lock();
        int idx = -1;
        for(int i=0; i<shm->user_count; i++) if(strcmp(shm->p[i].uname, me) == 0) idx = i;
        int g = shm->p[idx].gold; int x = shm->p[idx].xp; int l = 1 + (x/100);
        unlock();

        printf("PROFILE\nName: %-10s Lvl: %d\nGold: %-10d XP : %d\n\n", me, l, g, x);
        printf("1. Battle\n2. Armory\n3. History\n4. Logout\n> Choice: ");
        int c; scanf("%d", &c);
        
        if(c == 1) {
            req.cmd = 4; msgsnd(msgid, &req, sizeof(req)-sizeof(long), 0);
            msgrcv(msgid, &res, sizeof(res)-sizeof(long), getpid(), 0);
            
            int matched = 0;
            for(int i=35; i>0; i--) {
                system("clear");
                printf("Searching for an opponent... [%d s]\n", i);
                lock(); matched = shm->battle_active; unlock();
                if(matched) break;
                sleep(1);
            }
            if(!matched) {
                req.cmd = 5; msgsnd(msgid, &req, sizeof(req)-sizeof(long), 0);
                msgrcv(msgid, &res, sizeof(res)-sizeof(long), getpid(), 0);
            }

            pthread_t tid; pthread_create(&tid, NULL, render, NULL);
            set_raw(1); time_t last = 0;
            
            while(1) {
                lock(); int active = shm->battle_active; unlock();
                if(!active) break;
                
                char k = getchar();
                time_t now = time(NULL);
                if(now - last >= 1) { 
                    if(k == 'a') req.cmd = 6; else if(k == 'u') req.cmd = 7;
                    msgsnd(msgid, &req, sizeof(req)-sizeof(long), 0);
                    last = now;
                }
            }
            set_raw(0); pthread_join(tid, NULL);
            
            lock();
            int p1 = shm->p1_idx; 
            int am_i_p1 = (strcmp(shm->p[p1].uname, me) == 0);
            int win_side = shm->hp1 <= 0 ? 2 : 1;
            int i_win = (am_i_p1 && win_side == 1) || (!am_i_p1 && win_side == 2);
            unlock();

            system("clear");
            printf("\n%s\n", i_win ? "=== VICTORY ===" : "=== DEFEAT ===");
            printf("Battle ended. Press [ENTER] to continue...\n");
            getchar(); getchar();

        } else if(c == 2) {
            system("clear");
            printf("=== ARMORY ===\nGold: %d\n1. Wood Sword (100 G)\n2. Iron Sword (300 G)\n3. Steel Axe (600 G)\n4. Demon Blade (1500 G)\n5. God Slayer (5000 G)\n0. Back\nChoice: ", g);
            scanf("%d", &req.val);
            if(req.val > 0) {
                req.cmd = 3; msgsnd(msgid, &req, sizeof(req)-sizeof(long), 0);
                msgrcv(msgid, &res, sizeof(res)-sizeof(long), getpid(), 0);
                printf("%s\n", res.text); sleep(1);
            }
        } else if(c == 3) {
            system("clear");
            printf("=== MATCH HISTORY ===\nTime\t| Opponent\t| Res\t| XP\n");
            lock();
            for(int i=0; i<shm->p[idx].hist_count; i++) {
                MatchHistory mh = shm->p[idx].history[i];
                printf("--:--\t| %-10s\t| %s\t| +%d\n", mh.opponent, mh.result, mh.xp_gain);
            }
            unlock();
            printf("\nPress [ENTER] to continue..."); getchar(); getchar();
        } else if(c == 4) {
            req.cmd = 8; msgsnd(msgid, &req, sizeof(req)-sizeof(long), 0);
            break;
        }
    }
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    shm = shmat(shmid, NULL, 0);
    msgid = msgget(MSG_KEY, 0666);
    semid = semget(SEM_KEY, 1, 0666);

    MsgReq req; MsgRes res; req.type = 1; req.pid = getpid();
    while(1) {
        system("clear");
        printf("1. Register\n2. Login\n3. Exit\nChoice: ");
        int c; scanf("%d", &c);
        if(c == 3) break;
        
        printf("Username: "); scanf("%s", req.uname);
        printf("Password: "); scanf("%s", req.pass);
        req.cmd = c;
        msgsnd(msgid, &req, sizeof(req)-sizeof(long), 0);
        msgrcv(msgid, &res, sizeof(res)-sizeof(long), getpid(), 0);
        
        printf("%s\n", res.text); sleep(1);
        
        if(res.status == 1 && c == 2) {
            strcpy(me, req.uname); play();
        }
    }
}
