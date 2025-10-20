#include "vmm.h"
#include "pmm.h"
#include "../console/console.h"
#include "../io/serial.h"
#include "../libc/string.h"

// Page table entry indices from virtual address
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

// Current page table root (physical address)
static uint64_t current_pml4_phys = 0;

//Get pointer to page table(using physical memory map)
static inline uint64_t* get_table(uint64_t phys_addr) {
    return (uint64_t*)PHYS_TO_VIRT(phys_addr);
}

kerr_t vmm_init(void) {
    //Get current CR3
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    current_pml4_phys = cr3;

    serial_debug_puts("[VMM] Current PML4 at: ");
    serial_puthex(COM1, current_pml4_phys, 16);
    serial_debug_puts("\n");

    return E_OK;
}

kerr_t vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    if (!IS_PAGE_ALIGNED(virt_addr) || !IS_PAGE_ALIGNED(phys_addr)) return E_INVALID;

    //Get indices
    uint16_t pml4_i = PML4_INDEX(virt_addr);
    uint16_t pdpt_i = PDPT_INDEX(virt_addr);
    uint16_t pd_i = PD_INDEX(virt_addr);
    uint16_t pt_i = PT_INDEX(virt_addr);

    //Get PML4
    uint64_t* pml4 = get_table(current_pml4_phys);

    // Get or create PDPT
    uint64_t* pdpt;
    if (!(pml4[pml4_i] & PAGE_PRESENT)) {
        uint64_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) return E_NOMEM;

        pdpt = get_table(pdpt_phys);
        memset(pdpt, 0, PAGE_SIZE);

        pml4[pml4_i] = pdpt_phys | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pdpt = get_table(pte_get_address(pml4[pml4_i]));
    }

    // Get or create PD
    uint64_t* pd;
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) {
        uint64_t pd_phys = pmm_alloc_page();
        if (!pd_phys) return E_NOMEM;

        pd = get_table(pd_phys);
        memset(pd, 0, PAGE_SIZE);

        pdpt[pdpt_i] = pd_phys | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pd = get_table(pte_get_address(pdpt[pdpt_i]));
    }

    // Get or create PT
    uint64_t* pt;
    if (!(pd[pd_i] & PAGE_PRESENT)) {
        uint64_t pt_phys = pmm_alloc_page();
        if (!pt_phys) return E_NOMEM;

        pt = get_table(pt_phys);
        memset(pt, 0, PAGE_SIZE);

        pd[pd_i] = pt_phys | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pt = get_table(pte_get_address(pd[pd_i]));
    }

    // Map the page
    pt[pt_i] = phys_addr | flags;

    // Flush TLB for this page
    vmm_flush_tlb_page(virt_addr);

    return E_OK;
}

kerr_t vmm_unmap_page(uint64_t virt_addr) {
    if (!IS_PAGE_ALIGNED(virt_addr)) {
        return E_INVALID;
    }

    uint16_t pml4_i = PML4_INDEX(virt_addr);
    uint16_t pdpt_i = PDPT_INDEX(virt_addr);
    uint16_t pd_i = PD_INDEX(virt_addr);
    uint16_t pt_i = PT_INDEX(virt_addr);

    uint64_t* pml4 = get_table(current_pml4_phys);
    if (!(pml4[pml4_i] & PAGE_PRESENT)) return E_NOTFOUND;

    uint64_t* pdpt = get_table(pte_get_address(pml4[pml4_i]));
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) return E_NOTFOUND;

    uint64_t* pd = get_table(pte_get_address(pdpt[pdpt_i]));
    if (!(pd[pd_i] & PAGE_PRESENT)) return E_NOTFOUND;

    uint64_t* pt = get_table(pte_get_address(pd[pd_i]));
    if (!(pt[pt_i] & PAGE_PRESENT)) return E_NOTFOUND;

    pt[pt_i] = 0;
    vmm_flush_tlb_page(virt_addr);

    return E_OK;
}

uint64_t vmm_get_physical(uint64_t virt_addr) {
    uint16_t pml4_i = PML4_INDEX(virt_addr);
    uint16_t pdpt_i = PDPT_INDEX(virt_addr);
    uint16_t pd_i = PD_INDEX(virt_addr);
    uint16_t pt_i = PT_INDEX(virt_addr);

    uint64_t* pml4 = get_table(current_pml4_phys);
    if (!(pml4[pml4_i] & PAGE_PRESENT)) return 0;

    uint64_t* pdpt = get_table(pte_get_address(pml4[pml4_i]));
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) return 0;

    uint64_t* pd = get_table(pte_get_address(pdpt[pdpt_i]));
    if (!(pd[pd_i] & PAGE_PRESENT)) return 0;

    // Check for 2MB huge page
    if (pd[pd_i] & PAGE_HUGE) {
        uint64_t phys_base = pte_get_address(pd[pd_i]);
        uint64_t offset = virt_addr & 0x1FFFFF;  // 2MB offset
        return phys_base + offset;
    }

    uint64_t* pt = get_table(pte_get_address(pd[pd_i]));
    if (!(pt[pt_i] & PAGE_PRESENT)) return 0;

    uint64_t phys_base = pte_get_address(pt[pt_i]);
    uint64_t offset = virt_addr & 0xFFF;  // 4KB offset
    return phys_base + offset;
}

int vmm_is_mapped(uint64_t virt_addr) {
    return vmm_get_physical(virt_addr) != 0;
}

kerr_t vmm_alloc_page(uint64_t virt_addr, uint64_t flags) {
    uint64_t phys_addr = pmm_alloc_page();
    if (!phys_addr) return E_NOMEM;

    kerr_t err = vmm_map_page(virt_addr, phys_addr, flags);
    if (err != E_OK) {
        pmm_free_page(phys_addr);
        return err;
    }

    return E_OK;
}

kerr_t vmm_free_page(uint64_t virt_addr) {
    uint64_t phys_addr = vmm_get_physical(virt_addr);
    if (!phys_addr) return E_NOTFOUND;

    kerr_t err = vmm_unmap_page(virt_addr);
    if (err != E_OK) return err;

    pmm_free_page(PAGE_ALIGN_DOWN(phys_addr));
    return E_OK;
}

uint64_t vmm_get_cr3(void) {
    return current_pml4_phys;
}

void vmm_flush_tlb_page(uint64_t virt_addr) {
    asm volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
}

void vmm_flush_tlb(void) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}