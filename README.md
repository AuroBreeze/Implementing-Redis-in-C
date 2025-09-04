# C++ Redis Implementation

一个用C++实现的Redis-like键值存储系统，支持基本的键值操作和网络通信。

## 功能特性

### 核心功能
- ✅ **键值操作**: SET, GET, DEL, KEYS 命令
- ✅ **有序集合**: ZADD, ZREM, ZSCORE, ZQUERY 命令
- ✅ **数据类型**: 字符串、整数、浮点数、数组、nil、错误等多种类型

### 网络与性能
- ✅ **网络架构**: 基于Windows Socket API的客户端-服务器模型
- ✅ **高性能I/O**: 非阻塞I/O和多路复用 (WSAPoll)
- ✅ **内存优化**: 环形缓冲区实现零拷贝，减少内存碎片
- ✅ **并发支持**: 支持多客户端同时连接

### 数据结构
- ✅ **自定义哈希表**: 渐进式rehash，FNV哈希算法
- ✅ **AVL平衡树**: 用于有序集合排序，自动平衡维护
- ✅ **双索引结构**: AVL树按(score, name)排序 + 哈希表按name索引

### 协议格式
- ✅ **二进制协议**: 前4字节长度字段 + 变长参数
- ✅ **TLV响应**: Type-Length-Value格式，支持多种数据类型

## 项目结构

```
.
├── src/
│   ├── server.cpp     # Redis服务器主程序
│   ├── hashtable.h    # 自定义哈希表头文件
│   ├── hashtable.cpp  # 自定义哈希表实现
│   ├── avl.h          # AVL树头文件
│   ├── avl.cpp        # AVL树实现
│   ├── zset.h         # 有序集合头文件
│   ├── zset.cpp       # 有序集合实现
│   ├── common.h       # 公共定义和工具函数
│   ├── test_avl.cpp   # AVL树测试程序
│   └── test_offset.cpp # AVL树偏移测试程序
├── test/
│   ├── client.cpp     # Redis客户端实现
│   ├── server_test.cpp # 服务器压力测试代码
│   └── test.cpp       # 环形缓冲区测试代码
├── CMakeLists.txt     # CMake构建配置
├── .gitignore         # Git忽略文件
└── study/             # 学习版本目录
    ├── basic_1/       # 基础版本实现
    ├── baisc_2/       # 优化版本实现（带环形缓冲区）
    ├── dev_1/         # 开发版本1（自定义哈希表）
    ├── dev_2/         # 开发版本2（自定义TLV实现）
    ├── dev_3/         # 开发版本3（AVL树实现）
    └── dev_4/         # 开发版本4（有序集合实现）
```

## 编译方法

### 使用CMake编译（推荐）

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译所有目标
cmake --build .

# 或者编译特定目标
cmake --build . --target server    # 只编译服务器
cmake --build . --target client    # 只编译客户端
cmake --build . --target server_test # 只编译服务器测试程序
```

编译后的可执行文件将位于 `build/Debug/` 目录下。

### 手动编译

```bash
# 编译服务器
g++ -std=c++17 -o server src/server.cpp src/hashtable.cpp src/avl.cpp src/zset.cpp -lws2_32

# 编译客户端
g++ -std=c++17 -o client test/client.cpp -lws2_32

# 编译服务器测试程序
g++ -std=c++17 -o server_test test/server_test.cpp -lws2_32

# 编译环形缓冲区测试程序
g++ -std=c++17 -o test test/test.cpp

# 编译AVL树测试程序
g++ -std=c++17 -o test_avl src/test_avl.cpp src/avl.cpp

# 编译偏移测试程序
g++ -std=c++17 -o test_offset src/test_offset.cpp src/avl.cpp
```

## 使用方法

### 启动服务器

```bash
./server
```

### 使用客户端

```bash
# SET 操作
./client set key value

# GET 操作  
./client get key

# DEL 操作
./client del key

# ZADD 操作 (有序集合)
./client zadd zset 1.0 member1

# ZREM 操作 (有序集合)
./client zrem zset member1

# ZSCORE 操作 (有序集合)
./client zscore zset member1

