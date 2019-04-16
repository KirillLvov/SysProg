#include "userfs.h"

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 1024,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_errno = UFS_ERR_NO_ERR;

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

	/* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
	struct file *file;
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
get_ufs_errno()
{
	return ufs_errno;
}

int
ufs_open(const char *filename, int flags)
{
	struct file *probe;
	probe->next = file_list;
	struct filedesc new_filedesc;
	if(!flags)
	{
		ufs_errno = UFS_ERR_NO_FILE;
		return -1;
	}
	while(probe->next != NULL) {
		probe = probe->next;
		if(*(probe->name) == *filename) {
			new_filedesc.file = probe;
			probe->refs ++;
		}
	}
	struct file new_file;
	new_file.block_list = NULL;
	new_file.last_block = NULL;
	new_file.name = filename;
	new_file.next = NULL;
	new_file.prev = NULL;
	new_file.refs = 1;
	probe->next = &new_file;
	new_filedesc.file = &new_file;
	file_descriptor_count ++;
	file_descriptors = (struct filedesc **)realloc(file_descriptors, sizeof(struct filedesc *)*file_descriptor_count);
	file_descriptors[file_descriptor_count - 1] = &new_filedesc;
	return file_descriptor_count - 1;
}

int
ufs_write(int fd, const char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
}

int
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
}

int
ufs_close(int fd)
{
	if(fd >= file_descriptor_count) {
		ufs_errno = UFS_ERR_NO_FILE;
		return -1;
	}
	file_descriptors[fd]->file->refs --;
	file_descriptors[fd] = NULL;
	return 0;
}

void main (void)
{
	system("pause");
}