#include <linux/file.h>
#include <linux/less.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <asm/string.h>

#include "less_file.h"

int dump_regs(struct task_struct *task, struct file *filp);
int dump_tls(struct task_struct *task, struct file *filp);
int dump_addrspace(struct task_struct *task, struct file *filp);
int dump_vma(struct task_struct *task, struct vm_area_struct *vma, struct file *filp);
int dump_file_info(struct task_struct *task, struct file *filp);

struct task_struct *get_task(unsigned long pid);

/////////////////////////////////////////////////////////////////////
//
//
long ydump(unsigned long pid, char *filename)
{
	struct task_struct *task = NULL;
	struct file *filp = NULL;
	long ret = 0;

	task = get_task(pid);
	if(!task) {
		printi("No such process - %ld", pid);
		ret = -ESRCH;
		goto out;
	}

	filp = open_dump_file(filename, 1); // 1 ==> write
	if(!filp) {
		printi("Cant open file - %s", filename);
		ret = -ENOENT;
		goto err_task;
	}

	if((ret = dump_regs(task, filp)) < 0){
		printi("Error in dumping regs");
		goto err_file;
	}

	if((ret = dump_tls(task, filp)) < 0){
		printi("Error in dumping tls");
		goto err_file;
	}

	if((ret = dump_addrspace(task, filp)) < 0) {
		printi("Error in dumping address-space");
		goto err_file;
	}


	if((ret = dump_file_info(task, filp)) < 0) {
		printi("Error in dumping file info");
		goto err_file;
	}

err_file:
	close_dump_file(filp);
err_task:
	task_unlock(task);
	//put_task_struct(task);
out:
	return ret;
}
/////////////////////////////////////////////////////////////////////
//
//
int dump_regs(struct task_struct *task, struct file *filp)
{
	struct pt_regs regs;
	int ret;

	regs = *task_pt_regs(task);

	ret = dump_write(filp, (const char *)&regs, sizeof(regs));
	if(ret != sizeof(regs))
		return -1;
	return ret;
}
/////////////////////////////////////////////////////////////////////
//
//
int dump_tls(struct task_struct *task, struct file *filp)
{
	int i;
	int ret;
	
	ret = GDT_ENTRY_TLS_ENTRIES;
	if(dump_write(filp, (const char*)&ret, sizeof(ret)) != sizeof(ret)){
		printi("Error in dumping GDT_ENTRY_TLS_ENTRIES");
	}
	for( i = 0; i < GDT_ENTRY_TLS_ENTRIES; ++i){
		ret = dump_write(filp, (const char *)(task->thread.tls_array + i), sizeof(struct desc_struct));
		if( ret != sizeof(struct desc_struct))
			return -1;
	}
	return ret;
}
/////////////////////////////////////////////////////////////////////
//
//
struct task_struct *get_task(unsigned long pid)
{
	struct task_struct *task = NULL;

	rcu_read_lock();
	task = find_task_by_pid(pid);
	rcu_read_unlock();

	task_lock(task);

