// Support for generating ACPI tables (on emulators)
//
// Copyright (C) 2008-2010  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2006 Fabrice Bellard
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "acpi.h" // struct rsdp_descriptor
#include "util.h" // memcpy
#include "byteorder.h" // cpu_to_le16
#include "pci.h" // pci_find_init_device
#include "pci_ids.h" // PCI_VENDOR_ID_INTEL
#include "pci_regs.h" // PCI_INTERRUPT_LINE
#include "ioport.h" // inl
#include "config.h" // CONFIG_*
#include "paravirt.h" // RamSize
#include "dev-q35.h"

#define MCFG_SIGNATURE 0x4746434d
struct mcfg_descriptor {
    ACPI_TABLE_HEADER_DEF
    u64 reserved1;
    u64 base;
    u16 pci_seg;
    u8 start_bus;
    u8 end_bus;
    u32 reserved2;
} PACKED;

#include "acpi-dsdt.hex"

u32 acpi_pm1a_cnt VARFSEG;

static void
build_header(struct acpi_table_header *h, u32 sig, int len, u8 rev)
{
    h->signature = cpu_to_le32(sig);
    h->length = cpu_to_le32(len);
    h->revision = rev;
    memcpy(h->oem_id, BUILD_APPNAME6, 6);
    memcpy(h->oem_table_id, BUILD_APPNAME4, 4);
    memcpy(h->oem_table_id + 4, (void*)&sig, 4);
    h->oem_revision = cpu_to_le32(1);
    memcpy(h->asl_compiler_id, BUILD_APPNAME4, 4);
    h->asl_compiler_revision = cpu_to_le32(1);
    h->checksum -= checksum(h, len);
}

#define PIIX4_ACPI_ENABLE       0xf1
#define PIIX4_ACPI_DISABLE      0xf0
#define PIIX4_GPE0_BLK          0xafe0
#define PIIX4_GPE0_BLK_LEN      4

#define PIIX4_PM_INTRRUPT       9       // irq 9

static void piix4_fadt_setup(struct pci_device *pci, void *arg)
{
    struct fadt_descriptor_rev1 *fadt = arg;

    fadt->model = 1;
    fadt->reserved1 = 0;
    fadt->sci_int = cpu_to_le16(PIIX4_PM_INTRRUPT);
    fadt->smi_cmd = cpu_to_le32(PORT_SMI_CMD);
    fadt->acpi_enable = PIIX4_ACPI_ENABLE;
    fadt->acpi_disable = PIIX4_ACPI_DISABLE;
    fadt->pm1a_evt_blk = cpu_to_le32(PORT_ACPI_PM_BASE);
    fadt->pm1a_cnt_blk = cpu_to_le32(PORT_ACPI_PM_BASE + 0x04);
    fadt->pm_tmr_blk = cpu_to_le32(PORT_ACPI_PM_BASE + 0x08);
    fadt->gpe0_blk = cpu_to_le32(PIIX4_GPE0_BLK);
    fadt->pm1_evt_len = 4;
    fadt->pm1_cnt_len = 2;
    fadt->pm_tmr_len = 4;
    fadt->gpe0_blk_len = PIIX4_GPE0_BLK_LEN;
    fadt->plvl2_lat = cpu_to_le16(0xfff); // C2 state not supported
    fadt->plvl3_lat = cpu_to_le16(0xfff); // C3 state not supported
    /* WBINVD + PROC_C1 + SLP_BUTTON + RTC_S4 + USE_PLATFORM_CLOCK */
    fadt->flags = cpu_to_le32((1 << 0) | (1 << 2) | (1 << 5) | (1 << 7) |
                              (1 << 15));
}

/* PCI_VENDOR_ID_INTEL && PCI_DEVICE_ID_INTEL_ICH9_LPC */
void ich9_lpc_fadt_setup(struct pci_device *dev, void *arg)
{
    struct fadt_descriptor_rev1 *fadt = arg;

    fadt->model = 1;
    fadt->reserved1 = 0;
    fadt->sci_int = cpu_to_le16(9);
    fadt->smi_cmd = cpu_to_le32(PORT_SMI_CMD);
    fadt->acpi_enable = ICH9_ACPI_ENABLE;
    fadt->acpi_disable = ICH9_ACPI_DISABLE;
    fadt->pm1a_evt_blk = cpu_to_le32(PORT_ACPI_PM_BASE);
    fadt->pm1a_cnt_blk = cpu_to_le32(PORT_ACPI_PM_BASE + 0x04);
    fadt->pm_tmr_blk = cpu_to_le32(PORT_ACPI_PM_BASE + 0x08);
    fadt->gpe0_blk = cpu_to_le32(PORT_ACPI_PM_BASE + ICH9_PMIO_GPE0_STS);
    fadt->pm1_evt_len = 4;
    fadt->pm1_cnt_len = 2;
    fadt->pm_tmr_len = 4;
    fadt->gpe0_blk_len = ICH9_PMIO_GPE0_BLK_LEN;
    fadt->plvl2_lat = cpu_to_le16(0xfff); // C2 state not supported
    fadt->plvl3_lat = cpu_to_le16(0xfff); // C3 state not supported
    /* WBINVD + PROC_C1 + SLP_BUTTON + RTC_S4 + USE_PLATFORM_CLOCK */
    fadt->flags = cpu_to_le32((1 << 0) | (1 << 2) | (1 << 5) | (1 << 7) |
                              (1 << 15));
}

