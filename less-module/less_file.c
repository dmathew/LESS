#include "less_file.h"
#include <linux/fs.h>
#include <asm/processor.h>

struct file* open_dump_file(char *name, int write)
{
	struct file *filp = NULL;
	int flags = O_NOFOLLOW | O_LARGEFILE;

	if(write)
		flags |= O_CREAT | O_WRONLY;
	else
		flags |= O_RDONLY;

	filp = filp_open(name, flags, 0644);
	if(IS_ERR(filp))
		return NULL;

	return filp;

}

int close_dump_file(struct file *filp)
{
	return filp_close(filp, NULL);
}

int dump_write(struct file *filp, const char *buffer, size_t count)
{
	mm_segment_t fs;
	int ret = 0;
	loff_t offset;

	offset = filp->f_pos;

	fs = get_fs();
	set_fs(KERNEL_DS);

	ret = vfs_write(filp, buffer, count, &offset);
	filp->f_pos = offset; 
	

	set_fs(fs);

	return ret;
}

int dump_read(struct file *filp, char *buffer, size_t count)
{
	mm_segment_t fs;
	int ret_value = 0;
	loff_t offset;

	offset = filp->f_pos;

	fs = get_fs();
	set_fs(KERNEL_DS);

	ret_value = vfs_read(filp, buffer, count, &offset);
	filp->f_pos = offset;
	
	set_fs(fs);

	return ret_value;
}
