#include <stdlib.h>
#include <stdio.h>

/**
 * User-defined in-memory filesystem. It is as simple as possible.
 * Each file lies in the memory as an array of blocks. A file
 * has an unique file name, and there are no directories, so the
 * FS is a monolite flat contiguous folder.
 */

/**
 * Flags for ufs_open call.
 */
enum open_flags {
	/**
	 * If the flag specified and a file does not exist -
	 * create it.
	 */
	UFS_CREATE = 1,
	/* Open file both to read and to write */
	UFS_READ_WRITE = 0,
	/* Open file only to read */
	UFS_READ_ONLY = 2,
	/* Open file only to write */
	UFS_WRITE_ONLY = 4,
};

/** Possible errors from all functions. */
enum ufs_error_code {
	UFS_ERR_NO_ERR = 0,
	UFS_ERR_NO_FILE,
	UFS_ERR_NO_MEM,
	UFS_ERR_NO_PERMISSION,
};

/** Get code of the last error. */
enum ufs_error_code
get_ufs_errno();

/**
 * Open a file by filename.
 * @param filename Name of a file to open.
 * @param flags Bitwise combination of open_flags.
 *
 * @retval > 0 File descriptor.
 * @retval -1 Error occured. Check ufs_errno() for a code.
 *     - UFS_ERR_NO_FILE - no such file, and UFS_CREATE flag is
 *       not specified.
 */
int
ufs_open(const char *filename, int flags);

/**
 * Write data to the file.
 * @param fd File descriptor from ufs_open().
 * @param buf Buffer to write.
 * @param size Size of @a buf.
 *
 * @retval > 0 How many bytes were written.
 * @retval -1 Error occured. Check ufs_errno() for a code.
 *     - UFS_ERR_NO_FILE - invalid file descriptor.
 *     - UFS_ERR_NO_MEM - not enough memory.
 *     - UFS_ERR_NO_PERMISSION - no permission to write.
 */
int
ufs_write(int fd, const char *buf, size_t size);

/**
 * Read data from the file.
 * @param fd File descriptor from ufs_open().
 * @param buf Buffer to read into.
 * @param size Maximum bytes to read.
 *
 * @retval > 0 How many bytes were read.
 * @retval 0 EOF.
 * @retval -1 Error occured. Check ufs_errno() for a code.
 *     - UFS_ERR_NO_FILE - invalid file descriptor.
 *     - UFS_ERR_NO_PERMISSION - no permission to read.
 */
int
ufs_read(int fd, char *buf, size_t size);

/**
 * Close a file.
 * @param fd File descriptor from ufs_open().
 * @retval 0 Success.
 * @retval -1 Error occured. Check ufs_errno() for a code.
 *     - UFS_ERR_NO_FILE - invalid file descriptor.
 */
int
ufs_close(int fd);

/**
 * Delete a file by its name. Note, that it is allowed to drop the
 * file even if there are opened descriptors. In such a case the
 * file content will live until the last descriptor is closed. If
 * the file is deleted, it is allowed to create a new one with the
 * same name immediately and it should not affect existing opened
 * descriptors of the deleted file.
 *
 * @param filename Name of a file to delete.
 * @retval -1 Error occured. Check ufs_errno() for a code.
 *     - UFS_ERR_NO_FILE - no such file.
 */
int
ufs_delete(const char *filename);

/**
 * Increase or decrease file size.
 * @param fd File descriptor from ufs_open()
 * @param new_size New file size
 * @retval 0 Success.
 *         -1 Error occured. Check ufs_errno() for a code.
 *     - UFS_ERR_NO_FILE - no such file.
 */
int
ufs_resize(int fd, size_t new_size);