#define VIRTUTECH_ACPI_ENABLE   0xf0
#define VIRTUTECH_ACPI_DISABLE  0xf1
#define VIRTUTECH_GPE0_BLK      (PORT_ACPI_PM_BASE + 0x0c)
#define VIRTUTECH_GPE0_BLK_LEN  4

static void piix4_virtutech_fadt_init(struct pci_device *pci, void *arg)
{
    struct fadt_descriptor_rev1 *fadt = arg;
    fadt->acpi_enable = VIRTUTECH_ACPI_ENABLE;
    fadt->acpi_disable = VIRTUTECH_ACPI_DISABLE;
    fadt->gpe0_blk = cpu_to_le32(VIRTUTECH_GPE0_BLK);
    fadt->gpe0_blk_len = VIRTUTECH_GPE0_BLK_LEN;
    /* WBINVD + PWR_BUTTON + SLP_BUTTON + FIX_RTC */
    fadt->flags = cpu_to_le32((1 << 0) | (1 << 4) | (1 << 5) | (1 << 6));
    /* Legacy devices and 8042 present */
    fadt->ia32_boot_arch = 3;
}


#define ICH10_ACPI_ENABLE   0xf0
#define ICH10_ACPI_DISABLE  0xf1
#define ICH10_GPE0_BLK      (PORT_ACPI_PM_BASE + 0x20)
#define ICH10_GPE0_BLK_LEN  16
static void ich10_fadt_init(struct pci_device *pci, void *arg)
{
    struct fadt_descriptor_rev1 *fadt = arg;
    fadt->acpi_enable = ICH10_ACPI_ENABLE;
    fadt->acpi_disable = ICH10_ACPI_DISABLE;
    fadt->gpe0_blk = cpu_to_le32(ICH10_GPE0_BLK);
    fadt->gpe0_blk_len = ICH10_GPE0_BLK_LEN;
    /* WBINVD + PWR_BUTTON + SLP_BUTTON + FIX_RTC */
    fadt->flags = cpu_to_le32((1 << 0) | (1 << 4) | (1 << 5) | (1 << 6));
    /* Legacy devices and 8042 present */
    fadt->ia32_boot_arch = 3;
}

static const struct pci_device_id fadt_init_tbl[] = {
    /* PIIX4 Power Management device (for ACPI) */
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_3,
               piix4_fadt_setup),
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH9_LPC,
               ich9_lpc_fadt_setup),
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_0,
               piix4_virtutech_fadt_init),
    // ICH10D0
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_0,
               ich10_fadt_init),
    // ICH10R
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_1,
               ich10_fadt_init),
    // ICH10 (consumer)
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_2,
               ich10_fadt_init),
    // ICH10D
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_3,
               ich10_fadt_init),

    PCI_DEVICE_END
};

static void fill_dsdt(struct fadt_descriptor_rev1 *fadt, void *dsdt)
{
    if (fadt->dsdt) {
        free((void *)le32_to_cpu(fadt->dsdt));
    }
    fadt->dsdt = cpu_to_le32((u32)dsdt);
    fadt->checksum -= checksum(fadt, sizeof(*fadt));
    dprintf(1, "ACPI DSDT=%p\n", dsdt);
}

static void *
build_fadt(struct pci_device *pci)
{
    struct fadt_descriptor_rev1 *fadt = malloc_high(sizeof(*fadt));
    struct facs_descriptor_rev1 *facs = memalign_high(64, sizeof(*facs));

    if (!fadt || !facs) {
        warn_noalloc();
        return NULL;
    }

    /* FACS */
    memset(facs, 0, sizeof(*facs));
    facs->signature = cpu_to_le32(FACS_SIGNATURE);
    facs->length = cpu_to_le32(sizeof(*facs));

    /* FADT */
    memset(fadt, 0, sizeof(*fadt));
    fadt->firmware_ctrl = cpu_to_le32((u32)facs);
    fadt->dsdt = 0;  /* dsdt will be filled later in acpi_setup()
                        by fill_dsdt() */
    pci_init_device(fadt_init_tbl, pci, fadt);

    build_header((void*)fadt, FACP_SIGNATURE, sizeof(*fadt), 1);

    return fadt;
}

