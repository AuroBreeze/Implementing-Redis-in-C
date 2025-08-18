#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(6379);

    // 改这里试试：正确是 sizeof(addr) == 16
    int len = sizeof(addr); // 改成 8, 16, 64 看效果

    if (bind(fd, (sockaddr*)&addr, len) == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(fd);
        WSACleanup();
        return 1;
    }

    std::cout << "bind success with len = " << len << std::endl;

    listen(fd, 1);

    sockaddr_in client{};
    int client_len = sizeof(client) + 20; // 故意大于16
    SOCKET cfd = accept(fd, (sockaddr*)&client, &client_len);
    if (cfd == INVALID_SOCKET) {
        std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
        closesocket(fd);
        WSACleanup();
        return 1;
    }

    std::cout << "accept success, client_len = " << client_len << std::endl;

    closesocket(cfd);
    closesocket(fd);
    WSACleanup();
    return 0;
}
