// Initialize PCI devices (on emulators)
//
// Copyright (C) 2008  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2006 Fabrice Bellard
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "util.h" // dprintf
#include "pci.h" // pci_config_readl
#include "biosvar.h" // GET_EBDA
#include "pci_ids.h" // PCI_VENDOR_ID_INTEL
#include "pci_regs.h" // PCI_COMMAND

#define PCI_ROM_SLOT 6
#define PCI_NUM_REGIONS 7

static u8 pci_bios_next_bus;
static u32 pci_bios_io_next;
static u32 pci_bios_io_limit; // first invalid port from pci_bios_io_next
static u32 pci_bios_io_next_chunk; // next port to use when reaching pci_bios_io_limit
static u32 pci_bios_mem_addr;
/* host irqs corresponding to PCI irqs A-D */
static u8 pci_irqs[4] = {
    10, 10, 11, 11
};

static void pci_set_io_region_addr(u16 bdf, int region_num, u32 addr)
{
    u32 ofs, old_addr;

    if (region_num == PCI_ROM_SLOT) {
        ofs = PCI_ROM_ADDRESS;
    } else {
        ofs = PCI_BASE_ADDRESS_0 + region_num * 4;
    }

    old_addr = pci_config_readl(bdf, ofs);

    pci_config_writel(bdf, ofs, addr);
    dprintf(1, "region %d: 0x%08x\n", region_num, addr);
}

