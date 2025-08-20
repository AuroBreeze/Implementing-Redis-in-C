#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

static void die(const char *msg) {
    int err = WSAGetLastError();
    fprintf(stderr, "[%d] %s\n", err, msg);
    WSACleanup();
    exit(1);
}

static int32_t read_full(int fd, uint8_t* buf, size_t n){
    while(n > 0){
        int rv = recv(fd,(char*)buf,n,0);
        if(rv <= 0){
            return -1;
        }
        n -= rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_full(int fd, const uint8_t* buf, size_t n){ 
    while(n>0){
        int rv = send(fd,(const char*)buf,n,0);
        if(rv <= 0){
            return -1;
        }
        n -= rv;
        buf += rv;
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
    if(read_full(fd,(uint8_t *)rbuf,4)) return -1;

    uint32_t len = 0;
    memcpy(&len,rbuf,4);
    if(len > k_max_msg){
        return -1;
    }
    
    if(read_full(fd,(uint8_t *)&rbuf[4],len)) return -1;

    uint32_t rescode = 0;
    memcpy(&rescode,&rbuf[4],4);

    printf("server says:[%u] %.*s\n",rescode,len-4,&rbuf[8]);
    return 0;
}

void client_task(int id, std::atomic<int>& success_count) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return;
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6379);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(fd);
        return;
    }

    std::vector<std::string> cmd = {"set", "key"+std::to_string(id), "value"+std::to_string(id)};
    if (send_req(fd, cmd) == 0 && read_res(fd) == 0) {
        success_count++;
    }

    closesocket(fd);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        return 1;
    }

    const int NUM_CLIENTS = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    auto start = std::chrono::high_resolution_clock::now();

    for(int i=0; i<NUM_CLIENTS; i++){
        threads.emplace_back(client_task, i, std::ref(success_count));
    }

    for(auto &t : threads){
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Finished " << NUM_CLIENTS << " clients, success = " 
              << success_count << ", time = " << elapsed.count() << "s" << std::endl;

    WSACleanup();
    return 0;
}
