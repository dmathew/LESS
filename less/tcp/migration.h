#include <linux/unistd.h>

long ydump(unsigned long pid, char *filename)
{
	return syscall(325, pid, filename);
}

long yresume(char *filename)
{
	return syscall(326, (unsigned long )filename);
}
