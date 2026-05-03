# Laporan Resmi Praktikum Sistem Operasi - Modul 3

## Disusun oleh
| No  | Nama                          | NRP        |
| --- | ----------------------------- | ---------- |
| 1   | Muhammad Syadzili Abdul Muhyi | 5027251030 |

---

## Soal 1 - Present Day, Present Time

### Deskripsi Soal
Pada soal ini dibuat sebuah sistem komunikasi bernama **The Wired** yang terdiri dari dua program utama, yaitu `wired.c` sebagai server pusat dan `navi.c` sebagai client. Seluruh konfigurasi komunikasi seperti port, ukuran buffer, struktur paket, serta kredensial admin disimpan di `protocol.h`.

Server harus mampu menerima banyak client secara bersamaan tanpa membuat satu client menghambat client lainnya. Karena itu, server memakai socket TCP dan mekanisme `select()` untuk memantau banyak file descriptor sekaligus. Client NAVI juga harus mampu mengirim pesan dan menerima pesan secara asinkron tanpa menggunakan `fork()`, sehingga pada sisi client digunakan `pthread` untuk membuat thread pendengar server.

Selain fitur chat biasa, terdapat identitas khusus bernama `The Knights` yang dapat login sebagai admin menggunakan password. Admin dapat menjalankan prosedur jarak jauh berupa melihat jumlah user aktif, melihat uptime server, dan mematikan server. Seluruh aktivitas penting, mulai dari server online, user connect/disconnect, chat, sampai command admin, dicatat ke dalam `history.log`.

### Source Code (`protocol.h`)

```c
#define PORT 8080
#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024
#define ADMIN_NAME "The Knights"
#define ADMIN_PASS "protocol7"
```

Bagian awal `protocol.h` berisi konfigurasi global yang dipakai bersama oleh server dan client. `PORT` menentukan port TCP yang digunakan, `MAX_CLIENTS` membatasi jumlah client aktif yang dapat ditangani server, dan `BUFFER_SIZE` menentukan ukuran maksimal isi pesan. Identitas admin juga didefinisikan di sini agar validasi antara `wired.c` dan `navi.c` tetap konsisten.

```c
typedef enum {
    TYPE_CONNECT,
    TYPE_AUTH_ADMIN,
    TYPE_CHAT,
    TYPE_CMD,
    TYPE_EXIT,
    TYPE_SYS_MSG
} MsgType;

typedef struct {
    MsgType type;
    char name[50];
    char payload[BUFFER_SIZE];
} Packet;
```

Komunikasi antara server dan client tidak dikirim sebagai string mentah, tetapi dibungkus dalam struktur `Packet`. Field `type` menentukan jenis pesan, misalnya koneksi awal, autentikasi admin, chat, command, exit, atau pesan sistem. Field `name` menyimpan identitas client, sedangkan `payload` menyimpan isi pesan atau data command.

Dengan format paket seperti ini, server dapat membedakan apakah data yang diterima perlu diproses sebagai chat biasa, autentikasi admin, atau RPC command tanpa perlu melakukan parsing string yang rumit.

### Source Code (`wired.c`)

```c
typedef struct {
    int fd;
    char name[50];
    int is_admin;
    int active;
} Client;

Client clients[MAX_CLIENTS];
time_t start_time;
```

Server menyimpan seluruh client aktif dalam array `clients`. Setiap client memiliki file descriptor socket (`fd`), nama identitas, status admin, dan penanda apakah slot tersebut sedang aktif. Variabel `start_time` digunakan untuk menghitung durasi server berjalan ketika admin menjalankan command uptime.

```c
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
```

Fungsi `get_timestamp()` mengambil waktu sistem lalu memformatnya ke bentuk `YYYY-MM-DD HH:MM:SS` sesuai ketentuan soal. Fungsi `write_log()` kemudian menuliskan aktivitas ke `history.log` dengan format `[timestamp] [role] [message]`. Mode `"a"` dipakai agar log baru selalu ditambahkan ke akhir file tanpa menghapus riwayat sebelumnya.

```c
server_fd = socket(AF_INET, SOCK_STREAM, 0);
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
address.sin_port = htons(PORT);

bind(server_fd, (struct sockaddr *)&address, sizeof(address));
listen(server_fd, 5);
```

Bagian ini adalah inisialisasi server TCP. `socket()` membuat endpoint komunikasi IPv4 berbasis stream. `setsockopt()` dipakai agar port dapat digunakan kembali setelah server berhenti. Setelah alamat dan port disiapkan, `bind()` memasangkan socket ke port `8080`, lalu `listen()` membuat server siap menerima koneksi masuk.

```c
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
```

Server menggunakan `select()` untuk melakukan multiplexing. Dengan cara ini, server dapat memantau socket utama untuk koneksi baru sekaligus seluruh socket client aktif untuk pesan masuk. Jika salah satu descriptor siap dibaca, `select()` akan mengembalikan kontrol ke program sehingga server dapat memproses event tersebut.

