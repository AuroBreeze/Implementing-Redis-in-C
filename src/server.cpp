#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iostream>
#include <map>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cassert>

#include "hashtable.h"
#include "common.h"
#include "zset.h"
#pragma comment(lib, "ws2_32.lib")

#define container_of(ptr,T,member) \
    ((T*)( (char*)ptr - offsetof(T,member) ))
using namespace std;



struct Ring_buf{
    std::vector<uint8_t> buf;
    size_t head;
    size_t tail;
    size_t cap;
    size_t status;

    Ring_buf():buf(1024),head(0),tail(0),cap(1024){
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

    bool clear(){
        head = tail;
        return true;
    }
};


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

const size_t k_max_msg = 4096;

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

static void buf_append(Ring_buf &buf, const uint8_t *data, size_t n){
    if (n > buf.free_cap()) return;
    size_t min = std::min(n,buf.cap - buf.tail);
    memcpy(&buf.buf[buf.tail], data, min);
    memcpy(&buf.buf[0], data + min, n - min);
    buf.tail = (buf.tail + n) % buf.cap;
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

//不再使用
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

enum {
    ERR_UNKNOWN = 1, // unknown command
    ERR_TOO_BIG = 2,  // response too big
    ERR_BAD_TYP = 3, // wrong value type
    ERR_BAD_ARG = 4 // wrong argument
};

enum{
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};
//  nil       int64           str                   array
// ┌─────┐   ┌─────┬─────┐   ┌─────┬─────┬─────┐   ┌─────┬─────┬─────┐
// │ tag │   │ tag │ int │   │ tag │ len │ ... │   │ tag │ len │ ... │
// └─────┘   └─────┴─────┘   └─────┴─────┴─────┘   └─────┴─────┴─────┘
//    1B        1B    8B        1B    4B   ...        1B    4B   ...

static void buf_append_u8(Ring_buf& buf, uint8_t data){
    buf_append(buf, (const uint8_t*)&data, sizeof(data));
}

static void buf_append_u32(Ring_buf& buf, uint32_t data){
    buf_append(buf, (const uint8_t*)&data, 4);
}

static void buf_append_i64(Ring_buf& buf, int64_t data){
    buf_append(buf, (const uint8_t*)&data, 8);
}

static void buf_append_dbl(Ring_buf& buf, double data){
    buf_append(buf, (const uint8_t*)&data, 8);
}


static void out_nil(Ring_buf& buf){
    buf_append_u8(buf, TAG_NIL);
}

static void out_str(Ring_buf& buf, const char* s, size_t size){
    buf_append_u8(buf, TAG_STR);
    buf_append_u32(buf, (uint32_t)size);
    buf_append(buf, (const uint8_t*)s, size);
}

static void out_int(Ring_buf& buf, int64_t val){
    buf_append_u8(buf, TAG_INT);
    buf_append_i64(buf, val);
}

static void out_dbl(Ring_buf& buf, double val){
    buf_append_u8(buf, TAG_DBL);
    buf_append_dbl(buf, val);
}

static void out_err(Ring_buf& buf, uint32_t code, const std::string &msg){
    buf_append_u8(buf, TAG_ERR);
    buf_append_u32(buf, code);
    buf_append_u32(buf, (uint32_t)msg.size());
    buf_append(buf, (const uint8_t*)msg.data(), msg.size());
}

static void out_arr(Ring_buf& buf, uint32_t n){
    buf_append_u8(buf, TAG_ARR);
    buf_append_u32(buf, n);
}

static size_t out_begin_arr(Ring_buf &buf){
    buf_append_u8(buf, TAG_ARR);
    buf_append_u32(buf, 0); // filled by out_end_arr()
    return buf.size() - 4; // the ctx arg
};

static void out_end_arr(Ring_buf &buf, size_t ctx, uint32_t n) {
    // ctx 是 out_begin_arr 返回的 "逻辑位置"，它指向数组长度字段的位置
    assert(buf.buf[(buf.head + ctx - 1) % buf.cap] == TAG_ARR);

    size_t pos = (buf.head + ctx) % buf.cap;
    size_t first = std::min(buf.cap - pos, sizeof(n));

    memcpy(&buf.buf[pos], &n, first);
    if (first < sizeof(n)) {
        memcpy(&buf.buf[0], (uint8_t*)&n + first, sizeof(n) - first);
    }
}



enum{
    RES_OK = 0,
    RES_ERR = 1, // error
    RES_NX = 2 , // key not found
};

enum{
    T_INIT = 0,
    T_STR = 1,
    T_ZSET = 2,
};

// +--------+---------+
// | status | data... |
// +--------+---------+


static struct {
    HMap db;
} g_data;

struct Entry{
    struct HNode node; // hashtable node
    std::string key;
    // std::string val;

    // value
    uint32_t type = 0;
    std::string str;
    ZSet zset;
};

static Entry* entry_new(uint32_t type){
    Entry* ent = new Entry();
    ent->type = type;
    return ent;
}

static void entry_delete(Entry* ent){
    if(ent->type == T_ZSET){
        zset_clear(&ent->zset);
    }
    delete ent;
}

struct LookupKey{
    struct HNode node;
    std::string key;
};

static bool entry_eq(HNode* lhs, HNode* rhs){
    struct Entry* ent = container_of(lhs,struct Entry, node);
    struct LookupKey* keydata = container_of(rhs,struct LookupKey, node);
    return ent->key == keydata->key;
}
// static std::map<std::string,std::string> g_data;


static void do_get(std::vector<std::string> &cmd, Ring_buf &buf){
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((const uint8_t*) key.key.data(), key.key.size());
    //hashtable lookup
    HNode* node = hm_lookup(&g_data.db,&key.node,&entry_eq);
    if(!node){
        out_nil(buf);
        return;
    }

    // copy the value
    Entry* ent = container_of(node, Entry, node);
    if(ent->type != T_STR){
        return out_err(buf, ERR_BAD_TYP, "not a string value");
    }
    return out_str(buf, ent->str.data(), ent->str.size());

    // const std::string* val = &container_of(node,Entry,node)->val;
    // out_str(buf, val->data(), val->size());
}

static void do_set(std::vector<std::string> &cmd, Ring_buf& buf){
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((const uint8_t*)key.key.data(),key.key.size());

    HNode *node = hm_lookup(&g_data.db,&key.node,&entry_eq);
    if(node){
        Entry* ent = container_of(node, Entry, node);
        if(ent->type != T_STR){
            return out_err(buf,ERR_BAD_TYP,"a non-string value exists");
        }
        ent->str.swap(cmd[2]);
    }else{
        Entry* ent = entry_new(T_STR);
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->str.swap(cmd[2]);
        hm_insert(&g_data.db, &ent->node);
    }
    out_nil(buf);
}

static void do_del(std::vector<std::string> &cmd, Ring_buf& buf) {
    // a dummy `Entry` just for the lookup
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    // hashtable delete
    HNode *node = hm_delete(&g_data.db, &key.node, &entry_eq);
    if (node) { // deallocate the pair
        delete container_of(node, Entry, node);
    }

    out_int(buf, node ? 1 : 0);
}

static bool cb_keys(HNode* node, void* arg){
    Ring_buf &buf = *(Ring_buf*)arg;
    const std::string& key = container_of(node, Entry, node)->key;
    out_str(buf, key.data(), key.size());
    return true;
}

static void do_keys(std::vector<string>&, Ring_buf &buf){
    out_arr(buf, (uint32_t)hm_size(&g_data.db));
    hm_foreach(&g_data.db, &cb_keys, (void*)&buf);
}

static bool str2dbl(const std::string &s, double &out){
    char* endp = nullptr;
    out = strtod(s.c_str(), &endp);
    return endp == s.c_str() + s.size() && !isnan(out);
}

static bool str2int(const std::string &s, int64_t &out){
    char* endp = nullptr;
    out = strtoll(s.c_str(), &endp, 10);
    return endp == s.c_str() + s.size();
}

// zadd zset score name
static void do_zadd(std::vector<std::string> &cmd, Ring_buf &buf){
    double score = 0;
    if(!str2dbl(cmd[2], score)){
        return out_err(buf, ERR_BAD_ARG, "expect float");
    }

    // lookup the zset
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t*)key.key.data(), key.key.size());
    HNode* hnode = hm_lookup(&g_data.db, &key.node, &entry_eq);


