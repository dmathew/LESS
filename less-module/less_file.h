#ifndef LESS_H
#define LESS_H

#include <linux/sched.h>
#include <asm/uaccess.h>

struct file* open_dump_file(char *name, int write);
int close_dump_file(struct file* filp);

int dump_write(struct file*, const char *, size_t);
int dump_read(struct file*, char *, size_t);

#endif
