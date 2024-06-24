// 근처 네트워크의 좌트리오 서버를 찾는 클라이언트 코드

#include <stdio.h>
#include <winsock2.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8888
#define BROADCAST_IP "255.255.255.255"
#define BUFFER_SIZE 1024
#define TIMEOUT 2 // 타임아웃 설정 (초)
#define MAX_SERVERS 100 // 최대 서버 수
#define INET_ADDRSTRLEN 16 // IPv4 주소 문자열 길이

// 서버 IP 주소를 저장할 배열
char server_ips[MAX_SERVERS][INET_ADDRSTRLEN];
int server_count = 0;

// 콘솔을 지우는 함수 (윈도우용)
void clear_console() {
    system("cls");
}

// 서버 IP 주소를 배열에 추가하는 함수
void add_server_ip(const char* ip) {
    for (int i = 0; i < server_count; i++) {
        if (strcmp(server_ips[i], ip) == 0) {
            return; // 이미 배열에 있는 IP는 추가하지 않음
        }
    }

    if (server_count < MAX_SERVERS) {
        strncpy(server_ips[server_count], ip, INET_ADDRSTRLEN);
        server_count++;
    }
}

// 서버 IP 주소를 출력하는 함수
void print_server_ips() {
    clear_console();
    printf("Valid server responses:\n");
    for (int i = 0; i < server_count; i++) {
        printf("%s\n", server_ips[i]);
    }
}

int main() {
    WSADATA wsa;
    SOCKET sockfd;
    struct sockaddr_in broadcast_addr, from_addr;
    int from_addr_len = sizeof(from_addr);
    char buffer[BUFFER_SIZE];
    fd_set readfds;
    struct timeval tv;

    // 윈속 초기화
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code : %d\n", WSAGetLastError());
        return 1;
    }

    // 소켓 생성
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket : %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // 브로드캐스트 사용 설정
    BOOL broadcast_enable = TRUE;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast_enable, sizeof(broadcast_enable)) == SOCKET_ERROR) {
        printf("setsockopt(SO_BROADCAST) failed with error code : %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    // 브로드캐스트 주소 설정
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    broadcast_addr.sin_port = htons(PORT);

    while (1) {
        // 메시지 전송
        const char *message = "jtr: udp broadcast check";
        if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) == SOCKET_ERROR) {
            printf("sendto() failed with error code : %d\n", WSAGetLastError());
            closesocket(sockfd);
            WSACleanup();
            return 1;
        }

        // 타임아웃 설정
        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;

        // 서버 응답 수신
        while (1) {
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);

            int activity = select(0, &readfds, NULL, NULL, &tv);

            if (activity == SOCKET_ERROR) {
                printf("select() failed with error code : %d\n", WSAGetLastError());
                break;
            } else if (activity == 0) {
                break;
            }

            if (FD_ISSET(sockfd, &readfds)) {
                int recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&from_addr, &from_addr_len);
                if (recv_len == SOCKET_ERROR) {
                    printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
                    break;
                }

                buffer[recv_len] = '\0';

                // 올바른 응답 확인 (여기서는 "Server Response"로 가정)
                if (strcmp(buffer, "jtr: check response from server") == 0) {
                    const char* server_ip = inet_ntoa(from_addr.sin_addr);
                    add_server_ip(server_ip);
                    print_server_ips();
                }
            }
        }
    }

    closesocket(sockfd);
    WSACleanup();
    return 0;
}
