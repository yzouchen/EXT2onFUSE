#include "../include/cyzfs.h"

/******************************************************************************
* SECTION: 宏定义
*******************************************************************************/
#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }

/******************************************************************************
* SECTION: 全局变量
*******************************************************************************/
static const struct fuse_opt option_spec[] = {		/* 用于FUSE文件系统解析参数 */
	OPTION("--device=%s", device),
	FUSE_OPT_END
};

struct custom_options cyzfs_options;			 /* 全局选项 */
struct cyzfs_super super; 
/******************************************************************************
* SECTION: FUSE操作定义
*******************************************************************************/
static struct fuse_operations operations = {
	.init = cyzfs_init,						 /* mount文件系统 */		
	.destroy = cyzfs_destroy,				 /* umount文件系统 */
	.mkdir = cyzfs_mkdir,					 /* 建目录，mkdir */
	.getattr = cyzfs_getattr,				 /* 获取文件属性，类似stat，必须完成 */
	.readdir = cyzfs_readdir,				 /* 填充dentrys */
	.mknod = cyzfs_mknod,					 /* 创建文件，touch相关 */
	.write = NULL,								  	 /* 写入文件 */
	.read = NULL,								  	 /* 读文件 */
	.utimens = cyzfs_utimens,				 /* 修改时间，忽略，避免touch报错 */
	.truncate = NULL,						  		 /* 改变文件大小 */
	.unlink = NULL,							  		 /* 删除文件 */
	.rmdir	= NULL,							  		 /* 删除目录， rm -r */
	.rename = NULL,							  		 /* 重命名，mv */

	.open = NULL,							
	.opendir = NULL,
	.access = NULL
};

/******************************************************************************
* SECTION: Assemble Function for disk operation for cyzfs	reference to sfs_utils.c
*******************************************************************************/

int assemble_read(int offset, uint8_t *buf, int size) {
	//从offset开始，读size个字节，存入buf
	//实现集成的辅助512字节对齐
    int      offset_aligned = BLK_ROUND_DOWN(offset, IO_SIZE);
    int      bias           = offset - offset_aligned;
    int      size_aligned   = BLK_ROUND_UP((size + bias), IO_SIZE);
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;

    ddriver_seek(super.fd, offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        ddriver_read(super.fd, cur, IO_SIZE);
        cur          += IO_SIZE;
        size_aligned -= IO_SIZE;   
    }
    memcpy(buf, temp_content + bias, size);		//读最初要求的数据，不含对齐
    free(temp_content);
    return 0;
}

int assemble_write(int offset, uint8_t *buf, int size) {
	//将buf的size字节写入offset开始的磁盘块中
	//因为原始写需要512整个写，故需要提前将非对齐部分读入内存并整合写入
    int      offset_aligned = BLK_ROUND_DOWN(offset, IO_SIZE);
    int      bias           = offset - offset_aligned;
    int      size_aligned   = BLK_ROUND_UP((size + bias), IO_SIZE);
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    assemble_read(offset_aligned, temp_content, size_aligned);	//补齐非对齐部分
    memcpy(temp_content + bias, buf, size);
    
    ddriver_seek(super.fd, offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        ddriver_write(super.fd, cur, IO_SIZE);
        cur          += IO_SIZE;
        size_aligned -= IO_SIZE;   
    }

    free(temp_content);
    return 0;
}

struct cyzfs_dentry* assemble_new_dentry(char * fname, CYZFS_FILE_TYPE ftype){
	//在内存中新建一个dentry
	struct cyzfs_dentry* new_dentry = (struct cyzfs_dentry*)malloc(sizeof(struct cyzfs_dentry));
	memset(new_dentry, 0, sizeof(struct cyzfs_dentry));
	memcpy(new_dentry->name, fname, MAX_NAME_LEN);
	new_dentry->ftype = ftype;
	new_dentry->ino = -1;
	new_dentry->inode = NULL;
	new_dentry->parent = NULL;
	new_dentry->brother = NULL;
	return new_dentry;
} 