# ZQUERY 操作 (有序集合)
./client zquery zset 1.0 "" 0 10
```

### 压力测试

```bash
./server_test
```

## 网络协议格式

### 请求协议格式
项目使用自定义二进制协议格式：

```
+----------------+----------------+----------------+-----+
|  4字节长度字段  |  命令参数1长度  |  命令参数1数据  | ... |
+----------------+----------------+----------------+-----+
```

- 前4字节：整个消息的长度（小端字节序）
- 后续数据：变长参数列表，每个参数包含长度字段和数据

### 响应协议格式 (TLV - Type-Length-Value)
dev_2版本引入了TLV格式的响应协议：

```
//  nil       int64           str                   array
// ┌─────┐   ┌─────┬─────┐   ┌─────┬─────┬─────┐   ┌─────┬─────┬─────┐
// │ tag │   │ tag │ int │   │ tag │ len │ ... │   │ tag │ len │ ... │
// └─────┘   └─────┴─────┘   └─────┴─────┴─────┘   └─────┴─────┴─────┘
//    1B        1B    8B        1B    4B   ...        1B    4B   ...
```

支持的数据类型：
- `TAG_NIL` (0): nil值 - 1字节类型标签
- `TAG_ERR` (1): 错误代码和消息 - 1字节类型 + 4字节错误码 + 4字节消息长度 + 消息数据
- `TAG_STR` (2): 字符串类型 - 1字节类型 + 4字节长度 + 字符串数据
- `TAG_INT` (3): 64位整数 - 1字节类型 + 8字节整数值
- `TAG_DBL` (4): 双精度浮点数 - 1字节类型 + 8字节浮点值
- `TAG_ARR` (5): 数组类型 - 1字节类型 + 4字节数组长度 + 数组元素

## 核心架构

### 服务器架构
1. **主循环**: 使用WSAPoll进行多路复用
2. **连接管理**: 非阻塞accept和处理客户端连接  
3. **请求处理**: 解析协议格式，执行相应命令
4. **响应发送**: 构建响应消息并发送给客户端

### 数据结构设计

#### 环形缓冲区
```cpp
struct Ring_buf {
    std::vector<uint8_t> buf;  // 缓冲区
    size_t head;               // 读指针
    size_t tail;               // 写指针  
    size_t cap;                // 容量
    size_t status;             // 状态标志
};
```
- ✅ 零拷贝数据访问
- ✅ 高效的内存利用率
- ✅ 避免内存碎片
- ✅ 线程安全的数据交换

#### 自定义哈希表
```cpp
struct HNode {
    HNode *next;
    uint64_t hcode = 0;
};

struct HTab {
    HNode* *tab = nullptr;
    size_t mask = 0;
    size_t size = 0;
};

struct HMap {
    HTab newer;
    HTab older;
    size_t migrate_pos = 0;
};
```
- ✅ **渐进式rehash**: 避免一次性rehash导致的性能抖动
- ✅ **FNV哈希算法**: 提供良好的键分布性
- ✅ **内存效率**: 手动内存管理减少碎片

#### 有序集合 (ZSet)
```cpp
struct ZSet {
    AVLNode* root = nullptr; // 按(score, name)排序
    HMap hmap;               // 按name索引
};

struct ZNode {
    AVLNode tree;           // AVL树节点
    HNode hmap;             // 哈希表节点  
    double score = 0;
    size_t len = 0;
    char name[0];           // 灵活数组
};
```
- ✅ **双索引结构**: AVL树 + 哈希表双重索引
- ✅ **高效查询**: O(log N)时间复杂度操作
- ✅ **内存优化**: 灵活数组存储成员名称

## 性能优化

- **高并发**: 支持数千个并发连接，非阻塞I/O架构
- **低延迟**: WSAPoll多路复用减少等待时间
- **高吞吐**: 环形缓冲区零拷贝优化数据读写
- **内存高效**: 渐进式rehash避免内存碎片，灵活数组优化存储

## 版本演进

### 基础版本 (basic_1)
- **架构**: 基本的客户端-服务器模型，阻塞I/O
- **功能**: SET, GET, DEL, KEYS基础命令
- **性能**: Finished 1000 clients, success = 993, time = 2.81091s

### 优化版本 (baisc_2)
- **优化**: 非阻塞I/O + WSAPoll多路复用
- **内存**: 环形缓冲区零拷贝优化
- **性能**: Finished 1000 clients, success = 1000, time = 2.63774s

### 开发版本1 (dev_1)
- **数据结构**: 自定义哈希表替代std::map
- **算法**: 渐进式rehash + FNV哈希算法
- **性能**: Finished 1000 clients, success = 1000, time = 2.73649s

### 开发版本2 (dev_2)
- **协议**: 完整的TLV (Type-Length-Value)响应格式
- **数据类型**: 支持nil、错误、字符串、整数、浮点数、数组
- **扩展性**: 易于添加新的数据类型

### 开发版本3 (dev_3)
- **数据结构**: AVL平衡二叉搜索树实现
- **功能**: 插入、删除、查找、旋转、中序遍历、偏移查询
- **应用**: 为有序集合提供排序基础

### 开发版本4 (dev_4)
- **数据结构**: Redis-like有序集合(ZSET)
- **命令**: ZADD, ZREM, ZSCORE, ZQUERY完整实现
- **架构**: AVL树(按score,name排序) + 哈希表(按name索引)双索引
- **优化**: 灵活数组存储，O(log N)高效查询

## 依赖项

- C++11 或更高版本
- Windows Socket 2 (Winsock2)
- CMake 3.10+ (可选)

## 贡献指南

1. Fork 本项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启Pull Request

## 许可证

本项目采用 GPL-V3 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情

## 作者

AuroBreeze - [GitHub](https://github.com/AuroBreeze)

## 致谢

- [build-your-own-x](https://github.com/codecrafters-io/build-your-own-x)提供的教程
- Redis 项目提供的设计灵感
- Windows Socket API 文档
- C++标准库提供的强大功能
