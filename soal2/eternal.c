#include "arena.h"

EterionData *sys_data;
int mq_id, sem_lock_id;
char player_name[50];

void acquire_mutex() { struct sembuf s = {0,-1,0}; semop(sem_lock_id, &s, 1); }
void release_mutex() { struct sembuf s = {0,1,0}; semop(sem_lock_id, &s, 1); }

void configure_terminal(int active) {
    struct termios cfg; tcgetattr(0, &cfg);
    if(active) cfg.c_lflag &= ~(ICANON|ECHO); else cfg.c_lflag |= (ICANON|ECHO);
    tcsetattr(0, TCSANOW, &cfg);
}

void* display_thread(void* arg) {
    while(1) {
        acquire_mutex();
        if(!sys_data->is_fighting) { release_mutex(); break; }
        int p_hp, p_max, e_hp, e_max, p_xp, p_wpn;
        char e_name[50];
        
        int f1 = sys_data->fighter1_idx; int f2 = sys_data->fighter2_idx;
        if(strcmp(sys_data->fighters[f1].account_name, player_name) == 0) {
            p_hp = sys_data->f1_hp; p_max = sys_data->f1_max_hp; e_hp = sys_data->f2_hp; e_max = sys_data->f2_max_hp;
            strcpy(e_name, f2 == -2 ? "Wild Beast" : sys_data->fighters[f2].account_name);
            p_xp = sys_data->fighters[f1].experience; p_wpn = sys_data->fighters[f1].weapon_power;
        } else {
            p_hp = sys_data->f2_hp; p_max = sys_data->f2_max_hp; e_hp = sys_data->f1_hp; e_max = sys_data->f1_max_hp;
            strcpy(e_name, sys_data->fighters[f1].account_name);
            p_xp = sys_data->fighters[f2].experience; p_wpn = sys_data->fighters[f2].weapon_power;
        }
        
        system("clear");
        printf("--- THE BATTLE ARENA ---\n");
        printf("%s (Lvl %d)\nHP: [%d / %d]\n\n   V S\n\n", e_name, (f2==-2 ? 1 : 1+(sys_data->fighters[f2].experience/100)), e_hp, e_max);
        printf("%s (Lvl %d) [%s]\nHP: [%d / %d]\n\n", player_name, 1+(p_xp/100), p_wpn > 0 ? "Weapon Ready" : "No Weapon", p_hp, p_max);
        
        printf("Live Combat Feed:\n");
        for(int i=0; i<5; i++) printf("%s\n", sys_data->combat_logs[i]);
        
        printf("\nCooldowns -> Attack(1s) | Ultimate(1s)\n");
        printf("Controls: [A] Basic Hit   [U] Ultimate Strike\n");
        release_mutex();
        usleep(200000);
    }
    return NULL;
}