struct cyzfs_inode* assemble_alloc_inode(struct cyzfs_dentry* dentry){
	// 新分配一个inode，分配的inode需要在inode位图中对应为0，注意inode未同步至磁盘
	int free_ino = -1;
	int inode_num,byte,bit;
	for(inode_num = 0; inode_num < MAX_INODE; inode_num++){
		byte = inode_num / 8;
		bit = inode_num % 8;
		if(((super.bitmap_inode_ptr[byte] >> bit) & 1) == 0){
			// no.i inode is free
			free_ino = inode_num;
			super.bitmap_inode_ptr[byte] |= (1 << bit);
			break;
		}
	}
	if(free_ino == -1){
		printf("alloc_inode: no free inode!\n");
		return NULL;//-ENOSPC;
	}
	/******** 分配新inode的in-memory物理空间 **************/
	struct cyzfs_inode* new_inode = (struct cyzfs_inode*)malloc(sizeof(struct cyzfs_inode));
	new_inode->ino = free_ino;
	new_inode->size = 0;
		/**********dentry指向inode***********/
	dentry->inode = new_inode;
	dentry->ino = free_ino;
		/*********inode回指向dentry**********/
	new_inode->ftype = dentry->ftype;
	new_inode->dir_cnt = 0;
	new_inode->dentry_parent = dentry;
	new_inode->dentry_children = NULL;
	for(int i=0; i<6; i++){
		new_inode->data_pointer[i] = -1;
		new_inode->data_pointer_mem[i] = NULL;
	}
	return new_inode;
}

struct cyzfs_inode* assemble_read_inode(struct cyzfs_dentry* dentry){
	struct cyzfs_inode* inode = (struct cyzfs_inode*)malloc(sizeof(struct cyzfs_inode));
	struct cyzfs_inode_d inode_d;
    struct cyzfs_dentry* sub_dentry;
    struct cyzfs_dentry_d dentry_d;
	//Q:为什么sfs每个inode占用一个Blk啊，好浪费，这里改了不同于sfs
	assemble_read((super.inode_offset * FS_BLOCK_SIZE + dentry->ino * sizeof(struct cyzfs_inode_d)), 
					(char*)&inode_d, 
					sizeof(struct cyzfs_dentry));
	inode->dir_cnt = inode_d.dir_cnt;
	inode->ino = inode_d.ino;
	inode->size = inode_d.size;
	inode->ftype =inode_d.ftype;
	inode->dentry_parent = dentry;
	inode->dentry_children = NULL;
	for(int i=0; i<6; i++){
		inode->data_pointer[i] = inode_d.data_pointer[i];
		// malloc data pointer in memory
		// Q:这里是每次把inode读入mem时，就把其对应的数据块都都装入内存，有待商榷
		if(inode_d.data_pointer[i] >= 0){
			inode->data_pointer_mem[i] = (char*)malloc(FS_BLOCK_SIZE);
			assemble_read((super.data_offset + inode->data_pointer[i]) * FS_BLOCK_SIZE, inode->data_pointer_mem[i], FS_BLOCK_SIZE);
		}
	}
	if(inode->ftype == TYPE_DIR){
		// DIR should init inode->dentry_children
		// read in (data)dentry block
		for(int i = 0; i < inode->dir_cnt; i++)
		{
			// 将第i个目录项读到dentry_d中
			assemble_read((super.data_offset+inode->ino)*FS_BLOCK_SIZE + i*sizeof(struct cyzfs_dentry_d),
							(uint8_t *)&dentry_d,
							sizeof(struct cyzfs_dentry_d));
			// 建立对应的内存中的目录项，插入inode链表
			sub_dentry = assemble_new_dentry(dentry_d.name, dentry_d.ftype);
			sub_dentry->parent = dentry;
			sub_dentry->brother = inode->dentry_children;
			inode->dentry_children = sub_dentry;
		}
	}
	/********* sfs中对普通文件就是读入数据，在前面已经处理过了 **********/

	return inode;
}

