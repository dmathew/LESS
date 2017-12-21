#include <linux/less.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/mman.h>
#include <linux/namei.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fsnotify.h>
#include <asm/unistd.h>

#include "less_file.h"

int resume_regs(struct file *filp);
int resume_tls(struct file *filp);
int resume_addrspace(struct file *filp);
int resume_vma(struct file *filp);
int resume_file_info(struct file *filp);
static void close_files(void);
/////////////////////////////////////////////////////////////////////
//
//
struct pt_regs pt_regs;
long yresume(char *filename)
{
	struct file *filp = NULL;
	long ret;

	filp = open_dump_file(filename, 0);
	if(!filp) {
		printi("Error in opening file - %s", filename);
		ret = -ENOENT;
		goto out;
	}

	close_files();

	if((ret = resume_regs(filp)) < 0) {
		printi("Error in resuming regs");
		goto file_out;
	}

	if((ret = resume_tls(filp)) < 0) {
		printi("Error in resuming tls");
		goto file_out;
	}

	if((ret = resume_addrspace(filp)) < 0) {
		printi("Error in resuming addresspace");
		goto file_out;
	}

	if((ret = resume_file_info(filp)) < 0) {
		printi("Error in resuming file info");
		goto file_out;
	}

file_out:
	close_dump_file(filp);
out:
	if(ret >= 0)
		set_thread_flag(TIF_IRET);

	return ret;
}
/////////////////////////////////////////////////////////////////////
//
//
int resume_regs(struct file *filp)
{
//	struct pt_regs pt_regs;
	int ret;

	ret = dump_read(filp, (char *)&pt_regs, sizeof(pt_regs));
	if(ret != sizeof(pt_regs)){
		printi("error in reading pt_regs");
		return -1;
	}
	*task_pt_regs(current) = pt_regs;
//	regs = pt_regs;

	return 0;
}
////////////////////////////////////////////////////////////
//
int resume_tls(struct file *filp)
{
	int i, count;
	struct desc_struct desc_struct;

	// read count - no. of tls entries
	dump_read(filp, (char *)&count, sizeof(count));
	printi("no of tls = %d", count);
	for(i = 0; i < count; ++i) {
		dump_read(filp, (char *)&desc_struct, sizeof(desc_struct));
		current->thread.tls_array[i] = desc_struct;
	}

	return 0;
}
////////////////////////////////////////////////////////////
//
int resume_addrspace(struct file *filp)
{
	int ret;
	struct mm_struct *mm = NULL;
	int i, count = 0;

	mm = current->mm;
	if(!mm){
		printi("no mm found");
		return -1;
	}
	down_write(&mm->mmap_sem);
	//unmap all addresspace
	ret = do_munmap(mm, 0x0, TASK_SIZE);
	up_write(&mm->mmap_sem);
	if(ret < 0) {
		printi("error: cannot unmap old");
		goto err_sem;
	}
	dump_read(filp, (char *)&count, sizeof(count));
	for(i = 0; i < count; ++i){
		if((ret = resume_vma(filp)) < 0){
			printi("error in resuming vma");
			goto err_sem;
		}
	}
	
	down_write(&mm->mmap_sem);

	dump_read(filp, (char *)&mm->start_code, sizeof(ulong));
        dump_read(filp, (char *)&mm->end_code, sizeof(ulong));
        dump_read(filp, (char *)&mm->start_data, sizeof(ulong));
        dump_read(filp, (char *)&mm->end_data, sizeof(ulong));
        dump_read(filp, (char *)&mm->start_brk, sizeof(ulong));
        dump_read(filp, (char *)&mm->brk, sizeof(ulong));
        dump_read(filp, (char *)&mm->start_stack, sizeof(ulong));
        dump_read(filp, (char *)&mm->arg_start, sizeof(ulong));
        dump_read(filp, (char *)&mm->arg_end, sizeof(ulong));
        dump_read(filp, (char *)&mm->env_start, sizeof(ulong));
        dump_read(filp, (char *)&mm->env_end, sizeof(ulong));

	ret = 0;

	up_write(&mm->mmap_sem);

err_sem:
	printi("Addr-space completed");
	return ret;

}
////////////////////////////////////////////////////////////
//
int resume_vma(struct file *filp)
{
	unsigned long start, end, length;
	unsigned long addr = 0;
	pgprot_t vm_page_prot;
	unsigned long vm_flags, vm_pgoff;
	int mapped;
	char file_name[250];
	struct file *file = NULL;

	unsigned long flags = 0, prot = 0;

	dump_read(filp, (char *)&start, sizeof(unsigned long));
	dump_read(filp, (char *)&length, sizeof(unsigned long));
	end = start + length;
	printi("start :%lx, end %lx", start, end);

	dump_read(filp, (char *)&vm_page_prot, sizeof(vm_page_prot));
	
	dump_read(filp, (char *)&vm_flags, sizeof(vm_flags));
	dump_read(filp, (char *)&vm_pgoff, sizeof(vm_pgoff));

	dump_read(filp, (char *)&mapped, sizeof(mapped));
	if(mapped){
		dump_read(filp, file_name, sizeof(file_name));
		printi("file: %s", file_name);
		file = open_library_file(file_name);
		if(!file){
			printi("error in opening - %s", file_name);
			return -1;
		}
	}

	// now do_mmap
	if(vm_flags & VM_READ) prot |= PROT_READ;
	if(vm_flags & VM_WRITE) prot |= PROT_WRITE;
	if(vm_flags & VM_EXEC) prot |= PROT_EXEC;
	if(vm_flags & VM_GROWSDOWN) prot |= PROT_GROWSDOWN;
	if(vm_flags & VM_GROWSUP) prot |= PROT_GROWSUP;

	if(vm_flags & VM_MAYSHARE || vm_flags & VM_SHARED)
		flags |= MAP_SHARED;
	else
		flags |= MAP_PRIVATE;
	if(vm_flags & VM_GROWSDOWN) flags |= MAP_GROWSDOWN;
	if(vm_flags & VM_DENYWRITE) flags |= MAP_DENYWRITE;
	if(vm_flags & VM_LOCKED) flags |= MAP_LOCKED;
	if(vm_flags & VM_EXECUTABLE) flags |= MAP_EXECUTABLE;

	flags |= MAP_FIXED;

	if(!file)
		flags |= MAP_ANONYMOUS;

	down_write(&current->mm->mmap_sem);

	addr = do_mmap_pgoff(file, start, length, prot, flags, vm_pgoff);

	up_write(&current->mm->mmap_sem);

	if(addr > TASK_SIZE) {
		printi("addr : %lx - may be error", -addr);
		return -1;
	}

	if(!mapped || vm_flags & VM_WRITE) {
		int num_pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
		int i;
		char *buffer = NULL;
		ulong src;

		buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if(!buffer){
			printi("no memory");
			return -1;
		}
		src = start;
		for(i = 0; i < num_pages; ++i) {
			ulong size = length - i * PAGE_SIZE < PAGE_SIZE ? length - i * PAGE_SIZE : PAGE_SIZE;
			dump_read(filp, buffer, size);
			if(access_process_vm(current, src, buffer, size, 1) == 0){
				printi("error in writing vma");
				return -1;
			}
			src += size;
		}
		kfree(buffer);
	}

	printi("completed one vma");
	return 0;
}
/////////////////////////////////////////////////////////////////////
//
//
extern void fastcall fd_install(unsigned int fd, struct file * file);
int resume_file_info(struct file *filp) 
{
	int count, i, name_length;
	char name[255];
	unsigned int f_flags;
	int fd;
	struct file *file;
	loff_t f_pos;
	struct fdtable *fdt;
	struct files_struct *files;

	dump_read(filp, (char *)&count, sizeof(count));
	printi("Count %d", count);
	for(i = 0; i < count; ++i){
		dump_read(filp, (char *)&name_length, sizeof(name_length));
		dump_read(filp, name, name_length);

		dump_read(filp, (char *)&fd, sizeof(fd));
		dump_read(filp, (char *)&f_flags, sizeof(f_flags));
		dump_read(filp, (char *)&f_pos, sizeof(f_pos));
			

		printi("File : %s, %d ,%d", name, fd, f_flags);
repeat:
		file = filp_open(name, f_flags, 0);
		if(IS_ERR(file))
			file = NULL;

		if(!file) {
			printi("Errror in restoring file, retrying - %s", name);
			if(f_flags & O_CREAT) {
				printi("Failed - file %s", name);
				continue;
			}
			f_flags |= O_CREAT;
			goto repeat;
		}
		
		files = current->files;
		spin_lock(&files->file_lock);
		fdt = files_fdtable(files);
		FD_SET(fd, fdt->open_fds);
		spin_lock(&files->file_lock);

		fsnotify_open(file->f_path.dentry);	
		fd_install(fd, file);
		file->f_pos = f_pos;

/*		if(vfs_llseek(file, f_pos, 0) != f_pos) {
			printi("Seeking error");
		}*/

		if(file) {
			printi("resumed file : %s", name);
		}
		else{
			printi("Can't resume file: %s", name);
		}
	}

	return 0;
}
/////////////////////////////////////////////////////////////////////
//
//
static void close_files(void)
{
	struct files_struct *files ;
	struct fdtable *fdt;

	int i, j;
	j = 0;

	files = current->files;
	fdt = files_fdtable(files);
	
	for (;;) { 
		unsigned long set; 
		i = j * __NFDBITS; 
		if (i >= fdt->max_fds) 
			break; 
		set = fdt->open_fds->fds_bits[j++];
		while (set) { 
			if(i < 3) goto next;
			if (set & 1) {
				struct file * file = xchg(&fdt->fd[i], NULL); 
				if (file) { 
					filp_close(file, files); 
				} 
			}
next:
			i++; 
			set >>= 1; 
		} 
	} 
}
/////////////////////////////////////////////////////////////////////
//
//