Pendekatan ini membuat server tetap responsif untuk banyak client tanpa perlu membuat proses baru untuk setiap koneksi.

```c
if (FD_ISSET(server_fd, &readfds)) {
    int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    Packet p;
    recv(new_socket, &p, sizeof(p), 0);

    int name_exists = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].name, p.name) == 0 && strcmp(p.name, ADMIN_NAME) != 0) {
            name_exists = 1;
            break;
        }
    }
```

Ketika socket server aktif, artinya ada client baru yang mencoba masuk. Server menerima koneksi dengan `accept()`, lalu membaca paket awal dari client. Setelah itu, server melakukan pengecekan nama agar tidak ada dua user biasa dengan identitas yang sama di The Wired. Pengecualian diberikan untuk nama admin karena `The Knights` diproses melalui jalur autentikasi khusus.

```c
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
```

Jika client masuk dengan nama `The Knights`, client akan mengirim paket bertipe `TYPE_AUTH_ADMIN`. Server membandingkan password yang diterima dengan `ADMIN_PASS`. Jika cocok, client diberi hak admin. Jika salah, server mengirim pesan gagal autentikasi dan langsung menutup koneksi.

Untuk user biasa, server mengirim pesan selamat datang dan memasukkan client ke slot aktif.

```c
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
```

Jika server menerima paket chat, isi pesan dicatat ke `history.log`, lalu disiapkan sebagai paket broadcast. Pesan dikirim ke seluruh client aktif selain pengirim. Admin tidak ikut menerima broadcast chat karena admin beroperasi melalui console command khusus, bukan ruang obrolan biasa.

```c
else if (p.type == TYPE_CMD && clients[i].is_admin) {
    int cmd = atoi(p.payload);

    if (cmd == 1) {
        write_log("Admin", "RPC_GET_USERS");
        int count = 0;
        for(int j=0; j<MAX_CLIENTS; j++) if(clients[j].active && !clients[j].is_admin) count++;
        sprintf(resp.payload, "[System] Active Entities: %d", count);
        send(clients[i].fd, &resp, sizeof(resp), 0);
    }
```

Command admin dikirim sebagai paket `TYPE_CMD`. Untuk command pertama, server menghitung jumlah client aktif yang bukan admin, lalu mengirimkan hasilnya kembali hanya ke admin. Command ini berfungsi sebagai RPC sederhana karena admin meminta data internal server tanpa melewati jalur broadcast chat.

```c
    else if (cmd == 2) {
        write_log("Admin", "RPC_GET_UPTIME");
        time_t now = time(NULL);
        sprintf(resp.payload, "[System] Server Uptime: %ld seconds", now - start_time);
        send(clients[i].fd, &resp, sizeof(resp), 0);
    }
```

Command kedua menghitung uptime server dari selisih waktu saat ini dengan `start_time`. Hasilnya dikirim kembali sebagai pesan sistem kepada admin.

```c
    else if (cmd == 3) {
        write_log("Admin", "RPC_SHUTDOWN");
        write_log("System", "EMERGENCY SHUTDOWN INITIATED");
        sprintf(resp.payload, "[System] Disconnecting from The Wired...");

        for(int j=0; j<MAX_CLIENTS; j++) {
            if(clients[j].active) {
                send(clients[j].fd, &resp, sizeof(resp), 0);
            }
        }
        sleep(1);
        exit(0);
    }
}
```

Command ketiga adalah emergency shutdown. Server mencatat command admin dan status shutdown ke log, lalu mengirim pesan pemutusan koneksi ke seluruh client aktif. Setelah diberi jeda singkat agar pesan sempat diterima client, server keluar dengan `exit(0)`.

### Source Code (`navi.c`)

```c
int sock;
int is_admin = 0;

void handle_sigint(int sig) {
    printf("\n[System] Disconnecting from The Wired...\n");
    Packet p;
    p.type = TYPE_EXIT;
    send(sock, &p, sizeof(p), 0);
    close(sock);
    exit(0);
}
```

Client memasang handler untuk `SIGINT`, yaitu sinyal yang muncul ketika user menekan `Ctrl+C`. Handler ini membuat client tidak langsung mati begitu saja, tetapi mengirim paket `TYPE_EXIT` ke server terlebih dahulu. Dengan begitu, server dapat menutup koneksi secara bersih dan mencatat disconnect ke `history.log`.

```c
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
}
```

Fungsi `listen_server()` dijalankan dalam thread terpisah. Tugasnya adalah terus menerima pesan dari server tanpa mengganggu input user pada thread utama. Jika server mengirim pesan shutdown atau disconnect, client langsung keluar. Untuk user biasa, prompt `>` ditampilkan ulang setelah pesan broadcast diterima agar tampilan chat tetap rapi.