static void*
build_madt(void)
{
    int madt_size = (sizeof(struct multiple_apic_table)
                     + sizeof(struct madt_processor_apic) * MaxCountCPUs
                     + sizeof(struct madt_io_apic)
                     + sizeof(struct madt_intsrcovr) * 16
                     + sizeof(struct madt_local_nmi));

    struct multiple_apic_table *madt = malloc_high(madt_size);
    if (!madt) {
        warn_noalloc();
        return NULL;
    }
    memset(madt, 0, madt_size);
    madt->local_apic_address = cpu_to_le32(BUILD_APIC_ADDR);
    madt->flags = cpu_to_le32(1);
    struct madt_processor_apic *apic = (void*)&madt[1];
    int i;
    for (i=0; i<MaxCountCPUs; i++) {
        apic->type = APIC_PROCESSOR;
        apic->length = sizeof(*apic);
        apic->processor_id = i;
        apic->local_apic_id = i;
        if (apic_id_is_present(apic->local_apic_id))
            apic->flags = cpu_to_le32(1);
        else
            apic->flags = cpu_to_le32(0);
        apic++;
    }
    struct madt_io_apic *io_apic = (void*)apic;
    io_apic->type = APIC_IO;
    io_apic->length = sizeof(*io_apic);
    io_apic->io_apic_id = BUILD_IOAPIC_ID;
    io_apic->address = cpu_to_le32(BUILD_IOAPIC_ADDR);
    io_apic->interrupt = cpu_to_le32(0);

    struct madt_intsrcovr *intsrcovr = (void*)&io_apic[1];
    if (romfile_loadint("etc/irq0-override", 0)) {
        memset(intsrcovr, 0, sizeof(*intsrcovr));
        intsrcovr->type   = APIC_XRUPT_OVERRIDE;
        intsrcovr->length = sizeof(*intsrcovr);
        intsrcovr->source = 0;
        intsrcovr->gsi    = cpu_to_le32(2);
        intsrcovr->flags  = cpu_to_le16(0); /* conforms to bus specifications */
        intsrcovr++;
    }
    for (i = 1; i < 16; i++) {
        if (!(BUILD_PCI_IRQS & (1 << i)))
            /* No need for a INT source override structure. */
            continue;
        memset(intsrcovr, 0, sizeof(*intsrcovr));
        intsrcovr->type   = APIC_XRUPT_OVERRIDE;
        intsrcovr->length = sizeof(*intsrcovr);
        intsrcovr->source = i;
        intsrcovr->gsi    = cpu_to_le32(i);
        intsrcovr->flags  = cpu_to_le16(0xd); /* active high, level triggered */
        intsrcovr++;
    }

    struct madt_local_nmi *local_nmi = (void*)intsrcovr;
    local_nmi->type         = APIC_LOCAL_NMI;
    local_nmi->length       = sizeof(*local_nmi);
    local_nmi->processor_id = 0xff; /* all processors */
    local_nmi->flags        = cpu_to_le16(0);
    local_nmi->lint         = 1; /* LINT1 */
    local_nmi++;

    build_header((void*)madt, APIC_SIGNATURE, (void*)local_nmi - (void*)madt, 1);
    return madt;
}

// Encode a hex value
static inline char getHex(u32 val) {
    val &= 0x0f;
    return (val <= 9) ? ('0' + val) : ('A' + val - 10);
}

// Encode a length in an SSDT.
static u8 *
encodeLen(u8 *ssdt_ptr, int length, int bytes)
{
    switch (bytes) {
    default:
    case 4: ssdt_ptr[3] = ((length >> 20) & 0xff);
    case 3: ssdt_ptr[2] = ((length >> 12) & 0xff);
    case 2: ssdt_ptr[1] = ((length >> 4) & 0xff);
            ssdt_ptr[0] = (((bytes-1) & 0x3) << 6) | (length & 0x0f);
            break;
    case 1: ssdt_ptr[0] = length & 0x3f;
    }
    return ssdt_ptr + bytes;
}

#include "ssdt-proc.hex"

/* 0x5B 0x83 ProcessorOp PkgLength NameString ProcID */
#define PROC_OFFSET_CPUHEX (*ssdt_proc_name - *ssdt_proc_start + 2)
#define PROC_OFFSET_CPUID1 (*ssdt_proc_name - *ssdt_proc_start + 4)
#define PROC_OFFSET_CPUID2 (*ssdt_proc_id - *ssdt_proc_start)
#define PROC_SIZEOF (*ssdt_proc_end - *ssdt_proc_start)
#define PROC_AML (ssdp_proc_aml + *ssdt_proc_start)

