// Support for generating ACPI tables (on emulators)
//
// Copyright (C) 2008,2009  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2006 Fabrice Bellard
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "acpi.h" // struct rsdp_descriptor
#include "util.h" // memcpy
#include "pci.h" // pci_find_device
#include "biosvar.h" // GET_EBDA
#include "pci_ids.h" // PCI_VENDOR_ID_INTEL
#include "pci_regs.h" // PCI_INTERRUPT_LINE
#include "paravirt.h"

/****************************************************/
/* ACPI tables init */

/* Table structure from Linux kernel (the ACPI tables are under the
   BSD license) */

#define ACPI_TABLE_HEADER_DEF   /* ACPI common table header */ \
    u32 signature;          /* ACPI signature (4 ASCII characters) */ \
    u32 length;                 /* Length of table, in bytes, including header */ \
    u8  revision;               /* ACPI Specification minor version # */ \
    u8  checksum;               /* To make sum of entire table == 0 */ \
    u8  oem_id [6];             /* OEM identification */ \
    u8  oem_table_id [8];       /* OEM table identification */ \
    u32 oem_revision;           /* OEM revision number */ \
    u8  asl_compiler_id [4];    /* ASL compiler vendor ID */ \
    u32 asl_compiler_revision;  /* ASL compiler revision number */


struct acpi_table_header         /* ACPI common table header */
{
    ACPI_TABLE_HEADER_DEF
} PACKED;

/*
 * ACPI 1.0 Root System Description Table (RSDT)
 */
#define RSDT_SIGNATURE 0x54445352 // RSDT
struct rsdt_descriptor_rev1
{
    ACPI_TABLE_HEADER_DEF       /* ACPI common table header */
    u32 table_offset_entry[0];  /* Array of pointers to other */
    /* ACPI tables */
} PACKED;

/*
 * ACPI 1.0 Firmware ACPI Control Structure (FACS)
 */
#define FACS_SIGNATURE 0x53434146 // FACS
struct facs_descriptor_rev1
{
    u32 signature;           /* ACPI Signature */
    u32 length;                 /* Length of structure, in bytes */
    u32 hardware_signature;     /* Hardware configuration signature */
    u32 firmware_waking_vector; /* ACPI OS waking vector */
    u32 global_lock;            /* Global Lock */
    u32 S4bios_f        : 1;    /* Indicates if S4BIOS support is present */
    u32 reserved1       : 31;   /* Must be 0 */
    u8  resverved3 [40];        /* Reserved - must be zero */
} PACKED;


/*
 * ACPI 1.0 Fixed ACPI Description Table (FADT)
 */