```c
printf("Enter your name: ");
fgets(name, 50, stdin);
name[strcspn(name, "\n")] = 0;

sock = socket(AF_INET, SOCK_STREAM, 0);
serv_addr.sin_family = AF_INET;
serv_addr.sin_port = htons(PORT);
inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
```

Pada awal program, NAVI meminta nama identitas user, membuat socket TCP, lalu menghubungi server di alamat `127.0.0.1` dan port yang didefinisikan pada `protocol.h`.

```c
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
recv(sock, &resp, sizeof(resp), 0);
```

Jika nama yang dimasukkan adalah `The Knights`, client akan meminta password dan mengirim paket autentikasi admin. Jika bukan, client mengirim paket koneksi biasa. Setelah itu, client menunggu respons server untuk menentukan apakah koneksi diterima atau ditolak.

```c
pthread_t listen_thread;
pthread_create(&listen_thread, NULL, listen_server, NULL);
```

Thread pendengar server dibuat setelah koneksi berhasil. Inilah bagian yang memenuhi kebutuhan asinkron tanpa `fork()`: thread utama tetap membaca input user, sedangkan thread kedua menerima pesan dari server.

```c
if (is_admin) {
    printf("\n=== THE KNIGHTS CONSOLE ===\n");
    printf("1. Check Active Entites (Users)\n");
    printf("2. Check Server Uptime\n");
    printf("3. Execute Emergency Shutdown\n");
    printf("4. Disconnect\n");
    printf("Command >> ");

    int cmd;
    scanf("%d", &cmd);

    if (cmd == 4) {
        Packet p; p.type = TYPE_EXIT;
        send(sock, &p, sizeof(p), 0);
        break;
    } else {
        Packet p; p.type = TYPE_CMD;
        sprintf(p.payload, "%d", cmd);
        send(sock, &p, sizeof(p), 0);
        sleep(1);
    }
}
```

Jika client adalah admin, tampilan berubah menjadi console `The Knights`. Pilihan 1 sampai 3 dikirim sebagai `TYPE_CMD` ke server, sedangkan pilihan 4 mengirim `TYPE_EXIT`. Command admin tidak dikirim sebagai chat sehingga tidak masuk ke broadcast user.

```c
else {
    printf("> ");
    char input[BUFFER_SIZE];
    fgets(input, BUFFER_SIZE, stdin);
    input[strcspn(input, "\n")] = 0;

    if (strcmp(input, "/exit") == 0) {
        Packet p; p.type = TYPE_EXIT;
        send(sock, &p, sizeof(p), 0);
        printf("\n[System] Disconnecting from The Wired...\n");
        break;
    }
    if (strlen(input) > 0) {
        Packet p; p.type = TYPE_CHAT;
        strcpy(p.payload, input);
        send(sock, &p, sizeof(p), 0);
    }
}
```

Untuk user biasa, setiap input dikirim sebagai `TYPE_CHAT`. Jika user mengetik `/exit`, client mengirim paket exit agar server dapat membersihkan slot client aktif dan menulis log disconnect.

### Output

1. Kompilasi program server dan client

```bash
gcc -Wall -pthread soal1/wired.c -o wired
gcc -Wall -pthread soal1/navi.c -o navi
```

2. Server berhasil online dan membuat log awal

```text
[YYYY-MM-DD HH:MM:SS] [System] [SERVER ONLINE]
```

3. User biasa berhasil masuk ke The Wired

```text
Enter your name: alice
--- Welcome to The Wired, alice ---
>
```

4. Jika nama user sudah dipakai, server menolak koneksi baru

```text
[System] The identity 'alice' is already synchronized in The Wired.
```

5. Admin berhasil login dan menjalankan RPC

```text
Enter your name: The Knights
Enter Password: protocol7
[System] Authentication Successful. Granted Admin privileges.

=== THE KNIGHTS CONSOLE ===
1. Check Active Entites (Users)
2. Check Server Uptime
3. Execute Emergency Shutdown
4. Disconnect
```

6. Format `history.log`

```text
[2026-04-26 19:06:40] [System] [SERVER ONLINE]
[2026-04-26 19:06:46] [System] [User 'alice' connected]
[2026-04-26 19:06:56] [User] [[alice]: hello lain]
[2026-04-26 19:07:29] [Admin] [RPC_GET_USERS]
[2026-04-26 19:07:31] [System] [EMERGENCY SHUTDOWN INITIATED]
```

---

## Soal 2 - The Battle of Eterion

### Deskripsi Soal
Pada soal ini dibuat simulasi game pertarungan bernama **The Battle of Eterion**. Program terdiri dari `orion.c` sebagai server internal dan `eternal.c` sebagai client. Berbeda dengan soal pertama yang menggunakan socket TCP, hubungan antara Orion dan Eternal pada soal ini berjalan secara lokal menggunakan **IPC**.