/* 0x5B 0x82 DeviceOp PkgLength NameString */
#define PCIHP_OFFSET_HEX (*ssdt_pcihp_name - *ssdt_pcihp_start + 1)
#define PCIHP_OFFSET_ID (*ssdt_pcihp_id - *ssdt_pcihp_start)
#define PCIHP_OFFSET_ADR (*ssdt_pcihp_adr - *ssdt_pcihp_start)
#define PCIHP_OFFSET_EJ0 (*ssdt_pcihp_ej0 - *ssdt_pcihp_start)
#define PCIHP_SIZEOF (*ssdt_pcihp_end - *ssdt_pcihp_start)
#define PCIHP_AML (ssdp_pcihp_aml + *ssdt_pcihp_start)
#define PCI_SLOTS 32

#define SSDT_SIGNATURE 0x54445353 // SSDT
#define SSDT_HEADER_LENGTH 36

#include "ssdt-misc.hex"
#include "ssdt-pcihp.hex"

#define PCI_RMV_BASE 0xae0c

static u8*
build_notify(u8 *ssdt_ptr, const char *name, int skip, int count,
             const char *target, int ofs)
{
    count -= skip;

    *(ssdt_ptr++) = 0x14; // MethodOp
    ssdt_ptr = encodeLen(ssdt_ptr, 2+5+(12*count), 2);
    memcpy(ssdt_ptr, name, 4);
    ssdt_ptr += 4;
    *(ssdt_ptr++) = 0x02; // MethodOp

    int i;
    for (i = skip; count-- > 0; i++) {
        *(ssdt_ptr++) = 0xA0; // IfOp
        ssdt_ptr = encodeLen(ssdt_ptr, 11, 1);
        *(ssdt_ptr++) = 0x93; // LEqualOp
        *(ssdt_ptr++) = 0x68; // Arg0Op
        *(ssdt_ptr++) = 0x0A; // BytePrefix
        *(ssdt_ptr++) = i;
        *(ssdt_ptr++) = 0x86; // NotifyOp
        memcpy(ssdt_ptr, target, 4);
        ssdt_ptr[ofs] = getHex(i >> 4);
        ssdt_ptr[ofs + 1] = getHex(i);
        ssdt_ptr += 4;
        *(ssdt_ptr++) = 0x69; // Arg1Op
    }
    return ssdt_ptr;
}

static void patch_pcihp(int slot, u8 *ssdt_ptr, u32 eject)
{
    ssdt_ptr[PCIHP_OFFSET_HEX] = getHex(slot >> 4);
    ssdt_ptr[PCIHP_OFFSET_HEX+1] = getHex(slot);
    ssdt_ptr[PCIHP_OFFSET_ID] = slot;
    ssdt_ptr[PCIHP_OFFSET_ADR + 2] = slot;

    /* Runtime patching of EJ0: to disable hotplug for a slot,
     * replace the method name: _EJ0 by EJ0_. */
    /* Sanity check */
    if (memcmp(ssdt_ptr + PCIHP_OFFSET_EJ0, "_EJ0", 4)) {
        warn_internalerror();
    }
    if (!eject) {
        memcpy(ssdt_ptr + PCIHP_OFFSET_EJ0, "EJ0_", 4);
    }
}

