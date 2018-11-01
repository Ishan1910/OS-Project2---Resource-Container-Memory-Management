//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "memory_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

struct mutex lock;

struct address_list{
    __u64 offset;
    __u64 pfn;
    struct address_list *next;
};

struct task_list{
    int task_id;
    struct task_list *next;
    struct task_struct *current_task;
};

struct container_list{
    int cid;
    struct container_list *next;
    struct task_list *tasks;
    struct address_list *address;
    struct mutex addr_lock;
}*cont_head = NULL;

void newContainer(int cid, int task_id)
{
    struct container_list *curr = NULL;
    struct container_list *temp = kmalloc(sizeof(struct container_list), GFP_KERNEL);
    if (cont_head == NULL)
    {
        cont_head = temp;
        cont_head->cid = cid;
        cont_head->next = NULL;
        cont_head->tasks = kmalloc(sizeof(struct task_list), GFP_KERNEL);
        (cont_head->tasks)->current_task = current;
        (cont_head->tasks)->task_id = task_id;
        (cont_head->tasks)->next = NULL;
        cont_head->address = NULL;
        // printk(KERN_ERR "Container is %d \n", cont_head->cid);
        mutex_unlock(&lock);
        return;
    }
    else
    {
        curr = cont_head;
        while(curr->next != NULL)
        {   
            curr = curr->next;
        }
        curr->next = temp;
        temp->cid = cid;
        temp->next = NULL;
        temp->tasks = kmalloc(sizeof(struct task_list), GFP_KERNEL);
        (temp->tasks)->current_task = current;
        (temp->tasks)->task_id = task_id;
        (temp->tasks)->next = NULL;
        temp->address = NULL;
        // printk(KERN_ERR "Next Container is %d\n", temp->cid);
        mutex_unlock(&lock);
        return;
    }
}

struct task_list *addTask(struct task_list *head_task, struct task_struct *current_task, int task_id){
    struct task_list *curr;
    struct task_list *new_task = kmalloc(sizeof(struct task_list), GFP_KERNEL);
    new_task->current_task = current_task;
    new_task->next = NULL;
    new_task->task_id = task_id;
    // printk("\n assigning id is %d \n", task_id);

    if (head_task == NULL)
    {
        head_task = new_task;
        // printk(KERN_ERR "\n 1st thread is %d\n", head_task->task_id);
        mutex_unlock(&lock);
        return head_task;
    }
    else
    {
        curr = head_task;
        if (curr->next == NULL)
        {
            curr->next = new_task;
            mutex_unlock(&lock);
            // printk("\n 1st thread is %d\n", head_task->task_id);
            // printk(KERN_ERR "\n2nd thread is %d \n", (head_task->next)->task_id);
            return head_task;
        }
        else
        {
            while(curr->next != NULL)
            {
                curr = curr->next;
            }
            curr->next = new_task;
            mutex_unlock(&lock);
            // printk(KERN_ERR "\nnext thread is %d \n", new_task->task_id);
            return head_task;
        }
        curr->next = new_task;
    }
}

struct address_list *newObject(struct address_list *addr, __u64 offset, __u64 pfn)
{
    struct address_list *curr = NULL;
    struct address_list *new = NULL;
    static int cnt = 0;
    if (addr == NULL)
    {
        addr = kmalloc(sizeof(struct address_list), GFP_KERNEL);
        addr->offset = offset;
        addr->next = NULL;
        addr->pfn = pfn;
        return addr;
    }
    else
    {
        curr = addr;
        if (curr->next == NULL)
        {
            new = kmalloc(sizeof(struct address_list), GFP_KERNEL);
            new->offset = offset;
            new->next = NULL;
            new->pfn = pfn;
            curr->next = new;
            return addr;
        }
        else
        {
            curr = addr;
            while (curr->next != NULL)
            {
                curr = curr->next;
            }
            new = kmalloc(sizeof(struct address_list), GFP_KERNEL);
            new->offset = offset;
            new->next = NULL;
            new->pfn = pfn;
            curr->next = new;
            return addr;
        }
    }
}

struct container_list *currContainer(struct task_struct *curr)
{
    struct container_list *cont = cont_head;
    struct task_list *task = NULL;

    while (cont != NULL)
    {
        task = cont->tasks;
        while (task != NULL)
        {
            if (task->current_task == curr)
            {
                return cont;
            }
            task = task->next;
        }
        cont = cont->next;
    }
    return NULL;
}

struct task_list *removeTask(struct task_struct *current_task, struct task_list *task)
{
    struct task_list *curr = task;
    struct task_list *temp = NULL;
    struct task_list *next = NULL;
    if (task == NULL)
    {
        printk("Task already deleted\n");
        mutex_unlock(&lock);
        return task;
    }
    else if (task->next == NULL)
    {
        if (task->current_task == current_task)
        {
            kfree(task);
            task = NULL;
            mutex_unlock(&lock);
            return task;
        }
    }
    else
    {
        while(curr != NULL)
        {
            next = curr->next;
            if (curr->current_task == current_task)
            {
                temp = curr->next;
                kfree(curr);
                curr = NULL;
                mutex_unlock(&lock);
                return temp;
            }
            else if (next != NULL && next->current_task == current_task)
            {
                temp = next->next;
                kfree(next);
                next = NULL;
                curr->next = temp;
                mutex_unlock(&lock);
                return task;
            }
            curr = curr->next;
        }
        printk("Task not found");
        return task;
    }
    mutex_unlock(&lock);
    return curr;
}

