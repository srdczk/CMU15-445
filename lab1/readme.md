### Lab 1

#### (1) 实现 Extendible Hash 数据结构

每一个桶有自己的depth, 还有一个总的depth, 当桶装不下, 则创建一个新桶, 将新以为为 1 的装入新桶, 并且将之前指向原桶的指针指向新桶。当桶的深度大于globalDepth时, 需要扩容。保证并发安全, 操作加锁。

#### (2) 实现一个 LRU Cache 
用hash双链表结构实现, 维护一个头节点和尾节点简化删除插入操作, 注意智能指针析构引用计数问题, 以及构造时默认为nullptr。

#### (3) 按照要求实现 bufferPoolManager
FetchPage -> 读取相应PageId 的页面, UnpinPage -> 当对Page 最修改时减少 Page的pinCount, 为 0 时, 放回replacer 待用, newPage -> 磁盘上申请一个新的 Page。