static void*
build_ssdt(void)
{
    int acpi_cpus = MaxCountCPUs > 0xff ? 0xff : MaxCountCPUs;
    int length = (sizeof(ssdp_misc_aml)                     // _S3_ / _S4_ / _S5_
                  + (1+3+4)                                 // Scope(_SB_)
                  + (acpi_cpus * PROC_SIZEOF)               // procs
                  + (1+2+5+(12*acpi_cpus))                  // NTFY
                  + (6+2+1+(1*acpi_cpus))                   // CPON
                  + (1+3+4)                                 // Scope(PCI0)
                  + ((PCI_SLOTS - 1) * PCIHP_SIZEOF)        // slots
                  + (1+2+5+(12*(PCI_SLOTS - 1))));          // PCNT
    u8 *ssdt = malloc_high(length);
    if (! ssdt) {
        warn_noalloc();
        return NULL;
    }
    u8 *ssdt_ptr = ssdt;

    // Copy header and encode fwcfg values in the S3_ / S4_ / S5_ packages
    int sys_state_size;
    char *sys_states = romfile_loadfile("etc/system-states", &sys_state_size);
    if (!sys_states || sys_state_size != 6)
        sys_states = (char[]){128, 0, 0, 129, 128, 128};

    memcpy(ssdt_ptr, ssdp_misc_aml, sizeof(ssdp_misc_aml));
    if (!(sys_states[3] & 128))
        ssdt_ptr[acpi_s3_name[0]] = 'X';
    if (!(sys_states[4] & 128))
        ssdt_ptr[acpi_s4_name[0]] = 'X';
    else
        ssdt_ptr[acpi_s4_pkg[0] + 1] = ssdt[acpi_s4_pkg[0] + 3] = sys_states[4] & 127;

    // store pci io windows
    *(u32*)&ssdt_ptr[acpi_pci32_start[0]] = cpu_to_le32(pcimem_start);
    *(u32*)&ssdt_ptr[acpi_pci32_end[0]] = cpu_to_le32(pcimem_end - 1);
    if (pcimem64_start) {
        ssdt_ptr[acpi_pci64_valid[0]] = 1;
        *(u64*)&ssdt_ptr[acpi_pci64_start[0]] = cpu_to_le64(pcimem64_start);
        *(u64*)&ssdt_ptr[acpi_pci64_end[0]] = cpu_to_le64(pcimem64_end - 1);
        *(u64*)&ssdt_ptr[acpi_pci64_length[0]] = cpu_to_le64(
            pcimem64_end - pcimem64_start);
    } else {
        ssdt_ptr[acpi_pci64_valid[0]] = 0;
    }

    int pvpanic_port = romfile_loadint("etc/pvpanic-port", 0x0);
    *(u16 *)(ssdt_ptr + *ssdt_isa_pest) = pvpanic_port;

    ssdt_ptr += sizeof(ssdp_misc_aml);

    // build Scope(_SB_) header
    *(ssdt_ptr++) = 0x10; // ScopeOp
    ssdt_ptr = encodeLen(ssdt_ptr, length - (ssdt_ptr - ssdt), 3);
    *(ssdt_ptr++) = '_';
    *(ssdt_ptr++) = 'S';
    *(ssdt_ptr++) = 'B';
    *(ssdt_ptr++) = '_';

    // build Processor object for each processor
    int i;
    for (i=0; i<acpi_cpus; i++) {
        memcpy(ssdt_ptr, PROC_AML, PROC_SIZEOF);
        ssdt_ptr[PROC_OFFSET_CPUHEX] = getHex(i >> 4);
        ssdt_ptr[PROC_OFFSET_CPUHEX+1] = getHex(i);
        ssdt_ptr[PROC_OFFSET_CPUID1] = i;
        ssdt_ptr[PROC_OFFSET_CPUID2] = i;
        ssdt_ptr += PROC_SIZEOF;
    }

    // build "Method(NTFY, 2) {If (LEqual(Arg0, 0x00)) {Notify(CP00, Arg1)} ...}"
    // Arg0 = Processor ID = APIC ID
    ssdt_ptr = build_notify(ssdt_ptr, "NTFY", 0, acpi_cpus, "CP00", 2);

    // build "Name(CPON, Package() { One, One, ..., Zero, Zero, ... })"
    *(ssdt_ptr++) = 0x08; // NameOp
    *(ssdt_ptr++) = 'C';
    *(ssdt_ptr++) = 'P';
    *(ssdt_ptr++) = 'O';
    *(ssdt_ptr++) = 'N';
    *(ssdt_ptr++) = 0x12; // PackageOp
    ssdt_ptr = encodeLen(ssdt_ptr, 2+1+(1*acpi_cpus), 2);
    *(ssdt_ptr++) = acpi_cpus;
    for (i=0; i<acpi_cpus; i++)
        *(ssdt_ptr++) = (apic_id_is_present(i)) ? 0x01 : 0x00;

    // build Scope(PCI0) opcode
    *(ssdt_ptr++) = 0x10; // ScopeOp
    ssdt_ptr = encodeLen(ssdt_ptr, length - (ssdt_ptr - ssdt), 3);
    *(ssdt_ptr++) = 'P';
    *(ssdt_ptr++) = 'C';
    *(ssdt_ptr++) = 'I';
    *(ssdt_ptr++) = '0';

    // build Device object for each slot
    u32 rmvc_pcrm = inl(PCI_RMV_BASE);
    for (i=1; i<PCI_SLOTS; i++) {
        u32 eject = rmvc_pcrm & (0x1 << i);
        memcpy(ssdt_ptr, PCIHP_AML, PCIHP_SIZEOF);
        patch_pcihp(i, ssdt_ptr, eject != 0);
        ssdt_ptr += PCIHP_SIZEOF;
    }

    ssdt_ptr = build_notify(ssdt_ptr, "PCNT", 1, PCI_SLOTS, "S00_", 1);

    build_header((void*)ssdt, SSDT_SIGNATURE, ssdt_ptr - ssdt, 1);

    //hexdump(ssdt, ssdt_ptr - ssdt);

    return ssdt;
}

#define HPET_ID         0x000
#define HPET_PERIOD     0x004