#define FACP_SIGNATURE 0x50434146 // FACP
struct fadt_descriptor_rev1
{
    ACPI_TABLE_HEADER_DEF     /* ACPI common table header */
    u32 firmware_ctrl;          /* Physical address of FACS */
    u32 dsdt;                   /* Physical address of DSDT */
    u8  model;                  /* System Interrupt Model */
    u8  reserved1;              /* Reserved */
    u16 sci_int;                /* System vector of SCI interrupt */
    u32 smi_cmd;                /* Port address of SMI command port */
    u8  acpi_enable;            /* Value to write to smi_cmd to enable ACPI */
    u8  acpi_disable;           /* Value to write to smi_cmd to disable ACPI */
    u8  S4bios_req;             /* Value to write to SMI CMD to enter S4BIOS state */
    u8  reserved2;              /* Reserved - must be zero */
    u32 pm1a_evt_blk;           /* Port address of Power Mgt 1a acpi_event Reg Blk */
    u32 pm1b_evt_blk;           /* Port address of Power Mgt 1b acpi_event Reg Blk */
    u32 pm1a_cnt_blk;           /* Port address of Power Mgt 1a Control Reg Blk */
    u32 pm1b_cnt_blk;           /* Port address of Power Mgt 1b Control Reg Blk */
    u32 pm2_cnt_blk;            /* Port address of Power Mgt 2 Control Reg Blk */
    u32 pm_tmr_blk;             /* Port address of Power Mgt Timer Ctrl Reg Blk */
    u32 gpe0_blk;               /* Port addr of General Purpose acpi_event 0 Reg Blk */
    u32 gpe1_blk;               /* Port addr of General Purpose acpi_event 1 Reg Blk */
    u8  pm1_evt_len;            /* Byte length of ports at pm1_x_evt_blk */
    u8  pm1_cnt_len;            /* Byte length of ports at pm1_x_cnt_blk */
    u8  pm2_cnt_len;            /* Byte Length of ports at pm2_cnt_blk */
    u8  pm_tmr_len;             /* Byte Length of ports at pm_tm_blk */
    u8  gpe0_blk_len;           /* Byte Length of ports at gpe0_blk */
    u8  gpe1_blk_len;           /* Byte Length of ports at gpe1_blk */
    u8  gpe1_base;              /* Offset in gpe model where gpe1 events start */
    u8  reserved3;              /* Reserved */
    u16 plvl2_lat;              /* Worst case HW latency to enter/exit C2 state */
    u16 plvl3_lat;              /* Worst case HW latency to enter/exit C3 state */
    u16 flush_size;             /* Size of area read to flush caches */
    u16 flush_stride;           /* Stride used in flushing caches */
    u8  duty_offset;            /* Bit location of duty cycle field in p_cnt reg */
    u8  duty_width;             /* Bit width of duty cycle field in p_cnt reg */
    u8  day_alrm;               /* Index to day-of-month alarm in RTC CMOS RAM */
    u8  mon_alrm;               /* Index to month-of-year alarm in RTC CMOS RAM */
    u8  century;                /* Index to century in RTC CMOS RAM */
    u8  ia32_boot_arch;         /* Architecture flags */
    u8  reserved4a;             /* Reserved */
    u8  reserved4b;             /* Reserved */
#if 0
    u32 wb_invd         : 1;    /* The wbinvd instruction works properly */
    u32 wb_invd_flush   : 1;    /* The wbinvd flushes but does not invalidate */
    u32 proc_c1         : 1;    /* All processors support C1 state */
    u32 plvl2_up        : 1;    /* C2 state works on MP system */
    u32 pwr_button      : 1;    /* Power button is handled as a generic feature */
    u32 sleep_button    : 1;    /* Sleep button is handled as a generic feature, or not present */
    u32 fixed_rTC       : 1;    /* RTC wakeup stat not in fixed register space */
    u32 rtcs4           : 1;    /* RTC wakeup stat not possible from S4 */
    u32 tmr_val_ext     : 1;    /* The tmr_val width is 32 bits (0 = 24 bits) */
    u32 reserved5       : 23;   /* Reserved - must be zero */
#else
    u32 flags;
#endif
} PACKED;

/*
 * MADT values and structures
 */

/* Values for MADT PCATCompat */

#define DUAL_PIC                0
#define MULTIPLE_APIC           1


/* Master MADT */

#define APIC_SIGNATURE 0x43495041 // APIC
struct multiple_apic_table
{
    ACPI_TABLE_HEADER_DEF     /* ACPI common table header */
    u32 local_apic_address;     /* Physical address of local APIC */
#if 0
    u32 PCATcompat      : 1;    /* A one indicates system also has dual 8259s */
    u32 reserved1       : 31;
#else
    u32 flags;
#endif
} PACKED;


/* Values for Type in APIC sub-headers */

#define APIC_PROCESSOR          0
#define APIC_IO                 1
#define APIC_XRUPT_OVERRIDE     2
#define APIC_NMI                3
#define APIC_LOCAL_NMI          4
#define APIC_ADDRESS_OVERRIDE   5
#define APIC_IO_SAPIC           6
#define APIC_LOCAL_SAPIC        7
#define APIC_XRUPT_SOURCE       8
#define APIC_RESERVED           9           /* 9 and greater are reserved */

/*
 * MADT sub-structures (Follow MULTIPLE_APIC_DESCRIPTION_TABLE)
 */
#define ACPI_SUB_HEADER_DEF   /* Common ACPI sub-structure header */\
    u8  type;                               \
    u8  length;

/* Sub-structures for MADT */

struct madt_processor_apic
{
    ACPI_SUB_HEADER_DEF
    u8  processor_id;           /* ACPI processor id */
    u8  local_apic_id;          /* Processor's local APIC id */
#if 0
    u32 processor_enabled: 1;   /* Processor is usable if set */
    u32 reserved2       : 31;   /* Reserved, must be zero */
#else
    u32 flags;
#endif
} PACKED;

struct madt_io_apic
{
    ACPI_SUB_HEADER_DEF
    u8  io_apic_id;             /* I/O APIC ID */
    u8  reserved;               /* Reserved - must be zero */
    u32 address;                /* APIC physical address */
    u32 interrupt;              /* Global system interrupt where INTI
                                 * lines start */
} PACKED;