int assemble_sync_inode(struct cyzfs_inode * inode){
	/************** 将inode对应内容递归的全部写回磁盘 ***************/
	struct cyzfs_inode_d inode_d;
	struct cyzfs_dentry* dentry;
	struct cyzfs_dentry_d dentry_d;
	int ino = inode->ino;

	/******************** 写回inode *********************/
	inode_d.ino = inode->ino;
	inode_d.size = inode->size;
	inode_d.ftype = inode->ftype;
	inode_d.dir_cnt = inode->dir_cnt;
	for(int i=0; i < 6; i++){
		inode_d.data_pointer[i] = inode->data_pointer[i];
	}
	assemble_write((super.inode_offset * FS_BLOCK_SIZE + ino * sizeof(struct cyzfs_inode_d)), 
					(char*)&inode_d, 
					sizeof(struct cyzfs_inode_d));
	

	/************************* 写回inode对应的data***************************/
	if(inode->ftype == TYPE_DIR){
		dentry = inode->dentry_children;
		int offset = (super.data_offset + dentry->ino) * FS_BLOCK_SIZE;
		while(dentry != NULL)
		{
			memcpy(dentry_d.name, dentry->name, MAX_NAME_LEN);
			dentry_d.ino = dentry->ino;
			dentry_d.ftype = dentry->ftype;
			assemble_write(offset, (uint8_t *)&dentry_d, sizeof(struct cyzfs_dentry_d));
			if(dentry->inode != NULL){
				assemble_sync_inode(dentry->inode);
			}
			dentry = dentry->brother;
			offset += sizeof(struct cyzfs_dentry_d);
		}
	}
	else if(inode->ftype == TYPE_FILE){
		//sfs这里怎么没有把6个字块全部写回？
		for(int i = 0; i<6; i++){
			if(inode->data_pointer[i] >= 0){
				assemble_write((super.data_offset+inode->data_pointer[i])*FS_BLOCK_SIZE,
								inode->data_pointer_mem[i], FS_BLOCK_SIZE);
			}
		}
	}
	return 0;
}

char* assemble_get_fname(const char* path) {
	/*******获取文件名***********/
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

int assemble_calc_lvl(const char * path) {
    /*******计算路径的层级************/
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}

struct cyzfs_dentry* assemble_find_dentry_of_path(const char * path, int* is_find, int* is_root){
	struct cyzfs_dentry* dentry_cursor = super.root_dentry;
    struct cyzfs_dentry* dentry_ret = NULL;
    struct cyzfs_inode*  inode; 
    int total_lvl = assemble_calc_lvl(path);
    int lvl = 0;
    int is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = super.root_dentry;
    }
	fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            assemble_read_inode(dentry_cursor);
        }

        inode = dentry_cursor->inode;

        if (inode->ftype == TYPE_FILE && lvl < total_lvl) {
            dentry_ret = inode->dentry_parent;
            break;
        }
        if (inode->ftype == TYPE_DIR) {
            dentry_cursor = inode->dentry_children;
            is_hit        = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->name, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            if (!is_hit) {
                *is_find = FALSE;
				printf("find_dentry_of_path: Found Fail!\n");
                dentry_ret = inode->dentry_parent;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }
	//add
	if(dentry_ret == NULL)
		return NULL;

    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = assemble_read_inode(dentry_ret);
    }
    
    return dentry_ret;
}

char* assemble_alloc_datablk(int *datablk_no){
	/***** refer to assemble_alloc_inode(bitmap!) *****/
	int dbno,bit,byte;
	for(dbno=0; dbno<MAX_INODE; dbno++){
		byte = dbno / 8;
		bit = dbno % 8;
		if(((super.bitmap_data_ptr[byte] >> bit) & 1) == 0){
			// no.i inode is free
			*datablk_no = dbno;
			super.bitmap_data_ptr[byte] |= (1 << bit);
			break;
		}
	}
	char* datablk_ptr = (char*)malloc(FS_BLOCK_SIZE);
	return datablk_ptr;
}

