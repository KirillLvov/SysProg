#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 1024,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	const char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;
	/** How many bytes are occupied. */
	int occupied;
	
	/* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;
	struct block *block_to_read;
	/* The position of the filedescriptor in the file*/
	int pos;
	/* A regime of the work */
	int regime;
	/* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

int
ufs_open(const char *filename, int flags)
{
	/* Return error if there are too many file descriptors */
	if(file_descriptor_count == file_descriptor_capacity) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct filedesc *new_filedesc = (struct filedesc *)malloc(sizeof(struct filedesc));
	/* A flag signaling that a file with 'filename' exists */
	int file_ok = 0;
	/* Try to find a file in the file_list and create file descriptor if it is success*/ 
	struct file *probe = (struct file *)malloc(sizeof(struct file));
	if(file_list != NULL)
		probe->next = file_list;
	else
		probe->next = NULL;
	while(probe->next != NULL) {
		probe = probe->next;
		if(strcmp(probe->name, filename) == 0) {
			new_filedesc->file = probe;
			new_filedesc->block_to_act = probe->block_list;
			new_filedesc->pos = 0;
			probe->refs ++;
			file_ok = 1;
			break;
		}
	}
	/* Create new file */
	if(!file_ok) {
		if(!(flags & UFS_CREATE)) {
			ufs_error_code = UFS_ERR_NO_FILE;
			return -1;
		}
		else {
			struct file *new_file = (struct file *)malloc(sizeof(struct file));
			new_file->block_list = (struct block *)malloc(sizeof(struct block));
			new_file->block_list->memory = NULL;
			new_file->block_list->next = NULL;
			new_file->block_list->prev = NULL;
			new_file->block_list->occupied = 0;
			new_file->last_block = new_file->block_list;
			new_file->name = (const char *)malloc(sizeof(char)*strlen(filename));
			strcpy((char*)new_file->name, filename);
			new_file->next = NULL;
			new_file->refs = 1;
			new_file->occupied = 0;
			if(file_list != NULL) {
				new_file->prev = probe;
				probe->next = new_file;
			}
			else {
				new_file->prev = NULL;
				file_list = new_file;
			}
			new_filedesc->file = new_file;
			new_filedesc->block_to_act = new_file->block_list;
			new_filedesc->pos = 0;
		}
	}
	new_filedesc->regime = flags;
	/* Add a new file descriptor to the file_descriptors */
	for(int i = 0; i < file_descriptor_capacity; i++)
		if(file_descriptors[i] == NULL) {
			file_descriptors[i] = new_filedesc;
			file_descriptor_count ++;
			return i;
		}
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	/* Check whether a file descriptor #fd exists */
	if(fd < 0 || fd > file_descriptor_capacity - 1 || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	/* Check permissions of the work */
	if(file_descriptors[fd]->regime & UFS_READ_ONLY) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	/* Extract file from file descriptor */
	struct file *file_to_write = file_descriptors[fd]->file;
	/* Check whether the file is short enought to write 'buf' */
	if(MAX_FILE_SIZE - file_descriptors[fd]->pos < size) {
		ufs_error_code = UFS_ERR_NO_MEM;
		return -1;
	}
	/* The number of bytes left to write */
	int left_to_write = size;
	/* Choose the last block in the file */
	struct block *cur_block = file_descriptors[fd]->block_to_act;
	if(cur_block->memory == NULL)
		cur_block->memory = (char *)malloc(sizeof(char)*BLOCK_SIZE);
	/* Write the 'buf' totally if there is enought memory in the last block */
	int offset = file_descriptors[fd]->pos % BLOCK_SIZE;
	if(BLOCK_SIZE - offset >= left_to_write) {
		sprintf(cur_block->memory + offset, "%s", buf);
		if(cur_block->occupied < offset + left_to_write) {
			file_to_write->occupied += offset + left_to_write - cur_block->occupied;
			cur_block->occupied = offset + left_to_write;
		}
		/* Create new block if previous one is full */
		if(BLOCK_SIZE - offset == left_to_write) {
			struct block *new_block = (struct block *)malloc(sizeof(struct block));
			new_block->memory = (char *)malloc(sizeof(char)*BLOCK_SIZE);
			new_block->occupied = 0;
			new_block->next = NULL;
			new_block->prev = cur_block;
			cur_block->next = new_block;
			cur_block = new_block;
			file_to_write->last_block = cur_block;
		}
		left_to_write = 0;
		file_descriptors[fd]->block_to_act = cur_block;
		file_descriptors[fd]->pos += size;
		return size;
	}
	else {
		/* Write to the current block until it becomes full */
		sprintf(cur_block->memory + offset, "%.*s", BLOCK_SIZE - offset, buf + size - left_to_write);
		left_to_write -= BLOCK_SIZE - offset;
		if(cur_block->occupied < BLOCK_SIZE) {
			file_to_write->occupied += BLOCK_SIZE - cur_block->occupied;
			cur_block->occupied = BLOCK_SIZE;
		}
		/* Rewrite following blocks */
		while(cur_block->next != NULL) {
			cur_block = cur_block->next;
			if(left_to_write <= BLOCK_SIZE) {
				sprintf(cur_block->memory, "%.*s", left_to_write, buf + size - left_to_write);
				if(cur_block->occupied < left_to_write) {
					file_to_write->occupied += left_to_write - cur_block->occupied;
					cur_block->occupied = left_to_write;
				}
				file_descriptors[fd]->block_to_act = cur_block;
				file_descriptors[fd]->pos += size;
				return size;
			}
			sprintf(cur_block->memory, "%.*s", BLOCK_SIZE, buf + size - left_to_write);
			left_to_write -= BLOCK_SIZE;
			if(cur_block->occupied < BLOCK_SIZE) {
				file_to_write->occupied += BLOCK_SIZE - cur_block->occupied;
				cur_block->occupied = BLOCK_SIZE;
			}
		}
		/* Create new block and write 'buf' to them totally */
		while(left_to_write >= BLOCK_SIZE) {
			struct block *new_block = (struct block *)malloc(sizeof(struct block));
			new_block->memory = (char *)malloc(sizeof(char)*BLOCK_SIZE);
			sprintf(new_block->memory, "%.*s", BLOCK_SIZE, buf + size - left_to_write);
			new_block->occupied = BLOCK_SIZE;
			new_block->next = NULL;
			new_block->prev = cur_block;
			cur_block->next = new_block;
			cur_block = new_block;
			left_to_write -= BLOCK_SIZE;
			file_to_write->last_block = cur_block;
			file_to_write->occupied += BLOCK_SIZE;
		}
		/* Create new block and write last data to it partially */
		if(left_to_write >= 0) {
			struct block *new_block = (struct block *)malloc(sizeof(struct block));
			new_block->memory = (char *)malloc(sizeof(char)*BLOCK_SIZE);
			sprintf(new_block->memory, "%.*s", left_to_write, buf + size - left_to_write);
			new_block->occupied = left_to_write;
			new_block->next = NULL;
			new_block->prev = cur_block;
			cur_block->next = new_block;
			cur_block = new_block;
			file_to_write->last_block = cur_block;
			file_to_write->occupied += left_to_write;
			left_to_write = 0;
		}
		file_descriptors[fd]->block_to_act = cur_block;
		file_descriptors[fd]->pos += size;
		return size;
	}
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* Check whether a file descriptor #fd exists */
	if(fd < 0 || fd > file_descriptor_capacity - 1 || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	/* Check permissions of the work */
	if(file_descriptors[fd]->regime & UFS_WRITE_ONLY) {
		ufs_error_code = UFS_ERR_NO_PERMISSION;
		return -1;
	}
	/* The number of bytes left to read */
	int left_to_read;
	int available_to_read = file_descriptors[fd]->file->occupied - file_descriptors[fd]->pos;
	if(available_to_read < size)
		left_to_read = available_to_read;
	else
		left_to_read = size;
	/* Choose the block to read */
	struct block *cur_block = file_descriptors[fd]->block_to_act;
	int offset = file_descriptors[fd]->pos % BLOCK_SIZE;
	/* Check whether the file is not empty */
	if(cur_block->memory == NULL)
		return 0;
	/* Read only a part of the block if the buf's size <= BLOCK_SIZE - offset */
	if(left_to_read <= BLOCK_SIZE - offset) {
		sprintf(buf, "%.*s", cur_block->occupied - offset, cur_block->memory + offset);
		left_to_read = 0;
		if(left_to_read == BLOCK_SIZE - offset)
			cur_block = cur_block->next;
	}
	else {
		/* Read the last part of the block */
		sprintf(buf, "%s", cur_block->memory + offset);
		left_to_read -= cur_block->occupied - offset;
		cur_block = cur_block->next;
		while(cur_block != NULL) {
			/* Read left data as a part of the block */
			if(left_to_read <= BLOCK_SIZE) {
				sprintf(buf + size - left_to_read, "%.*s", left_to_read, cur_block->memory);
				left_to_read = 0;
				return size;
			}
			/* Read whole block */
			sprintf(buf + size - left_to_read, "%.*s", BLOCK_SIZE, cur_block->memory);
			left_to_read -= cur_block->occupied;
			cur_block = cur_block->next;
		}
	}
	file_descriptors[fd]->block_to_act = cur_block;
	if(available_to_read < size) {
		file_descriptors[fd]->pos += available_to_read;
		return available_to_read;
	}
	else {
		file_descriptors[fd]->pos += size;
		return size;
	}
}

int
ufs_close(int fd)
{
	/* Check whether a file descriptor #fd exists */
	if(fd < 0 || fd > file_descriptor_capacity - 1 || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	/* Delete file descriptor */
	file_descriptors[fd]->file->refs --;
	file_descriptor_count --;
	file_descriptors[fd] = NULL;
	return 0;
}

int
ufs_delete(const char *filename)
{
	/* Try to find a file in the file_list and create file descriptor if it is success*/ 
	struct file *probe = (struct file *)malloc(sizeof(struct file));
	if(file_list != NULL)
		probe->next = file_list;
	else
		probe->next = NULL;
	while(probe->next != NULL) {
		probe = probe->next;
		if(strcmp(probe->name, filename) == 0) {
			if(probe->prev != NULL) {
				probe->prev->next = probe->next;
				probe->next = NULL;
			}
			else {
				if(probe->next != NULL) {
					probe->next->prev = NULL;
					file_list = probe->next;
				}
				else
					file_list = NULL;
			}
			return 0;
		}
	}
	ufs_error_code = UFS_ERR_NO_FILE;
	return -1;
}

int
ufs_resize(int fd, size_t new_size)
{
	/* Check whether a file descriptor #fd exists */
	if(fd < 0 || fd > file_descriptor_capacity - 1 || file_descriptors[fd] == NULL) {
		ufs_error_code = UFS_ERR_NO_FILE;
		return -1;
	}
	struct file *file_to_resize = file_descriptors[fd]->file;
	/* Increase the file size */ 
	if(new_size > (file_to_resize->occupied / BLOCK_SIZE + 1)*BLOCK_SIZE) {
		/* Check whether new_size is less than MAX_FILE_SIZE */ 
		if(new_size > MAX_FILE_SIZE) {
			ufs_error_code = UFS_ERR_NO_MEM;
			return -1;
		}
		/* Create new block */
		struct block *cur_block = file_to_resize->last_block;
		for(int i = 0; i < (new_size / BLOCK_SIZE - file_to_resize->occupied / BLOCK_SIZE); i++) {
			struct block *new_block = (struct block *)malloc(sizeof(struct block));
			new_block->memory = (char *)malloc(sizeof(char)*BLOCK_SIZE);
			new_block->occupied = 0;
			new_block->next = NULL;
			new_block->prev = cur_block;
			cur_block->next = new_block;
			cur_block = new_block;
		}
	}
	/* Decrease the file size */
	if(new_size < file_to_resize->occupied) {
		/* Find the last block needed to save*/
		int left = new_size;
		struct block *cur_block = file_to_resize->block_list;
		while(left >= BLOCK_SIZE) {
			cur_block = cur_block->next;
			left -= BLOCK_SIZE;
		}
		cur_block->next = NULL;
		cur_block->occupied = left;
		/* Change the pos of filedescriptors which points to the uot of new_size */
		int refs = file_to_resize->refs;
		int i = 0;
		while(refs > 0) {
			if(file_descriptors[i]->file == file_to_resize) {
				if(file_descriptors[i]->pos > new_size) {
					file_descriptors[i]->block_to_act = cur_block;
					file_descriptors[i]->pos = new_size;
				}
				refs --;
			}
			i ++;
		}
	}
	return 0;
}