/* IRQs 5,9,10,11 */
#define PCI_ISA_IRQ_MASK    0x0e20

struct madt_intsrcovr {
    ACPI_SUB_HEADER_DEF
    u8  bus;
    u8  source;
    u32 gsi;
    u16 flags;
} PACKED;

/*
 * ACPI 2.0 Generic Address Space definition.
 */
struct acpi_20_generic_address {
    u8  address_space_id;
    u8  register_bit_width;
    u8  register_bit_offset;
    u8  reserved;
    u64 address;
} PACKED;

/*
 * HPET Description Table
 */
struct acpi_20_hpet {
    ACPI_TABLE_HEADER_DEF                    /* ACPI common table header */
    u32           timer_block_id;
    struct acpi_20_generic_address addr;
    u8            hpet_number;
    u16           min_tick;
    u8            page_protect;
} PACKED;
#define ACPI_HPET_ADDRESS 0xFED00000UL

/*
 * SRAT (NUMA topology description) table
 */

#define SRAT_PROCESSOR          0
#define SRAT_MEMORY             1

struct system_resource_affinity_table
{
    ACPI_TABLE_HEADER_DEF
    u32    reserved1;
    u32    reserved2[2];
} PACKED;

struct srat_processor_affinity
{
    ACPI_SUB_HEADER_DEF
    u8     proximity_lo;
    u8     local_apic_id;
    u32    flags;
    u8     local_sapic_eid;
    u8     proximity_hi[3];
    u32    reserved;
} PACKED;

struct srat_memory_affinity
{
    ACPI_SUB_HEADER_DEF
    u8     proximity[4];
    u16    reserved1;
    u32    base_addr_low,base_addr_high;
    u32    length_low,length_high;
    u32    reserved2;
    u32    flags;
    u32    reserved3[2];
} PACKED;

typedef struct {
    u16 vendid;
    u16 devid;
    u32 gpe0_blk;
    u32 gpe0_blk_len;
} southbridge_info_t;

static const southbridge_info_t acpi_southbridge_vec[] = {
        // PIIX4
        { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_3,
          0xafe0, 4 },

        // ICH10D0
        { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_0,
          PORT_ACPI_PM_BASE + 0x20, 16 },

        // ICH10R
        { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_1,
          PORT_ACPI_PM_BASE + 0x20, 16 },

        // ICH10 (consumer)
        { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_2,
          PORT_ACPI_PM_BASE + 0x20, 16 },

        // ICH10D
        { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICH10_3,
          PORT_ACPI_PM_BASE + 0x20, 16 },

        { 0, 0 }
};

#include "acpi-dsdt.hex"

static inline u16 cpu_to_le16(u16 x)
{
    return x;
}

static inline u32 cpu_to_le32(u32 x)
{
    return x;
}

static void
build_header(struct acpi_table_header *h, u32 sig, int len, u8 rev)
{
    h->signature = sig;
    h->length = cpu_to_le32(len);
    h->revision = rev;
    memcpy(h->oem_id, CONFIG_APPNAME6, 6);
    memcpy(h->oem_table_id, CONFIG_APPNAME4, 4);
    memcpy(h->asl_compiler_id, CONFIG_APPNAME4, 4);
    memcpy(h->oem_table_id + 4, (void*)&sig, 4);
    h->oem_revision = cpu_to_le32(1);
    h->asl_compiler_revision = cpu_to_le32(1);
    h->checksum -= checksum(h, len);
}