static void*
build_hpet(void)
{
    struct acpi_20_hpet *hpet;
    const void *hpet_base = (void *)BUILD_HPET_ADDRESS;
    u32 hpet_vendor = readl(hpet_base + HPET_ID) >> 16;
    u32 hpet_period = readl(hpet_base + HPET_PERIOD);

    if (hpet_vendor == 0 || hpet_vendor == 0xffff ||
        hpet_period == 0 || hpet_period > 100000000)
        return NULL;

    hpet = malloc_high(sizeof(*hpet));
    if (!hpet) {
        warn_noalloc();
        return NULL;
    }

    memset(hpet, 0, sizeof(*hpet));
    /* Note timer_block_id value must be kept in sync with value advertised by
     * emulated hpet
     */
    hpet->timer_block_id = cpu_to_le32(0x8086a201);
    hpet->addr.address = cpu_to_le64(BUILD_HPET_ADDRESS);
    build_header((void*)hpet, HPET_SIGNATURE, sizeof(*hpet), 1);

    return hpet;
}

static void
acpi_build_srat_memory(struct srat_memory_affinity *numamem,
                       u64 base, u64 len, int node, int enabled)
{
    numamem->type = SRAT_MEMORY;
    numamem->length = sizeof(*numamem);
    memset(numamem->proximity, 0, 4);
    numamem->proximity[0] = node;
    numamem->flags = cpu_to_le32(!!enabled);
    numamem->base_addr = cpu_to_le64(base);
    numamem->range_length = cpu_to_le64(len);
}

static void *
build_srat(void)
{
    int numadatasize, numacpusize;
    u64 *numadata = romfile_loadfile("etc/numa-nodes", &numadatasize);
    u64 *numacpumap = romfile_loadfile("etc/numa-cpu-map", &numacpusize);
    if (!numadata || !numacpumap)
        goto fail;
    int max_cpu = numacpusize / sizeof(u64);
    int nb_numa_nodes = numadatasize / sizeof(u64);

    struct system_resource_affinity_table *srat;
    int srat_size = sizeof(*srat) +
        sizeof(struct srat_processor_affinity) * max_cpu +
        sizeof(struct srat_memory_affinity) * (nb_numa_nodes + 2);

    srat = malloc_high(srat_size);
    if (!srat) {
        warn_noalloc();
        goto fail;
    }

    memset(srat, 0, srat_size);
    srat->reserved1=cpu_to_le32(1);
    struct srat_processor_affinity *core = (void*)(srat + 1);
    int i;
    u64 curnode;

    for (i = 0; i < max_cpu; ++i) {
        core->type = SRAT_PROCESSOR;
        core->length = sizeof(*core);
        core->local_apic_id = i;
        curnode = *numacpumap++;
        core->proximity_lo = curnode;
        memset(core->proximity_hi, 0, 3);
        core->local_sapic_eid = 0;
        if (apic_id_is_present(i))
            core->flags = cpu_to_le32(1);
        else
            core->flags = cpu_to_le32(0);
        core++;
    }


    /* the memory map is a bit tricky, it contains at least one hole
     * from 640k-1M and possibly another one from 3.5G-4G.
     */
    struct srat_memory_affinity *numamem = (void*)core;
    int slots = 0;
    u64 mem_len, mem_base, next_base = 0;

    acpi_build_srat_memory(numamem, 0, 640*1024, 0, 1);
    next_base = 1024 * 1024;
    numamem++;
    slots++;
    for (i = 1; i < nb_numa_nodes + 1; ++i) {
        mem_base = next_base;
        mem_len = *numadata++;
        if (i == 1)
            mem_len -= 1024 * 1024;
        next_base = mem_base + mem_len;

        /* Cut out the PCI hole */
        if (mem_base <= RamSize && next_base > RamSize) {
            mem_len -= next_base - RamSize;
            if (mem_len > 0) {
                acpi_build_srat_memory(numamem, mem_base, mem_len, i-1, 1);
                numamem++;
                slots++;
            }
            mem_base = 1ULL << 32;
            mem_len = next_base - RamSize;
            next_base += (1ULL << 32) - RamSize;
        }
        acpi_build_srat_memory(numamem, mem_base, mem_len, i-1, 1);
        numamem++;
        slots++;
    }
    for (; slots < nb_numa_nodes + 2; slots++) {
        acpi_build_srat_memory(numamem, 0, 0, 0, 0);
        numamem++;
    }

    build_header((void*)srat, SRAT_SIGNATURE, srat_size, 1);

    free(numadata);
    free(numacpumap);
    return srat;
fail:
    free(numadata);
    free(numacpumap);
    return NULL;
}

