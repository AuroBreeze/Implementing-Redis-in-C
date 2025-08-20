# Implementing Redis in C++ : C

## 前言

本章代码及思路均来自[Build Your Own Redis with C/C++](https://build-your-own.org/redis/)

本文章只阐述我的理解想法，以及需要注意的地方。

本文章为续<<[Implementing Redis in C++ : B](https://blog.csdn.net/m0_58288142/article/details/150531872?fromshare=blogdetail&sharetype=blogdetail&sharerId=150531872&sharerefer=PC&sharesource=m0_58288142&sharefrom=from_link)>>所以本文章不再完整讲解全部代码，只讲解其不同的地方

## 完成优化后的代码

- 自定义哈希表实现，替代std::map
- 渐进式rehash机制，避免性能抖动
- FNV哈希算法，提供更好的分布性
- 内存管理优化，减少内存碎片
- Finished 1000 clients, success = 1000, time = 2.73649s

## socket(server)

### 主体思路

本文章在延续上文的非阻塞性能提升的网络连接的代码上，修改键值对存储从`map`修改为**自定义的`HMap`**。

大家或许都知道**哈希表**的功能，哈希表拥有O(1)的时间复杂度，在网络连接中，进行键值对存储时，哈希表会比`map`性能高很多。

原文作者，为优化存储性能，将`map`修改为**自定义的`HMap`**，并使用**自定义的`HMap`**进行键值对存储。

> 你或许可能会问，为什么不使用`STL`中的<unordered_map>或者<unordered_set> ?
> 原文作者提到了**侵入式**和**非侵入式**的数据结构，而我们通常使用的`STL`库中的数据结构，是**非侵入式**的，这种**非侵入式**的数据机构通常会有**额外**的内存分配**开销**，当然优点就是，泛用性强。
> 而对于**侵入式**的数据结构会减少**额外**的开销，但是**缺点**就是，泛用性弱，自己需要修改指针等细节。

区分**侵入式**和**非侵入式**的数据结构可能就需要读者自己具体了解了。

对于本文的**侵入式**的链式哈希表，特点就是，容器中内置**节点**，需我们自己维护节点。

我们要实现的也就是`hashtable`的那几个功能，**增**(set) **删**(del) **改**(set) **查**(get)，也就是前几章我们实现的`servers`的功能，同时我们还要注意`hashtable`的**扩容**的问题，在本文处理**扩容**时，选择同时管理两个`hashtable`，具体我们后面在讨论。

在本文中，使用的是链式的`hashtable`，所以我们也会用到**链表**。

### HNode{}, HTable{}

```cpp
struct HNode{
    HNode* next;
    uint64_t hcode = 0;
};

struct HTab{
    HNode* *tab = nullptr;
    size_t mask = 0;
    size_t size = 0;
};
```

`HNode`结构体表示一个节点,同时存储**被`hash`的内容**

`HTable`结构体表示一个哈希表，在这个结构体中我们使用了**指向指针的指针**，这个指针的指针，指向的就是一个**链表**的头结点。

这两个结构体的关系：

`HNode`作为一个节点，是需要往`HTab`这个表中添加的，所以当新的数据被**哈希**放入`hcode`后，`HNode`中的`next`就会指向`HTab`中`tab[pos]`(pos:位置)所指向的数据，这之后，`tab[pos]`指向刚刚的`HNode`，形成**链**,所以越**后**来的数据，在链表中就越考**前**

我们可以通过下图来描述这两个个结构体：

```cpp
HTab
+------------------+
| tab ------------+-------> tab[ 0 ] --> (HNode*) --> [hcode=42] -> [hcode=99] -> nullptr
| mask = 3        |          tab[ 1 ] --> (HNode*) --> [hcode=123] -> nullptr
| size = 3        |          tab[ 2 ] --> (HNode*) --> nullptr
+------------------+          tab[ 3 ] --> (HNode*) --> [hcode=77] -> nullptr
```

除了这两个结构体的关系外，`HTab`结构体还维护了一个`size`成员变量，用于记录哈希表中的元素个数，还有一个`mask`用来计算哈希，这个我们后面讲解。

### h_insert()

```cpp
static void h_insert(HTab* htab,HNode* node){
    size_t pos = node->hcode & htab->mask;
    HNode* next = htab->tab[pos];
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}
```

函数功能：将节点插入到哈希表中。(所以需要两个参数，想要插入的哈希表和节点)

这里的代码的思路我们上面已经讲过了，大家看一下思考一下就可以跳过了，我们主要来说一下`mask`这个参数，`mask`参数是为了计算我们的哈希表的索引。

在上面我们已经说过了`hcode`是被`hash`后的内容,我们通常来说计算哈希表的索引是使用**pose = hcode % capacity**，但是**CPU**处理**模运算**的效率比**位运算**慢，所以我们这次考虑了**位运算**来优化这个过程。

我们通过使 **`mask`是(2的幂次方-1)** 来使用`&`进行位运算 **(取低n位全1)**，获取到哈希的索引，假如我们要计算**37 & 7(2^3-1) = 5**

> 关于为什么`mask`必须是(2的幂次方-1)，假设2^3 = 8，那么转化为二进制就是：1000，mask = 7,，转化为二进制后：0111，只有这样才能取**低n位全1**

```
37 = 100101
 7 = 000111

100101
000111
------
000101   (二进制) = 5 (十进制)
```

这样我们就完成了索引的计算的优化了。

### h_init() 

```cpp
static void h_init(HTab* htab, size_t n){ // n must be power of 2
    assert(n > 0 && ((n-1) & n) == 0); // n must be power of 2
    htab->tab = (HNode* *)calloc(n,sizeof(HNode* ));
    htab->mask = n-1;
    htab->size = 0;
}
```

这个函数用于初始化哈希表。

在这个初始化函数中，我们使用了`callco`进行内存的分配(没有使用`malloc`)，为什么？

使用`calloc`会分配**内存 + 清零**(把每个字节都设成 0)，而使用`malloc`只会分配内存，不会清零，里面会有未定义的垃圾值，一旦后续逻辑认为它是有效指针并解引用，就会**段错误(Segfault)**。

> 有意思的是，使用`calloc`和使用`malloc`都是在第一次访问该内存的时候才会分配对应的内存。同时`calloc`可能会有优于`malloc`的写入，这可能需要更深入的知识，目前我的知识尚浅不知。

### h_lookup(), h_detach()

```cpp
static HNode* *h_lookup(HTab* htab, HNode* key, bool (*eq)(HNode* , HNode*)){
    if(!htab->tab) return nullptr;
    size_t pos = key->hcode & htab->mask;
    HNode* *from = &htab->tab[pos];
    for(HNode* cur;(cur = *from) != nullptr;from = &cur->next){
        if(cur->hcode == key->hcode && eq(cur,key)) return from;
    }
    return nullptr;
}

//remove a node from chain
static HNode* h_detach(HTab* htab, HNode* *from){
    HNode* node = *from;
    *from = node->next;
    htab->size--;
    return node;
}
```

首先看第一个函数以一个函数的返回值的定义是(HNode**)，从对应的`HTab`表中查找`HNode`中的哈希内容(key)，形参中的`HNode* key`的意思就是要找专门找`key`，而形参中的`bool (*eq)(HNode*, HNode*)`,翻译成人话:`eq`是一个函数指针，指向一个函数，这个函数接受两个`HNode*`参数，返回一个`bool`值。

