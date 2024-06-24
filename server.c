#include <stdio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8888
#define BUFFER_SIZE 1024

int main() {
    WSADATA wsa;
    SOCKET sockfd;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

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

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 소켓에 주소 바인딩
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed with error code : %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    // 서버가 켜졌음을 알림
    printf("Server started on port %d\n", PORT);

    // 클라이언트의 메시지 수신 및 응답
    while (1) {
        int recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (recv_len == SOCKET_ERROR) {
            printf("recvfrom() failed with error code : %d\n", WSAGetLastError());
            break;
        }

        buffer[recv_len] = '\0';
        printf("Received packet from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("Data: %s\n", buffer);

        if(strcmp(buffer, "jtr: udp broadcast check") == 0) {
            const char *response = "jtr: check response from server";
            if (sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&client_addr, client_addr_len) == SOCKET_ERROR) {
                printf("sendto() failed with error code : %d\n", WSAGetLastError());
                break;
            }
        }
    }

    closesocket(sockfd);
    WSACleanup();
    return 0;
}