    Entry* ent = nullptr;
    if(!hnode){
        // insert a new key
        ent = entry_new(T_ZSET);
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        hm_insert(&g_data.db, &ent->node);
    }
    else{
        ent = container_of(hnode, Entry, node);
        if(ent->type != T_ZSET){
            return out_err(buf, ERR_BAD_TYP, "expect zset");
        }
    }

    // add or update the tuple
    const std::string &name = cmd[3];
    bool added = zset_insert(&ent->zset, name.data(), name.size(), score);
    return out_int(buf, (int64_t)added);
}

static const ZSet k_empty_zset;

static ZSet* expect_zset(std::string &s){
    LookupKey key;
    key.key.swap(s);
    key.node.hcode = str_hash((uint8_t*)key.key.data(), key.key.size());
    HNode* hnode = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if(!hnode){
        // a non-existent key is treated as an empty zset
        return (ZSet*)&k_empty_zset;
    }
    Entry* ent = container_of(hnode, Entry, node);
    return ent->type == T_ZSET ? &ent->zset : nullptr;
}

static void do_zrem(std::vector<std::string> &cmd, Ring_buf &buf){
    ZSet* zset = expect_zset(cmd[1]);
    if(!zset){
        return out_err(buf, ERR_BAD_TYP, "expect zset");
    }

    const std::string &name = cmd[2];
    ZNode* znode = zset_lookup(zset, name.data(), name.size());
    if(znode){
        zset_delete(zset, znode);
    }
    return out_int(buf, znode ? 1 : 0);
}

