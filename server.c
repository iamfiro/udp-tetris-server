#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 12345
#define PING_PORT 6974
#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024

typedef enum {
    PENDING, // 방 이름이 지정되기 전 상태
    WAITING, // 방 이름이 지정이 되고 1명의 클라이언트가 접속한 상태
    START // 게임이 시작한 상태
} GameState;

typedef struct {
    SOCKET socket;
    int id;
    int cleared_blocks;
    int combo_count;
    time_t last_clear_time;
    int score;
} Client;

Client clients[MAX_CLIENTS];
GameState game_state = PENDING;
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
}

void init_room_name(char *name) {
    strncpy(room_name, name, sizeof(room_name) - 1); // 전체 문자열 복사
    room_name[sizeof(room_name) - 1] = '\0'; // null-terminator 추가
    game_state = WAITING;
    printf("[info] Room name set to %s\n", room_name);
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
    clients[client_count - 1].score = 0;

    client_count--;

    printf("Client %d removed. Current client count: %d\n", dead_socket_id, client_count);

    // 게임 상태 업데이트
    if (client_count == 0) {
        // game_state = PENDING;
    } else if (client_count == 1) {
        game_state = WAITING;
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

    printf("Ping Server started on port %d\n", PING_PORT);

    // UDP 핸들러 스레드 생성
    CreateThread(NULL, 0, udp_ping_handler, NULL, 0, NULL);
}

int main() {
    SOCKET server_socket, new_socket;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);

    init_winsock();
    create_udp_socket();

    // 클라이언트 배열 초기화
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = INVALID_SOCKET;
        clients[i].id = -1;
        clients[i].cleared_blocks = 0;
        clients[i].combo_count = 0;
        clients[i].last_clear_time = 0;
        clients[i].score = 0;
    }

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind failed with error code : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Listen
    listen(server_socket, 3);

    printf("Waiting for incoming connections...\n");

    // Accept and incoming connection
    while ((new_socket = accept(server_socket, (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        if (client_count < MAX_CLIENTS) {
            clients[client_count].socket = new_socket;
            clients[client_count].id = client_count;
            clients[client_count].cleared_blocks = 0;
            clients[client_count].combo_count = 0;
            clients[client_count].last_clear_time = time(NULL);
            clients[client_count].score = 0;
            client_threads[client_count] = CreateThread(NULL, 0, client_handler, (void*)&clients[client_count], 0, NULL);
            client_count++;

            if (client_count == 2) {
                game_state = START;
            }

            const char *server_message = "Connection complete";
            send(new_socket, server_message, strlen(server_message), 0);
        } else {
            printf("Maximum clients connected. Connection rejected.\n");
            const char *reject_message = "Connection rejected";
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

DWORD WINAPI client_handler(void *arg) {
    Client *client = (Client *)arg;
    char buffer[1024];
    int read_size;

    printf("[info] Client %d connected.\n", client->id);

    while ((read_size = recv(client->socket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Received from client %d: %s\n", client->id, buffer);

        // 클라이언트가 세션 이름을 설정했을 때
        if (strncmp(buffer, "jtr: set name: ", strlen("jtr: set name: ")) == 0) {
            char *name_start = buffer + strlen("jtr: set name: ");
            init_room_name(name_start);
        }

        // 클라이언트가 라인 클리어를 보냈을 때
        if (strncmp(buffer, "jtr: line clear: ", strlen("jtr: line clear: ")) == 0) {
            int cleared_blocks = atoi(buffer + strlen("jtr: line clear: "));
            time_t current_time = time(NULL);

            if (cleared_blocks > 0) {
                double time_diff = difftime(current_time, client->last_clear_time);

                if (time_diff < 2) { // If the block is cleared within 2 seconds, count as a combo
                    client->combo_count++;
                } else {
                    client->combo_count = 1; // Reset combo if not cleared within 2 seconds
                }

                client->cleared_blocks += cleared_blocks;
                client->last_clear_time = current_time;

                // 점수 계산
                int base_score;
                if (cleared_blocks >= 4) {
                    base_score = 1000;
                } else {
                    switch (cleared_blocks) {
                        case 1:
                            base_score = 100;
                            break;
                        case 2:
                            base_score = 300;
                            break;
                        case 3:
                            base_score = 500;
                            break;
                        default:
                            base_score = 0;
                            break;
                    }
                }

                int combo_bonus = (client->combo_count - 1) * 50; // 콤보 점수는 콤보 수에 따라 50점씩 증가
                client->score += base_score + combo_bonus;

                // Calculate damage based on combo (simplified example)
                int damage = client->combo_count;

                if (damage > 5) {
                    damage = 5; // 대미지 최대값을 5로 제한
                }

                // Send damage packet to the other client
                int other_client_id = (client->id == 0) ? 1 : 0;
                char damage_packet[1024];
                sprintf(damage_packet, "jtr damage: %d\n", damage);
                send(clients[other_client_id].socket, damage_packet, strlen(damage_packet), 0);

                // 클라이언트에게 점수 업데이트 메시지 전송
                char score_update[1024];
                sprintf(score_update, "jtr score: %d\n", client->score);
                send(client->socket, score_update, strlen(score_update), 0);
            }
        }

        // 클라이언트가 블록 이동을 보냈을 때
        if (strncmp(buffer, "jtr: block move: ", strlen("jtr: block move: ")) == 0) {
            int other_client_id = (client->id == 0) ? 1 : 0;
            send(clients[other_client_id].socket, buffer, strlen(buffer), 0);
        }
    }

    if (read_size == 0) {
        printf("Client %d disconnected.\n", client->id);
    } else if (read_size == SOCKET_ERROR) {
        printf("recv failed with error code : %d\n", WSAGetLastError());
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
        int recv_len = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (recv_len == SOCKET_ERROR) {
            printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
            continue;
        }

        buffer[recv_len] = '\0';

        printf("Received: %s\n", buffer); // 디버그용 출력

        if (strcmp(buffer, "jtr: udp broadcast check") == 0) {
            // if (game_state != PENDING) {
                printf("Received PING from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                char response[1024];
                sprintf(response, "jtr: check response from server|asd,0", room_name, client_count);
                sendto(udp_socket, response, strlen(response), 0, (struct sockaddr *)&client_addr, client_addr_len);
            // }
        }
    }

    return 0;
}