static void *
build_mcfg_q35(void)
{
    struct acpi_table_mcfg *mcfg;

    int len = sizeof(*mcfg) + 1 * sizeof(mcfg->allocation[0]);
    mcfg = malloc_high(len);
    if (!mcfg) {
        warn_noalloc();
        return NULL;
    }
    memset(mcfg, 0, len);
    mcfg->allocation[0].address = cpu_to_le64(Q35_HOST_BRIDGE_PCIEXBAR_ADDR);
    mcfg->allocation[0].pci_segment = cpu_to_le16(Q35_HOST_PCIE_PCI_SEGMENT);
    mcfg->allocation[0].start_bus_number = Q35_HOST_PCIE_START_BUS_NUMBER;
    mcfg->allocation[0].end_bus_number = Q35_HOST_PCIE_END_BUS_NUMBER;

    build_header((void *)mcfg, MCFG_SIGNATURE, len, 1);
    return mcfg;
}

static const struct pci_device_id acpi_find_tbl[] = {
    /* PIIX4 Power Management device. */
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_3, NULL),
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH9_LPC, NULL),
    // ICH10D0
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_0, NULL),
    // ICH10R
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_1, NULL),
    // ICH10 (consumer)
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_2, NULL),
    // ICH10D
    PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_3, NULL),

    PCI_DEVICE_END,
};

static void *
build_mcfg(void)
{
    u64 base = pci_get_MCFG_base();
    if (base == 0)
        return NULL;

    int mcfg_size = sizeof(struct mcfg_descriptor);
    struct mcfg_descriptor *mcfg = malloc_high(mcfg_size);    
    memset(mcfg, 0, mcfg_size);
    mcfg->base = base;
    mcfg->pci_seg = 0;
    mcfg->start_bus = 0;
    mcfg->end_bus = ((pci_get_MCFG_size() - 1) >> 20) & 0xff;
    build_header((void*)mcfg, MCFG_SIGNATURE, mcfg_size, 1);
    return mcfg;
}

struct rsdp_descriptor *RsdpAddr;

#define MAX_ACPI_TABLES 20
void
acpi_setup(void)
{
    if (! CONFIG_ACPI)
        return;

    dprintf(3, "init ACPI tables\n");

    // This code is hardcoded for PIIX4/ICH10 Power Management device.
    struct pci_device *pci = pci_find_init_device(acpi_find_tbl, NULL);
    if (!pci)
        // Device not found
        return;

    // Build ACPI tables
    u32 tables[MAX_ACPI_TABLES], tbl_idx = 0;

#define ACPI_INIT_TABLE(X)                                   \
    do {                                                     \
        tables[tbl_idx] = cpu_to_le32((u32)(X));             \
        if (le32_to_cpu(tables[tbl_idx]))                    \
            tbl_idx++;                                       \
    } while(0)

    struct fadt_descriptor_rev1 *fadt = build_fadt(pci);
    ACPI_INIT_TABLE(fadt);
    ACPI_INIT_TABLE(build_ssdt());
    ACPI_INIT_TABLE(build_madt());
    if (CONFIG_ACPI_HPET)
            ACPI_INIT_TABLE(build_hpet());
    ACPI_INIT_TABLE(build_srat());
    if (pci->device == PCI_DEVICE_ID_INTEL_ICH9_LPC)
        ACPI_INIT_TABLE(build_mcfg_q35());
    void *mcfg = build_mcfg();
    if (mcfg)
        ACPI_INIT_TABLE(mcfg);

    struct romfile_s *file = NULL;
    for (;;) {
        file = romfile_findprefix("acpi/", file);
        if (!file)
            break;
        struct acpi_table_header *table = malloc_high(file->size);
        if (!table) {
            warn_noalloc();
            continue;
        }
        int ret = file->copy(file, table, file->size);
        if (ret <= sizeof(*table))
            continue;
        if (table->signature == DSDT_SIGNATURE) {
            if (fadt) {
                fill_dsdt(fadt, table);
            }
        } else {
            ACPI_INIT_TABLE(table);
        }
        if (tbl_idx == MAX_ACPI_TABLES) {
            warn_noalloc();
            break;
        }
    }

    if (CONFIG_ACPI_DSDT && fadt && !fadt->dsdt) {
        /* default DSDT */
        void *dsdt = malloc_high(sizeof(AmlCode));
        if (!dsdt) {
            warn_noalloc();
            return;
        }
        memcpy(dsdt, AmlCode, sizeof(AmlCode));
        fill_dsdt(fadt, dsdt);
    }

    // Build final rsdt table
    struct rsdt_descriptor_rev1 *rsdt;
    size_t rsdt_len = sizeof(*rsdt) + sizeof(u32) * tbl_idx;
    rsdt = malloc_high(rsdt_len);
    if (!rsdt) {
        warn_noalloc();
        return;
    }
    memset(rsdt, 0, rsdt_len);
    memcpy(rsdt->table_offset_entry, tables, sizeof(u32) * tbl_idx);
    build_header((void*)rsdt, RSDT_SIGNATURE, rsdt_len, 1);

    // Build rsdp pointer table
    struct rsdp_descriptor *rsdp = malloc_fseg(sizeof(*rsdp));
    if (!rsdp) {
        warn_noalloc();
        return;
    }
    memset(rsdp, 0, sizeof(*rsdp));
    rsdp->signature = cpu_to_le64(RSDP_SIGNATURE);
    memcpy(rsdp->oem_id, BUILD_APPNAME6, 6);
    rsdp->rsdt_physical_address = cpu_to_le32((u32)rsdt);
    rsdp->checksum -= checksum(rsdp, 20);
    RsdpAddr = rsdp;
    dprintf(1, "ACPI tables: RSDP=%p RSDT=%p\n", rsdp, rsdt);
}

