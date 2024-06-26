#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 6974
#define PING_PORT 6974
#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024

typedef enum {
    WAITING, // 방이 생성 된 상태
    START // 게임이 시작한 상태
} GameState;

typedef struct {
    SOCKET socket;
    int id;
    int cleared_blocks;
    int combo_count;
    time_t last_clear_time;
    char name[100];
} Client;

Client clients[MAX_CLIENTS];
GameState game_state = WAITING;
int client_count = 0;
char room_name[1024];
HANDLE client_threads[MAX_CLIENTS];
SOCKET udp_socket;

DWORD WINAPI client_handler(void *arg);
DWORD WINAPI udp_ping_handler(void *arg);

void init_winsock() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("[info] Initialized Winsock\n");
}

void handle_dead_socket(int dead_socket_id) {
    // 클라이언트 배열에서 제거하고 앞으로 이동
    for (int i = dead_socket_id; i < client_count - 1; i++) {
        clients[i] = clients[i + 1];
        clients[i].id = i; // id 업데이트
    }

    // 마지막 클라이언트를 초기화
    clients[client_count - 1].socket = INVALID_SOCKET;
    clients[client_count - 1].id = -1;
    clients[client_count - 1].cleared_blocks = 0;
    clients[client_count - 1].combo_count = 0;
    clients[client_count - 1].last_clear_time = 0;

    client_count--;

    printf("[info] Client %d removed. Current client count: %d\n", dead_socket_id, client_count);

    // 게임 상태 업데이트
    if (client_count == 1) {
        game_state = WAITING;
        printf("[info] Game state changed to WAITING\n");
    }
}

void create_udp_socket() {
    struct sockaddr_in server_addr;

    // UDP 소켓 생성
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        printf("Could not create UDP socket : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // 소켓 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PING_PORT);

    // 소켓에 주소 바인딩
    if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed with error code : %d\n", WSAGetLastError());
        closesocket(udp_socket);
        exit(EXIT_FAILURE);
    }

    printf("[info] Ping Server started on port %d\n", PING_PORT);

    // UDP 핸들러 스레드 생성
    CreateThread(NULL, 0, udp_ping_handler, NULL, 0, NULL);
}

void handle_new_client() {

}

DWORD WINAPI client_handler(void *arg) {
    Client *client = (Client *)arg;
    char buffer[1024];
    int read_size;

    printf("[info] Client %d connected.\n", client->id);

    while ((read_size = recv(client->socket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[read_size] = '\0';
        printf("[data] Received from client %d: %s\n", client->id, buffer);

        if(game_state == WAITING) {
            const char *waiting_message = "jtr: game is not started yet. Please wait for other player";
            send(client->socket, waiting_message, strlen(waiting_message), 0);
        } else {
            // 클라이언트가 라인 클리어를 보냈을 때
            if (strncmp(buffer, "jtr: line clear: ", strlen("jtr: line clear: ")) == 0) {
                // 다른 클라이언트에게도 같은 정보 전송
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].id != client->id && clients[i].socket != INVALID_SOCKET) {
                        send(clients[i].socket, buffer, strlen(buffer), 0);
                    }
                }
            }

            // 클라이언트가 블록 이동을 보냈을 때
            if (strncmp(buffer, "jtr: block move: ", strlen("jtr: block move: ")) == 0) {
                // 다른 클라이언트에게도 같은 정보 전송
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].id != client->id && clients[i].socket != INVALID_SOCKET) {
                        send(clients[i].socket, buffer, strlen(buffer), 0);
                    }
                }
            }

            // 클라이언트가 블록 회전을 보냈을 때
            if (strncmp(buffer, "jtr: block rotate: ", strlen("jtr: block roatate: ")) == 0) {
                // 다른 클라이언트에게도 같은 정보 전송
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].id != client->id && clients[i].socket != INVALID_SOCKET) {
                        send(clients[i].socket, buffer, strlen(buffer), 0);
                    }
                }
            }

            // 클라이언트가 일시 정지를 보냈을 때
            if (strncmp(buffer, "jtr: pause", strlen("jtr: pause")) == 0) {
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].id != client->id && clients[i].socket != INVALID_SOCKET) {
                        send(clients[i].socket, buffer, strlen(buffer), 0);
                    }
                }
            }
        }

        // 클라이언트가 자신의 이름을 보냈을때
        if (strncmp(buffer, "jtr: set name: ", strlen("jtr: set name: ")) == 0) {
            strcpy(client->name, buffer + strlen("jtr: set name: "));
        }
    }

    if (read_size == 0) {
        printf("[info] Client %d disconnected.\n", client->id);
    } else if (read_size == SOCKET_ERROR) {
        printf("[error] recv failed with error code : %d\n", WSAGetLastError());
    }

    closesocket(client->socket);
    handle_dead_socket(client->id); // 클라이언트 제거 함수 호출

    return 0;
}

