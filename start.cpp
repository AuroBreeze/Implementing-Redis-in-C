#include <iostream>
#include <cerrno>
#include <cstring>
#include <cassert>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 添加错误处理函数
static void die(const char *msg) {
    int err = WSAGetLastError();
    fprintf(stderr, "[%d] %s\n", err, msg);
    WSACleanup();
    exit(1);
}

static int32_t read_full(int fd,char* buf, size_t n){
  while(n >0){

    ssize_t rv = recv(fd,buf,n,0);
    if(rv<=0){
      return -1;
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += (size_t)rv;
  }
  return 0;
}

static int32_t write_full(int fd,char* buf, size_t n){
  while(n >0){
    ssize_t rv = send(fd,buf,n,0);
    if(rv<=0){
      return -1;
    }
    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += (size_t)rv;
  }
  return 0;
}

const size_t k_max_msg = 4096;

static int32_t one_request(int connfd){
  char rbuf[4+k_max_msg];
  int32_t err = read_full(connfd,rbuf,4);
  if(err){
    return err;
  }

  uint32_t len = 0;
  memcpy(&len,rbuf,4);

  if(len>k_max_msg){
    std::cerr << "message is too long" << std::endl;
    return -1;
  }

  err = read_full(connfd,rbuf+4,len);
  if(err){
    std::cerr << "read() error" << std::endl;
    return err;
  }

  std::cout << "client says: " << rbuf+4 << std::endl;

  const char reply[] = "hello from server";
  char wbuf[4+sizeof(reply)];
  len = (uint32_t)strlen(reply);
  memcpy(wbuf,&len,4);
  memcpy(wbuf+4,reply,len);
  return write_full(connfd,wbuf,4+len);
}

int main() {
  // 初始化Winsock
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
    std::cerr << "WSAStartup failed" << std::endl;
    return 1;
  }

  // std::cout << "Hello, Redis!" << std::endl;
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
      die("socket()");
  }

  // 设置服务器地址和端口
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(6379);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 绑定到所有可用接口

  // 设置套接字选项，允许地址重用
  int opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
      die("setsockopt(SO_REUSEADDR) failed");
  }

  int rv = bind(fd, (const struct sockaddr*)&addr, sizeof(addr));
  if (rv) { 
      die("bind()");
  }

  rv = listen(fd, 4096);
  if (rv){ die("listen()");}


  std::cout << "Server listening on port 6379..." << std::endl;

  while(true){
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr*)&client_addr, &addrlen);
    if (connfd < 0) {
        std::cerr << "accept() failed: " << WSAGetLastError() << std::endl;
        continue;
    }


    std::cout << "Client connected!" << std::endl;

    while(true){ 
      int32_t err = one_request(connfd);
      if(err){
        std::cerr << "one_request() failed: " << err << std::endl;
        break;
      }
    }
    closesocket(connfd);
  }

  // 清理Winsock
  closesocket(fd);
  WSACleanup();
  return 0;
}