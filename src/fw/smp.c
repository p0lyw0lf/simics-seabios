// CPU count detection
//
// Copyright (C) 2008  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2006 Fabrice Bellard
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "config.h" // CONFIG_*
#include "hw/rtc.h" // CMOS_BIOS_SMP_COUNT
#include "output.h" // dprintf
#include "romfile.h" // romfile_loadint
#include "stacks.h" // yield
#include "util.h" // smp_setup, msr_feature_control_setup
#include "x86.h" // wrmsr
#include "paravirt.h" // qemu_*_present_cpus_count

#define APIC_ICR_LOW ((u8*)BUILD_APIC_ADDR + 0x300)
#define APIC_SVR     ((u8*)BUILD_APIC_ADDR + 0x0F0)
#define APIC_LINT0   ((u8*)BUILD_APIC_ADDR + 0x350)
#define APIC_LINT1   ((u8*)BUILD_APIC_ADDR + 0x360)

#define APIC_ENABLED 0x0100
#define MSR_IA32_APIC_BASE 0x01B
#define MSR_LOCAL_APIC_ID 0x802
#define MSR_IA32_APICBASE_EXTD (1ULL << 10) /* Enable x2APIC mode */

struct { u32 index; u64 val; } smp_msr[33] VARFSEG;
u32 smp_msr_count VARFSEG;

void
wrmsr_smp(u32 index, u64 val)
{
    unsigned i;
    wrmsr(index, val);
    for (i = 0; i < smp_msr_count; i++)
        if (smp_msr[i].index == index)
            break;
    if (i >= ARRAY_SIZE(smp_msr))
        panic("smp_msr table overflow");
    if (i == smp_msr_count)
        smp_msr_count++;

    smp_msr[i].index = index;
    smp_msr[i].val = val;
}

static void
smp_write_msrs(void)
{
    // MTRR and MSR_IA32_FEATURE_CONTROL setup
    int i;
    for (i=0; i<smp_msr_count; i++)
        wrmsr(smp_msr[i].index, smp_msr[i].val);
}

u32 CountCPUs VARFSEG;
u32 MaxCountCPUs;
// 256 bits for the found APIC IDs
u32 FoundAPICIDs[256/32] VARFSEG;
extern void smp_ap_boot_code(void);
ASM16(
    "  .global smp_ap_boot_code\n"
    "smp_ap_boot_code:\n"

    // Setup data segment
    "  movw $" __stringify(SEG_BIOS) ", %ax\n"
    "  movw %ax, %ds\n"

    // MTRR setup
    "  movl $smp_msr, %esi\n"
    "  movl smp_msr_count, %ebx\n"
    "1:testl %ebx, %ebx\n"
    "  jz 2f\n"
    "  movl 0(%esi), %ecx\n"
    "  movl 4(%esi), %eax\n"
    "  movl 8(%esi), %edx\n"
    "  wrmsr\n"
    "  addl $12, %esi\n"
    "  decl %ebx\n"
    "  jmp 1b\n"
    "2:\n"

    // get apic ID on EBX, set bit on FoundAPICIDs
    "  movl $1, %eax\n"
    "  cpuid\n"
    "  shrl $24, %ebx\n"
    "  lock btsl %ebx, FoundAPICIDs\n"

    // Increment the cpu counter
    "  lock incl CountCPUs\n"

    // Halt the processor.
    "1:hlt\n"
    "  jmp 1b\n"
    );

int apic_id_is_present(u8 apic_id)
{
    return !!(FoundAPICIDs[apic_id/32] & (1ul << (apic_id % 32)));
}

// find and initialize the CPUs by launching a SIPI to them
static void
smp_scan(void)
{
    ASSERT32FLAT();
    u32 eax, ebx, ecx, cpuid_features;
    cpuid(1, &eax, &ebx, &ecx, &cpuid_features);
    if (eax < 1 || !(cpuid_features & CPUID_APIC)) {
        // No apic - only the main cpu is present.
        dprintf(1, "No apic - only the main cpu is present.\n");
        CountCPUs= 1;
        return;
    }

    // mark the BSP initial APIC ID as found, too:
    u8 apic_id = ebx>>24;
    FoundAPICIDs[apic_id/32] |= (1 << (apic_id % 32));

    // Init the counter.
    writel(&CountCPUs, 1);

    // Setup jump trampoline to counter code.
    u64 old = *(u64*)BUILD_AP_BOOT_ADDR;
    // ljmpw $SEG_BIOS, $(smp_ap_boot_code - BUILD_BIOS_ADDR)
    u64 new = (0xea | ((u64)SEG_BIOS<<24)
               | (((u32)smp_ap_boot_code - BUILD_BIOS_ADDR) << 8));
    *(u64*)BUILD_AP_BOOT_ADDR = new;

    // enable local APIC
    u32 val = readl(APIC_SVR);
    writel(APIC_SVR, val | APIC_ENABLED);

    /* Set LINT0 as Ext_INT, level triggered */
    writel(APIC_LINT0, 0x8700);

    /* Set LINT1 as NMI, level triggered */
    writel(APIC_LINT1, 0x8400);

    // broadcast SIPI
    barrier();
    writel(APIC_ICR_LOW, 0x000C4500);
    u32 sipi_vector = BUILD_AP_BOOT_ADDR >> 12;
    writel(APIC_ICR_LOW, 0x000C4600 | sipi_vector);

    // Wait for other CPUs to process the SIPI.
    if (!CONFIG_USE_CMOS_BIOS_SMP_COUNT) {
        MaxCountCPUs = romfile_loadint("etc/max-cpus", 0);
        
        /* Wait up to 10 ms, with early out */
        int ms_count;
        for (ms_count = 0; ms_count < 10000; ms_count++) {
            if (MaxCountCPUs && CountCPUs == MaxCountCPUs)
                break;
            udelay(1);
        }
    } else {
        u8 expected_cpus_count = qemu_get_present_cpus_count();
    	while (expected_cpus_count + 1 != readl(&CountCPUs))
        	yield();
    }

    // Restore memory.
    *(u64*)BUILD_AP_BOOT_ADDR = old;

    dprintf(1, "Found %d cpu(s) max supported %d cpu(s)\n", readl(&CountCPUs),
        MaxCountCPUs);
}

void
smp_setup(void)
{
    if (!CONFIG_QEMU)
        return;

    MaxCountCPUs = romfile_loadint("etc/max-cpus", 0);
    u16 smp_count = qemu_get_present_cpus_count();
    if (MaxCountCPUs < smp_count)
        MaxCountCPUs = smp_count;

    smp_scan();
}

void
smp_resume(void)
{
    if (!CONFIG_QEMU)
        return;

    smp_write_msrs();
    smp_scan();
}
