#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cassert>
#pragma comment(lib, "ws2_32.lib")

static void die(const char *msg) {
    int err = WSAGetLastError();
    fprintf(stderr, "[%d] %s\n", err, msg);
    WSACleanup();
    exit(1);
}

static int32_t read_full(int fd, char* buf, size_t n){
    while(n > 0){
        ssize_t rv = recv(fd,buf,n,0);
        if(rv < 0){
            std::cerr << "read error" << std::endl;
            return -1;
        }
        assert((size_t)rv <=n);
        n -= (size_t)rv;
        buf += (size_t)rv;
    }
    return 0;
}

static int32_t write_full(int fd, char* buf, size_t n){ 
    while(n>0){
        ssize_t rv = send(fd,buf,n,0);
        if(rv < 0){
            std::cerr << "write error" << std::endl;
            return -1;
        }
        assert((size_t)rv <=n);
        n -= (size_t)rv;
        buf += (size_t)rv;
    }
    return 0;
}

const size_t k_max_msg = 4096;
static int32_t query(int fd,const char* context){
    uint32_t len = (uint32_t)strlen(context);
    if(len>k_max_msg){
        std::cerr << "msg too long" << std::endl;
    }
    char buf[4+k_max_msg];
    memcpy(buf,&len,4);
    memcpy(buf+4,context,len);
    if(write_full(fd,buf,4+len)){
        std::cerr << "write error" << std::endl;
    }

    char rbuf[4+k_max_msg+1];
    int32_t err = read_full(fd,rbuf,4);
    if(err){
        std::cerr << "read error" << std::endl;
        return -1;
    }
    memcpy(&len,rbuf,4);
    if(len > k_max_msg){
        std::cerr << "message too long" << std::endl;
        return -1;
    }
    err = read_full(fd,rbuf+4,len);
    if(err){
        std::cerr << "read error" << std::endl;
        return -1;
    }

    std::cout << "received: " << std::string(rbuf+4,len) << std::endl;
    return 0;
}

int main() {
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6379);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    std::cout << "Connecting to server..." << std::endl;
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }

    std::cout << "Connected to server!" << std::endl;

    {
        int32_t err = query(fd, "SET foo bar");
        if (err) {
            goto DONE;
        }
        err = query(fd, "GET foo");
        if (err) {
            goto DONE;
        }
        err = query(fd, "DEL foo");
        if (err) {
            goto DONE;
        }
        
    }
DONE:
        
    std::cout << "Done!" << std::endl;
    closesocket(fd);
    
    // 清理Winsock
    WSACleanup();
    return 0;
}