int assemble_alloc_insert_dentry2inode(struct cyzfs_inode* inode, struct cyzfs_dentry* dentry){
	/******* 对应于sfs_alloc_dentry ********/
	/******************** 为inode分配dentry的位置并插入，若块满，需要扩充 **********************/
	/*** Q:sfs里好像没对这个处理啊，要是文件数量大一些可能就出问题了，测试脚本还是小了 ***/
	int dentry_per_datablock = FS_BLOCK_SIZE / sizeof(struct cyzfs_dentry_d);
	inode->dir_cnt++;
	if((inode->dir_cnt - 1) % dentry_per_datablock == 0){
		/********* 需要分配一个新的块来存放dentry ***********/
		int blkno_in_dir = (inode->dir_cnt - 1) / dentry_per_datablock;
		int datablk_no = -1;
		if(blkno_in_dir >= 6){
			/***** 目录项大小超过6个数据块 ****/
			printf("Error: alloc_dentry: inode is full\n");
			inode->dir_cnt--;
			return -1;
		}
		/***** refer to assemble_alloc_inode(bitmap!) *****/
		inode->data_pointer_mem[blkno_in_dir] = (char *) assemble_alloc_datablk(&datablk_no);
		inode->data_pointer[blkno_in_dir] = datablk_no;
	}
	/****** 将dentry插入inode *******/
	dentry->brother = inode->dentry_children;
	inode->dentry_children = dentry;
	return 0;
}

struct cyzfs_dentry * assemble_get_dentry(struct cyzfs_inode* inode, int dir){
    struct cyzfs_dentry* dentry_cursor = inode->dentry_children;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}




/******************************************************************************
* SECTION: 必做函数实现
*******************************************************************************/
/**
 * @brief 挂载（mount）文件系统
 * 
 * @param conn_info 可忽略，一些建立连接相关的信息 
 * @return void*
 */
void* cyzfs_init(struct fuse_conn_info * conn_info) {
	/********* Layout 如下 ****************/
/*********** super | bitmap_inode | bitmap_data | inode | data		**************/
	int is_init = FALSE;
	struct cyzfs_super_d super_d;
	struct cyzfs_inode* root_inode;

	super.is_mounted = FALSE;
	
	super.fd = ddriver_open(cyzfs_options.device);
	if(super.fd < 0){
		return super.fd;
	}
	
	/********************** 读入超级块 ********************/
	assemble_read(0, (char*)(&super_d), sizeof(struct cyzfs_super_d));
	if(super_d.magic != CYZFS_MAGIC){
		//幻数不匹配，进行初始化，修改磁盘super块
		//共有4M/1K=4K个数据块，最多4K个inode
		//4Kbit = 512B = 1BLK

		printf("The Disk is not a CYZFS Disk, start initializing......\n");
		super_d.bitmap_inode_blks = BLK_ROUND_UP(MAX_INODE/8,FS_BLOCK_SIZE) / FS_BLOCK_SIZE;
		super_d.bitmap_inode_offset = BLK_ROUND_UP(sizeof(struct cyzfs_super_d), FS_BLOCK_SIZE) / FS_BLOCK_SIZE;
		super_d.bitmap_data_blks = super_d.bitmap_inode_blks;
		super_d.bitmap_data_offset = super_d.bitmap_inode_offset + super_d.bitmap_inode_blks;
		super_d.magic = CYZFS_MAGIC;
		super_d.sz_usage = 0;
		is_init = TRUE;
		printf("......Initialization finished!\n");
	}

	/********************** 利用磁盘超级块建立in-memory超级块 ********************/
	super.bitmap_inode_blks = super_d.bitmap_inode_blks;
	super.bitmap_inode_offset = super_d.bitmap_inode_offset;
	super.bitmap_data_blks = super_d.bitmap_data_blks;
	super.bitmap_data_offset = super_d.bitmap_data_offset;
	super.sz_usage = super_d.sz_usage;
	
	super.inode_blks = BLK_ROUND_UP(MAX_INODE*sizeof(struct cyzfs_inode_d), FS_BLOCK_SIZE) / FS_BLOCK_SIZE;
	super.inode_offset = super.bitmap_data_offset + super.bitmap_data_blks;
	super.data_blks = (DISK_SIZE/FS_BLOCK_SIZE) - super.bitmap_inode_offset
						- super.bitmap_inode_blks - super.bitmap_data_blks - super.inode_blks;
	super.data_offset = super.inode_offset + super.inode_blks;

	super.bitmap_inode_ptr = (uint8_t*) malloc(super.bitmap_inode_blks * FS_BLOCK_SIZE);
	assemble_read((super.bitmap_inode_offset * FS_BLOCK_SIZE), super.bitmap_inode_ptr, (super.bitmap_inode_blks * FS_BLOCK_SIZE));
	super.bitmap_data_ptr = (uint8_t*) malloc(super.bitmap_data_blks * FS_BLOCK_SIZE);
	assemble_read((super.bitmap_data_offset * FS_BLOCK_SIZE), super.bitmap_data_ptr, (super.bitmap_data_blks * FS_BLOCK_SIZE));
	
	super.root_dentry = assemble_new_dentry("/", TYPE_DIR);
	//Q:sfs对根目录的inode好像没处理
	if(is_init){
		//分配根目录的inode
		super.root_dentry->ino = ROOT_INODE_NUM;	//根目录的inode号固定
		root_inode = assemble_alloc_inode(super.root_dentry);
		// assemble_sync_inode(root_inode);
		/************** 这里sfs把inode同步回了磁盘，后续在umount的时候一块同步回磁盘？ *****************/
	} else {
		// 读入固定inode号的inode信息与root_dentry连接
		root_inode = assemble_read_inode(super.root_dentry);
    	super.root_dentry->inode = root_inode;
	}
	
	super.is_mounted = TRUE;

	return NULL;
}

