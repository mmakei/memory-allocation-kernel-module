#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <asm/tlbflush.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include "../common.h"
#include "memalloc-common.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maryam Makei");
MODULE_DESCRIPTION("Project 4, CSE 330 Spring 2025");

#define DEVICE_NAME "memalloc"
#define CLASS_NAME "memalloc_class"

static int majorNumber;
static struct class* memallocClass = NULL;
static struct device* memallocDevice = NULL;

#define PAGE_PERMS_RW __pgprot(PTE_TYPE_PAGE | PTE_AF | PTE_UXN | PTE_PXN | PTE_WRITE)
#define PAGE_PERMS_R  __pgprot(PTE_TYPE_PAGE | PTE_AF | PTE_UXN | PTE_PXN)

static int is_page_mapped(unsigned long vaddr) {
    pgd_t *pgd = pgd_offset(current->mm, vaddr);
    if (pgd_none(*pgd)) return 0;
    p4d_t *p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d)) return 0;
    pud_t *pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud)) return 0;
    pmd_t *pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd)) return 0;
    pte_t *pte = pte_offset_kernel(pmd, vaddr);
    return !pte_none(*pte);
}

static long memalloc_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    if (cmd == ALLOCATE) {
        struct alloc_info req;
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;

        if (req.num_pages > 4096)
            return -ENOENT;

        for (int i = 0; i < req.num_pages; i++) {
            unsigned long vaddr = req.vaddr + i * PAGE_SIZE;
            if (is_page_mapped(vaddr)) return -EINVAL;

            void *page = (void *)get_zeroed_page(GFP_KERNEL_ACCOUNT);
            if (!page) return -ENOMEM;

            unsigned long paddr = __pa(page);
            pgd_t *pgd = pgd_offset(current->mm, vaddr);
            p4d_t *p4d = p4d_offset(pgd, vaddr);
            if (p4d_none(*p4d)) memalloc_pud_alloc(p4d, vaddr);
            pud_t *pud = pud_offset(p4d, vaddr);
            if (pud_none(*pud)) memalloc_pmd_alloc(pud, vaddr);
            pmd_t *pmd = pmd_offset(pud, vaddr);
            if (pmd_none(*pmd)) memalloc_pte_alloc(pmd, vaddr);
            pte_t *pte = pte_offset_kernel(pmd, vaddr);
            pte_t new_pte = pfn_pte(paddr >> PAGE_SHIFT,
                req.write ? PAGE_PERMS_RW : PAGE_PERMS_R);
            set_pte_at(current->mm, vaddr, pte, new_pte);
            flush_tlb_kernel_range(vaddr, vaddr + PAGE_SIZE);
        }

    } else if (cmd == FREE) {
        struct free_info req;
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;

        unsigned long vaddr = req.vaddr;
        pgd_t *pgd = pgd_offset(current->mm, vaddr);
        p4d_t *p4d = p4d_offset(pgd, vaddr);
        pud_t *pud = pud_offset(p4d, vaddr);
        pmd_t *pmd = pmd_offset(pud, vaddr);
        pte_t *pte = pte_offset_kernel(pmd, vaddr);

        if (!pte_none(*pte)) {
            unsigned long paddr = (pte_val(*pte) & PAGE_MASK);
            void *page = __va(paddr);
            free_page((unsigned long)page);
            pte_clear(current->mm, vaddr, pte);
            flush_tlb_kernel_range(vaddr, vaddr + PAGE_SIZE);
        }

    } else {
        return -EINVAL;
    }

    return 0;
}

static int dev_open(struct inode *inodep, struct file *filep) { return 0; }
static int dev_release(struct inode *inodep, struct file *filep) { return 0; }

static int memalloc_mmap(struct file *filp, struct vm_area_struct *vma) {
    return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff << PAGE_SHIFT,
                           vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static struct file_operations fops = {
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = memalloc_ioctl,
    .mmap = memalloc_mmap,
};

bool memalloc_ioctl_init(void) { return true; }
void memalloc_ioctl_teardown(void) {}

static int __init memalloc_init(void) {
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) return majorNumber;

    memallocClass = class_create(CLASS_NAME);
    if (IS_ERR(memallocClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(memallocClass);
    }

    memallocDevice = device_create(memallocClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(memallocDevice)) {
        class_destroy(memallocClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(memallocDevice);
    }

    return 0;
}

static void __exit memalloc_exit(void) {
    device_destroy(memallocClass, MKDEV(majorNumber, 0));
    class_unregister(memallocClass);
    class_destroy(memallocClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
}

module_init(memalloc_init);
module_exit(memalloc_exit);