static void*
build_fadt(int bdf, int sb_id)
{
    struct fadt_descriptor_rev1 *fadt = malloc_high(sizeof(*fadt));
    struct facs_descriptor_rev1 *facs = memalign_high(64, sizeof(*facs));
    void *dsdt = malloc_high(sizeof(AmlCode));

    if (!fadt || !facs || !dsdt) {
        warn_noalloc();
        return NULL;
    }

    /* FACS */
    memset(facs, 0, sizeof(*facs));
    facs->signature = FACS_SIGNATURE;
    facs->length = cpu_to_le32(sizeof(*facs));

    /* DSDT */
    memcpy(dsdt, AmlCode, sizeof(AmlCode));

    /* FADT */
    memset(fadt, 0, sizeof(*fadt));
    fadt->firmware_ctrl = cpu_to_le32((u32)facs);
    fadt->dsdt = cpu_to_le32((u32)dsdt);
    fadt->model = 1;
    fadt->reserved1 = 0;
    int pm_sci_int = pci_config_readb(bdf, PCI_INTERRUPT_LINE);
    fadt->sci_int = cpu_to_le16(pm_sci_int);
    fadt->smi_cmd = cpu_to_le32(PORT_SMI_CMD);
    if (CONFIG_VIRTUTECH_MODEL) {
            fadt->acpi_enable = 0xf0;
            fadt->acpi_disable = 0xf1;
    } else {
            fadt->acpi_enable = 0xf1;
            fadt->acpi_disable = 0xf0;
    }
    fadt->pm1a_evt_blk = cpu_to_le32(PORT_ACPI_PM_BASE);
    fadt->pm1a_cnt_blk = cpu_to_le32(PORT_ACPI_PM_BASE + 0x04);
    fadt->pm_tmr_blk = cpu_to_le32(PORT_ACPI_PM_BASE + 0x08);
    fadt->pm1_evt_len = 4;
    fadt->pm1_cnt_len = 2;
    fadt->pm_tmr_len = 4;
    fadt->plvl2_lat = cpu_to_le16(0xfff); // C2 state not supported
    fadt->plvl3_lat = cpu_to_le16(0xfff); // C3 state not supported
    fadt->gpe0_blk = acpi_southbridge_vec[sb_id].gpe0_blk;
    fadt->gpe0_blk_len = acpi_southbridge_vec[sb_id].gpe0_blk_len;
    if (CONFIG_VIRTUTECH_MODEL) {
            /* WBINVD + PWR_BUTTON + SLP_BUTTON + FIX_RTC */
            fadt->flags = cpu_to_le32((1 << 0) | (1 << 4) | (1 << 5) | (1 << 6));
            /* Legacy devices and 8042 present */
            fadt->ia32_boot_arch = 3;
    } else {
            /* WBINVD + PROC_C1 + SLP_BUTTON + FIX_RTC */
            fadt->flags = cpu_to_le32((1 << 0) | (1 << 2) | (1 << 5) | (1 << 6));
    }

    build_header((void*)fadt, FACP_SIGNATURE, sizeof(*fadt), 1);

    return fadt;
}

static void*
build_madt(void)
{
    int madt_size = (sizeof(struct multiple_apic_table)
                     + sizeof(struct madt_processor_apic) * MaxCountCPUs
                     + sizeof(struct madt_io_apic)
                     + sizeof(struct madt_intsrcovr) * 16);
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
        if (i < CountCPUs)
            apic->flags = cpu_to_le32(1);
        else
            apic->flags = cpu_to_le32(0);
        apic++;
    }
    struct madt_io_apic *io_apic = (void*)apic;
    io_apic->type = APIC_IO;
    io_apic->length = sizeof(*io_apic);
    io_apic->io_apic_id = CountCPUs;
    io_apic->address = cpu_to_le32(BUILD_IOAPIC_ADDR);
    io_apic->interrupt = cpu_to_le32(0);

    struct madt_intsrcovr *intsrcovr = (void*)&io_apic[1];
    if (qemu_cfg_irq0_override()) {
        memset(intsrcovr, 0, sizeof(*intsrcovr));
        intsrcovr->type   = APIC_XRUPT_OVERRIDE;
        intsrcovr->length = sizeof(*intsrcovr);
        intsrcovr->source = 0;
        intsrcovr->gsi    = 2;
        intsrcovr->flags  = 0; /* conforms to bus specifications */
        intsrcovr++;
    }
    for (i = 1; i < 16; i++) {
        if (!(PCI_ISA_IRQ_MASK & (1 << i)))
            /* No need for a INT source override structure. */
            continue;
        memset(intsrcovr, 0, sizeof(*intsrcovr));
        intsrcovr->type   = APIC_XRUPT_OVERRIDE;
        intsrcovr->length = sizeof(*intsrcovr);
        intsrcovr->source = i;
        intsrcovr->gsi    = i;
        intsrcovr->flags  = 0xd; /* active high, level triggered */
        intsrcovr++;
    }

    build_header((void*)madt, APIC_SIGNATURE, (void*)intsrcovr - (void*)madt, 1);
    return madt;
}

