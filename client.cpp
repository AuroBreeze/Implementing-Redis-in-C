#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cassert>
#include <vector>
#include <string>
#pragma comment(lib, "ws2_32.lib")


static void msg(const char* msg){
    fprintf(stderr, "%s\n", msg);
}
static void die(const char *msg) {
    int err = WSAGetLastError();
    fprintf(stderr, "[%d] %s\n", err, msg);
    WSACleanup();
    exit(1);
}



static int32_t read_full(int fd, uint8_t* buf, size_t n){
    while(n > 0){
        ssize_t rv = recv(fd,(char*)buf,n,0);
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

static int32_t write_full(int fd, uint8_t* buf, size_t n){ 
    while(n>0){
        ssize_t rv = send(fd,(const char*)buf,n,0);
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


static int32_t send_req(int fd, const std::vector<std::string> &cmd){
    uint32_t len = 4;
    for(const std::string &s:cmd){
        len += 4+s.size();
    }
    if(len > k_max_msg) return -1;
    char wbuf[4+k_max_msg];
    memcpy(wbuf,&len,4);
    uint32_t n = cmd.size();
    memcpy(&wbuf[4],&n,4);

    size_t cur = 8;
    for(const std::string &s:cmd){
        uint32_t p = (uint32_t)s.size();
        memcpy(&wbuf[cur],&p,4);
        memcpy(&wbuf[cur+4],s.data(),s.size());
        cur += 4+p;
    }

    return write_full(fd,(uint8_t *)wbuf,4+len);
}

static int32_t read_res(int fd){
    char rbuf[4+k_max_msg];

    int32_t err = read_full(fd,(uint8_t *)rbuf,4);
    if(err){
    }

    uint32_t len = 0;
    memcpy(&len,rbuf,4);
    if(len > k_max_msg){
        msg("msg too long");
        return -1;
    }
    
    err = read_full(fd,(uint8_t *)&rbuf[4],len);
    if(err){
        msg("read failed");
        return err;
    }

    uint32_t rescode = 0;
    if(len < 4){
        msg("bad response");
        return -1;
    }
    memcpy(&rescode,&rbuf[4],4);

    printf("server says:[%u] %.*s\n",rescode,len-4,&rbuf[8]);
    return 0;
}

int main(int argc, char **argv) {
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

    std::vector<std::string> cmd;
    for(int i = 1; i<argc ; ++i){
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(fd, cmd);
    if (err) {
        die("send_request");
    }

    err = read_res(fd);
    if (err) {
        die("read_res");
    }

    std::cout << "Done!" << std::endl;
    closesocket(fd);
    
    // 清理Winsock
    WSACleanup();
    return 0;
}