static struct fadt_descriptor_rev1 *
find_fadt(void)
{
    dprintf(4, "rsdp=%p\n", RsdpAddr);
    if (!RsdpAddr || RsdpAddr->signature != RSDP_SIGNATURE)
        return NULL;
    struct rsdt_descriptor_rev1 *rsdt = (void*)RsdpAddr->rsdt_physical_address;
    dprintf(4, "rsdt=%p\n", rsdt);
    if (!rsdt || rsdt->signature != RSDT_SIGNATURE)
        return NULL;
    void *end = (void*)rsdt + rsdt->length;
    int i;
    for (i=0; (void*)&rsdt->table_offset_entry[i] < end; i++) {
        struct fadt_descriptor_rev1 *fadt = (void*)rsdt->table_offset_entry[i];
        if (!fadt || fadt->signature != FACP_SIGNATURE)
            continue;
        dprintf(4, "fadt=%p\n", fadt);
        return fadt;
    }
    dprintf(4, "no fadt found\n");
    return NULL;
}

u32
find_resume_vector(void)
{
    struct fadt_descriptor_rev1 *fadt = find_fadt();
    if (!fadt)
        return 0;
    struct facs_descriptor_rev1 *facs = (void*)fadt->firmware_ctrl;
    dprintf(4, "facs=%p\n", facs);
    if (! facs || facs->signature != FACS_SIGNATURE)
        return 0;
    // Found it.
    dprintf(4, "resume addr=%d\n", facs->firmware_waking_vector);
    return facs->firmware_waking_vector;
}

void
find_acpi_features(void)
{
    struct fadt_descriptor_rev1 *fadt = find_fadt();
    if (!fadt)
        return;
    u32 pm_tmr = le32_to_cpu(fadt->pm_tmr_blk);
    u32 pm1a_cnt = le32_to_cpu(fadt->pm1a_cnt_blk);
    dprintf(4, "pm_tmr_blk=%x\n", pm_tmr);
    if (pm_tmr)
        pmtimer_setup(pm_tmr, 3579);
    if (pm1a_cnt)
        acpi_pm1a_cnt = pm1a_cnt;

    // Theoretically we should check the 'reset_reg_sup' flag, but Windows
    // doesn't and thus nobody seems to *set* it. If the table is large enough
    // to include it, let the sanity checks in acpi_set_reset_reg() suffice.
    if (fadt->length >= 129) {
        void *p = fadt;
        acpi_set_reset_reg(p + 116, *(u8 *)(p + 128));
    }
}

static struct acpi_20_generic_address acpi_reset_reg;
static u8 acpi_reset_val;

void
acpi_reboot(void)
{
    // Check it passed the sanity checks in acpi_set_reset_reg() and was set
    if (acpi_reset_reg.register_bit_width != 8)
        return;

    u64 addr = le64_to_cpu(acpi_reset_reg.address);

    dprintf(1, "ACPI hard reset %d:%llx (%x)\n",
            acpi_reset_reg.address_space_id, addr, acpi_reset_val);

    switch (acpi_reset_reg.address_space_id) {
    case 0: // System Memory
	writeb((void *)(u32)addr, acpi_reset_val);
        break;
    case 1: // System I/O
        outb(acpi_reset_val, addr);
        break;
    case 2: // PCI config space
        pci_config_writeb(acpi_ga_to_bdf(addr), addr & 0xffff, acpi_reset_val);
        break;
    }
}

void
acpi_set_reset_reg(struct acpi_20_generic_address *reg, u8 val)
{
    if (!reg || reg->address_space_id > 2 ||
        reg->register_bit_width != 8 || reg->register_bit_offset)
        return;

    acpi_reset_reg = *reg;
    acpi_reset_val = val;
}