static int pci_bios_init_device(u16 bdf, int irq_offset)
{
    int class, header;
    u32 *paddr;
    int i, pin, pic_irq, vendor_id, device_id;
    int enable_vga_out = 0;

    class = pci_config_readw(bdf, PCI_CLASS_DEVICE);
    header = pci_config_readb(bdf, PCI_HEADER_TYPE);
    vendor_id = pci_config_readw(bdf, PCI_VENDOR_ID);
    device_id = pci_config_readw(bdf, PCI_DEVICE_ID);
    dprintf(1, "PCI: bus=%d devfn=0x%02x: vendor_id=0x%04x device_id=0x%04x class=0x%x\n"
            , pci_bdf_to_bus(bdf), pci_bdf_to_devfn(bdf), vendor_id, device_id, class);
    switch (class) {
    case PCI_CLASS_STORAGE_IDE:
        if (vendor_id == PCI_VENDOR_ID_INTEL) {
            /* PIIX3/PIIX4/ICH IDE */
            pci_config_writew(bdf, 0x40, 0x8000); // enable IDE0
            pci_config_writew(bdf, 0x42, 0x8000); // enable IDE1
            goto default_map;
        } else {
            /* IDE: we map it as in ISA mode */
            pci_set_io_region_addr(bdf, 0, PORT_ATA1_CMD_BASE);
            pci_set_io_region_addr(bdf, 1, PORT_ATA1_CTRL_BASE);
            pci_set_io_region_addr(bdf, 2, PORT_ATA2_CMD_BASE);
            pci_set_io_region_addr(bdf, 3, PORT_ATA2_CTRL_BASE);
        }
        break;
    case PCI_CLASS_SYSTEM_PIC:
        /* PIC */
        if (vendor_id == PCI_VENDOR_ID_IBM) {
            /* IBM */
            if (device_id == 0x0046 || device_id == 0xFFFF) {
                /* MPIC & MPIC2 */
                pci_set_io_region_addr(bdf, 0, 0x80800000 + 0x00040000);
            }
        }
        break;
    case 0xff00:
        if (vendor_id == PCI_VENDOR_ID_APPLE &&
            (device_id == 0x0017 || device_id == 0x0022)) {
            /* macio bridge */
            pci_set_io_region_addr(bdf, 0, 0x80800000);
        }
        break;
    default:
    default_map:
        /* default memory mappings */
        for (i = 0; i < PCI_NUM_REGIONS; i++) {
            if ((header & ~PCI_HEADER_TYPE_MF) != PCI_HEADER_TYPE_NORMAL &&
                i >= 2 && i != PCI_ROM_SLOT)
                // Only 2 BARs for non-normal
                continue;

            int ofs;
            if (i == PCI_ROM_SLOT)
                ofs = PCI_ROM_ADDRESS;
            else
                ofs = PCI_BASE_ADDRESS_0 + i * 4;

            u32 old = pci_config_readl(bdf, ofs);
            u32 mask;
            if (i == PCI_ROM_SLOT) {
                mask = PCI_ROM_ADDRESS_MASK;
                pci_config_writel(bdf, ofs, mask);
            } else {
                if (old & PCI_BASE_ADDRESS_SPACE_IO)
                    mask = PCI_BASE_ADDRESS_IO_MASK;
                else
                    mask = PCI_BASE_ADDRESS_MEM_MASK;
                pci_config_writel(bdf, ofs, ~0);
            }
            u32 val = pci_config_readl(bdf, ofs);
            pci_config_writel(bdf, ofs, old);

            if (val != 0) {
                u32 size = (~(val & mask)) + 1;
                if (val & PCI_BASE_ADDRESS_SPACE_IO) {
                    u32 aligned_addr = ALIGN(pci_bios_io_next, size);
                    if (pci_bios_io_limit && aligned_addr + size > pci_bios_io_limit) {
                            // Does not fit, use next chunk
                            pci_bios_io_next = pci_bios_io_next_chunk;
                            pci_bios_io_limit = 0;
                            paddr = &pci_bios_io_next;
                    } else {
                            paddr = &pci_bios_io_next;
                    }
                } else {
                    if ((val & PCI_BASE_ADDRESS_MEM_TYPE_MASK) ==
                         PCI_BASE_ADDRESS_MEM_TYPE_64 &&
                        (val & mask) == PCI_BASE_ADDRESS_MEM_MASK) {
                        panic("64-bit BAR mappings larger than 4GB unsupported\n");
                        i++; // 64-bit BAR
                        continue;
                    }
                    paddr = &pci_bios_mem_addr;
                }
                *paddr = ALIGN(*paddr, size);
                pci_set_io_region_addr(bdf, i, *paddr);
                *paddr += size;

                if (pci_bios_io_next < 0x4000)
                    panic("PCI I/O address wrap-around\n");
                if (pci_bios_mem_addr < BUILD_PCIMEM_START)
                    panic("PCI memory address wrap-around\n");

                /* 64-bit BARs, we assume upper 4-bytes can be ignored, just clear it */
                if ((val & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY &&
                    (val & PCI_BASE_ADDRESS_MEM_TYPE_MASK) == PCI_BASE_ADDRESS_MEM_TYPE_64) {
                    pci_config_writel(bdf, ofs + 4, 0);
                    i++;  // 64-bit BAR
                }
            }
        }
        break;
    }

    if (class == PCI_CLASS_BRIDGE_PCI) {
        int enable_vga = 0;
        u16 secondary_bus = pci_bios_next_bus++;
        
        pci_config_writeb(bdf, PCI_PRIMARY_BUS, pci_bdf_to_bus(bdf));
        pci_config_writeb(bdf, PCI_SECONDARY_BUS, secondary_bus);
        pci_config_writeb(bdf, PCI_SUBORDINATE_BUS, 255); // temporary

        u32 old_next = pci_bios_io_next;
        u32 old_limit = pci_bios_io_limit;

        // Round next_memory to a 1Mb boundary
        // (smallest region forwardable for memory)
        pci_bios_mem_addr = ALIGN(pci_bios_mem_addr, 1*1024*1024);

        // Round next_io to a 4kb boundary (smallest
        // region forwardable for I/O)
        if (pci_bios_io_limit) {
                pci_bios_io_next = pci_bios_io_next_chunk;
                pci_bios_io_limit = 0;
        }
        u32 io_before = pci_bios_io_next;
        pci_bios_io_next = ALIGN(pci_bios_io_next, 4*1024);

        // Set memory base
        pci_config_writew(bdf, PCI_MEMORY_BASE, pci_bios_mem_addr >> 16);
        dprintf(1, "PCI bus %d memory base 0x%08x\n", secondary_bus, pci_bios_mem_addr);

        // Set I/O base
        u32 io_start = pci_bios_io_next;
        pci_config_writeb(bdf, PCI_IO_BASE, pci_bios_io_next >> 8);
        dprintf(1, "PCI bus %d i/o base 0x%08x\n", secondary_bus, pci_bios_io_next);

        // Initialize everything on subordinate buses
        int sub_bdf, sub_max;
        int sub_irq_offset = 0;
        if (pci_bdf_to_bus(bdf) > 0)
                sub_irq_offset = irq_offset + pci_bdf_to_dev(bdf);
        for (sub_max = (secondary_bus + 1) << 8, sub_bdf = (secondary_bus << 8);
             sub_bdf >= 0 && sub_bdf < (secondary_bus + 1) << 8;
             sub_bdf = pci_next(sub_bdf+1, &sub_max)) {
            enable_vga |= pci_bios_init_device(sub_bdf, sub_irq_offset);
            enable_vga_out |= enable_vga;
        }

        // Set real subordinate bus number
        pci_config_writeb(bdf, PCI_SUBORDINATE_BUS, pci_bios_next_bus - 1);

        // Round to 1Mb and set memory limit
        pci_bios_mem_addr = ALIGN(pci_bios_mem_addr, 1*1024*1024);
        pci_config_writew(bdf, PCI_MEMORY_LIMIT, (pci_bios_mem_addr - 1*1024*1024) >> 16);
        dprintf(1, "PCI subordinate bus %d memory limit 0x%08x\n", pci_bios_next_bus - 1, pci_bios_mem_addr-1);

        if (pci_bios_io_next == io_start) {
                // Nothing mapped. Let's not forward anything and reclaim the
                // pre-allocated address space.
                pci_config_writeb(bdf, PCI_IO_LIMIT, (io_start - 4*1024) >> 8);
                pci_bios_io_next_chunk = io_before;
                dprintf(1, "PCI subordinate bus %d no i/o forwarding\n", pci_bios_next_bus - 1);
                pci_bios_io_next = old_next;
                pci_bios_io_limit = old_limit;
        } else {
                // Round to 4kb and set I/O limit
                pci_bios_io_next_chunk = ALIGN(pci_bios_io_next, 4*1024);
                pci_config_writeb(bdf, PCI_IO_LIMIT, (pci_bios_io_next_chunk - 4*1024) >> 8);
                dprintf(1, "PCI subordinate bus %d i/o limit 0x%08x\n", pci_bios_next_bus - 1, pci_bios_io_next_chunk-1);
                pci_bios_io_next = old_next;
                if (old_limit)
                        pci_bios_io_limit = old_limit;
                else
                        pci_bios_io_limit = io_start;
        }

        // Disable prefetchable memory
        pci_config_writew(bdf, PCI_PREF_MEMORY_BASE, 0x10);
        pci_config_writew(bdf, PCI_PREF_MEMORY_LIMIT, 0x00);

        // Enable I/O space, memory space, and bus master
        pci_config_writeb(bdf, PCI_COMMAND, PCI_COMMAND_MASTER | PCI_COMMAND_IO | PCI_COMMAND_MEMORY);

        if (enable_vga)
            pci_config_writeb(bdf, PCI_BRIDGE_CONTROL, 0x08);
    } else {
        if (pci_config_readw(bdf, PCI_CLASS_DEVICE) == PCI_CLASS_DISPLAY_VGA &&
            pci_config_readb(bdf, PCI_CLASS_PROG) == 0) {
            dprintf(1, "PCI: Enabling VGA decode for bus=%d devfn=0x%02x\n",
                    pci_bdf_to_bus(bdf), pci_bdf_to_devfn(bdf));
            enable_vga_out = 1;
        }

        // enable memory mappings
        pci_config_maskw(bdf, PCI_COMMAND, 0, PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
    }

    /* map the interrupt */
    pin = pci_config_readb(bdf, PCI_INTERRUPT_PIN);
    if (pin != 0) {
        int irq = pin - 1;

        // Rotate INTx lines as defined in the PCI-to-PCI bridge standard
        if (pci_bdf_to_bus(bdf) > 0)
                irq += pci_bdf_to_dev(bdf) & 0x3;
        irq += irq_offset;
        irq &= 3;

        pic_irq = pci_irqs[irq];
        pci_config_writeb(bdf, PCI_INTERRUPT_LINE, pic_irq);
    }

    if (vendor_id == PCI_VENDOR_ID_INTEL
        && device_id == PCI_DEVICE_ID_INTEL_82371AB_3) {
        /* PIIX4 Power Management device (for ACPI) */
        // acpi sci is hardwired to 9
        pci_config_writeb(bdf, PCI_INTERRUPT_LINE, 9);

        pci_config_writel(bdf, 0x40, PORT_ACPI_PM_BASE | 1);
        pci_config_writeb(bdf, 0x80, 0x01); /* enable PM io space */
        pci_config_writel(bdf, 0x90, PORT_SMB_BASE | 1);
        pci_config_writeb(bdf, 0xd2, 0x09); /* enable SMBus io space */
    }
    if (vendor_id == PCI_VENDOR_ID_INTEL
        && (device_id == PCI_DEVICE_ID_INTEL_ICH10_0 ||
            device_id == PCI_DEVICE_ID_INTEL_ICH10_1 ||
            device_id == PCI_DEVICE_ID_INTEL_ICH10_2 ||
            device_id == PCI_DEVICE_ID_INTEL_ICH10_3)) {
        /* ICH10 LPC device, power management (for ACPI) */
        pci_config_writeb(bdf, PCI_INTERRUPT_LINE, 9); // SCI IRQ 9

        pci_config_writel(bdf, 0x40, PORT_ACPI_PM_BASE | 1);
        pci_config_writeb(bdf, 0x44, 0x80); // ACPI enabled, SCI IRQ 9
    }
    if (vendor_id == PCI_VENDOR_ID_INTEL
        && (device_id == PCI_DEVICE_ID_INTEL_82371SB_0
            || device_id == PCI_DEVICE_ID_INTEL_82371AB_0
            || device_id == PCI_DEVICE_ID_INTEL_ICH10_0
            || device_id == PCI_DEVICE_ID_INTEL_ICH10_1
            || device_id == PCI_DEVICE_ID_INTEL_ICH10_2
            || device_id == PCI_DEVICE_ID_INTEL_ICH10_3)) {
        int i, irq;
        u8 elcr[2];

        /* PIIX3/PIIX4/ICH10 PCI to ISA bridge */

        elcr[0] = 0x00;
        elcr[1] = 0x00;
        for (i = 0; i < 4; i++) {
            irq = pci_irqs[i];
            /* set to trigger level */
            elcr[irq >> 3] |= (1 << (irq & 7));
            /* activate irq remapping in PIIX */
            pci_config_writeb(bdf, 0x60 + i, irq);
        }
        outb(elcr[0], 0x4d0);
        outb(elcr[1], 0x4d1);
        dprintf(1, "PIIX3/PIIX4/ICH10 init: elcr=%02x %02x\n",
                elcr[0], elcr[1]);
    }

    return enable_vga_out;
}

void
pci_setup(void)
{
    if (CONFIG_COREBOOT)
        // Already done by coreboot.
        return;

    dprintf(3, "pci setup\n");

    pci_bios_next_bus = 1;
    pci_bios_io_next = 0x4000;
    pci_bios_io_next_chunk = 0;
    pci_bios_io_limit = 0;
    pci_bios_mem_addr = BUILD_PCIMEM_START;

    int bdf, max;
    // Initialize PCI through DFS starting from the root
    for (max = 0x100, bdf = pci_next(0, &max);
         bdf >= 0 && bdf < 0x100;
         bdf=pci_next(bdf+1, &max)) {
            pci_bios_init_device(bdf, 0);
    }
}
