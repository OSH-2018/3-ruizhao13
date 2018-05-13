/*
 * Copyright Â© 2018 Zitian Li <ztlizitian@gmail.com>
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */
#define _OSH_FS_VERSION 2018051000
#define FUSE_USE_VERSION 26
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fuse.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdbool.h>

#define INDEX_NUMBER_MAX 128

int inode_num = 13*64;
int data_num = 64*1024 - 16;

struct filenode {
    char filename[256];
    int32_t content[INDEX_NUMBER_MAX];
    struct stat st;
    //struct filenode *next;
};

static const size_t size = 4 * 1024 * 1024 * (size_t)1024;
static void *mem[64 * 1024];
size_t blocknr = sizeof(mem) / sizeof(mem[0]);
size_t blocksize = 64*1024;
static struct filenode *inode;//[13 * 64]

//bitmap: true for used while false for unused
static bool *inode_map;//13*64 inodes most
static bool *data_map;//[64*1024 - 16] for most

static struct filenode *get_filenode(const char *name)
{
    //struct filenode *node = root;
    for(int i = 0; i < inode_num ;i++){
        if(inode_map[i]){
            if(strcmp(inode[i].filename, name + 1)==0){
                return (inode + i);
            }
        }
    }
    return NULL;
    /*
    while(node) {
        if(strcmp(node->filename, name + 1) != 0)
            node = node->next;
        else
            return node;
    }
    return NULL;
    */
}


static void create_filenode(const char *filename, const struct stat *st)
{
    int new_inode = -1;
    for(int i = 0; i< inode_num; i++){
        if(!inode_map[i]){
            new_inode = i;
            inode_map[i] = 1;
            break;
        }
    }
    /*
    struct filenode *new = (struct filenode *)malloc(sizeof(struct filenode));
    new->filename = (char *)malloc(strlen(filename) + 1);
    memcpy(new->filename, filename, strlen(filename) + 1);
    //new.st = (struct stat *)malloc(sizeof(struct stat));
    memcpy(new.st, st, sizeof(struct stat));
    new->next = root;
    new->content = NULL;
    root = new;
     */

    memcpy(inode[new_inode].filename, filename, strlen(filename) + 1);
    memcpy(&(inode[new_inode].st), st, sizeof(struct stat));
    memset(inode[new_inode].content, 0, INDEX_NUMBER_MAX * sizeof(int32_t));
}


static void *oshfs_init(struct fuse_conn_info *conn)
{
    size_t blocknr = sizeof(mem) / sizeof(mem[0]);
    size_t blocksize = size / blocknr;
    size_t inodenr = sizeof(inode) / sizeof(inode[0]);
    // Demo 1
    for(int i = 0; i < blocknr; i++) {
        mem[i] = mmap(NULL, blocksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        memset(mem[i], 0, blocksize);
    }
    for(int i = 3; i < blocknr; i++) {
        munmap(mem[i], blocksize);
    }
    
    inode_map = (bool*)mem[1];//most for 13*64 inodes
    data_map = (bool*)mem[2];//most for 64*1024 - 16 data blocks
    memset(mem[0], 0, blocksize);
    memset(mem[1], 0, 13*64*sizeof(bool));
    memset(mem[2], 0, (64*1024-16)*sizeof(bool));
    mem[3] = mmap(NULL, 13 * blocksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (int i = 3; i< 16; i++){
        mem[i] = (char*)mem[3] + blocksize * (i-3);
    }
    inode = (struct filenode *)mem[3];
    memset(mem[3], 0, (13*64)*sizeof(blocksize));
    for (int i = 0;i<16;i++){
        data_map[i] = 1;
    }

 /*   // Demo 2
    mem[0] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for(int i = 0; i < blocknr; i++) {
        mem[i] = (char *)mem[0] + blocksize * i;
        memset(mem[i], 0, blocksize);
    }
    for(int i = 0; i < blocknr; i++) {
        munmap(mem[i], blocksize);
    }
*/
    return NULL;
}

static int oshfs_getattr(const char *path, struct stat *stbuf)
{
    int ret = 0;
    struct filenode *node = get_filenode(path);
    if(strcmp(path, "/") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0755;
    } else if(node) {
        memcpy(stbuf, &(node->st), sizeof(struct stat));
    } else {
        ret = -ENOENT;
    }
    return ret;
}

static int oshfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    //struct filenode *node = root;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    for(int i = 0; i < inode_num; i++){
        if(inode_map[i]){
            filler(buf, inode[i].filename, &(inode[i].st), 0);
        }
    }
    /*
    while(node) {
        filler(buf, node->filename, node->st, 0);
        node = node->next;
    }
    */
    return 0;
}

static int oshfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int flag =1;
    for(int i = 0;i<inode_num;i++){
        if(inode_map[i]==0){
            flag = 0;
        }
    }
    if(flag){
        return -errno;
    }
    struct stat st;
    st.st_mode = S_IFREG | 0644;
    st.st_uid = fuse_get_context()->uid;
    st.st_gid = fuse_get_context()->gid;
    st.st_nlink = 1;
    st.st_size = 0;
    create_filenode(path + 1, &st);
    return 0;
}