Tiga mekanisme IPC utama digunakan dalam implementasi ini. **Shared Memory** digunakan untuk menyimpan data akun, status matchmaking, state battle, HP, dan combat log agar dapat diakses bersama oleh server dan client. **Message Queue** digunakan untuk mengirim request dari client ke server dan response dari server ke client. **Semaphore** digunakan sebagai mutex agar tidak terjadi race condition ketika beberapa proses membaca atau mengubah shared memory.

Fitur utama yang dibuat meliputi register, login, armory, matchmaking selama 35 detik, pertarungan PvP atau melawan bot, serangan basic, ultimate attack, update XP/gold, level berdasarkan XP, serta penyimpanan riwayat pertarungan.

### Source Code (`arena.h`)

```c
#define MEM_ID 0x2233
#define QUEUE_ID 0x4455
#define MUTEX_ID 0x6677
#define MAX_FIGHTERS 20
```

`arena.h` menyimpan konfigurasi IPC yang dipakai bersama oleh `orion.c` dan `eternal.c`. `MEM_ID` adalah key untuk shared memory, `QUEUE_ID` adalah key untuk message queue, dan `MUTEX_ID` adalah key untuk semaphore. `MAX_FIGHTERS` membatasi jumlah akun fighter yang dapat disimpan.

```c
typedef struct {
    char enemy_name[50];
    char status_win[10];
    int gained_xp;
    int gained_gold;
} CombatRecord;
```

`CombatRecord` menyimpan satu catatan hasil pertarungan. Data yang dicatat meliputi nama lawan, status menang atau kalah, XP yang diperoleh, dan gold yang diperoleh. Struktur ini digunakan untuk fitur match history.

```c
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
```

`Fighter` adalah struktur utama untuk akun pemain. Setiap akun memiliki username, password, gold (`money`), XP (`experience`), bonus damage senjata (`weapon_power`), status login aktif, dan maksimal 10 riwayat pertarungan.

Nilai awal fighter baru pada implementasi ini mengikuti ketentuan soal: gold 150, XP 0, level 1 yang dihitung dari XP, dan belum memiliki senjata.

```c
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
```

`EterionData` adalah data besar yang diletakkan di shared memory. Di dalamnya terdapat daftar akun, jumlah akun, index pemain yang sedang menunggu matchmaking, status battle, index kedua fighter, HP masing-masing pihak, serta lima log combat terbaru.

Karena seluruh proses client dan server mengakses struktur yang sama, setiap perubahan state penting perlu dijaga dengan semaphore.

```c
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
```

`RequestPacket` dipakai client untuk mengirim permintaan ke server, misalnya register, login, membeli senjata, matchmaking, attack, ultimate, atau logout. `sender_pid` digunakan sebagai alamat balasan agar server dapat mengirim response ke message type sesuai PID client.

`ResponsePacket` berisi status berhasil/gagal dan teks informasi yang ditampilkan kepada user.

### Source Code (`orion.c`)

```c
EterionData *sys_data;
int mq_id, sem_lock_id;

void acquire_mutex() {
    struct sembuf s = {0,-1,0};
    semop(sem_lock_id, &s, 1);
}

void release_mutex() {
    struct sembuf s = {0,1,0};
    semop(sem_lock_id, &s, 1);
}
```

`orion.c` menyimpan pointer ke shared memory dan ID IPC global. Fungsi `acquire_mutex()` melakukan operasi `P` pada semaphore, sedangkan `release_mutex()` melakukan operasi `V`. Keduanya membuat akses ke shared memory berjalan aman sehingga dua proses tidak mengubah data yang sama pada waktu bersamaan.

```c
void push_log_entry(char *text) {
    if(sys_data->log_index < 5) {
        strcpy(sys_data->combat_logs[sys_data->log_index++], text);
    } else {
        for(int i=0; i<4; i++) strcpy(sys_data->combat_logs[i], sys_data->combat_logs[i+1]);
        strcpy(sys_data->combat_logs[4], text);
    }
}
```

Fungsi `push_log_entry()` menjaga agar combat log hanya berisi lima log terbaru. Jika slot log belum penuh, log langsung ditambahkan. Jika sudah penuh, seluruh log digeser ke atas dan log terbaru ditempatkan di posisi terakhir.

```c
void end_match(int victorious_side) {
    sys_data->is_fighting = 0;
    int f1 = sys_data->fighter1_idx;
    int f2 = sys_data->fighter2_idx;

    if(victorious_side == 1) {
        sys_data->fighters[f1].experience += 50;
        sys_data->fighters[f1].money += 120;
        ...
    } else {
        sys_data->fighters[f1].experience += 15;
        sys_data->fighters[f1].money += 30;
        ...
    }
```

`end_match()` dipanggil ketika salah satu pihak kehabisan HP. Fungsi ini mematikan status battle dengan `is_fighting = 0`, lalu memberi reward sesuai hasil pertandingan. Pemenang mendapatkan 50 XP dan 120 gold, sedangkan pihak kalah mendapatkan 15 XP dan 30 gold.

