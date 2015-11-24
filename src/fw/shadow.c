// Support for enabling/disabling BIOS ram shadowing.
//
// Copyright (C) 2008-2010  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2006 Fabrice Bellard
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "config.h" // CONFIG_*
#include "dev-q35.h" // PCI_VENDOR_ID_INTEL
#include "dev-piix.h" // I440FX_PAM0
#include "hw/pci.h" // pci_config_writeb
#include "hw/pci_ids.h" // PCI_VENDOR_ID_INTEL
#include "hw/pci_regs.h" // PCI_VENDOR_ID
#include "malloc.h" // rom_get_last
#include "output.h" // dprintf
#include "paravirt.h" // runningOnXen
#include "string.h" // memset
#include "util.h" // make_bios_writable
#include "x86.h" // wbinvd

// On the emulators, the bios at 0xf0000 is also at 0xffff0000
#define BIOS_SRC_OFFSET 0xfff00000

#if CONFIG_VIRTUTECH_PC_SHADOW

#define Read_Only 1
#define Write_Only 2
#define Read_Write 3

static void modify_shadow(unsigned long start, unsigned long len, int mode)
{
    int start_loc = ((int)(start >> 10) - 640) >> 1;
    int len_remaining = len >> 11;
    int i;
    for (i = 0; i < len_remaining; i++) {
        outb(start_loc + i, 0xfff4);
        if (mode == Read_Only) {
            outb(1, 0xfff5);
        } else if (mode == Read_Write) {
            outb(3, 0xfff5);
        } else if (mode == Write_Only) {
            outb(2, 0xfff5);
        } else {
            outb(0, 0xfff5);
        }
    }
}

static void
__copy_bios(void)
{
    /* Read (and execute) from PCI, write to RAM */
    modify_shadow(0xe0000, 0x10000, Write_Only);
    modify_shadow(0xd0000, 0x10000, Write_Only);
    modify_shadow(0xc0000, 0x10000, Write_Only);

    if (CONFIG_OPTIONROMS_DEPLOYED) {
            int i;
            for (i=0; i<6; i++) {
                    void *mem = (void*)(BUILD_ROM_START + i * 32*1024);
                    memcpy((void*)BUILD_BIOS_TMP_ADDR, mem, 32*1024);
                    memcpy(mem, (void*)BUILD_BIOS_TMP_ADDR, 32*1024);
            }
    }
    
    modify_shadow(0xf0000, 0x10000, Write_Only);

    // Copy bios.
    memcpy(VSYMBOL(code32flat_start),
           VSYMBOL(code32flat_start) + BIOS_SRC_OFFSET,
           SYMBOL(code32flat_end) - SYMBOL(code32flat_start));
    
    /* Keep BIOS read/write */
    modify_shadow(0xf0000, 0x10000, Read_Write);
    modify_shadow(0xe0000, 0x10000, Read_Write);
    modify_shadow(0xd0000, 0x10000, Read_Write);
    modify_shadow(0xc0000, 0x10000, Read_Write);
}

#else

// Enable shadowing and copy bios.
static void
__make_bios_writable_intel(u16 bdf, u32 pam0)
{
    // Make ram from 0xc0000-0xf0000 writable
    int clear = 0;
    int i;
    for (i=0; i<6; i++) {
        u32 pam = pam0 + 1 + i;
        int reg = pci_config_readb(bdf, pam);
        if (CONFIG_OPTIONROMS_DEPLOYED && (reg & 0x11) != 0x11) {
            // Need to copy optionroms to work around qemu implementation
            void *mem = (void*)(BUILD_ROM_START + i * 32*1024);
            memcpy((void*)BUILD_BIOS_TMP_ADDR, mem, 32*1024);
            pci_config_writeb(bdf, pam, 0x33);
            memcpy(mem, (void*)BUILD_BIOS_TMP_ADDR, 32*1024);
            clear = 1;
        } else {
            pci_config_writeb(bdf, pam, 0x33);
        }
    }
    if (clear)
        memset((void*)BUILD_BIOS_TMP_ADDR, 0, 32*1024);

    // Make ram from 0xf0000-0x100000 writable
    int reg = pci_config_readb(bdf, pam0);
    pci_config_writeb(bdf, pam0, 0x30);
    if (reg & 0x10)
        // Ram already present.
        return;

    // Copy bios.
    memcpy(VSYMBOL(code32flat_start), VSYMBOL(code32flat_start) + BIOS_SRC_OFFSET
           , SYMBOL(code32flat_end) - SYMBOL(code32flat_start));
}

