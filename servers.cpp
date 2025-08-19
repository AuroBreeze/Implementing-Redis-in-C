#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#pragma comment(lib, "ws2_32.lib")

using namespace std;



struct Ring_buf{
    std::vector<uint8_t> buf;
    size_t head;
    size_t tail;
    size_t cap;
    size_t status;

    Ring_buf():buf(256),head(0),tail(0),cap(256){
    }

    size_t size() const{
        return (cap + tail - head) % cap;
    }
    size_t free_cap() const{
        return cap - size() -1 ;
    }

    bool full() const{
        return (tail + 1) % cap == head;
    }

    bool empty() const{
        return head == tail;
    }
};

static void make_response(const string &resp, Ring_buf &out);
static void msg(const char *fmt) {
    fprintf(stderr, "%s\n",fmt);
}

static void die(const char *msg) {
    int err = WSAGetLastError();
    fprintf(stderr, "[%d] %s\n", err, msg);
    WSACleanup();
    exit(1);
}

static void hex_dump(const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        printf("%02X", p[i]);
        if ((i + 1) % 16 == 0) printf("\n");
        else printf(" ");
    }
    if (n % 16) printf("\n");
}

// 设置 socket 为非阻塞
static void fd_set_nb(SOCKET fd){
    u_long mode = 1;
    if (ioctlsocket(fd, FIONBIO, &mode) != 0) {
        die("ioctlsocket FIONBIO failed");
    }
}

const size_t k_max_msg = 32 << 20;

struct Conn {
    SOCKET fd;
    bool want_read = true;
    bool want_write = false;
    bool want_close = false;
    // vector<uint8_t> incoming;
    // vector<uint8_t> outgoing;
    Ring_buf incoming;
    Ring_buf outgoing;
};

static bool buf_append(Ring_buf &buf, const uint8_t *data, size_t n){
    if (n > buf.free_cap()) return false; // not enough space
    size_t min = std::min(n,buf.cap - buf.tail);
    memcpy(&buf.buf[buf.tail], data, min);
    memcpy(&buf.buf[0], data + min, n - min);
    buf.tail = (buf.tail + n) % buf.cap;
    return true;
}

static void buf_consume(Ring_buf &buf, size_t n){
    buf.head = (buf.head + n) % buf.cap;
}

static Conn* handle_accept(SOCKET listen_fd) {
    sockaddr_in client_addr{};
    int addrlen = sizeof(client_addr);
    SOCKET connfd = accept(listen_fd, (sockaddr*)&client_addr, &addrlen);
    if (connfd == INVALID_SOCKET) {
        int err = WSAGetLastError();
        if(err != WSAEWOULDBLOCK) 
            fprintf(stderr, "accept() error: %d\n", err);
        return nullptr;
    }

    uint32_t ip = ntohl(client_addr.sin_addr.s_addr);
    fprintf(stderr,
        "new client from %u.%u.%u.%u:%u\n",
        (ip >> 24) & 255,
        (ip >> 16) & 255,
        (ip >> 8) & 255,
        ip & 255,
        ntohs(client_addr.sin_port)
    );

    fd_set_nb(connfd);

    Conn* conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}

const size_t k_max_args = 200 * 1000;

static bool read_u32(const uint8_t* &cur, const uint8_t* end, uint32_t &out){
    if(cur + 4 > end){ // not enough data for the first length
        return false;
    }
    memcpy(&out, cur , 4);
    cur += 4;
    return true;
}

static bool read_str(const uint8_t* &cur, const uint8_t* end, size_t n,std::string &out){
    if(cur + n > end) return false; // not enough data for the string
    out.assign(cur,cur + n);
    cur += n;
    return true;
}


// +-----+------+-----+------+-----+------+-----+-----+------+
// | len | nstr | len | str1 | len | str2 | ... | len | strn |
// +-----+------+-----+------+-----+------+-----+-----+------+

static int32_t parse_req(const uint8_t* data, size_t size,std::vector<std::string> &out){
    const uint8_t* end = data+size;

    uint32_t nstr = 0;
    if(!read_u32(data,end,nstr)) return -1;
    if(nstr > k_max_args) return -1;

    while(out.size() < nstr){
        uint32_t len = 0;
        if(!read_u32(data,end,len)) return -1;

        out.push_back(std::string());
        if(!read_str(data,end,len,out.back())) return -1;
    }

    if(data != end) return -1;
    return 0;
}

enum{
    RES_OK = 0,
    RES_ERR = 1, // error
    RES_NX = 2 , // key not found
};

// +--------+---------+
// | status | data... |
// +--------+---------+

struct Response{
    uint32_t status;
    std::vector<uint8_t> data;
};

static std::map<std::string,std::string> g_data;

static string do_request(std::vector<std::string> &cmd,Ring_buf &buf){
    if(cmd.size() == 2 && cmd[0] == "get"){
        auto it = g_data.find(cmd[1]);
        if(it == g_data.end()){
            buf.status = RES_NX;
            return "0";
        }
        const std::string &val = it->second;
        // out.data.assign(val.begin(),val.end());
        // make_response(val,buf);
        buf.status = RES_OK;
        return val;
    }else if(cmd.size() == 3 && cmd[0] == "set"){
        g_data[cmd[1]].swap(cmd[2]);
        buf.status = RES_OK;
    }else if(cmd.size() == 2 && cmd[0] == "del"){
        g_data.erase(cmd[1]);
        buf.status = RES_OK;
        
    }else{
        buf.status = RES_ERR;
        
    }
    return "0";
};

