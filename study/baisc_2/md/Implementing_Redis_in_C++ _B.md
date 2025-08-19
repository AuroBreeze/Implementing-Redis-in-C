# Implementing Redis in C++ : B

## 前言

本章代码及思路均来自[Build Your Own Redis with C/C++](https://build-your-own.org/redis/)

本文章只阐述我的理解想法，以及需要注意的地方。

本文章为续<<[Implementing Redis in C++ : A](https://blog.csdn.net/m0_58288142/article/details/150490656?fromshare=blogdetail&sharetype=blogdetail&sharerId=150490656&sharerefer=PC&sharesource=m0_58288142&sharefrom=from_link)>>所以本文章不再完整讲解全部代码，只讲解其不同的地方

## socket(servers)

### 主体思路

本文章在延续上文的非阻塞网络连接的代码上，修改为键值对存储，并使用socket进行通信。

在上文中，我们传输的数据的格式为(多条消息)：

```
+-----+------+-----+------+-----+-----+------+
| len | str1 | len | str2 | ... | len | strn |
+-----+------+-----+------+-----+-----+------+
```

在此基础上，我们修改了数据的格式为(单条消息)：

```
+-----+------+-----+------+-----+------+-----+-----+------+
| len | nstr | len | str1 | len | str2 | ... | len | strn |
+-----+------+-----+------+-----+------+-----+-----+------+
```

其中第一个`len`字段标识整个这单独一条消息的长度，第二个字段`nstr`标识这条消息的元素个数，第三个字段标识这条消息的第一个元素长度，第四个字段标识这条消息的第一个元素内容，以此类推。

其中我们打算使用的命令是`get`，`set`和`del`三个命令，其中`get`命令用于获取一个键所对应的值，`set`命令用于设置一个键所对应的值，`del`命令用于删除一个键所对应的值。

完整`client`端命令示例：

```bash
./client get key
./client set key value
./client del key
```

所以在我们的`server`端，我们需要额外的实现**键值对的存储**，**client端命令的解析**，以及**命令的响应**。

简单的思路:

**键值对的存储**：我们可以使用一个`map`来存储键值对，键为`string`，值为`string`，用`find`函数来查找键值对，用`swap`来设置键值对，用`erase`来删除键值对。

**client端命令的解析**：获取第一个`len`和`nstr`,之后解析剩下的消息。

### read_u32()
```cpp
const size_t k_max_args = 200 * 1000;

static bool read_u32(const uint8_t* &cur, const uint8_t* end, uint32_t &out){
    if(cur + 4 > end){ // not enough data for the first length
        return false;
    }
    memcpy(&out, cur , 4);
    cur += 4;
    return true;
}
```

首先是`read_u32()`这个函数，这个函数的主要功能是，当长度足够的时候(cur + 4 < end)，获取数据并将数据复制到指定的地址中，同时将数据的指针后移4个字节。

```
+-----+------+-----+------+-----+------+-----+-----+------+
| len | nstr | len | str1 | len | str2 | ... | len | strn |
+-----+------+-----+------+-----+------+-----+-----+------+
| 4   | 4    | 4   | str1 | 4   | str2 | ... | 4   | strn |
```

### read_str()

```cpp
static bool read_str(const uint8_t* &cur, const uint8_t* end, size_t n,std::string &out){
    if(cur + n > end) return false; // not enough data for the string
    out.assign(cur,cur + n);
    cur += n;
    return true;
}
```

这里的功能也并不难懂，需要注意的一点就是，`string`的`assign()`函数，在复制新内容到新空间时，会先清空就内容，再复制新内容。

### parse_req()

```cpp
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
```

```
+-----+------+-----+------+-----+------+-----+-----+------+
| len | nstr | len | str1 | len | str2 | ... | len | strn |
+-----+------+-----+------+-----+------+-----+-----+------+
| 4   | 4    | 4   | str1 | 4   | str2 | ... | 4   | strn |
```

先说这里的形参，`const uint8_t* data`，是已经处理好**前四个字节**的数据，也就是现在`data`的数据是从`nstr`开始了，而`std::vector<std::string> &out`中要存放的数据，是我们经过处理后获取到的完整数据。

比如(client -> server)：
```bash
./client set key value
```

那么最后，我们`out`中解析完的数据就是，

```python
out[0] = "set"
out[1] = "key"
out[2] = "value"
```

在上面的代码中，我们还看到了这段代码

```cpp
out.push_back(std::string());
if(!read_str(data,end,len,out.back())) return -1;
```

这段代码也是很有意思的，通过提前`push_back()`一个空的`string`，然后调用`read_str()`，将数据读入到这个空的`string`中，这样，`out`中，`out[i]`就是我们输入的参数了。

### do_request()

```cpp
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

static void do_request(std::vector<std::string> &cmd,Response &out){
    if(cmd.size() == 2 && cmd[0] == "get"){
        auto it = g_data.find(cmd[1]);
        if(it == g_data.end()){
            out.status = RES_NX;
            return ;
        }
        const std::string &val = it->second;
        out.data.assign(val.begin(),val.end());
        out.status = RES_OK;
    }else if(cmd.size() == 3 && cmd[0] == "set"){
        g_data[cmd[1]].swap(cmd[2]);
        out.status = RES_OK;
    }else if(cmd.size() == 2 && cmd[0] == "del"){
        g_data.erase(cmd[1]);
        out.status = RES_OK;
    }else{
        out.status = RES_ERR;

    }
}
```

这里具体的代码就是我们的**键值对**的处理的实现了

在这里的`std::vector<std::string> cmd`就是我们上面讲的`std::vector<std::string> out`，而在此处的`Response &out`就仅是一个准备处理响应的引用

### try_one_request()

```cpp
static bool try_one_requests(Conn* conn){
    if(conn->incoming.size() < 4) return false;
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);

    if(len > k_max_msg){
        msg("message too long");
        conn->want_close = true;
        return false;
    }

    if(4 + len > conn->incoming.size()) return false;
    const uint8_t* request = &conn->incoming[4];
    printf("client request: len: %u data: %.*x\n", len, (int)len, request);

    std::vector<std::string> cmd;
    if(parse_req(request, len, cmd)<0){
        msg("parse_req failed");
        conn->want_close = true;
        return false;
    }

    Response resp;
    do_request(cmd,resp);
    make_response(resp,conn->outgoing);
    buf_consume(conn->incoming,4+len);

    return true;
}
```

这里的代码也就不过多讲解了，就是将前面的将的函数结合起来，先解析请求，然后执行请求，将响应数据放到缓冲区中，删除输入缓冲区中的数据，最后，等下一次循环的时候，将数据发送出去。

## optimize(servers)





## socket(client)

### start

同上文一样，我们不再讲解`client`端的代码，具体代码可以读者自行查看

### end

```cpp
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
```




