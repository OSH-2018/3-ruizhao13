# In memory file system

## 基本数据

number of blocks: `blocknr` = 64 * 1024

`blocksize` = 64*1024 Byte

max number of files :`inodenr` = 13*64

Maximum file size : INDEX_NUMBER_MAX * `blocksize`

​	the INDEX_NUMBER_MAX is defined as 128, so the maximum file size is 8MB

## 实现思路

把前16个block放信息，后面的64*1024 - 16个block放data

第二块block：放`inode_map` :记录哪些文件节点被占用了

第三块block：放`data_map` :记录哪些mem的块被占用了

```c
static bool *inode_map;//13*64 inodes most
static bool *data_map;//[64*1024 - 16] for most
```

第四块到第16块：放`inode` ： `inode`是结构体，如下

```c
struct filenode {
    char filename[256];
    int32_t content[INDEX_NUMBER_MAX];
    struct stat st;
    //struct filenode *next;
};
```

其中`filename`存放文件名，假定文件名不超过256个字符

`content`数组存放指针，指向这个文件的对应的data块，所以`content`的大小和`blocksize`决定了该文件系统最大支持的文件大小。

> 改进方案：Multi-Level Index， 可以用来支持更大的文件