Selain reward, fungsi ini juga menambahkan data ke `records` agar pemain dapat melihat riwayat pertarungan dari menu history.

```c
int mem_seg = shmget(MEM_ID, sizeof(EterionData), IPC_CREAT|0666);
sys_data = shmat(mem_seg, NULL, 0);
mq_id = msgget(QUEUE_ID, IPC_CREAT|0666);
sem_lock_id = semget(MUTEX_ID, 1, IPC_CREAT|0666);
semctl(sem_lock_id, 0, SETVAL, 1);

sys_data->total_fighters = 0;
sys_data->is_fighting = 0;
sys_data->waiting_player_idx = -1;
```

Pada awal program, Orion membuat shared memory dengan `shmget()`, menempelkannya ke proses menggunakan `shmat()`, membuat message queue dengan `msgget()`, dan membuat semaphore dengan `semget()`. Nilai semaphore diinisialisasi menjadi 1 agar dapat berfungsi sebagai mutex biner.

Setelah IPC siap, data awal arena di-reset: belum ada fighter, belum ada battle aktif, dan belum ada pemain yang menunggu matchmaking.

```c
if(msgrcv(mq_id, &req, sizeof(req)-sizeof(long), 1, IPC_NOWAIT) >= 0) {
    res.msg_type = req.sender_pid;
    res.success_flag = 0;
    acquire_mutex();

    int active_idx = -1;
    for(int i=0; i<sys_data->total_fighters; i++)
        if(strcmp(sys_data->fighters[i].account_name, req.username) == 0) active_idx = i;
```

Loop utama Orion membaca request dari message queue dengan `msgrcv()`. Semua request dari client masuk melalui `msg_type = 1`. Server kemudian mengatur `res.msg_type` menjadi PID pengirim agar response hanya diterima oleh client yang meminta.

Setelah request diterima, server mengunci semaphore dan mencari index akun berdasarkan username.

```c
if(req.action_id == 1) {
    if(active_idx == -1) {
        strcpy(sys_data->fighters[sys_data->total_fighters].account_name, req.username);
        strcpy(sys_data->fighters[sys_data->total_fighters].account_pass, req.password);
        sys_data->fighters[sys_data->total_fighters].money = 150;
        sys_data->fighters[sys_data->total_fighters].experience = 0;
        sys_data->fighters[sys_data->total_fighters].total_records = 0;
        sys_data->fighters[sys_data->total_fighters].weapon_power = 0;
        sys_data->total_fighters++;
        res.success_flag = 1;
        strcpy(res.info_text, "Registration successful!");
    } else strcpy(res.info_text, "Fighter already exists!");
}
```

Action `1` adalah register. Server hanya membuat akun baru jika username belum ditemukan. Akun baru diberi gold 150, XP 0, belum memiliki weapon, dan jumlah history 0. Jika username sudah ada, server mengembalikan pesan bahwa fighter sudah terdaftar.

```c
else if(req.action_id == 2) {
    if(active_idx != -1 && strcmp(sys_data->fighters[active_idx].account_pass, req.password) == 0) {
        sys_data->fighters[active_idx].is_active = 1;
        res.success_flag = 1;
        strcpy(res.info_text, "Welcome to Eterion!");
    } else strcpy(res.info_text, "Authentication failed!");
}
```

Action `2` adalah login. Server mengecek apakah username ada dan password sesuai. Jika berhasil, status `is_active` diubah menjadi 1 dan client diperbolehkan masuk ke menu utama Eterion.

```c
else if(req.action_id == 3 && active_idx != -1) {
    int price = 0, power = 0;
    if(req.payload==1){price=100; power=5;}
    else if(req.payload==2){price=300; power=15;}
    else if(req.payload==3){price=600; power=30;}
    else if(req.payload==4){price=1500; power=60;}
    else if(req.payload==5){price=5000; power=150;}

    if(sys_data->fighters[active_idx].money >= price) {
        sys_data->fighters[active_idx].money -= price;
        if(power > sys_data->fighters[active_idx].weapon_power)
            sys_data->fighters[active_idx].weapon_power = power;
        res.success_flag = 1;
        strcpy(res.info_text, "Weapon equipped!");
    } else strcpy(res.info_text, "Not enough Gold!");
}
```

Action `3` digunakan untuk membeli weapon dari armory. Client mengirim ID item melalui `payload`, lalu server menentukan harga dan bonus damage-nya. Jika gold mencukupi, gold dikurangi dan weapon dengan damage terbesar otomatis dipakai. Jika weapon yang dibeli lebih lemah dari weapon lama, bonus damage lama tetap dipertahankan.

