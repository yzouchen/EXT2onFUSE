#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128     

struct custom_options {
	const char*        device;
};

//MACRO
typedef enum cyzfs_file_type {
    TYPE_FILE,           // common file
    TYPE_DIR             // directory file
} CYZFS_FILE_TYPE;

#define TRUE                    1
#define FALSE                   0
#define DISK_SIZE               (4*1024*1024)   // ddrive capacity = 4MiB
#define FS_BLOCK_SIZE           1024            // EXT2 BLOCK SIZE = 1KiB
#define MAX_INODE               (4*1024)
#define IO_SIZE                 512
#define MAX_NAME_LEN            128     
#define ROOT_INODE_NUM          0               // 根据指导书，EXT2文件系统根目录的索引号为2
#define BLK_ROUND_DOWN(value, round)    ((value / round) * round)
#define BLK_ROUND_UP(value, round)      ((value / round + 1) * round)


//PS: 偏移指偏移块数

/******************************************************************************
* SECTION: FS Specific Structure - Disk structure
*******************************************************************************/

struct cyzfs_super_d {
    uint32_t magic;                       // 幻数      
    int      sz_usage;                   
    int      bitmap_inode_blks;              // inode位图占用的块数
    int      bitmap_inode_offset;            // inode位图在磁盘上的偏移
    int      bitmap_data_blks;               // data位图占用的块数
    int      bitmap_data_offset;             // data位图在磁盘上的偏移
};

struct cyzfs_inode_d {
    int                ino;                // 在inode位图中的下标
    int                size;               // 文件已占用空间
    int                link;               // 链接数
    CYZFS_FILE_TYPE    ftype;              // 文件类型（目录类型、普通文件类型）
    int                dir_cnt;            // 如果是目录类型文件，下面有几个目录项
    int                data_pointer[6];   // 数据块指针（可固定分配） 
};

struct cyzfs_dentry_d {
    char     name[MAX_NAME_LEN];
    uint32_t ino;
    CYZFS_FILE_TYPE ftype;
    int valid;
};

/******************************************************************************
* SECTION: FS Specific Structure - In memory structure
*******************************************************************************/
struct cyzfs_super ;
struct cyzfs_inode;
struct cyzfs_dentry;

struct cyzfs_super {
    int                 fd;
    int                 sz_usage;

    uint8_t*            bitmap_inode_ptr;               // inode位图in memory
    uint8_t*            bitmap_data_ptr;                // data位图in memory
    struct cyzfs_dentry*     root_dentry;           // 根目录dentry
    int                 is_mounted;

    int                 bitmap_inode_blks;              // inode位图占用的块数
    int                 bitmap_inode_offset;            // inode位图在磁盘上的偏移
    int                 bitmap_data_blks;               // data位图占用的块数
    int                 bitmap_data_offset;             // data位图在磁盘上的偏移

    int                 inode_blks;
    int                 inode_offset;
    int                 data_blks;
    int                 data_offset;
};



struct cyzfs_inode {
    uint32_t            ino;                  // 在inode位图中的下标
    int                 size;                 // 文件已占用空间
    CYZFS_FILE_TYPE     ftype;
    int                 dir_cnt;              // 目录项数量
    struct cyzfs_dentry*      dentry_parent;        // 指向该inode的dentry
    struct cyzfs_dentry*      dentry_children;      // 所有子目录项  
    int                 data_pointer[6];        // 数据块指针
    char*               data_pointer_mem[6];    //数据块在内存中的指针
};



struct cyzfs_dentry {
    char     name[MAX_NAME_LEN];
    uint32_t ino;                                     /* 在inode位图中的下标 */
    struct cyzfs_dentry* parent;                        /* 父亲Inode的dentry */
    struct cyzfs_dentry* brother;                       /* 兄弟 */
    struct cyzfs_inode*  inode;                         /* 指向inode */
    CYZFS_FILE_TYPE    ftype;     
};

#endif /* _TYPES_H_ */