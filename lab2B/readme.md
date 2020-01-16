### lab2B 实现B+ 树的删除

删除具体步骤 -> 找到leaf 节点上的位置, 并且删除, 如果小于Minsize, 优先向左寻找节点合并, 如果不能合并(和Size大于MaxSize), 则向兄弟节点借一个array元素, 并且更新child和parent对应的指针。