void removeContainer(void)
{
    struct container_list *curr = cont_head;
    struct container_list *temp = NULL;

    if (curr == NULL)
    {
        printk("Container is already deleted\n");
        return;
    }
    else
    {
        while(curr != NULL)
        {
            temp = curr->next;
            kfree(curr);
            curr = NULL;
            curr = temp;
        }
        printk("Containers freed\n");
        return;
    }
}

struct address_list *removeobject(struct address_list *addr, __u64 offset)
{
    struct container_list *cont = currContainer(current);
    struct address_list *curr = addr;
    struct address_list *temp = NULL;
    struct address_list *next = NULL;

    if (addr->next == NULL)
    {
        // printk("Only one task\n");
        if (addr->offset == offset)
        {
            kfree(addr);
            addr = NULL;
            return addr;
        }
    }
    else
    {
        while (curr != NULL)
        {
            // printk("More than one\n");
            next = curr->next;
            if (curr->offset == offset)
            {
                // printk("Current matched\n");
                temp = curr->next;
                kfree(curr);
                curr = NULL;
                return temp;
            }
            else if (next != NULL && next->offset == offset)
            {
                // printk("Next matched\n");
                temp = next->next;
                kfree(next);
                next = NULL;
                curr->next = temp;
                return addr;
            }
            curr = curr->next;  
        }
        // printk("No object matched to be freed\n");
        return addr;
    }
    return addr;
}


int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct container_list *curr = currContainer(current);
    struct address_list *addr = curr->address;
    int obj_present = 0;

    // printk(KERN_ERR "\n Current object is %lu \n\n", vma->vm_pgoff);

    if (addr != NULL){
        while (addr != NULL)
        {
            if (addr->offset == vma->vm_pgoff)
            {
                obj_present = 1;
                __u64 pfn = addr->pfn;
                // printk("\n PFN for %lu is \t %llu \n", vma->vm_pgoff, pfn);
                remap_pfn_range(vma, vma->vm_start, pfn, vma->vm_end - vma->vm_start, vma->vm_page_prot);
                break;
            }
            addr = addr->next;
        }

    }

    if (!obj_present)
    {
        __u64 phys = virt_to_phys((void *)kmalloc(vma->vm_end - vma->vm_start, GFP_KERNEL));
        __u64 pfn = phys >> PAGE_SHIFT;
        // printk("\n Outside PFN for %lu is \t %llu \n", vma->vm_pgoff, pfn);
        remap_pfn_range(vma, vma->vm_start, pfn, vma->vm_end - vma->vm_start, vma->vm_page_prot);
        curr->address = newObject(curr->address, vma->vm_pgoff, pfn);
    }
    

    return 0;
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
    struct container_list *cont = currContainer(current);

    if (cont->address == NULL) mutex_init(&(cont->addr_lock));
    mutex_lock(&(cont->addr_lock));
    // printk("\nLock acquired by %d\n", current->pid);
    return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
    struct container_list *cont = currContainer(current);
    struct task_list *task = cont->tasks;

    while (task != NULL)
    {
        if (task->current_task == current)
        {
            mutex_unlock(&(cont->addr_lock));
            // printk("\n Unlocked by %d\n", current->pid);
        }
        task = task->next;
    }
    return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
    struct container_list *curr = currContainer(current);
    // printk("Deletion by %d", current->pid);

    mutex_lock(&lock);

    // static int cnt = 0;
    if (curr->tasks != NULL)
    {
        // cnt++;
        // printk("%d\n", cnt);
        curr->tasks = removeTask(current, curr->tasks);
    }
    // removeContainer();
    // printk("\n ***************Finished Deleting*******************\n");
    return 0;
}


int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
    int cid=0, task_id=0;
    int cont_present = 0;
    struct memory_container_cmd *cont = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
    copy_from_user(cont, user_cmd, sizeof(struct memory_container_cmd));

    if (cont_head == NULL)
    {
        mutex_init(&lock);
    } 
    
    mutex_lock(&lock);
    cid = cont->cid;
    task_id = current->pid;

    kfree(cont);

    // add tasks to the existing container
    if (cont_head != NULL)
    {
        struct container_list *curr = cont_head;
        while(curr->next != NULL)
        {
            if (curr->cid == cid)
            {
                cont_present = 1;
                curr->tasks = addTask(curr->tasks, current, current->pid);
                return 0;
            }
            curr = curr->next;
        }

        if (curr->next == NULL && curr->cid == cid)
        {
            cont_present = 1;
            curr->tasks = addTask(curr->tasks, current, current->pid);
            return 0;
        }
    }

    // add a new container    
    if (!cont_present)
    {
        newContainer(cid, task_id);
        // printk("Head container is %d", cont_head->cid);
    }

    return 0;
}


int memory_container_free(struct memory_container_cmd __user *user_cmd)
{
    __u64 oid = 0;
    struct container_list *cont = currContainer(current);
    struct memory_container_cmd *obj = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
    copy_from_user(obj, user_cmd, sizeof(struct memory_container_cmd));
    oid = obj->oid;
    kfree(obj);

    if (cont->address != NULL)
    {   
        cont->address = removeobject(cont->address, oid); 
    }
    else printk("Already freed\n");
    // printk("\n **************After freeing***************\n");
    return 0;
}


/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int memory_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case MCONTAINER_IOCTL_CREATE:
        return memory_container_create((void __user *)arg);
    case MCONTAINER_IOCTL_DELETE:
        return memory_container_delete((void __user *)arg);
    case MCONTAINER_IOCTL_LOCK:
        return memory_container_lock((void __user *)arg);
    case MCONTAINER_IOCTL_UNLOCK:
        return memory_container_unlock((void __user *)arg);
    case MCONTAINER_IOCTL_FREE:
        return memory_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