/**
 * @brief 卸载（umount）文件系统
 * 
 * @param p 可忽略
 * @return void
 */
void cyzfs_destroy(void* p) {
	struct cyzfs_super_d super_d;

	if(!super.is_mounted){
		return 0;
	}

/********************* 写inode和数据 ******************************/
	assemble_sync_inode(super.root_dentry->inode);

/************************写两个位图********************************/
	assemble_write(super.bitmap_inode_offset * FS_BLOCK_SIZE, 
					super.bitmap_inode_ptr, 
					super.bitmap_inode_blks * FS_BLOCK_SIZE);
	assemble_write(super.bitmap_data_offset * FS_BLOCK_SIZE, 
					super.bitmap_data_ptr, 
					super.bitmap_data_blks * FS_BLOCK_SIZE);

/*********************** 写超级块 ********************************/
	super_d.magic = CYZFS_MAGIC;
	super_d.sz_usage = super.sz_usage;
	super_d.bitmap_inode_blks = super.bitmap_inode_blks;
	super_d.bitmap_inode_offset = super.bitmap_inode_offset;
	super_d.bitmap_data_blks = super.bitmap_data_blks;
	super_d.bitmap_data_offset = super.bitmap_data_offset;
	assemble_write(0, (char *)&super_d, sizeof(struct cyzfs_super_d));

/****************** free in memory ************************/
	free(super.bitmap_inode_ptr);
	free(super.bitmap_data_ptr);

	ddriver_close(super.fd);

	return;
}

/**
 * @brief 创建目录
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建模式（只读？只写？），可忽略
 * @return int 0成功，否则失败
 */
int cyzfs_mkdir(const char* path, mode_t mode) {
	/*  解析路径，创建目录 */
	(void)mode;
	int is_find, is_root;
	char* fname;
	struct cyzfs_dentry* last_dentry = assemble_find_dentry_of_path(path, &is_find, &is_root);
	struct cyzfs_dentry* dentry;
	struct cyzfs_inode*  inode;

	if (is_find) {
		printf("Error: mkdir: dir already exists!\n");
		return -EEXIST;
	}

	if (last_dentry->ftype == TYPE_FILE) {
		printf("Error: mkdir: not create under a dir\n");
		return -ENXIO;
	}

	fname  = assemble_get_fname(path);
	dentry = assemble_new_dentry(fname, TYPE_DIR); 
	dentry->parent = last_dentry;
	inode  = assemble_alloc_inode(dentry);
	assemble_alloc_insert_dentry2inode(last_dentry->inode, dentry);

	return 0;
}