static void do_zscore(std::vector<std::string> &cmd, Ring_buf &buf){
    ZSet* zset = expect_zset(cmd[1]);
    if(!zset){
        return out_err(buf, ERR_BAD_TYP, "expect zset");
    }

    const std::string &name = cmd[2];
    ZNode* znode = zset_lookup(zset, name.data(), name.size());
    return znode ? out_dbl(buf, znode->score) : out_nil(buf);
}

// zquery zset score name offset limit 
static void do_zquery(std::vector<std::string> &cmd, Ring_buf &buf){
    // parse args
    double score = 0;
    if(!str2dbl(cmd[2], score)){
        return out_err(buf, ERR_BAD_ARG, "expect fp number");
    }

    const std::string &name = cmd[3];
    int64_t offset = 0, limit = 0;
    if(!str2int(cmd[4], offset) || !str2int(cmd[5],limit)){
        return out_err(buf, ERR_BAD_ARG, "expect int");
    }

    // get the zset
    ZSet* zset = expect_zset(cmd[1]);
    if(!zset){
        return out_err(buf, ERR_BAD_TYP, "expect zset");
    }

    // seek to the key
    if(limit <= 0){
        return out_arr(buf,0);
    }
    ZNode* znode = zset_seekge(zset, score, name.data(), name.size());
    znode = znode_offset(znode, offset);

    // output 
    size_t ctx = out_begin_arr(buf);
    int64_t n = 0;
    while(znode && n < limit){
        out_str(buf, znode->name, znode->len);
        out_dbl(buf, znode->score);
        znode = znode_offset(znode, 1);
        n += 2;
    }
    out_end_arr(buf, ctx, (uint32_t)n);

}




static void do_request(std::vector<std::string> &cmd,Ring_buf &buf){
    if(cmd.size() == 2 && cmd[0] == "get"){
        do_get(cmd, buf);
    }else if(cmd.size() == 3 && cmd[0] == "set"){
        do_set(cmd, buf);
    }else if(cmd.size() == 2 && cmd[0] == "del"){
        do_del(cmd, buf);
    }else if(cmd.size() == 1 && cmd[0] == "keys"){
        do_keys(cmd, buf);
    }else if(cmd.size() == 4 && cmd[0] == "zadd"){
        return do_zadd(cmd, buf);
    }else if(cmd.size() == 3 && cmd[0] == "zrem"){
        return do_zrem(cmd, buf);
    }else if(cmd.size() == 3 && cmd[0] == "zscore"){
        return do_zscore(cmd, buf);
    }else if (cmd.size() == 6 && cmd[0] == "zquery"){
        return do_zquery(cmd, buf);
    }else{
        return out_err(buf, ERR_UNKNOWN, "unknown command.");    
    }
};

static void response_begin(Ring_buf& buf, size_t *header){
    *header = buf.size(); // message header position
    buf_append_u32(buf, 0);
}

static size_t response_size(Ring_buf& buf, size_t header){
    return buf.size() - header - 4;
}

static void response_end(Ring_buf& buf, size_t header){
    size_t msg_size = response_size(buf, header);
    if(msg_size > k_max_msg){
        // buf.tail = (buf.tail + buf.cap - msg_size) % buf.cap ;
        buf.clear();
        out_err(buf, ERR_TOO_BIG, "response too big");
        msg_size = response_size(buf, header);
    }
    uint32_t len = (uint32_t)msg_size;
    // buf_append(buf, (const uint8_t*)&len, sizeof(len));

    size_t pos = (buf.head + header) % buf.cap;
    size_t first = std::min(buf.cap - pos, sizeof(len));
    memcpy(&buf.buf[pos], &len, first);
    if(first < sizeof(len)){
        memcmp(&buf.buf[0], (uint8_t*)&len + first, sizeof(len)-first);}
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
    // hex_dump(request.data(),len);

    std::vector<std::string> cmd;
    if(parse_req(request.data(), len, cmd)<0){
        msg("parse_req failed");
        conn->want_close = true;
        return false;
    }

    // Response
    size_t header_pos = 0;
    response_begin(conn->outgoing, &header_pos);
    do_request(cmd, conn->outgoing);
    response_end(conn->outgoing, header_pos);



    // make_response(resp,conn->outgoing);
    buf_consume(conn->incoming,4+len);

    return true;
}

static void send_all(Conn* conn,Ring_buf &buf){
    size_t n = buf.size();
    size_t min = std::min(n,buf.cap - buf.head);
    //cout <<" size: " << n << endl; 
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
