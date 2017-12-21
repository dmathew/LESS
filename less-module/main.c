#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/less.h>

extern long ydump(unsigned long pid, char *filename);
extern long yresume(char *filename);

int init_function(void)
{
	set_ydump_fn(ydump);
	set_yresume_fn(yresume);

	return 0;
}
/////////////////////////////////////////////////////////////////////
//
void cleanup_function(void)
{
	set_ydump_fn(NULL);
	set_yresume_fn(NULL);
}
/////////////////////////////////////////////////////////////////////
//
//
module_init(init_function);
module_exit(cleanup_function);

MODULE_LICENSE("GPL");