void main_game_loop() {
    RequestPacket req; ResponsePacket res; req.msg_type = 1; req.sender_pid = getpid(); strcpy(req.username, player_name);
    while(1) {
        system("clear");
        acquire_mutex();
        int loc = -1;
        for(int i=0; i<sys_data->total_fighters; i++) if(strcmp(sys_data->fighters[i].account_name, player_name) == 0) loc = i;
        int mny = sys_data->fighters[loc].money; int exp = sys_data->fighters[loc].experience; int lvl = 1 + (exp/100);
        release_mutex();

        printf("--- ETERION MAIN MENU ---\nFighter: %-10s Level: %d\nWallet : %-10d XP   : %d\n\n", player_name, lvl, mny, exp);
        printf("1. Enter Matchmaking\n2. Visit Armory\n3. View Match History\n4. Disconnect\nSelect: ");
        int choice; scanf("%d", &choice);
        
        if(choice == 1) {
            req.action_id = 4; msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
            msgrcv(mq_id, &res, sizeof(res)-sizeof(long), getpid(), 0);
            
            int is_matched = 0;
            for(int sec=35; sec>0; sec--) {
                system("clear");
                printf("Looking for challengers... [%d s]\n", sec);
                acquire_mutex(); is_matched = sys_data->is_fighting; release_mutex();
                if(is_matched) break;
                sleep(1);
            }
            if(!is_matched) {
                req.action_id = 5; msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
                msgrcv(mq_id, &res, sizeof(res)-sizeof(long), getpid(), 0);
            }

            pthread_t ui_thread; pthread_create(&ui_thread, NULL, display_thread, NULL);
            configure_terminal(1); time_t prev_hit = 0;
            
            while(1) {
                acquire_mutex(); int fighting_now = sys_data->is_fighting; release_mutex();
                if(!fighting_now) break;
                
                char key = getchar();
                time_t time_now = time(NULL);
                if(time_now - prev_hit >= 1) { 
                    if(key == 'a') req.action_id = 6; else if(key == 'u') req.action_id = 7;
                    msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
                    prev_hit = time_now;
                }
            }
            configure_terminal(0); pthread_join(ui_thread, NULL);
            
            acquire_mutex();
            int p1 = sys_data->fighter1_idx; 
            int im_p1 = (strcmp(sys_data->fighters[p1].account_name, player_name) == 0);
            int victor = sys_data->f1_hp <= 0 ? 2 : 1;
            int did_i_win = (im_p1 && victor == 1) || (!im_p1 && victor == 2);
            release_mutex();

            system("clear");
            printf("\n%s\n", did_i_win ? "+++ MATCH WON +++" : "--- MATCH LOST ---");
            printf("Press [ENTER] to return to menu...\n");
            getchar(); getchar();

        } else if(choice == 2) {
            system("clear");
            printf("--- THE ARMORY ---\nGold: %d\n1. Wood Sword (100 G)\n2. Iron Sword (300 G)\n3. Steel Axe (600 G)\n4. Demon Blade (1500 G)\n5. God Slayer (5000 G)\n0. Go Back\nItem ID: ", mny);
            scanf("%d", &req.payload);
            if(req.payload > 0) {
                req.action_id = 3; msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
                msgrcv(mq_id, &res, sizeof(res)-sizeof(long), getpid(), 0);
                printf("%s\n", res.info_text); sleep(1);
            }
        } else if(choice == 3) {
            system("clear");
            printf("--- BATTLE LOGS ---\nTime\t| Foe\t\t| Status\t| Earned XP\n");
            acquire_mutex();
            for(int i=0; i<sys_data->fighters[loc].total_records; i++) {
                CombatRecord rec = sys_data->fighters[loc].records[i];
                printf("--:--\t| %-10s\t| %s\t| +%d\n", rec.enemy_name, rec.status_win, rec.gained_xp);
            }
            release_mutex();
            printf("\nPress [ENTER] to exit logs..."); getchar(); getchar();
        } else if(choice == 4) {
            req.action_id = 8; msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
            break;
        }
    }
}

int main() {
    int mem_seg = shmget(MEM_ID, sizeof(EterionData), 0666);
    sys_data = shmat(mem_seg, NULL, 0);
    mq_id = msgget(QUEUE_ID, 0666);
    sem_lock_id = semget(MUTEX_ID, 1, 0666);

    RequestPacket req; ResponsePacket res; req.msg_type = 1; req.sender_pid = getpid();
    while(1) {
        system("clear");
        printf("1. Create Account\n2. Sign In\n3. Quit\nInput: ");
        int act; scanf("%d", &act);
        if(act == 3) break;
        
        printf("Username: "); scanf("%s", req.username);
        printf("Password: "); scanf("%s", req.password);
        req.action_id = act;
        msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
        msgrcv(mq_id, &res, sizeof(res)-sizeof(long), getpid(), 0);
        
        printf("%s\n", res.info_text); sleep(1);
        
        if(res.success_flag == 1 && act == 2) {
            strcpy(player_name, req.username); main_game_loop();
        }
    }
}
