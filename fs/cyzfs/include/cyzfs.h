#ifndef _CYZFS_H_
#define _CYZFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"

#define CYZFS_MAGIC       87654233   /* TODO: Define by yourself */
#define CYZFS_DEFAULT_PERM    0777   /* 全权限打开 */

/******************************************************************************
* SECTION: cyzfs.c
*******************************************************************************/

int assemble_read(int, char *, int);
int assemble_write(int offset, char *buf, int size);
struct cyzfs_dentry* assemble_new_dentry(char *, CYZFS_FILE_TYPE);
struct cyzfs_inode* assemble_alloc_inode(struct cyzfs_dentry* );
struct cyzfs_inode* assemble_read_inode(struct cyzfs_dentry* );
void assemble_sync_inode(struct cyzfs_inode * );
char* assemble_get_fname(const char* ) ;
int assemble_calc_lvl(const char * );
struct cyzfs_dentry* assemble_find_dentry_of_path(const char * , int* , int* );
char* assemble_alloc_datablk(int *);
int assemble_alloc_insert_dentry2inode(struct cyzfs_inode* , struct cyzfs_dentry* );
struct cyzfs_dentry * assemble_get_dentry(struct cyzfs_inode* , int );

void* 			   cyzfs_init(struct fuse_conn_info *);
void  			   cyzfs_destroy(void *);
int   			   cyzfs_mkdir(const char *, mode_t);
int   			   cyzfs_getattr(const char *, struct stat *);
int   			   cyzfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   cyzfs_mknod(const char *, mode_t, dev_t);
int   			   cyzfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   cyzfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   cyzfs_access(const char *, int);
int   			   cyzfs_unlink(const char *);
int   			   cyzfs_rmdir(const char *);
int   			   cyzfs_rename(const char *, const char *);
int   			   cyzfs_utimens(const char *, const struct timespec tv[2]);
int   			   cyzfs_truncate(const char *, off_t);
			
int   			   cyzfs_open(const char *, struct fuse_file_info *);
int   			   cyzfs_opendir(const char *, struct fuse_file_info *);

#endif  /* _cyzfs_H_ */