/**
 * @brief 获取文件或目录的属性，该函数非常重要
 * 
 * @param path 相对于挂载点的路径
 * @param cyzfs_stat 返回状态
 * @return int 0成功，否则失败
 */
int cyzfs_getattr(const char* path, struct stat * cyzfs_stat) {
	/*  解析路径，获取Inode，填充cyzfs_stat，可参考/fs/simplefs/sfs.c的sfs_getattr()函数实现 */
	int	is_find, is_root;
	struct cyzfs_dentry* dentry = assemble_find_dentry_of_path(path, &is_find, &is_root);
	if (is_find == FALSE) {
		/****** not found ******/
		return -ENOENT;
	}

	if (dentry->inode->ftype == TYPE_DIR) {
		cyzfs_stat->st_mode = S_IFDIR | CYZFS_DEFAULT_PERM;
		cyzfs_stat->st_size = dentry->inode->dir_cnt * sizeof(struct cyzfs_dentry_d);
	}
	else if (dentry->inode->ftype == TYPE_FILE) {
		cyzfs_stat->st_mode = S_IFREG | CYZFS_DEFAULT_PERM;
		cyzfs_stat->st_size = dentry->inode->size;
	}

	cyzfs_stat->st_nlink = 1;
	cyzfs_stat->st_uid 	 = getuid();
	cyzfs_stat->st_gid 	 = getgid();
	cyzfs_stat->st_atime   = time(NULL);
	cyzfs_stat->st_mtime   = time(NULL);
	cyzfs_stat->st_blksize = FS_BLOCK_SIZE;

	if (is_root) {
		cyzfs_stat->st_size	= super.sz_usage;  //Q: super.sz_usage到这里才发现好像sfs没修改过啊 
		cyzfs_stat->st_blocks = DISK_SIZE / FS_BLOCK_SIZE;
		cyzfs_stat->st_nlink  = 2;		/* !特殊，根目录link数为2 */
	}

	return 0;
}

/**
 * @brief 遍历目录项，填充至buf，并交给FUSE输出
 * 
 * @param path 相对于挂载点的路径
 * @param buf 输出buffer
 * @param filler 参数讲解:
 * 
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *				const struct stat *stbuf, off_t off)
 * buf: name会被复制到buf中
 * name: dentry名字
 * stbuf: 文件状态，可忽略
 * off: 下一次offset从哪里开始，这里可以理解为第几个dentry
 * 
 * @param offset 第几个目录项？
 * @param fi 可忽略
 * @return int 0成功，否则失败
 */
int cyzfs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
			    		 struct fuse_file_info * fi) {
    /*  解析路径，获取目录的Inode，并读取目录项，利用filler填充到buf，可参考/fs/simplefs/sfs.c的sfs_readdir()函数实现 */
    int		is_find, is_root;
	int		cur_dir = offset;

	struct cyzfs_dentry* dentry = assemble_find_dentry_of_path(path, &is_find, &is_root);
	struct cyzfs_dentry* sub_dentry;
	struct cyzfs_inode* inode;
	if (is_find) {
		inode = dentry->inode;
		sub_dentry = assemble_get_dentry(inode, cur_dir);
		if (sub_dentry) {
			filler(buf, sub_dentry->name, NULL, ++offset);
		}
		return 0;
	}
	return -ENOENT;
}

/**
 * @brief 创建文件
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建文件的模式，可忽略
 * @param dev 设备类型，可忽略
 * @return int 0成功，否则失败
 */