static int oshfs_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static int oshfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    //struct filenode *node = get_filenode(path);
    struct filenode *node = NULL;
    int k = -1;
    for(int i = 0; i < inode_num ;i++){
        if(inode_map[i]){
            if(strcmp(inode[i].filename, path + 1)==0){
                node =  (inode + i);
                k = i;
            }
        }
    }
    if(  (!node)  || (k == -1)  ){
        return -1;
    }




    //node->content = realloc(node->content, node->st->st_size);

    size_t block_used_num = node->st.st_size % blocksize ? (node->st.st_size / blocksize + 1) : (node->st.st_size / blocksize);
    size_t new_block_used_num = block_used_num;
    //change the size of the file
    if(offset + size > node->st.st_size){
        node->st.st_size = offset + size;
    }
    //change the block numbers used by the file
    if(offset + size > block_used_num * blocksize){
        //file need more blocks
        new_block_used_num = node->st.st_size % blocksize ? (node->st.st_size / blocksize + 1) : (node->st.st_size / blocksize);
        for(size_t i = block_used_num; i<new_block_used_num; i++){
            bool is_mem_full = 1;
            for(int j = 16;j <blocknr; j++){

                if(data_map[j] == 0){
                    is_mem_full = 0;
                    mem[j] = mmap(NULL, blocksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    data_map[j] = 1;
                    node->content[i] = j;
                    break;
                }
            }
            if(is_mem_full){
                return -errno;
            }
        }
    }

    //memcpy(node->content + offset, buf, size);
    size_t blk1 = offset / blocksize;
    size_t off1 = offset % blocksize;
    size_t blk2 = (offset + size) /blocksize;
    size_t off2 = (offset + size) %blocksize;
    if(blk1 == blk2){
        memcpy(mem[node->content[blk1]]+off1,buf, size);
        return (int)size;
    }else{
        memcpy(mem[node->content[blk1]]+off1,buf, blocksize- off1);
        for(size_t i = blk1+1;i<blk2;i++){
            memcpy(mem[node->content[i]], buf+blocksize-off1 + (i-blk1-1)*blocksize, blocksize);

        }
        memcpy(mem[node->content[blk2]], buf+blocksize-off1 + (blk2-blk1-1)*blocksize, off2);
        return (int)size;
    }

    return (int)size;
}

static int oshfs_truncate(const char *path, off_t size)
{
    //struct filenode *node = get_filenode(path);
    struct filenode *node = NULL;
    int k = -1;
    for(int i = 0; i < inode_num ;i++){
        if(inode_map[i]){
            if(strcmp(inode[i].filename, path + 1)==0){
                node =  (inode + i);
                k = i;
            }
        }
    }
    if(  (!node)  || (k == -1)  ){
        return -1;
    }




    size_t block_used_num = node->st.st_size % blocksize ? (node->st.st_size / blocksize + 1) : (node->st.st_size / blocksize);
    size_t new_block_used_num = block_used_num;




    //change the size of the file

    node->st.st_size = size;

    //change the block numbers used by the file
    if(size > block_used_num * blocksize){
        //file need more blocks
        new_block_used_num = node->st.st_size % blocksize ? (node->st.st_size / blocksize + 1) : (node->st.st_size / blocksize);
        for(size_t i = block_used_num; i<new_block_used_num; i++){
            bool is_mem_full = 1;
            for(int j = 16;j <blocknr; j++){

                if(data_map[j] == 0){
                    is_mem_full = 0;
                    mem[j] = mmap(NULL, blocksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    data_map[j] = 1;
                    node->content[i] = j;
                    break;
                }
            }
            if(is_mem_full){
                return -errno;
            }
        }
    }
    else if(size <= (block_used_num - 1) * blocksize ){
        //file don't need less blocks
        new_block_used_num = node->st.st_size % blocksize ? (node->st.st_size / blocksize + 1) : (node->st.st_size / blocksize);
        for(size_t i = new_block_used_num; i< block_used_num; i++){
            data_map[node->content[i]] = 0;

            munmap(mem[node->content[i]], blocksize);
        }



    }else{
        //file don't have to chage the numbers of the blocks
        //so we don't have to do anything
        ;



    }


    //node->content = realloc(node->content, size);
    return 0;
}

static int oshfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    struct filenode *node = get_filenode(path);
    size_t ret = size;
    if(offset + size > node->st.st_size)
        ret = node->st.st_size - offset;

    size_t blk1 = offset / blocksize;
    size_t off1 = offset % blocksize;
    size_t blk2 = (offset + ret) /blocksize;
    size_t off2 = (offset + ret) %blocksize;
    if(blk1 == blk2){
        memcpy(buf, mem[node->content[blk1]]+off1,ret);
        return ret;
    }else{
        memcpy(buf, mem[node->content[blk1]]+off1, blocksize- off1);
        for(int i = blk1+1;i<blk2;i++){
            memcpy(buf+blocksize-off1 + (i-blk1-1)*blocksize,mem[node->content[i]],blocksize);

        }
        memcpy(buf+blocksize-off1 + (blk2-blk1-1)*blocksize, mem[node->content[blk2]], off2);
        return ret;
    }

    //memcpy(buf, node->content + offset, ret);
    return ret;
}

static int oshfs_unlink(const char *path)
{
    // Not Implemented
    struct filenode *node = NULL;
    int k = -1;
    for(int i = 0; i < inode_num ;i++){
        if(inode_map[i]){
            if(strcmp(inode[i].filename, path + 1)==0){
                node =  (inode + i);
                k = i;
            }
        }
    }
    if(  (!node)  || (k == -1)  ){
        return -1;
    }
    for(int i = 0;i<INDEX_NUMBER_MAX;i++){
        if(inode[k].content[i] < 3){
            break;
        }
        munmap(mem[inode[k].content[i]],blocksize);
        data_map[inode[k].content[i]] = 0;
    }
    inode_map[k]=0;





    return 0;
}

static const struct fuse_operations op = {
        .init = oshfs_init,
        .getattr = oshfs_getattr,
        .readdir = oshfs_readdir,
        .mknod = oshfs_mknod,
        .open = oshfs_open,
        .write = oshfs_write,
        .truncate = oshfs_truncate,
        .read = oshfs_read,
        .unlink = oshfs_unlink,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