```c
else if(req.action_id == 4 && active_idx != -1) {
    if(sys_data->waiting_player_idx == -1) {
        sys_data->waiting_player_idx = active_idx;
        res.success_flag = 0;
    } else {
        sys_data->fighter1_idx = sys_data->waiting_player_idx;
        sys_data->fighter2_idx = active_idx;
        sys_data->f1_max_hp = sys_data->f1_hp = 100 + (sys_data->fighters[sys_data->fighter1_idx].experience/10);
        sys_data->f2_max_hp = sys_data->f2_hp = 100 + (sys_data->fighters[active_idx].experience/10);
        sys_data->is_fighting = 1;
        sys_data->waiting_player_idx = -1;
        push_log_entry("PvP Match Commenced!");
        res.success_flag = 1;
    }
}
```

Action `4` adalah matchmaking. Jika belum ada pemain yang menunggu, pemain saat ini disimpan sebagai `waiting_player_idx`. Jika sudah ada pemain yang menunggu, server memasangkan kedua fighter dan memulai PvP battle. HP dihitung dari formula `100 + XP/10`.

```c
else if(req.action_id == 5 && active_idx != -1) {
    sys_data->waiting_player_idx = -1;
    sys_data->fighter1_idx = active_idx;
    sys_data->fighter2_idx = -2;
    sys_data->f1_max_hp = sys_data->f1_hp = 100 + (sys_data->fighters[active_idx].experience/10);
    sys_data->f2_max_hp = sys_data->f2_hp = 100;
    sys_data->is_fighting = 1;
    push_log_entry("A Wild Beast appears!");
    res.success_flag = 1;
}
```

Action `5` dipakai ketika matchmaking selama 35 detik tidak menemukan lawan. Server membuat battle melawan bot dengan penanda `fighter2_idx = -2`. Bot diberi nama `Wild Beast` dan HP dasar 100.

```c
else if((req.action_id == 6 || req.action_id == 7) && active_idx != -1 && sys_data->is_fighting) {
    int base_dmg = 10 + (sys_data->fighters[active_idx].experience/50);
    if(req.action_id == 7)
        base_dmg = (base_dmg + sys_data->fighters[active_idx].weapon_power) * 3;
    else
        base_dmg += sys_data->fighters[active_idx].weapon_power;

    int is_f1 = (sys_data->fighter1_idx == active_idx);
    if(is_f1) sys_data->f2_hp -= base_dmg;
    else sys_data->f1_hp -= base_dmg;

    char l[100];
    sprintf(l, "> %s hits for %d damage!", req.username, base_dmg);
    push_log_entry(l);
}
```

Action `6` adalah basic attack, sedangkan action `7` adalah ultimate attack. Damage dasar mengikuti formula `10 + XP/50 + weapon_power`. Untuk ultimate, total damage dikalikan 3. Setelah damage dihitung, server mengurangi HP lawan dan menambahkan log serangan ke combat feed.

```c
if(sys_data->is_fighting && sys_data->fighter2_idx == -2 && (clock_now - last_ai_hit >= 2)) {
    acquire_mutex();
    if(sys_data->is_fighting && sys_data->f1_hp > 0) {
        int ai_hit = 10 + (rand() % 5);
        sys_data->f1_hp -= ai_hit;
        char l[100];
        sprintf(l, "> Wild Beast strikes back for %d damage!", ai_hit);
        push_log_entry(l);
        if(sys_data->f1_hp <= 0) end_match(2);
    }
    release_mutex();
    last_ai_hit = clock_now;
}
```

Jika battle berlangsung melawan bot, Orion menjalankan serangan otomatis dari `Wild Beast` setiap 2 detik. Damage bot dibuat acak pada rentang sederhana `10 + rand() % 5`. Jika HP pemain habis, pertandingan selesai dan bot dianggap menang.

### Source Code (`eternal.c`)

```c
void configure_terminal(int active) {
    struct termios cfg;
    tcgetattr(0, &cfg);
    if(active) cfg.c_lflag &= ~(ICANON|ECHO);
    else cfg.c_lflag |= (ICANON|ECHO);
    tcsetattr(0, TCSANOW, &cfg);
}
```

Fungsi ini mengubah mode terminal saat battle berlangsung. Ketika `active` bernilai 1, mode canonical dan echo dimatikan sehingga tombol `a` atau `u` dapat dibaca langsung tanpa perlu menekan Enter. Setelah battle selesai, konfigurasi terminal dikembalikan seperti semula.

```c
void* display_thread(void* arg) {
    while(1) {
        acquire_mutex();
        if(!sys_data->is_fighting) {
            release_mutex();
            break;
        }

        system("clear");
        printf("--- THE BATTLE ARENA ---\n");
        printf("%s (Lvl %d)\nHP: [%d / %d]\n\n   V S\n\n", e_name, ..., e_hp, e_max);
        printf("%s (Lvl %d) [%s]\nHP: [%d / %d]\n\n", player_name, ..., ..., p_hp, p_max);

        printf("Live Combat Feed:\n");
        for(int i=0; i<5; i++) printf("%s\n", sys_data->combat_logs[i]);

        release_mutex();
        usleep(200000);
    }
    return NULL;
}
```

