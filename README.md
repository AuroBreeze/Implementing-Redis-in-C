# C++ Redis Implementation

一个用C++实现的Redis-like键值存储系统，支持基本的键值操作和网络通信。

## 功能特性

- ✅ **基本命令支持**: SET, GET, DEL, KEYS 操作
- ✅ **网络通信**: 基于Windows Socket API的客户端-服务器架构
- ✅ **高性能I/O**: 非阻塞I/O和多路复用 (WSAPoll)
- ✅ **内存优化**: 环形缓冲区 (Ring Buffer) 实现零拷贝优化
- ✅ **多客户端支持**: 支持多个客户端同时连接
- ✅ **自定义协议**: 二进制协议格式，前4字节表示消息长度
- ✅ **TLV响应格式**: Type-Length-Value二进制响应协议，支持多种数据类型
- ✅ **自定义哈希表**: 渐进式rehash的高性能哈希表实现
- ✅ **数据类型丰富**: 支持字符串、整数、浮点数、数组、nil、错误等多种数据类型

## 项目结构

```
.
├── client.cpp          # Redis客户端实现
├── servers.cpp         # Redis服务器实现（包含环形缓冲区优化）
├── server_test.cpp     # 服务器压力测试代码
├── test.cpp           # 环形缓冲区测试代码
├── hashtable.h        # 自定义哈希表头文件
├── hashtable.cpp      # 自定义哈希表实现
├── .gitignore         # Git忽略文件
└── study/             # 学习版本目录
    ├── basic_1/       # 基础版本实现
    ├── baisc_2/       # 优化版本实现（带环形缓冲区）
    ├── dev_1/         # 开发版本1（自定义哈希表）
    └── dev_2/         # 开发版本2（自定义TLV实现）
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
g++ -std=c++17 -o server src/server.cpp src/hashtable.cpp src/hashtable.h -lws2_32

# 编译客户端
g++ -std=c++17 -o client test/client.cpp -lws2_32

# 编译服务器测试程序
g++ -std=c++17 -o server_test test/server_test.cpp -lws2_32

# 编译环形缓冲区测试程序
g++ -std=c++17 -o test test/test.cpp
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

## 架构设计

### 服务器架构

1. **主循环**: 使用WSAPoll进行多路复用
2. **连接管理**: 非阻塞accept和处理客户端连接
3. **请求处理**: 解析协议格式，执行相应命令
4. **响应发送**: 构建响应消息并发送给客户端

### 环形缓冲区优化

```cpp
struct Ring_buf {
    std::vector<uint8_t> buf;  // 缓冲区
    size_t head;               // 读指针
    size_t tail;               // 写指针  
    size_t cap;                // 容量
    size_t status;             // 状态标志
};
```

环形缓冲区提供了：
- ✅ 零拷贝数据访问
- ✅ 高效的内存利用率
- ✅ 避免内存碎片
- ✅ 线程安全的数据交换

### 自定义哈希表设计

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

哈希表特性：
- ✅ **渐进式rehash**: 避免一次性rehash导致的性能抖动
- ✅ **FNV哈希算法**: 提供良好的键分布性
- ✅ **内存效率**: 手动内存管理减少碎片
- ✅ **线程安全**: 支持并发访问（需要外部同步）

## 性能特性

- **高并发**: 支持数千个并发连接
- **低延迟**: 非阻塞I/O减少等待时间
- **高吞吐**: 环形缓冲区优化数据读写
- **内存高效**: 避免不必要的内存拷贝

## 开发版本

### 基础版本 (basic_1)
- 基本的客户端-服务器架构
- 简单的阻塞I/O模型
- 基础命令实现
- Finished 1000 clients, success = 993, time = 2.81091s

### 优化版本 (baisc_2) 
- 非阻塞I/O和多路复用
- 环形缓冲区内存优化
- 性能提升和资源优化
- Finished 1000 clients, success = 1000, time = 2.63774s

### 开发版本 (dev_1)
- 自定义哈希表实现，替代std::map
- 渐进式rehash机制，避免性能抖动
- FNV哈希算法，提供更好的分布性
- 内存管理优化，减少内存碎片
- Finished 1000 clients, success = 1000, time = 2.73649s

### 开发版本2 (dev_2)
- **TLV (Type-Length-Value) 协议格式**: 实现了完整的二进制TLV响应格式
- **多种数据类型支持**:
  - `TAG_NIL` (0): nil值
  - `TAG_ERR` (1): 错误代码和消息
  - `TAG_STR` (2): 字符串类型
  - `TAG_INT` (3): 64位整数
  - `TAG_DBL` (4): 双精度浮点数
  - `TAG_ARR` (5): 数组类型
- **优化的响应序列化**: 使用TLV格式替代简单的状态码+数据格式
- **二进制协议兼容性**: 支持更复杂的数据结构和类型系统
- **扩展性**: 易于添加新的数据类型和协议功能

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
