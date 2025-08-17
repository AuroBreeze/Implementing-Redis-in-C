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

static void buf_append(std::vector<uint8_t> &buf,const uint8_t* data,size_t len){
    buf.insert(buf.end(),data,data+len);
}

const size_t k_max_msg = 32 << 20;


static int32_t send_req(int fd, const uint8_t* text, size_t len){
    if(len > k_max_msg) return -1;
    std::vector<uint8_t> wbuf;
    buf_append(wbuf,(const uint8_t*)&len,4);
    buf_append(wbuf,text,len);

    return write_full(fd,wbuf.data(),wbuf.size());
}

static int32_t read_res(int fd){
    std::vector<uint8_t> rbuf;
    rbuf.resize(4);

    int32_t err = read_full(fd,rbuf,4);
    if(err){
    }

    uint32_t len = 0;
    memcpy(&len,rbuf.data(),4);
    if(len > k_max_msg){
        msg("msg too long");
        return -1;
    }
    rbuf.resize(4+len);
    err = read_full(fd,&rbuf[4],len);
    if(err){
        msg("read failed");
        return err;
    }

    printf("len: %u data: %s\n",len, len < 100 ? len : 100,&rbuf[4]);
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

    std::vector<std::string> query_list = {
        "hello1","hello2","hello3",
        std::string(k_max_msg,'z'),
        "hello5"
    };

    for(const std::string &s:query_list){

        int32_t err = send_req(fd,(uint8_t *)s.data(),s.size());
        if(err){
            std::cout << "Error: " << err << std::endl;
            break;
        }
    }

    for(size_t i = 0; i < query_list.size();++i){
        int32_t err = read_res(fd);
        if (err)
        {
            std::cout << "Error: " << err << std::endl;
            break;
        }
        
    }
        
    std::cout << "Done!" << std::endl;
    closesocket(fd);
    
    // 清理Winsock
    WSACleanup();
    return 0;
}