`display_thread()` bertugas menampilkan arena secara realtime. Thread ini membaca shared memory untuk mengetahui HP pemain, HP lawan, level, status weapon, dan lima combat log terbaru. Tampilan diperbarui setiap 0,2 detik sehingga perubahan battle terlihat langsung oleh user.

Semaphore tetap digunakan saat membaca data agar tampilan tidak mengambil data yang sedang diubah oleh Orion.

```c
printf("--- ETERION MAIN MENU ---\n");
printf("Fighter: %-10s Level: %d\nWallet : %-10d XP   : %d\n\n", player_name, lvl, mny, exp);
printf("1. Enter Matchmaking\n2. Visit Armory\n3. View Match History\n4. Disconnect\nSelect: ");
```

Setelah login berhasil, Eternal menampilkan main menu. User dapat masuk matchmaking, membuka armory, melihat history pertandingan, atau disconnect dari server.

```c
if(choice == 1) {
    req.action_id = 4;
    msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
    msgrcv(mq_id, &res, sizeof(res)-sizeof(long), getpid(), 0);

    int is_matched = 0;
    for(int sec=35; sec>0; sec--) {
        system("clear");
        printf("Looking for challengers... [%d s]\n", sec);
        acquire_mutex();
        is_matched = sys_data->is_fighting;
        release_mutex();
        if(is_matched) break;
        sleep(1);
    }
```

Saat memilih matchmaking, Eternal mengirim action `4` ke Orion. Setelah itu client menunggu maksimal 35 detik sambil memantau status `is_fighting` di shared memory. Jika ada lawan ditemukan, battle langsung dimulai.

```c
if(!is_matched) {
    req.action_id = 5;
    msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
    msgrcv(mq_id, &res, sizeof(res)-sizeof(long), getpid(), 0);
}
```

Jika 35 detik berlalu tanpa lawan, Eternal mengirim action `5` agar Orion membuat pertandingan melawan bot `Wild Beast`.

```c
pthread_t ui_thread;
pthread_create(&ui_thread, NULL, display_thread, NULL);
configure_terminal(1);
time_t prev_hit = 0;

while(1) {
    acquire_mutex();
    int fighting_now = sys_data->is_fighting;
    release_mutex();
    if(!fighting_now) break;

    char key = getchar();
    time_t time_now = time(NULL);
    if(time_now - prev_hit >= 1) {
        if(key == 'a') req.action_id = 6;
        else if(key == 'u') req.action_id = 7;
        msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
        prev_hit = time_now;
    }
}
```

Saat battle dimulai, Eternal membuat thread UI dan mengubah terminal ke mode realtime. User dapat menekan `a` untuk basic attack atau `u` untuk ultimate. Cooldown 1 detik diterapkan menggunakan selisih `time_now - prev_hit`, sehingga serangan tidak dapat dikirim terus-menerus tanpa jeda.

```c
else if(choice == 2) {
    system("clear");
    printf("--- THE ARMORY ---\nGold: %d\n", mny);
    printf("1. Wood Sword (100 G)\n2. Iron Sword (300 G)\n3. Steel Axe (600 G)\n4. Demon Blade (1500 G)\n5. God Slayer (5000 G)\n0. Go Back\nItem ID: ", mny);
    scanf("%d", &req.payload);
    if(req.payload > 0) {
        req.action_id = 3;
        msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
        msgrcv(mq_id, &res, sizeof(res)-sizeof(long), getpid(), 0);
        printf("%s\n", res.info_text);
        sleep(1);
    }
}
```

Menu armory menampilkan daftar weapon dan harganya. ID weapon dikirim melalui `payload`, sedangkan action `3` memberi tahu Orion bahwa request tersebut adalah pembelian weapon.

```c
else if(choice == 3) {
    system("clear");
    printf("--- BATTLE LOGS ---\nTime\t| Foe\t\t| Status\t| Earned XP\n");
    acquire_mutex();
    for(int i=0; i<sys_data->fighters[loc].total_records; i++) {
        CombatRecord rec = sys_data->fighters[loc].records[i];
        printf("--:--\t| %-10s\t| %s\t| +%d\n", rec.enemy_name, rec.status_win, rec.gained_xp);
    }
    release_mutex();
}
```

Menu history membaca array `records` dari shared memory dan menampilkan riwayat pertandingan akun tersebut. Setiap record menampilkan lawan, status menang/kalah, dan XP yang diperoleh.

```c
int mem_seg = shmget(MEM_ID, sizeof(EterionData), 0666);
sys_data = shmat(mem_seg, NULL, 0);
mq_id = msgget(QUEUE_ID, 0666);
sem_lock_id = semget(MUTEX_ID, 1, 0666);
```

Pada awal program, Eternal tidak membuat IPC baru, tetapi hanya membuka IPC yang sudah dibuat oleh Orion. Artinya Orion harus dijalankan terlebih dahulu. Jika Orion belum aktif, Eternal tidak memiliki shared memory, message queue, dan semaphore untuk diakses.