DWORD WINAPI udp_ping_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while (1) {
        const int recv_len = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (recv_len == SOCKET_ERROR) {
            printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
            return 1;
        }

        buffer[recv_len] = '\0';

        printf("[Ping Server] Received: %s\n", buffer); // 디버그용 출력

        if (strcmp(buffer, "jtr: udp broadcast check") == 0) {
            printf("[Ping Server] Received packet from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            char response[1024];
            sprintf(response, "jtr: check response from server|%s,%d", room_name, client_count);
            sendto(udp_socket, response, strlen(response), 0, (struct sockaddr *)&client_addr, client_addr_len);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--name") != 0) {
        printf("Usage: %s --name <room_name>\n", argv[0]);
        return 0;
    }

    strcpy(room_name, argv[2]);

    system("cls");

    SOCKET server_socket, new_socket;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);

    init_winsock();

    // 클라이언트 배열 초기화
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = INVALID_SOCKET;
        clients[i].id = -1;
        clients[i].cleared_blocks = 0;
        clients[i].combo_count = 0;
        clients[i].last_clear_time = 0;
    }

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    printf("[info] Socket created\n");

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind failed with error code : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    printf("[info] Socket bind done\n");

    // Listen
    listen(server_socket, 3);

    // UDP 소켓 생성
    create_udp_socket();

    printf("[info] Waiting for incoming connections...\n");

    // Accept and incoming connection
    while ((new_socket = accept(server_socket, (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        if (client_count < MAX_CLIENTS) {
            clients[client_count].socket = new_socket;
            clients[client_count].id = client_count;
            clients[client_count].cleared_blocks = 0;
            clients[client_count].combo_count = 0;
            clients[client_count].last_clear_time = time(NULL);
            client_threads[client_count] = CreateThread(NULL, 0, client_handler, (void*)&clients[client_count], 0, NULL);
            client_count++;

            printf("cli count : %d\n", client_count);

            const char *message = 'jtr: connection complete';
            send(clients[client_count - 1].socket, message, strlen(message), 0);

            if (client_count == 2) {
                game_state = START;
                printf("[info] Game state changed to START\n");

                for(int x=0; x<MAX_CLIENTS; x++) {
                    char start_message[1024];

                    sprintf(start_message, "jtr: game started|%s,%s", room_name, clients[!x].name);
                    send(clients[x].socket, start_message, strlen(start_message), 0);
                }
            }
        } else {
            printf("Maximum clients connected. Connection rejected.\n");
            const char *reject_message = "jtr: connection rejected";
            send(new_socket, reject_message, strlen(reject_message), 0);
        }
    }

    if (new_socket == INVALID_SOCKET) {
        printf("accept failed with error code : %d\n", WSAGetLastError());
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        WaitForSingleObject(client_threads[i], INFINITE);
    }

    closesocket(server_socket);
    closesocket(udp_socket);
    WSACleanup();

    return 0;
}