static void make_response(const string &resp, Ring_buf &out){
    uint32_t resp_len = 4 + (uint32_t)resp.size();
    buf_append(out,(const uint8_t*)&resp_len,4);
    buf_append(out,(const uint8_t*)&out.status,4);
    buf_append(out,(const uint8_t*)resp.data(),resp.size());
}


static bool try_one_requests(Conn* conn){
    if(conn->incoming.size() < 4) return false;
    uint32_t len = 0;

    { // 头部可能被环形分割成两个部分
    size_t  head = conn->incoming.head;
    size_t first = std::min<size_t>(conn->incoming.cap-head,4);
    uint8_t hdr[4];
    memcpy(hdr,&conn->incoming.buf[head],first);
    if(first < 4){
        memcpy(&hdr[first],&conn->incoming.buf[0],4-first);
    }
    memcpy(&len,hdr,4);
    }


    if(len > k_max_msg){
        msg("message too long");
        conn->want_close = true;
        return false;
    }

    if(4 + len > conn->incoming.size()) return false;

    std::vector<uint8_t> request(len);
    size_t start = (conn->incoming.head + 4)%conn->incoming.cap;
    size_t first = std::min<size_t>(len,conn->incoming.cap - start);
    memcpy(request.data(),&conn->incoming.buf[start],first);
    if(first < len){
        memcpy(request.data() + first,&conn->incoming.buf[0],len - first);
    }

    printf("client request: len: %u \n", len);
    //hex_dump(request.data(),len);

    std::vector<std::string> cmd;
    if(parse_req(request.data(), len, cmd)<0){
        msg("parse_req failed");
        conn->want_close = true;
        return false;
    }

    // Response resp;
    std::string s = do_request(cmd,conn->outgoing);
    if(!s.empty()){
        make_response(s,conn->outgoing);
    }else{
        std::string s = "Done!";
        make_response(s,conn->outgoing);
    }
    // make_response(resp,conn->outgoing);
    buf_consume(conn->incoming,4+len);

    return true;
}

static void send_all(Conn* conn,Ring_buf &buf){
    size_t n = buf.size();
    size_t min = std::min(n,buf.cap - buf.head);
    int rv = send(conn->fd, (const char*)&buf.buf[buf.head],min,0);
    if(rv == SOCKET_ERROR){
        int err = WSAGetLastError();
        if(err == WSAEWOULDBLOCK) return;
        msg("send() error");
        conn->want_close = true;
        return;
    }
    if(rv > 0) buf_consume(buf, (size_t)rv);
}

static void handle_write(Conn* conn){
    if(conn->outgoing.empty()) return;

    send_all(conn, conn->outgoing);
    // buf_consume(conn->outgoing, rv);
    if(conn->outgoing.empty()){
        conn->want_read = true;
        conn->want_write = false;
    }
}

static void handle_read(Conn* conn){
    uint8_t buf[64*1024];
    int rv = recv(conn->fd, (char*)buf, sizeof(buf), 0);
    if(rv == 0){
        msg("connection closed by client");
        conn->want_close = true;
        return;
    }
    if(rv == SOCKET_ERROR){
        int err = WSAGetLastError();
        if(err == WSAEWOULDBLOCK) return;
        msg("recv() error");
        conn->want_close = true;
        return;
    }

    buf_append(conn->incoming, buf, rv);
    while(try_one_requests(conn)){}
    if(!conn->outgoing.empty()){
        conn->want_write = true;
        conn->want_read = false;
        handle_write(conn);
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
        cerr << "WSAStartup failed" << endl;
        return 1;
    }

    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == INVALID_SOCKET) die("socket() failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6379);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if(bind(fd, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
        die("bind() failed");

    if(listen(fd, SOMAXCONN) == SOCKET_ERROR)
        die("listen() failed");

    fd_set_nb(fd);
    cout << "Server listening on port 6379..." << endl;

    unordered_map<SOCKET, Conn*> fd2conn_map;
    vector<WSAPOLLFD> poll_args;

    while(true){
        poll_args.clear();

        // 监听 socket
        WSAPOLLFD pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        poll_args.push_back(pfd);

        // 所有客户端
        for(auto& kv : fd2conn_map){
            Conn* conn = kv.second;
            WSAPOLLFD p{};
            p.fd = conn->fd;
            p.events = 0;
            if(conn->want_read) p.events |= POLLIN;
            if(conn->want_write) p.events |= POLLOUT;
            poll_args.push_back(p);
        }

        int rv = WSAPoll(poll_args.data(), (ULONG)poll_args.size(), -1);
        if(rv == SOCKET_ERROR){
            int err = WSAGetLastError();
            if(err == WSAEINTR) continue;
            die("WSAPoll() failed");
        }

        // 新连接
        if(poll_args[0].revents & POLLIN){
            if(Conn* conn = handle_accept(fd)){
                fd2conn_map[conn->fd] = conn;
            }
        }

        // 客户端读写
        vector<SOCKET> to_close;
        for(size_t i=1; i<poll_args.size(); ++i){
            WSAPOLLFD &p = poll_args[i];
            Conn* conn = fd2conn_map[p.fd];
            if(!conn) continue;

            if(p.revents & POLLIN) handle_read(conn);
            if(p.revents & POLLOUT) handle_write(conn);
            if((p.revents & POLLERR) || conn->want_close) to_close.push_back(conn->fd);
        }

        for(SOCKET cfd : to_close){
            closesocket(cfd);
            delete fd2conn_map[cfd];
            fd2conn_map.erase(cfd);
        }
    }

    closesocket(fd);
    WSACleanup();
    return 0;
}