static void
make_bios_writable_intel(u16 bdf, u32 pam0)
{
    int reg = pci_config_readb(bdf, pam0);
    if (!(reg & 0x10)) {
        // QEMU doesn't fully implement the piix shadow capabilities -
        // if ram isn't backing the bios segment when shadowing is
        // disabled, the code itself won't be in memory.  So, run the
        // code from the high-memory flash location.
        u32 pos = (u32)__make_bios_writable_intel + BIOS_SRC_OFFSET;
        void (*func)(u16 bdf, u32 pam0) = (void*)pos;
        func(bdf, pam0);
        return;
    }
    // Ram already present - just enable writes
    __make_bios_writable_intel(bdf, pam0);
}

static void
make_bios_readonly_intel(u16 bdf, u32 pam0)
{
    // Flush any pending writes before locking memory.
    wbinvd();

    // Write protect roms from 0xc0000-0xf0000
    u32 romlast = BUILD_BIOS_ADDR, rommax = BUILD_BIOS_ADDR;
    if (CONFIG_WRITABLE_UPPERMEMORY)
        romlast = rom_get_last();
    if (CONFIG_MALLOC_UPPERMEMORY)
        rommax = rom_get_max();
    int i;
    for (i=0; i<6; i++) {
        u32 mem = BUILD_ROM_START + i * 32*1024;
        u32 pam = pam0 + 1 + i;
        if (romlast < mem + 16*1024 || rommax < mem + 32*1024) {
            if (romlast >= mem && rommax >= mem + 16*1024)
                pci_config_writeb(bdf, pam, 0x31);
            break;
        }
        pci_config_writeb(bdf, pam, 0x11);
    }

    // Write protect 0xf0000-0x100000
    pci_config_writeb(bdf, pam0, 0x10);
}

static int ShadowBDF = -1;

#endif // CONFIG_VIRTUTECH_PC_SHADOW

// Make the 0xc0000-0x100000 area read/writable.
void
make_bios_writable(void)
{
    if (!CONFIG_QEMU || runningOnXen())
        return;

    dprintf(3, "enabling shadow ram\n");

#if CONFIG_VIRTUTECH_PC_SHADOW

    /* Run the copy from the high address to avoid simulator flushes after each
       write (the flushes ruin performance) */
    u32 pos = (u32)__copy_bios + BIOS_SRC_OFFSET;
    void (*func)(void) = (void*)pos;
    func();

#else

    // At this point, statically allocated variables can't be written,
    // so do this search manually.
    int bdf;
    foreachbdf(bdf, 0) {
        u32 vendev = pci_config_readl(bdf, PCI_VENDOR_ID);
        u16 vendor = vendev & 0xffff, device = vendev >> 16;
        if (vendor == PCI_VENDOR_ID_INTEL
            && device == PCI_DEVICE_ID_INTEL_82441) {
            make_bios_writable_intel(bdf, I440FX_PAM0);
            ShadowBDF = bdf;
            return;
        }
        if (vendor == PCI_VENDOR_ID_INTEL
            && device == PCI_DEVICE_ID_INTEL_Q35_MCH) {
            make_bios_writable_intel(bdf, Q35_HOST_BRIDGE_PAM0);
            ShadowBDF = bdf;
            return;
        }
    }
    dprintf(1, "Unable to unlock ram - bridge not found\n");
#endif
}

// Make the BIOS code segment area (0xf0000) read-only.
void
make_bios_readonly(void)
{
    if (!CONFIG_QEMU || runningOnXen())
        return;

    dprintf(3, "locking shadow ram\n");
#if CONFIG_VIRTUTECH_PC_SHADOW
    modify_shadow(0xf0000, 0x10000, Read_Only);
#else
    if (ShadowBDF < 0) {
        dprintf(1, "Unable to lock ram - bridge not found\n");
        return;
    }

    u16 device = pci_config_readw(ShadowBDF, PCI_DEVICE_ID);
    if (device == PCI_DEVICE_ID_INTEL_82441)
        make_bios_readonly_intel(ShadowBDF, I440FX_PAM0);
    else
        make_bios_readonly_intel(ShadowBDF, Q35_HOST_BRIDGE_PAM0);
#endif
}

void
qemu_prep_reset(void)
{
    if (!CONFIG_QEMU || runningOnXen())
        return;
    // QEMU doesn't map 0xc0000-0xfffff back to the original rom on a
    // reset, so do that manually before invoking a hard reset.
    make_bios_writable();
    memcpy(VSYMBOL(code32flat_start), VSYMBOL(code32flat_start) + BIOS_SRC_OFFSET
           , SYMBOL(code32flat_end) - SYMBOL(code32flat_start));
}