	return task;
	
}
/////////////////////////////////////////////////////////////////////
//
//
int dump_addrspace(struct task_struct *task, struct file *filp)
{
	struct mm_struct *mm = NULL;
	struct vm_area_struct *vma = NULL;
	int ret = -1;
	int count = 0;

	mm = get_task_mm(task);
	if(!mm){
		printi("No mm for process");
		return -1;
	}

	down_read(&mm->mmap_sem);

	for(vma = mm->mmap; vma; vma = vma->vm_next)
		++count;
	dump_write(filp, (const char *)&count, sizeof(count));

	for(vma = mm->mmap; vma; vma = vma->vm_next) {
		if( dump_vma(task, vma, filp) < 0){
			printi("Error in dumping vma");
			break;
		}
	}
	if(!vma)	// if dumped all vma
		ret = 1;

	dump_write(filp, (const char *)&mm->start_code, sizeof(ulong));
	dump_write(filp, (const char *)&mm->end_code, sizeof(ulong));
        dump_write(filp, (const char *)&mm->start_data, sizeof(ulong));
        dump_write(filp, (const char *)&mm->end_data, sizeof(ulong));
        dump_write(filp, (const char *)&mm->start_brk, sizeof(ulong));
        dump_write(filp, (const char *)&mm->brk, sizeof(ulong));
        dump_write(filp, (const char *)&mm->start_stack, sizeof(ulong));
        dump_write(filp, (const char *)&mm->arg_start, sizeof(ulong));
        dump_write(filp, (const char *)&mm->arg_end, sizeof(ulong));
        dump_write(filp, (const char *)&mm->env_start, sizeof(ulong));
        dump_write(filp, (const char *)&mm->env_end, sizeof(ulong));

	up_read(&mm->mmap_sem);
	mmput(mm);
	return ret;
}
////////////////////////////////////////////////////////////
//
char *file_path(struct file *file, char *buf, int count)
{
	struct dentry *d;
	struct vfsmount *v;
	if (!buf)
		return NULL;
	d = file->f_path.dentry;
	v = file->f_path.mnt;
	buf = d_path(d, v, buf, count);
	return IS_ERR(buf) ? NULL : buf; 
}
////////////////////////////////////////////////////////////
//
int dump_vma(struct task_struct *task, struct vm_area_struct *vma, struct file *filp)
{
	int ret = 1;
	unsigned long length;

	ret = dump_write(filp, (const char*)&vma->vm_start, sizeof(unsigned long));
	if(ret != sizeof(unsigned long)){
		printi("Error in dumping vm_start");
		return -1;
	}

	length = vma->vm_end - vma->vm_start;
	ret = dump_write(filp, (const char*)&length, sizeof(length));
	if(ret != sizeof(length)){
		printi("Error in dumping legnth");
		return -1;
	}

	ret = dump_write(filp, (const char*)&vma->vm_page_prot, sizeof(pgprot_t));
	if(ret != sizeof(pgprot_t)){
		printi("Error in dumping pgprot_t");
		return -1;
	}

	// vm_flags
	ret = dump_write(filp, (const char*)&vma->vm_flags, sizeof(vma->vm_flags));
	if(ret != sizeof(vma->vm_flags)){
		printi("Error in dumping vm_flags");
		return -1;
	}

	//vm_pgoff
	ret = dump_write(filp, (const char*)&vma->vm_pgoff, sizeof(vma->vm_pgoff));

	if(vma->vm_file) {
		char buffer[250], *name;
		ret = 1;		// means mapped to a file
		ret = dump_write(filp, (const char*)&ret, sizeof(ret));
		// dump file name
		name = file_path(vma->vm_file, buffer, sizeof(buffer));
		printi("File: %s", name);
		//printi("File: %s", d_path(vma->vm_file->f_path.dentry, vma->vm_file->f_path.mnt, buffer, 100));
		dump_write(filp, (const char*)name, sizeof(buffer));
	}
	else{
		ret = 0;
		dump_write(filp, (const char*)&ret, sizeof(ret));	// means unmapped area
/*		if (vma->vm_start <= vma->vm_mm->start_brk && vma->vm_end >= vma->vm_mm->brk)
			ret = 1; // means headp
		else if(vma->vm_start <= vma->vm_mm->start_stack && vma->vm_end >= vma->vm_mm->start_stack)
			ret = 2; // means stack
		dump_write(filp, (const char*)&ret, sizeof(ret)); */
	}
 	
	if(!vma->vm_file || vma->vm_flags & VM_WRITE){		// dump all data
		int num_pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
		int i;
		char *buffer = NULL;
		unsigned long start;

		buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if(buffer == NULL) {
			printi("No memory");
			return -1;
		}
		start = vma->vm_start;
		for(i = 0 ; i < num_pages; ++i){
			ulong size = length - i * PAGE_SIZE < PAGE_SIZE ? length - i * PAGE_SIZE : PAGE_SIZE;
			if(access_process_vm(task, start, buffer, size, 0) == 0){
				printi("ERrror in copying vma");
			}
			dump_write(filp, buffer, size);
			start += size;
		}
		kfree(buffer);
	}

	ret = 1;
//	printi("Vma copied successfully");

	return ret;
}
/////////////////////////////////////////////////////////////////////
//
//
int count_open_files(struct fdtable *fdt)
{
	int size = fdt->max_fds;
	int i;
	for (i = size/(8*sizeof(long)); i > 0; ) {
		if (fdt->open_fds->fds_bits[--i])
			break; 
	} 
	i = (i+1) * 8 * sizeof(long);
	return i;
}
/////////////////////////////////////////////////////////////////////
//
//
int dump_file_info(struct task_struct *task, struct file *filp)
{
	struct files_struct *files = task->files;
	struct fdtable  *fdt = NULL;
	int fd = 3;
	int count = 0;
	struct file *file = NULL;
	char buffer[255], *name;
	int name_length;
	loff_t offset, temp_offset;
	int number_of_fds = 0;

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);

/*	while(fdt->fd[count++]);
	count -= 1 + 3;*/
	count = count_open_files(fdt);
	offset = filp->f_pos;
	dump_write(filp, (const char *)&count, sizeof(count));

	for(fd = 3; fd < count; fd++) {
		file = fdt->fd[fd];
		if(!file)
			continue;
		name = file_path(file, buffer, sizeof(buffer));
		name_length = strlen(name) + 1;  //for including '\0'
		dump_write(filp, (const char *)&name_length, sizeof(int));
		dump_write(filp, name, name_length);

		dump_write(filp, (const char *)&fd, sizeof(fd));
		dump_write(filp, (const char *)&file->f_flags, sizeof(unsigned int));
		dump_write(filp, (const char *)&file->f_pos, sizeof(file->f_pos));

//		dump_write(filp, (const char *)&file->f_mode, sizeof(file->f_mode));

		number_of_fds++;
		printi("File : %s, %d", name, fd);
	}
	spin_unlock(&files->file_lock);
	
	temp_offset = filp->f_pos;
	filp->f_pos = offset;
	dump_write(filp, (const char *)&number_of_fds, sizeof(number_of_fds));
	filp->f_pos = temp_offset;

	return 0;
}