```c
printf("1. Create Account\n2. Sign In\n3. Quit\nInput: ");
scanf("%d", &act);

printf("Username: ");
scanf("%s", req.username);
printf("Password: ");
scanf("%s", req.password);
req.action_id = act;
msgsnd(mq_id, &req, sizeof(req)-sizeof(long), 0);
msgrcv(mq_id, &res, sizeof(res)-sizeof(long), getpid(), 0);
```

Menu awal Eternal berisi register, login, dan quit. Untuk register dan login, client mengirim username, password, dan action ID ke Orion lewat message queue. Setelah response diterima, teks status ditampilkan ke user. Jika login berhasil, program masuk ke `main_game_loop()`.

### Source Code (`Makefile`)

```makefile
CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -lrt

all: server client

server: orion.c arena.h
	$(CC) $(CFLAGS) orion.c -o orion $(LDFLAGS)

client: eternal.c arena.h
	$(CC) $(CFLAGS) eternal.c -o eternal $(LDFLAGS)
```

Makefile membantu proses kompilasi agar praktikan cukup menjalankan `make`. Target `server` akan menghasilkan binary `orion`, sedangkan target `client` menghasilkan binary `eternal`. Flag `-pthread` digunakan karena client memakai thread, sedangkan `-lrt` disiapkan untuk kebutuhan linking IPC/time pada sistem Linux tertentu.

```makefile
clean:
	rm -f orion eternal

clear_ipc:
	ipcs -m | grep 0x00001234 | awk '{print $$2}' | xargs -r ipcrm -m
	ipcs -q | grep 0x00005678 | awk '{print $$2}' | xargs -r ipcrm -q
	ipcs -s | grep 0x00009012 | awk '{print $$2}' | xargs -r ipcrm -s
```

Target `clean` dipakai untuk menghapus binary hasil kompilasi. Target `clear_ipc` disediakan untuk membersihkan IPC yang masih tertinggal setelah debugging, meskipun key pada target ini dapat disesuaikan kembali dengan key aktual di `arena.h`.

### Output

1. Kompilasi program

```bash
cd soal2
make
```

Output kompilasi:

```text
gcc -Wall -pthread orion.c -o orion -lrt
gcc -Wall -pthread eternal.c -o eternal -lrt
```

2. Orion berhasil dijalankan sebagai server IPC

```text
>> Orion is Online [PID: xxxx]
```

3. Menu awal Eternal

```text
1. Create Account
2. Sign In
3. Quit
Input:
```

4. Register akun baru

```text
Username: rootkids
Password: 123
Registration successful!
```

5. Login ke dunia Eterion

```text
Username: rootkids
Password: 123
Welcome to Eterion!
```

6. Main menu setelah login

```text
--- ETERION MAIN MENU ---
Fighter: rootkids   Level: 1
Wallet : 150        XP   : 0

1. Enter Matchmaking
2. Visit Armory
3. View Match History
4. Disconnect
Select:
```

7. Matchmaking

```text
Looking for challengers... [35 s]
```

Jika tidak ada lawan dalam 35 detik, client meminta Orion membuat battle melawan bot.

8. Tampilan battle arena

```text
--- THE BATTLE ARENA ---
Wild Beast (Lvl 1)
HP: [100 / 100]

   V S

rootkids (Lvl 1) [No Weapon]
HP: [100 / 100]

Live Combat Feed:
A Wild Beast appears!
> rootkids hits for 10 damage!
> Wild Beast strikes back for 12 damage!

Cooldowns -> Attack(1s) | Ultimate(1s)
Controls: [A] Basic Hit   [U] Ultimate Strike
```

9. Armory

```text
--- THE ARMORY ---
Gold: 150
1. Wood Sword (100 G)
2. Iron Sword (300 G)
3. Steel Axe (600 G)
4. Demon Blade (1500 G)
5. God Slayer (5000 G)
0. Go Back
Item ID:
```

10. Battle history

```text
--- BATTLE LOGS ---
Time    | Foe        | Status | Earned XP
--:--   | Wild Beast | WIN    | +50
```

---

## Kendala

Pada saat kompilasi, program soal 1 berhasil dibuat, namun `wired.c` menampilkan warning dari compiler pada bagian `sprintf()` broadcast karena payload chat berukuran sampai 1024 byte dan masih ditambah format nama user. Program tetap berhasil dikompilasi, tetapi secara keamanan buffer sebaiknya dapat ditingkatkan dengan `snprintf()` agar lebih aman.

Pada soal 2, target `clear_ipc` di Makefile masih memakai key contoh dari soal, sedangkan implementasi di `arena.h` memakai key `0x2233`, `0x4455`, dan `0x6677`. Karena itu, jika ingin membersihkan IPC hasil program ini, key pada Makefile perlu disesuaikan dengan konfigurasi aktual.