int cyzfs_mknod(const char* path, mode_t mode, dev_t dev) {
	/*  解析路径，并创建相应的文件 */
	int	is_find, is_root;
	
	struct cyzfs_dentry* last_dentry = assemble_find_dentry_of_path(path, &is_find, &is_root);
	struct cyzfs_dentry* dentry;
	struct cyzfs_inode* inode;
	char* fname;
	
	if (is_find == TRUE) {
		return -EEXIST;
	}

	fname = assemble_get_fname(path);
	
	if (S_ISREG(mode)) {
		dentry = assemble_new_dentry(fname, TYPE_FILE);
	}
	else if (S_ISDIR(mode)) {
		dentry = assemble_new_dentry(fname, TYPE_DIR);
	}
	/******* 设置父指针并插入到链表中 ********/
	dentry->parent = last_dentry;
	inode = assemble_alloc_inode(dentry);
	assemble_alloc_insert_dentry2inode(last_dentry->inode, dentry);

	return 0;
}

/**
 * @brief 修改时间，为了不让touch报错 
 * 
 * @param path 相对于挂载点的路径
 * @param tv 实践
 * @return int 0成功，否则失败
 */
int cyzfs_utimens(const char* path, const struct timespec tv[2]) {
	(void)path;
	return 0;
}
/******************************************************************************
* SECTION: 选做函数实现
*******************************************************************************/
/**
 * @brief 写入文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 写入的内容
 * @param size 写入的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 写入大小
 */
int cyzfs_write(const char* path, const char* buf, size_t size, off_t offset,
		        struct fuse_file_info* fi) {
	/* 选做 */
	return size;
}

/**
 * @brief 读取文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 读取的内容
 * @param size 读取的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 读取大小
 */
int cyzfs_read(const char* path, char* buf, size_t size, off_t offset,
		       struct fuse_file_info* fi) {
	/* 选做 */
	return size;			   
}

/**
 * @brief 删除文件
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则失败
 */
int cyzfs_unlink(const char* path) {
	/* 选做 */
	return 0;
}

/**
 * @brief 删除目录
 * 
 * 一个可能的删除目录操作如下：
 * rm ./tests/mnt/j/ -r
 *  1) Step 1. rm ./tests/mnt/j/j
 *  2) Step 2. rm ./tests/mnt/j
 * 即，先删除最深层的文件，再删除目录文件本身
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则失败
 */
int cyzfs_rmdir(const char* path) {
	/* 选做 */
	return 0;
}

/**
 * @brief 重命名文件 
 * 
 * @param from 源文件路径
 * @param to 目标文件路径
 * @return int 0成功，否则失败
 */
int cyzfs_rename(const char* from, const char* to) {
	/* 选做 */
	return 0;
}

/**
 * @brief 打开文件，可以在这里维护fi的信息，例如，fi->fh可以理解为一个64位指针，可以把自己想保存的数据结构
 * 保存在fh中
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则失败
 */
int cyzfs_open(const char* path, struct fuse_file_info* fi) {
	/* 选做 */
	return 0;
}

/**
 * @brief 打开目录文件
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则失败
 */
int cyzfs_opendir(const char* path, struct fuse_file_info* fi) {
	/* 选做 */
	return 0;
}

/**
 * @brief 改变文件大小
 * 
 * @param path 相对于挂载点的路径
 * @param offset 改变后文件大小
 * @return int 0成功，否则失败
 */
int cyzfs_truncate(const char* path, off_t offset) {
	/* 选做 */
	return 0;
}


/**
 * @brief 访问文件，因为读写文件时需要查看权限
 * 
 * @param path 相对于挂载点的路径
 * @param type 访问类别
 * R_OK: Test for read permission. 
 * W_OK: Test for write permission.
 * X_OK: Test for execute permission.
 * F_OK: Test for existence. 
 * 
 * @return int 0成功，否则失败
 */
int cyzfs_access(const char* path, int type) {
	/* 选做: 解析路径，判断是否存在 */
	return 0;
}	
/******************************************************************************
* SECTION: FUSE入口
*******************************************************************************/
int main(int argc, char **argv)
{
    int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	cyzfs_options.device = strdup("TODO: 这里填写你的ddriver设备路径");

	if (fuse_opt_parse(&args, &cyzfs_options, option_spec, NULL) == -1)
		return -1;
	
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}