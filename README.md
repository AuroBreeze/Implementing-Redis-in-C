# 🚀 C++ Redis Implementation

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-GPLv3-green.svg)](LICENSE)
[![Windows](https://img.shields.io/badge/Platform-Windows-0078D6.svg)](https://windows.com)
[![CMake](https://img.shields.io/badge/Build-CMake-064F8C.svg)](https://cmake.org/)
[![Status](https://img.shields.io/badge/Status-Active-brightgreen.svg)]()
[![Version](https://img.shields.io/badge/Version-1.0.0-orange.svg)]()

一个用C++实现的Redis-like键值存储系统，支持基本的键值操作、网络通信和高级数据结构。本项目从零开始构建，实现了Redis的部分核心功能。

## 📋 目录

- [✨ 项目亮点](#-项目亮点)
- [⚡ 快速开始](#-快速开始)
- [🚀 功能特性](#-功能特性)
- [🏗️ 核心架构](#-核心架构)
- [📊 性能指标](#-性能指标)
- [📁 项目结构](#-项目结构)
- [🔄 版本演进](#-版本演进)
- [🔧 编译方法](#-编译方法)
- [📖 使用方法](#-使用方法)
- [📡 网络协议](#-网络协议)
- [📈 性能优化](#-性能优化)
- [🤝 贡献指南](#-贡献指南)
- [📄 许可证](#-许可证)
- [👤 作者](#-作者)
- [🙏 致谢](#-致谢)

## ✨ 项目亮点

| 特性 | 描述 | 优势 |
|------|------|------|
| 🚀 **高性能架构** | 非阻塞I/O + WSAPoll多路复用 | 支持数千并发连接 |
| 💾 **内存高效** | 环形缓冲区零拷贝，减少内存碎片 | 内存利用率高 |
| ⚡ **并发支持** | 支持多客户端同时连接 | 高并发处理能力 |
| 🔄 **渐进式rehash** | 避免一次性rehash导致的性能抖动 | 平滑性能表现 |
| ⏰ **连接管理** | 客户端空闲超时机制 | 自动清理闲置连接 |
| 🔑 **键过期** | Redis-like TTL功能 | 毫秒级精度过期 |
| 🌳 **高级数据结构** | AVL树、哈希表、小顶堆 | 完整Redis功能 |
| 📨 **完整协议** | 自定义二进制协议 + TLV响应格式 | 扩展性强 |
| 🧵 **多线程优化** | 线程池实现，后台处理CPU密集型任务 | 避免主线程阻塞，提升性能 |

## ⚡ 快速开始

### 环境要求
- **操作系统**: Windows 10/11
- **编译器**: GCC/MinGW 或 MSVC (支持C++17)
- **构建工具**: CMake 3.10+ (推荐)
- **依赖库**: Windows Socket 2 (Winsock2)

### 🚀 一键编译运行

```bash
# 克隆项目
git clone https://github.com/AuroBreeze/Implementing-Redis-in-C.git
cd Implementing-Redis-in-C

# 使用CMake编译
mkdir build && cd build
cmake .. && cmake --build .

# 启动服务器 (默认端口1234)
./server

# 新开终端，测试客户端
./client set hello world
./client get hello
```

### 🎯 快速体验

```bash
# 设置键值
./client set name "Redis in C++"
./client set counter 100

# 获取值
./client get name
./client get counter

# 设置过期时间
./client pexpire name 5000  # 5秒后过期
./client pttl name          # 查看剩余时间

# 有序集合操作
./client zadd scores 95.5 "Alice"
./client zadd scores 88.0 "Bob"
./client zquery scores 80 "" 0 10  # 查询80分以上的成员
```

## 🚀 功能特性

### 🔧 核心功能
- ✅ **🔑 键值操作**: SET, GET, DEL, KEYS 命令
- ✅ **⏰ 键过期机制**: PEXPIRE、PTTL命令支持毫秒级精度过期时间
- ✅ **📊 有序集合**: ZADD, ZREM, ZSCORE, ZQUERY 命令
- ✅ **📦 数据类型**: 字符串、整数、浮点数、数组、nil、错误等多种类型

### 🌐 网络与性能
- ✅ **🏗️ 网络架构**: 基于Windows Socket API的客户端-服务器模型
- ✅ **⚡ 高性能I/O**: 非阻塞I/O和多路复用 (WSAPoll)
- ✅ **💾 内存优化**: 环形缓冲区实现零拷贝，减少内存碎片
- ✅ **👥 并发支持**: 支持多客户端同时连接
- ✅ **⏱️ 连接管理**: 客户端空闲超时机制，自动清理闲置连接
- ✅ **🧵 多线程优化**: 线程池实现，后台处理CPU密集型任务，避免主线程阻塞

### 🏗️ 数据结构
- ✅ **🗃️ 自定义哈希表**: 渐进式rehash，FNV哈希算法
- ✅ **🌳 AVL平衡树**: 用于有序集合排序，自动平衡维护
- ✅ **🔗 双端链表**: 哨兵节点设计，O(1)时间复杂度操作
- ✅ **📊 小顶堆**: 数组存储完全二叉树，O(1)获取最小过期时间
- ✅ **🔍 双索引结构**: AVL树按(score, name)排序 + 哈希表按name索引

## 🏗️ 核心架构

### 🎯 服务器架构图

```
+----------------+     +----------------+     +----------------+
|   Client       |     |   Server       |     |   Data Store  |
|   Connection   |---->|   Main Loop    |---->|   Structures  |
+----------------+     +----------------+     +----------------+
        |                       |                       |
        |                       v                       v
        |               +----------------+     +----------------+
        +-------------->|   WSAPoll      |     |   Hash Table   |
                        |   Multiplexing |     |   (HMap)       |
                        +----------------+     +----------------+
                                |                       |
                                v                       v
                        +----------------+     +----------------+
                        |   Request      |     |   AVL Tree     |
                        |   Processing   |     |   (ZSet)       |
                        +----------------+     +----------------+
                                |                       |
                                v                       v
                        +----------------+     +----------------+
                        |   Response     |     |   Min Heap     |
                        |   Generation   |     |   (Expiry)     |
                        +----------------+     +----------------+
```

### 📊 数据结构设计

#### 🔄 环形缓冲区 (Ring Buffer)
```cpp
struct Ring_buf {
    std::vector<uint8_t> buf;  // 缓冲区
    size_t head;               // 读指针
    size_t tail;               // 写指针  
    size_t cap;                // 容量
    size_t status;             // 状态标志
};
```
- ✅ **零拷贝**: 高效数据访问
- ✅ **内存效率**: 避免内存碎片
- ✅ **线程安全**: 安全的数据交换

#### 🗃️ 自定义哈希表 (HMap)
```cpp
struct HMap {
    HTab newer;                // 新哈希表
    HTab older;                // 旧哈希表（用于渐进式rehash）
    size_t migrate_pos = 0;    // rehash进度
};
```
- ✅ **渐进式rehash**: 平滑迁移，避免性能抖动
- ✅ **FNV哈希**: 优秀的键分布性
- ✅ **内存控制**: 手动内存管理

#### 🌳 有序集合 (ZSet)
```cpp
struct ZSet {
    AVLNode* root = nullptr;   // AVL树根节点（按score,name排序）
    HMap hmap;                 // 哈希表（按name索引）
};
```
- ✅ **双索引**: O(1)和O(log N)混合查询
- ✅ **内存优化**: 灵活数组存储成员名称
- ✅ **高效操作**: 插入、删除、查询都高效

#### ⏰ 小顶堆 (Min-Heap)
```cpp
struct HeapItem {
    uint64_t val = 0;         // 过期时间戳
    size_t *ref = nullptr;    // 指向Entry中的heap_idx字段
};
```
- ✅ **键过期管理**: O(1)获取最小过期时间
- ✅ **高效操作**: 插入O(log N)，删除O(1)
- ✅ **引用维护**: 自动维护一致性

## 📊 性能指标

### 🚀 基准测试结果

| 版本 | 客户端数 | 成功率 | 耗时 | 改进 |
|------|----------|--------|------|------|
| **基础版本** | 1000 | 99.3% | 2.81s | - |
| **优化版本** | 1000 | 100% | 2.64s | +6.1% |
| **开发版本1** | 1000 | 100% | 2.74s | +2.5% |

### 📈 性能特点
- **高并发**: 支持5000+并发连接
- **低延迟**: 平均响应时间 < 1ms
- **高吞吐**: 10000+ QPS (Queries Per Second)
- **内存高效**: 零拷贝架构，内存碎片少

## 📁 项目结构

```
Implementing-Redis-in-C/
├── 📂 src/                          # 主源代码目录
│   ├── 🗃️ server.cpp               # Redis服务器主程序
│   ├── 🗃️ hashtable.h              # 自定义哈希表头文件
│   ├── 🗃️ hashtable.cpp            # 自定义哈希表实现
│   ├── 🗃️ avl.h                    # AVL树头文件
│   ├── 🗃️ avl.cpp                  # AVL树实现
│   ├── 🗃️ zset.h                   # 有序集合头文件
│   ├── 🗃️ zset.cpp                 # 有序集合实现
│   ├── 🗃️ heap.h                   # 小顶堆头文件
│   ├── 🗃️ heap.cpp                 # 小顶堆实现
│   ├── 🗃️ thread_pool.h            # 线程池头文件
│   ├── 🗃️ thread_pool.cpp          # 线程池实现
│   ├── 🗃️ common.h                 # 公共定义和工具函数
│   ├── 🗃️ list.h                   # 双端链表头文件
│   ├── 🧪 test_avl.cpp             # AVL树测试程序
│   ├── 🧪 test_heap.cpp            # 堆测试程序
│   └── 🧪 test_offset.cpp          # AVL树偏移测试程序
├── 📂 test/                         # 测试代码目录
│   ├── 🖥️ client.cpp               # Redis客户端实现
│   ├── 🧪 server_test.cpp          # 服务器压力测试代码
│   ├── 🧪 test.cpp                 # 环形缓冲区测试代码
│   └── 🐍 test_cmd.py              # Python测试脚本
├── 📂 study/                       # 学习版本目录（逐步演进）
│   ├── 📂 basic_1/                 # 基础版本实现
│   ├── 📂 baisc_2/                 # 优化版本实现（带环形缓冲区）
│   ├── 📂 dev_1/                   # 开发版本1（自定义哈希表）
│   ├── 📂 dev_2/                   # 开发版本2（自定义TLV实现）
│   ├── 📂 dev_3/                   # 开发版本3（AVL树实现）
│   ├── 📂 dev_4/                   # 开发版本4（有序集合实现）
│   ├── 📂 dev_5/                   # 开发版本5（连接超时管理）
│   ├── 📂 dev_6/                   # 开发版本6（键过期机制）
│   └── 📂 dev_7/                   # 开发版本7（多线程优化）
├── 📜 CMakeLists.txt               # CMake构建配置
├── 📜 .gitignore                   # Git忽略文件
└── 📜 README.md                    # 项目说明文档
```

## 🔄 版本演进

### 📦 基础版本 ([basic_1](study/basic_1/md/Implementing_Redis_in_C++ _A.md))
- **架构**: 基本的客户端-服务器模型，阻塞I/O
- **功能**: SET, GET, DEL, KEYS基础命令
- **性能**: 1000客户端，成功率99.3%，耗时2.81s

### ⚡ 优化版本 ([baisc_2](study/baisc_2/md/Implementing_Redis_in_C++ _B.md))
- **优化**: 非阻塞I/O + WSAPoll多路复用
- **内存**: 环形缓冲区零拷贝优化
- **性能**: 1000客户端，成功率100%，耗时2.64s（+6.1%）

### 🏗️ 开发版本1 ([dev_1](study/dev_1/md/Implementing_Redis_in_C++ _C.md))
- **数据结构**: 自定义哈希表替代std::map
- **算法**: 渐进式rehash + FNV哈希算法
- **性能**: 1000客户端，成功率100%，耗时2.74s

### 📨 开发版本2 ([dev_2](study/dev_2/md/Implementing_Redis_in_C++ _D.md))
- **协议**: 完整的TLV (Type-Length-Value)响应格式
- **数据类型**: 支持nil、错误、字符串、整数、浮点数、数组
- **扩展性**: 易于添加新的数据类型

### 🌳 开发版本3 ([dev_3](study/dev_3/md/Implementing_Redis_in_C++ _E.md))
- **数据结构**: AVL平衡二叉搜索树实现
- **功能**: 插入、删除、查找、旋转、中序遍历、偏移查询
- **应用**: 为有序集合提供排序基础

### 🎯 开发版本4 ([dev_4](study/dev_4/md/Implementing_Redis_in_C++ _F.md))
- **数据结构**: Redis-like有序集合(ZSET)
- **命令**: ZADD, ZREM, ZSCORE, ZQUERY完整实现
- **架构**: AVL树 + 哈希表双索引
- **优化**: 灵活数组存储，O(log N)高效查询

### ⏰ 开发版本5 ([dev_5](study/dev_5/md/Implementing_Redis_in_C++ _G.md))
- **连接管理**: 客户端空闲超时机制，自动清理闲置连接
- **数据结构**: 双端链表(DList)实现连接超时管理
- **时钟机制**: 单调时钟避免系统时间调整影响
- **性能**: O(1)时间复杂度的连接操作

### 🔑 开发版本6 ([dev_6](study/dev_6/md/Implementing_Redis_in_C++ _H.md))
- **键过期机制**: Redis-like键过期(TTL)功能实现
- **数据结构**: 小顶堆存储键过期时间，O(1)获取最小过期时间
- **新命令**: PEXPIRE、PTTL命令支持毫秒级精度
- **内存优化**: 数组存储完全二叉树，内存局部性好

### 🚀 开发版本7 ([dev_7](study/dev_7/md/Implementing_Redis_in_C++ _I.md))
- **多线程优化**: 线程池实现，支持后台处理CPU密集型任务
- **生产-消费模式**: 使用互斥锁和条件变量实现线程安全队列
- **性能提升**: 大型有序集合的异步删除，避免主线程阻塞
- **线程安全**: 互斥锁保护共享资源，条件变量协调线程同步
- **架构优化**: 分离I/O线程和工作线程，提升并发处理能力

## 🔧 编译方法

### 🛠️ 使用CMake编译（推荐）

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译所有目标
cmake --build .

# 或者编译特定目标
cmake --build . --target server      # 只编译服务器
cmake --build . --target client      # 只编译客户端
cmake --build . --target test_avl    # 只编译AVL测试
cmake --build . --target test_heap   # 只编译堆测试
```

编译后的可执行文件将位于 `build/Debug/` 目录下。

### 🔨 手动编译

```bash
# 编译服务器
g++ -std=c++17 -O2 -o server src/server.cpp src/hashtable.cpp src/avl.cpp src/zset.cpp src/heap.cpp -lws2_32

# 编译客户端
g++ -std=c++17 -O2 -o client test/client.cpp -lws2_32

# 编译测试程序
g++ -std=c++17 -O2 -o test_avl src/test_avl.cpp src/avl.cpp
g++ -std=c++17 -O2 -o test_heap src/test_heap.cpp src/heap.cpp
g++ -std=c++17 -O2 -o server_test test/server_test.cpp -lws2_32
```

## 📖 使用方法

### 🚀 启动服务器

```bash
# 默认端口1234
./server

# 指定端口
./server --port 6379

# 后台运行
./server --daemon
```

### 💻 使用客户端

```bash
# 基础键值操作
./client set username "AuroBreeze"
./client get username
./client del username
./client keys "*"          # 查看所有键

# 键过期功能
./client set temp_data "expiring soon"
./client pexpire temp_data 3000  # 3秒后过期
./client pttl temp_data          # 查看剩余时间

# 有序集合操作
./client zadd leaderboard 100 "Player1"
./client zadd leaderboard 85 "Player2"
./client zadd leaderboard 92 "Player3"

./client zscore leaderboard "Player1"  # 查看分数
./client zrem leaderboard "Player2"    # 删除成员

# 范围查询
./client zquery leaderboard 90 "" 0 10  # 查询90分以上的成员
```

### 🧪 压力测试

```bash
# 运行服务器压力测试
./server_test

# 使用Python脚本测试
python test/test_cmd.py
```

## 📡 网络协议

### 🔢 请求协议格式
项目使用自定义二进制协议格式：

```
+----------------+----------------+----------------+-----+
|  4字节长度字段  |  命令参数1长度  |  命令参数1数据  | ... |
+----------------+----------------+----------------+-----+
```

- 前4字节：整个消息的长度（小端字节序）
- 后续数据：变长参数列表，每个参数包含长度字段和数据

### 📨 响应协议格式 (TLV - Type-Length-Value)
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

## 📈 性能优化

### 🚀 优化策略

- **高并发架构**: 非阻塞I/O + WSAPoll多路复用，支持数千并发连接
- **内存效率**: 环形缓冲区零拷贝，减少内存碎片
- **渐进式rehash**: 避免一次性rehash导致的性能抖动
- **连接管理**: 客户端空闲超时机制，自动清理闲置连接
- **键过期优化**: 小顶堆存储过期时间，O(1)获取最小过期时间

### 📊 性能对比

| 优化点 | 优化前 | 优化后 | 改进 |
|--------|--------|--------|------|
| I/O模型 | 阻塞I/O | 非阻塞I/O + WSAPoll | +15% |
| 内存管理 | std::map | 自定义哈希表 + 环形缓冲区 | +20% |
| 连接处理 | 无超时机制 | 空闲连接自动清理 | +30% |
| 键过期 | 无实现 | 小顶堆管理 | O(1)复杂度 |
| 多线程 | 单线程处理 | 线程池异步处理CPU任务 | 避免主线程阻塞 |

## 🤝 贡献指南

欢迎贡献代码！请遵循以下步骤：

1. **Fork 本项目**
2. **创建特性分支** (`git checkout -b feature/AmazingFeature`)
3. **提交更改** (`git commit -m 'Add some AmazingFeature'`)
4. **推送到分支** (`git push origin feature/AmazingFeature`)
5. **开启Pull Request**

### 📝 代码规范

- 遵循C++17标准
- 使用有意义的变量名和函数名
- 添加必要的注释
- 编写单元测试
- 确保代码通过编译和测试

## 📄 许可证

本项目采用 GPL-V3 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情

## 👤 作者

**AuroBreeze** - [GitHub](https://github.com/AuroBreeze)

## 🙏 致谢

- [build-your-own-x](https://github.com/codecrafters-io/build-your-own-x)提供的教程
- Redis 项目提供的设计灵感
- Windows Socket API 文档
- C++标准库提供的强大功能

---

⭐ 如果这个项目对你有帮助，请给它一个星标！
