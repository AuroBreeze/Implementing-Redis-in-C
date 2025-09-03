#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cassert>
#include <vector>
#include <string>
#include <cstdint>
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

enum{
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

static int32_t print_response(const uint8_t* data, size_t size){
    if(size < 1){
        msg("bad response 86");
    }
    switch(data[0]){
        case TAG_NIL:
            printf("(nil)\n");
            return -1;
        case TAG_ERR:
            if(size< 1 + 8){
                msg("bad response TAG_ERR_1");
                return -1;
            }
            {
                int32_t code = 0;
                uint32_t len = 0;
                memcpy(&code, &data[1], 4);
                memcpy(&len, &data[1+4], 4);
                if(size < 1 + 8 + len){
                    msg("bad response TAG_ERR_2");
                    return -1;
                }
                printf("(err) %d %.*s\n", code, len, &data[1 + 8]);
                return 1+8+len;
            }
        case TAG_STR:
            if(size < 1 + 4){
                msg("bad response TAG_STR_1");
                return -1;
            }
            {
                uint32_t len = 0;
                memcpy(&len, &data[1], 4);
                if(size< 1 + 4 + len){
                    msg("bad response TAG_STR_2");
                    return -1;
                }
                printf("(str) %.*s\n", len, &data[1 + 4]);
                return 1 + 4 + len;
            }
        case TAG_INT:
            if (size < 1 + 8) {
                msg("bad response TAG_INT");
                return -1;
            }
            {
                int64_t val = 0;
                memcpy(&val, &data[1], 8);
                printf("(int) %ld\n", val);
                return 1 + 8;
            }
        case TAG_DBL:
            if (size < 1 + 8) {
                msg("bad response TAG_DBL");
                return -1;
            }
            {
                double val = 0;
                memcpy(&val, &data[1], 8);
                printf("(dbl) %g\n", val);
                return 1 + 8;
            }
        case TAG_ARR:
            if(size < 1 + 4){
                msg("bad response TAG_ARR");
                return -1;
            }
            {
                uint32_t len =0;
                memcpy(&len, &data[1], 4);
                printf("(arr) len=%u", len);
                size_t arr_bytes = 1 + 4;
                for(uint32_t i = 0; i < len; i++){
                    int32_t rv = print_response(&data[arr_bytes], size-arr_bytes);
                    if(rv < 0){
                        return rv;
                    }
                    arr_bytes += (size_t)rv;
                }
                printf("(arr) end\n");
                return (int32_t)arr_bytes;
            }
        default:
            msg("bad response default");
            return -1;
    }
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

    int32_t rv = print_response((uint8_t*)&rbuf[4], len);
    if(rv > 0 && (uint32_t)rv != len){
        msg("bad response 194");
        rv = -1;
    }
    return rv;
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