#define SSDT_SIGNATURE 0x54445353 // SSDT
static void*
build_ssdt(void)
{
    int acpi_cpus = MaxCountCPUs > 0xff ? 0xff : MaxCountCPUs;
    // calculate the length of processor block and scope block
    // excluding PkgLength
    int cpu_length = 13 * acpi_cpus + 4;

    int length = sizeof(struct acpi_table_header) + 3 + cpu_length;
    u8 *ssdt = malloc_high(length);
    if (! ssdt) {
        warn_noalloc();
        return NULL;
    }

    u8 *ssdt_ptr = ssdt;
    ssdt_ptr[9] = 0; // checksum;
    ssdt_ptr += sizeof(struct acpi_table_header);

    // build processor scope header
    *(ssdt_ptr++) = 0x10; // ScopeOp
    if (cpu_length <= 0x3e) {
        /* Handle 1-4 CPUs with one byte encoding */
        *(ssdt_ptr++) = cpu_length + 1;
    } else {
        /* Handle 5-314 CPUs with two byte encoding */
        *(ssdt_ptr++) = 0x40 | ((cpu_length + 2) & 0xf);
        *(ssdt_ptr++) = (cpu_length + 2) >> 4;
    }
    *(ssdt_ptr++) = '_'; // Name
    *(ssdt_ptr++) = 'P';
    *(ssdt_ptr++) = 'R';
    *(ssdt_ptr++) = '_';

    // build object for each processor
    int i;
    for (i=0; i<acpi_cpus; i++) {
        *(ssdt_ptr++) = 0x5B; // ProcessorOp
        *(ssdt_ptr++) = 0x83;
        *(ssdt_ptr++) = 0x0B; // Length
        *(ssdt_ptr++) = 'C';  // Name (CPUxx)
        *(ssdt_ptr++) = 'P';
        if ((i & 0xf0) != 0)
            *(ssdt_ptr++) = (i >> 4) < 0xa ? (i >> 4) + '0' : (i >> 4) + 'A' - 0xa;
        else
            *(ssdt_ptr++) = 'U';
        *(ssdt_ptr++) = (i & 0xf) < 0xa ? (i & 0xf) + '0' : (i & 0xf) + 'A' - 0xa;
        *(ssdt_ptr++) = i;
        *(ssdt_ptr++) = 0x10; // Processor block address
        *(ssdt_ptr++) = 0xb0;
        *(ssdt_ptr++) = 0;
        *(ssdt_ptr++) = 0;
        *(ssdt_ptr++) = 6;    // Processor block length
    }

    build_header((void*)ssdt, SSDT_SIGNATURE, ssdt_ptr - ssdt, 1);

    return ssdt;
}

#define HPET_SIGNATURE 0x54455048 //HPET
static void*
build_hpet(void)
{
    struct acpi_20_hpet *hpet = malloc_high(sizeof(*hpet));
    if (!hpet) {
        warn_noalloc();
        return NULL;
    }

    memset(hpet, 0, sizeof(*hpet));
    /* Note timer_block_id value must be kept in sync with value advertised by
     * emulated hpet
     */
    hpet->timer_block_id = cpu_to_le32(0x8086a201);
    hpet->addr.address = cpu_to_le32(ACPI_HPET_ADDRESS);
    build_header((void*)hpet, HPET_SIGNATURE, sizeof(*hpet), 1);

    return hpet;
}

static void
acpi_build_srat_memory(struct srat_memory_affinity *numamem,
                       u64 base, u64 len, int node, int enabled)
{
    numamem->type = SRAT_MEMORY;
    numamem->length = sizeof(*numamem);
    memset (numamem->proximity, 0 ,4);
    numamem->proximity[0] = node;
    numamem->flags = cpu_to_le32(!!enabled);
    numamem->base_addr_low = base & 0xFFFFFFFF;
    numamem->base_addr_high = base >> 32;
    numamem->length_low = len & 0xFFFFFFFF;
    numamem->length_high = len >> 32;
}

#define SRAT_SIGNATURE 0x54415253 //HPET
static void *
build_srat(void)
{
    int nb_numa_nodes = qemu_cfg_get_numa_nodes();

    if (nb_numa_nodes == 0)
        return NULL;

    u64 *numadata = malloc_tmphigh(sizeof(u64) * (MaxCountCPUs + nb_numa_nodes));
    if (!numadata) {
        warn_noalloc();
        return NULL;
    }

    qemu_cfg_get_numa_data(numadata, MaxCountCPUs + nb_numa_nodes);

    struct system_resource_affinity_table *srat;
    int srat_size = sizeof(*srat) +
        sizeof(struct srat_processor_affinity) * MaxCountCPUs +
        sizeof(struct srat_memory_affinity) * (nb_numa_nodes + 2);

    srat = malloc_high(srat_size);
    if (!srat) {
        warn_noalloc();
        free(numadata);
        return NULL;
    }

    memset(srat, 0, srat_size);
    srat->reserved1=1;
    struct srat_processor_affinity *core = (void*)(srat + 1);
    int i;
    u64 curnode;

    for (i = 0; i < MaxCountCPUs; ++i) {
        core->type = SRAT_PROCESSOR;
        core->length = sizeof(*core);
        core->local_apic_id = i;
        curnode = *numadata++;
        core->proximity_lo = curnode;
        memset(core->proximity_hi, 0, 3);
        core->local_sapic_eid = 0;
        if (i < CountCPUs)
            core->flags = cpu_to_le32(1);
        else
            core->flags = 0;
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
    return srat;
}

struct rsdp_descriptor *RsdpAddr;

#define MAX_ACPI_TABLES 20
void
acpi_bios_init(void)
{
    if (! CONFIG_ACPI)
        return;

    dprintf(3, "init ACPI tables\n");

    int bdf = -1;
    u16 i;
    int sb_id = -1;
    for (i = 0; acpi_southbridge_vec[i].vendid; i++) {
            bdf = pci_find_device(acpi_southbridge_vec[i].vendid,
                                  acpi_southbridge_vec[i].devid);
            if (bdf >= 0) {
                    sb_id = i;
                    break;
            }
    }

    if (bdf < 0)
        // Device not found
        return;

    // Create initial rsdt table
    struct rsdp_descriptor *rsdp = malloc_fseg(sizeof(*rsdp));
    if (!rsdp) {
        warn_noalloc();
        return;
    }

    u32 tables[MAX_ACPI_TABLES], tbl_idx = 0;

#define ACPI_INIT_TABLE(X)                                   \
    do {                                                     \
        tables[tbl_idx] = (u32)(X);                          \
        if (tables[tbl_idx])                                 \
            tbl_idx++;                                       \
    } while(0)

    // Add tables
    ACPI_INIT_TABLE(build_fadt(bdf, sb_id));
    ACPI_INIT_TABLE(build_ssdt());
    ACPI_INIT_TABLE(build_madt());
    if (CONFIG_ACPI_HPET)
            ACPI_INIT_TABLE(build_hpet());
    ACPI_INIT_TABLE(build_srat());

    u16 external_tables = qemu_cfg_acpi_additional_tables();

    for(i = 0; i < external_tables; i++) {
        u16 len = qemu_cfg_next_acpi_table_len();
        void *addr = malloc_high(len);
        if (!addr) {
            warn_noalloc();
            continue;
        }
        ACPI_INIT_TABLE(qemu_cfg_next_acpi_table_load(addr, len));
        if (tbl_idx == MAX_ACPI_TABLES) {
            warn_noalloc();
            break;
        }
    }

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
    memset(rsdp, 0, sizeof(*rsdp));
    rsdp->signature = RSDP_SIGNATURE;
    memcpy(rsdp->oem_id, CONFIG_APPNAME6, 6);
    rsdp->rsdt_physical_address = cpu_to_le32((u32)rsdt);
    rsdp->checksum -= checksum(rsdp, 20);
    RsdpAddr = rsdp;
    dprintf(1, "ACPI tables: RSDP=%p RSDT=%p\n", rsdp, rsdt);
}

u32
find_resume_vector(void)
{
    dprintf(4, "rsdp=%p\n", RsdpAddr);
    if (!RsdpAddr || RsdpAddr->signature != RSDP_SIGNATURE)
        return 0;
    struct rsdt_descriptor_rev1 *rsdt = (void*)RsdpAddr->rsdt_physical_address;
    dprintf(4, "rsdt=%p\n", rsdt);
    if (!rsdt || rsdt->signature != RSDT_SIGNATURE)
        return 0;
    void *end = (void*)rsdt + rsdt->length;
    int i;
    for (i=0; (void*)&rsdt->table_offset_entry[i] < end; i++) {
        struct fadt_descriptor_rev1 *fadt = (void*)rsdt->table_offset_entry[i];
        if (!fadt || fadt->signature != FACP_SIGNATURE)
            continue;
        dprintf(4, "fadt=%p\n", fadt);
        struct facs_descriptor_rev1 *facs = (void*)fadt->firmware_ctrl;
        dprintf(4, "facs=%p\n", facs);
        if (! facs || facs->signature != FACS_SIGNATURE)
            return 0;
        // Found it.
        dprintf(4, "resume addr=%d\n", facs->firmware_waking_vector);
        return facs->firmware_waking_vector;
    }
    return 0;
}
