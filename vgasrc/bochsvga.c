// Bochs VGA interface to extended "VBE" modes
//
// Copyright (C) 2012  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2011  Julian Pidancet <julian.pidancet@citrix.com>
//  Copyright (C) 2002 Jeroen Janssen
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // GET_GLOBAL
#include "bochsvga.h" // bochsvga_set_mode
#include "config.h" // CONFIG_*
#include "hw/pci.h" // pci_config_readl
#include "hw/pci_regs.h" // PCI_BASE_ADDRESS_0
#include "output.h" // dprintf
#include "std/vbe.h" // VBE_CAPABILITY_8BIT_DAC
#include "stdvga.h" // stdvga_get_linelength
#include "vgabios.h" // SET_VGA
#include "vgautil.h" // VBE_total_memory
#include "x86.h" // outw
#include "vbe_edid.h"

/****************************************************************
 * Mode tables
 ****************************************************************/

static struct bochsvga_mode
{
    u16 mode;
    struct vgamode_s info;
} bochsvga_modes[] VAR16 = {
    /* standard modes */
    { 0x100, { MM_PACKED, 640,  400,  8,  8, 16, SEG_GRAPH } },
    { 0x101, { MM_PACKED, 640,  480,  8,  8, 16, SEG_GRAPH } },
    { 0x102, { MM_PLANAR, 800,  600,  4,  8, 16, SEG_GRAPH } },
    { 0x103, { MM_PACKED, 800,  600,  8,  8, 16, SEG_GRAPH } },
    { 0x104, { MM_PLANAR, 1024, 768,  4,  8, 16, SEG_GRAPH } },
    { 0x105, { MM_PACKED, 1024, 768,  8,  8, 16, SEG_GRAPH } },
    { 0x106, { MM_PLANAR, 1280, 1024, 4,  8, 16, SEG_GRAPH } },
    { 0x107, { MM_PACKED, 1280, 1024, 8,  8, 16, SEG_GRAPH } },
    { 0x10D, { MM_DIRECT, 320,  200,  15, 8, 16, SEG_GRAPH } },
    { 0x10E, { MM_DIRECT, 320,  200,  16, 8, 16, SEG_GRAPH } },
    { 0x10F, { MM_DIRECT, 320,  200,  24, 8, 16, SEG_GRAPH } },
    { 0x110, { MM_DIRECT, 640,  480,  15, 8, 16, SEG_GRAPH } },
    { 0x111, { MM_DIRECT, 640,  480,  16, 8, 16, SEG_GRAPH } },
    { 0x112, { MM_DIRECT, 640,  480,  24, 8, 16, SEG_GRAPH } },
    { 0x113, { MM_DIRECT, 800,  600,  15, 8, 16, SEG_GRAPH } },
    { 0x114, { MM_DIRECT, 800,  600,  16, 8, 16, SEG_GRAPH } },
    { 0x115, { MM_DIRECT, 800,  600,  24, 8, 16, SEG_GRAPH } },
    { 0x116, { MM_DIRECT, 1024, 768,  15, 8, 16, SEG_GRAPH } },
    { 0x117, { MM_DIRECT, 1024, 768,  16, 8, 16, SEG_GRAPH } },
    { 0x118, { MM_DIRECT, 1024, 768,  24, 8, 16, SEG_GRAPH } },
    { 0x119, { MM_DIRECT, 1280, 1024, 15, 8, 16, SEG_GRAPH } },
    { 0x11A, { MM_DIRECT, 1280, 1024, 16, 8, 16, SEG_GRAPH } },
    { 0x11B, { MM_DIRECT, 1280, 1024, 24, 8, 16, SEG_GRAPH } },
    { 0x11C, { MM_PACKED, 1600, 1200, 8,  8, 16, SEG_GRAPH } },
    { 0x11D, { MM_DIRECT, 1600, 1200, 15, 8, 16, SEG_GRAPH } },
    { 0x11E, { MM_DIRECT, 1600, 1200, 16, 8, 16, SEG_GRAPH } },
    { 0x11F, { MM_DIRECT, 1600, 1200, 24, 8, 16, SEG_GRAPH } },
    /* BOCHS modes */
    { 0x140, { MM_DIRECT, 320,  200,  32, 8, 16, SEG_GRAPH } },
    { 0x141, { MM_DIRECT, 640,  400,  32, 8, 16, SEG_GRAPH } },
    { 0x142, { MM_DIRECT, 640,  480,  32, 8, 16, SEG_GRAPH } },
    { 0x143, { MM_DIRECT, 800,  600,  32, 8, 16, SEG_GRAPH } },
    { 0x144, { MM_DIRECT, 1024, 768,  32, 8, 16, SEG_GRAPH } },
    { 0x145, { MM_DIRECT, 1280, 1024, 32, 8, 16, SEG_GRAPH } },
    { 0x146, { MM_PACKED, 320,  200,  8,  8, 16, SEG_GRAPH } },
    { 0x147, { MM_DIRECT, 1600, 1200, 32, 8, 16, SEG_GRAPH } },
    { 0x148, { MM_PACKED, 1152, 864,  8,  8, 16, SEG_GRAPH } },
    { 0x149, { MM_DIRECT, 1152, 864,  15, 8, 16, SEG_GRAPH } },
    { 0x14a, { MM_DIRECT, 1152, 864,  16, 8, 16, SEG_GRAPH } },
    { 0x14b, { MM_DIRECT, 1152, 864,  24, 8, 16, SEG_GRAPH } },
    { 0x14c, { MM_DIRECT, 1152, 864,  32, 8, 16, SEG_GRAPH } },
    { 0x175, { MM_DIRECT, 1280, 768,  16, 8, 16, SEG_GRAPH } },
    { 0x176, { MM_DIRECT, 1280, 768,  24, 8, 16, SEG_GRAPH } },
    { 0x177, { MM_DIRECT, 1280, 768,  32, 8, 16, SEG_GRAPH } },
    { 0x178, { MM_DIRECT, 1280, 800,  16, 8, 16, SEG_GRAPH } },
    { 0x179, { MM_DIRECT, 1280, 800,  24, 8, 16, SEG_GRAPH } },
    { 0x17a, { MM_DIRECT, 1280, 800,  32, 8, 16, SEG_GRAPH } },
    { 0x17b, { MM_DIRECT, 1280, 960,  16, 8, 16, SEG_GRAPH } },
    { 0x17c, { MM_DIRECT, 1280, 960,  24, 8, 16, SEG_GRAPH } },
    { 0x17d, { MM_DIRECT, 1280, 960,  32, 8, 16, SEG_GRAPH } },
    { 0x17e, { MM_DIRECT, 1440, 900,  16, 8, 16, SEG_GRAPH } },
    { 0x17f, { MM_DIRECT, 1440, 900,  24, 8, 16, SEG_GRAPH } },
    { 0x180, { MM_DIRECT, 1440, 900,  32, 8, 16, SEG_GRAPH } },
    { 0x181, { MM_DIRECT, 1400, 1050, 16, 8, 16, SEG_GRAPH } },
    { 0x182, { MM_DIRECT, 1400, 1050, 24, 8, 16, SEG_GRAPH } },
    { 0x183, { MM_DIRECT, 1400, 1050, 32, 8, 16, SEG_GRAPH } },
    { 0x184, { MM_DIRECT, 1680, 1050, 16, 8, 16, SEG_GRAPH } },
    { 0x185, { MM_DIRECT, 1680, 1050, 24, 8, 16, SEG_GRAPH } },
    { 0x186, { MM_DIRECT, 1680, 1050, 32, 8, 16, SEG_GRAPH } },
    { 0x187, { MM_DIRECT, 1920, 1200, 16, 8, 16, SEG_GRAPH } },
    { 0x188, { MM_DIRECT, 1920, 1200, 24, 8, 16, SEG_GRAPH } },
    { 0x189, { MM_DIRECT, 1920, 1200, 32, 8, 16, SEG_GRAPH } },
    { 0x18a, { MM_DIRECT, 2560, 1600, 16, 8, 16, SEG_GRAPH } },
    { 0x18b, { MM_DIRECT, 2560, 1600, 24, 8, 16, SEG_GRAPH } },
    { 0x18c, { MM_DIRECT, 2560, 1600, 32, 8, 16, SEG_GRAPH } },
    { 0x18d, { MM_DIRECT, 1280, 720,  16, 8, 16, SEG_GRAPH } },
    { 0x18e, { MM_DIRECT, 1280, 720,  24, 8, 16, SEG_GRAPH } },
    { 0x18f, { MM_DIRECT, 1280, 720,  32, 8, 16, SEG_GRAPH } },
    { 0x190, { MM_DIRECT, 1920, 1080, 16, 8, 16, SEG_GRAPH } },
    { 0x191, { MM_DIRECT, 1920, 1080, 24, 8, 16, SEG_GRAPH } },
    { 0x192, { MM_DIRECT, 1920, 1080, 32, 8, 16, SEG_GRAPH } },
};

static int dispi_found VAR16 = 0;

static int is_bochsvga_mode(struct vgamode_s *vmode_g)
{
    return (vmode_g >= &bochsvga_modes[0].info
            && vmode_g <= &bochsvga_modes[ARRAY_SIZE(bochsvga_modes)-1].info);
}

struct vgamode_s *bochsvga_find_mode(int mode)
{
    struct bochsvga_mode *m = bochsvga_modes;
    if (GET_GLOBAL(dispi_found))
        for (; m < &bochsvga_modes[ARRAY_SIZE(bochsvga_modes)]; m++)
            if (GET_GLOBAL(m->mode) == mode)
                return &m->info;
    return stdvga_find_mode(mode);
}

void
bochsvga_list_modes(u16 seg, u16 *dest, u16 *last)
{
    struct bochsvga_mode *m = bochsvga_modes;
    if (GET_GLOBAL(dispi_found)) {
        for (; m < &bochsvga_modes[ARRAY_SIZE(bochsvga_modes)] && dest<last; m++) {
            u16 mode = GET_GLOBAL(m->mode);
            if (mode == 0xffff)
                continue;
            SET_FARVAR(seg, *dest, mode);
            dest++;
        }
    }
    stdvga_list_modes(seg, dest, last);
}


/****************************************************************
 * Helper functions
 ****************************************************************/

static inline u16 dispi_read(u16 reg)
{
    outw(reg, VBE_DISPI_IOPORT_INDEX);
    return inw(VBE_DISPI_IOPORT_DATA);
}
static inline void dispi_write(u16 reg, u16 val)
{
    outw(reg, VBE_DISPI_IOPORT_INDEX);
    outw(val, VBE_DISPI_IOPORT_DATA);
}

static u8
bochsvga_dispi_enabled(void)
{
    if (!GET_GLOBAL(dispi_found))
        return 0;
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    if (!(en & VBE_DISPI_ENABLED))
        return 0;
    return 1;
}

int
bochsvga_get_window(struct vgamode_s *vmode_g, int window)
{
    if (!bochsvga_dispi_enabled())
        return stdvga_get_window(vmode_g, window);
    if (window != 0)
        return -1;
    return dispi_read(VBE_DISPI_INDEX_BANK);
}

int
bochsvga_set_window(struct vgamode_s *vmode_g, int window, int val)
{
    if (!bochsvga_dispi_enabled())
        return stdvga_set_window(vmode_g, window, val);
    if (window != 0)
        return -1;
    dispi_write(VBE_DISPI_INDEX_BANK, val);
    if (dispi_read(VBE_DISPI_INDEX_BANK) != val)
        return -1;
    return 0;
}

int
bochsvga_get_linelength(struct vgamode_s *vmode_g)
{
    if (!bochsvga_dispi_enabled())
        return stdvga_get_linelength(vmode_g);
    return dispi_read(VBE_DISPI_INDEX_VIRT_WIDTH) * vga_bpp(vmode_g) / 8;
}

int
bochsvga_set_linelength(struct vgamode_s *vmode_g, int val)
{
    stdvga_set_linelength(vmode_g, val);
    if (bochsvga_dispi_enabled()) {
        int pixels = (val * 8) / vga_bpp(vmode_g);
        dispi_write(VBE_DISPI_INDEX_VIRT_WIDTH, pixels);
    }
    return 0;
}

int
bochsvga_get_displaystart(struct vgamode_s *vmode_g)
{
    if (!bochsvga_dispi_enabled())
        return stdvga_get_displaystart(vmode_g);
    int bpp = vga_bpp(vmode_g);
    int linelength = dispi_read(VBE_DISPI_INDEX_VIRT_WIDTH) * bpp / 8;
    int x = dispi_read(VBE_DISPI_INDEX_X_OFFSET);
    int y = dispi_read(VBE_DISPI_INDEX_Y_OFFSET);
    return x * bpp / 8 + linelength * y;
}

int
bochsvga_set_displaystart(struct vgamode_s *vmode_g, int val)
{
    stdvga_set_displaystart(vmode_g, val);
    if (bochsvga_dispi_enabled()) {
        int bpp = vga_bpp(vmode_g);
        int linelength = dispi_read(VBE_DISPI_INDEX_VIRT_WIDTH) * bpp / 8;
        if (!linelength)
            return 0;
        dispi_write(VBE_DISPI_INDEX_X_OFFSET, (val % linelength) * 8 / bpp);
        dispi_write(VBE_DISPI_INDEX_Y_OFFSET, val / linelength);
    }
    return 0;
}

int
bochsvga_get_dacformat(struct vgamode_s *vmode_g)
{
    if (!bochsvga_dispi_enabled())
        return stdvga_get_dacformat(vmode_g);
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    return (en & VBE_DISPI_8BIT_DAC) ? 8 : 6;
}

int
bochsvga_set_dacformat(struct vgamode_s *vmode_g, int val)
{
    if (!bochsvga_dispi_enabled())
        return stdvga_set_dacformat(vmode_g, val);
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    if (val == 6)
        en &= ~VBE_DISPI_8BIT_DAC;
    else if (val == 8)
        en |= VBE_DISPI_8BIT_DAC;
    else
        return -1;
    dispi_write(VBE_DISPI_INDEX_ENABLE, en);
    return 0;
}

static int
bochsvga_save_state(u16 seg, u16 *info)
{
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    SET_FARVAR(seg, *info, en);
    info++;
    if (!(en & VBE_DISPI_ENABLED))
        return 0;
    int i;
    for (i = VBE_DISPI_INDEX_XRES; i <= VBE_DISPI_INDEX_Y_OFFSET; i++)
        if (i != VBE_DISPI_INDEX_ENABLE) {
            u16 v = dispi_read(i);
            SET_FARVAR(seg, *info, v);
            info++;
        }
    return 0;
}

static int
bochsvga_restore_state(u16 seg, u16 *info)
{
    u16 en = GET_FARVAR(seg, *info);
    info++;
    if (!(en & VBE_DISPI_ENABLED)) {
        dispi_write(VBE_DISPI_INDEX_ENABLE, en);
        return 0;
    }
    int i;
    for (i = VBE_DISPI_INDEX_XRES; i <= VBE_DISPI_INDEX_Y_OFFSET; i++)
        if (i == VBE_DISPI_INDEX_ENABLE) {
            dispi_write(i, en);
        } else {
            dispi_write(i, GET_FARVAR(seg, *info));
            info++;
        }
    return 0;
}

int
bochsvga_save_restore(int cmd, u16 seg, void *data)
{
    int ret = stdvga_save_restore(cmd, seg, data);
    if (ret < 0 || !(cmd & SR_REGISTERS) || !GET_GLOBAL(dispi_found))
        return ret;

    u16 *info = (data + ret);
    if (cmd & SR_SAVE)
        bochsvga_save_state(seg, info);
    if (cmd & SR_RESTORE)
        bochsvga_restore_state(seg, info);
    return ret + (VBE_DISPI_INDEX_Y_OFFSET-VBE_DISPI_INDEX_XRES+1)*sizeof(u16);
}


/****************************************************************
 * Mode setting
 ****************************************************************/

int
bochsvga_set_mode(struct vgamode_s *vmode_g, int flags)
{
    if (GET_GLOBAL(dispi_found))
        dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    if (! is_bochsvga_mode(vmode_g))
        return stdvga_set_mode(vmode_g, flags);
    if (!GET_GLOBAL(dispi_found))
        return -1;

    u8 memmodel = GET_GLOBAL(vmode_g->memmodel);
    if (memmodel == MM_PLANAR)
        stdvga_set_mode(stdvga_find_mode(0x6a), 0);
    if (memmodel == MM_PACKED && !(flags & MF_NOPALETTE))
        stdvga_set_packed_palette();

    dispi_write(VBE_DISPI_INDEX_BPP, GET_GLOBAL(vmode_g->depth));
    u16 width = GET_GLOBAL(vmode_g->width);
    u16 height = GET_GLOBAL(vmode_g->height);
    dispi_write(VBE_DISPI_INDEX_XRES, width);
    dispi_write(VBE_DISPI_INDEX_YRES, height);
    dispi_write(VBE_DISPI_INDEX_BANK, 0);
    u16 bf = ((flags & MF_NOCLEARMEM ? VBE_DISPI_NOCLEARMEM : 0)
              | (flags & MF_LINEARFB ? VBE_DISPI_LFB_ENABLED : 0));
    dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | bf);

    /* VGA compat setup */
    u16 crtc_addr = VGAREG_VGA_CRTC_ADDRESS;
    stdvga_crtc_write(crtc_addr, 0x11, 0x00);
    stdvga_crtc_write(crtc_addr, 0x01, width / 8 - 1);
    stdvga_set_linelength(vmode_g, width);
    stdvga_crtc_write(crtc_addr, 0x12, height - 1);
    u8 v = 0;
    if ((height - 1) & 0x0100)
        v |= 0x02;
    if ((height - 1) & 0x0200)
        v |= 0x40;
    stdvga_crtc_mask(crtc_addr, 0x07, 0x42, v);

    stdvga_crtc_write(crtc_addr, 0x09, 0x00);
    stdvga_crtc_mask(crtc_addr, 0x17, 0x00, 0x03);
    stdvga_attr_mask(0x10, 0x00, 0x01);
    stdvga_grdc_write(0x06, 0x05);
    stdvga_sequ_write(0x02, 0x0f);
    if (memmodel != MM_PLANAR) {
        stdvga_crtc_mask(crtc_addr, 0x14, 0x00, 0x40);
        stdvga_attr_mask(0x10, 0x00, 0x40);
        stdvga_sequ_mask(0x04, 0x00, 0x08);
        stdvga_grdc_mask(0x05, 0x20, 0x40);
    }
    stdvga_attrindex_write(0x20);

    return 0;
}

int bochsvga_get_ddc_capabilities(u16 unit)
{
    if (unit != 0)
        return -1;

    return (1 << 8) | VBE_DDC1_PROTOCOL_SUPPORTED;
}

u8 most_chromaticity[8] VAR16 = {0xA6,0x55,0x48,0x9B,0x26,0x12,0x50,0x54};
unsigned char vgabios_name[] VAR16 = "Sea VGABIOS";
struct edid_detailed_timing vbe_edid_dtd_1920x1080 VAR16 = {
     WORDBE(0x3a02), 0x80, 0x90, 0x72, 0x38, 0x26,
     0x40, 0x78, 0xD0, 0x13, 0x00, 0x14, 0x2B, 0x21,
     0x00, 0x00, 0x1E};
struct edid_detailed_timing vbe_edid_dtd_1280x1024 VAR16 = {
     WORDBE(0x2a30), 0x00, 0x98, 0x51, 0x00, 0x2A, 
     0x40, 0x30, 0x70, 0x13, 0x00, 0x2C, 0xE1, 0x10,
     0x00, 0x00, 0x1E};
struct edid_detailed_timing vbe_edid_dtd_1152x864 VAR16 = {
     WORDBE(0x2a30), 0x80, 0xC0, 0x41, 0x60, 0x24,
     0x30, 0x40, 0x80, 0x13, 0x00, 0x2C, 0xE1, 0x10,
     0x00, 0x00, 0x1E};
struct edid_detailed_timing vbe_edid_dtd_1600x1200 VAR16 = {
     WORDBE(0x3c8c), 0x40, 0x30, 0x62, 0xB0, 0x32,
     0x40, 0x40, 0xC0, 0x13, 0x00, 0x2C, 0xE1, 0x10,
     0x00, 0x00, 0x1E};

int bochsvga_read_edid_block0(u16 unit, u16 block, u16 seg, void *data)
{
    struct vbe_edid_info  *info = data;
    int i;

    memset_far(seg, info, 0, sizeof(*info));
    /* header */
    SET_FARVAR(seg, info->header[0], 0);
    for (i = 1; i < 7; i++) {
        SET_FARVAR(seg, info->header[i], 0xFF);
    }
    SET_FARVAR(seg, info->header[7], 0);
    /* Vendor/Product/Serial/Date */
    SET_FARVAR(seg, info->vendor, WORDBE(0x0421));
    SET_FARVAR(seg, info->product,WORDBE(0xABCD));
    SET_FARVAR(seg, info->serial, DWORDBE(0));
    /* date/version  */
    SET_FARVAR(seg, info->week,54);
    SET_FARVAR(seg, info->year,10); /* 2000 */
    SET_FARVAR(seg, info->major_version,1);
    SET_FARVAR(seg, info->minor_version,3); /* 1.3 */
    /* video prameters */
    SET_FARVAR(seg, info->video_setup,0x0F);
    /* Video signal interface (analogue, 0.700 : 0.300 : 1.000 V p-p,
       Video Setup: Blank Level = Black Level, Separate Sync H & V Signals are
       supported, Composite Sync Signal on Horizontal is supported, Composite 
       Sync Signal on Green Video is supported, Serration on the Vertical Sync
       is supported) */
    SET_FARVAR(seg, info->screen_width,0x21);
    SET_FARVAR(seg, info->screen_height,0x19); /* 330 mm * 250 mm */
    SET_FARVAR(seg, info->gamma,0x78); /* 2.2 */
    SET_FARVAR(seg, info->feature_flag,0x0D); /* no DMPS states, RGB, display is continuous frequency */
    SET_FARVAR(seg, info->least_chromaticity[0],0x78);
    SET_FARVAR(seg, info->least_chromaticity[1],0xF5);
    memcpy_far(seg, info->most_chromaticity, get_global_seg(), most_chromaticity,
               sizeof (most_chromaticity));

    SET_FARVAR(seg, info->established_timing[0], 0xFF);
    SET_FARVAR(seg, info->established_timing[1], 0xEF);
    SET_FARVAR(seg, info->established_timing[2], 0x80);
    /* 720x400@70Hz, 720x400@88Hz, 640x480@60Hz, 640x480@67Hz, 640x480@72Hz, 640x480@75Hz,
       800x600@56Hz, 800x600@60Hz, 800x600@72Hz, 800x600@75Hz, 832x624@75Hz, 1152x870@75Hz,
       not 1024x768@87Hz(I), 1024x768@60Hz, 1024x768@70Hz, 1024x768@75Hz, 1280x1024@75Hz */
    /* standard timings */
    SET_FARVAR(seg, info->standard_timing[0], VBE_EDID_STD_640x480_85Hz);
    SET_FARVAR(seg, info->standard_timing[1], VBE_EDID_STD_800x600_85Hz);
    SET_FARVAR(seg, info->standard_timing[2], VBE_EDID_STD_1024x768_85Hz);
    SET_FARVAR(seg, info->standard_timing[3], VBE_EDID_STD_1280x720_70Hz);
    SET_FARVAR(seg, info->standard_timing[4], VBE_EDID_STD_1280x960_60Hz);
    SET_FARVAR(seg, info->standard_timing[5], VBE_EDID_STD_1440x900_60Hz);
    SET_FARVAR(seg, info->standard_timing[6], VBE_EDID_STD_1600x1200_60Hz);
    SET_FARVAR(seg, info->standard_timing[7], VBE_EDID_STD_1680x1050_60Hz);
    /* detailed timing blocks */
    memcpy_far(seg, &(info->desc[0].dtd), get_global_seg(), &vbe_edid_dtd_1152x864,
               sizeof (vbe_edid_dtd_1152x864));
    memcpy_far(seg, &(info->desc[1].dtd), get_global_seg(), &vbe_edid_dtd_1280x1024,
               sizeof (vbe_edid_dtd_1280x1024));
    /* serial */
    for (i = 0; i < 5; i++) {
        SET_FARVAR(seg, info->desc[2].mtxtd.header[i], 0);
    }
    SET_FARVAR(seg, info->desc[2].mtxtd.header[3], 0xFF);
    for (i = 0; i < 10; i++) {
        SET_FARVAR(seg, info->desc[2].mtxtd.text[i], i+0x30);
    }
    SET_FARVAR(seg, info->desc[2].mtxtd.text[10], 0x0A);
    SET_FARVAR(seg, info->desc[2].mtxtd.text[11], 0x20);
    SET_FARVAR(seg, info->desc[2].mtxtd.text[12], 0x20);
    /* monitor name */
    for (i = 0; i < 5; i++) {
         SET_FARVAR(seg, info->desc[3].mtxtd.header[i], 0);
    }
    SET_FARVAR(seg, info->desc[3].mtxtd.header[3], 0xFC);
    memcpy_far(seg, info->desc[3].mtxtd.text, get_global_seg(), vgabios_name, 12);
    SET_FARVAR(seg, info->desc[3].mtxtd.text[12], 0x0A);
    /* ext */
    SET_FARVAR(seg, info->extensions, 0);

    /* checksum */
    u8 sum = -checksum_far(get_global_seg(), info, sizeof(info));
    SET_FARVAR(seg, info->checksum, sum);

    return 0;
}

int bochsvga_read_edid(u16 unit, u16 block, u16 seg, void *data)
{
    if (unit != 0)
        return -1;

    switch (block) {
    case 0:
        return bochsvga_read_edid_block0(unit, block, seg, data);
    default:
        return -1;
    }
}

/****************************************************************
 * Init
 ****************************************************************/

int
bochsvga_setup(void)
{
    int ret = stdvga_setup();
    if (ret)
        return ret;

    /* Sanity checks */
    dispi_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID0);
    if (dispi_read(VBE_DISPI_INDEX_ID) != VBE_DISPI_ID0) {
        dprintf(1, "No VBE DISPI interface detected, falling back to stdvga\n");
        return 0;
    }

    dispi_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID5);
    SET_VGA(dispi_found, 1);

    if (GET_GLOBAL(HaveRunInit))
        return 0;

    u32 lfb_addr = VBE_DISPI_LFB_PHYSICAL_ADDRESS;
    int bdf = GET_GLOBAL(VgaBDF);
    if (CONFIG_VGA_PCI && bdf >= 0) {
        u16 vendor = pci_config_readw(bdf, PCI_VENDOR_ID);
        int barid;
        switch (vendor) {
        case 0x15ad: /* qemu vmware vga */
            barid = 1;
            break;
        default: /* stdvga, qxl, virtio */
            barid = 0;
            break;
        }
        u32 bar = pci_config_readl(bdf, PCI_BASE_ADDRESS_0 + barid * 4);
        lfb_addr = bar & PCI_BASE_ADDRESS_MEM_MASK;
        dprintf(1, "VBE DISPI: bdf %02x:%02x.%x, bar %d\n", pci_bdf_to_bus(bdf)
                , pci_bdf_to_dev(bdf), pci_bdf_to_fn(bdf), barid);
    }

    SET_VGA(VBE_framebuffer, lfb_addr);
    u32 totalmem = dispi_read(VBE_DISPI_INDEX_VIDEO_MEMORY_64K) * 64 * 1024;
    SET_VGA(VBE_total_memory, totalmem);
    SET_VGA(VBE_win_granularity, 64);
    SET_VGA(VBE_capabilities, VBE_CAPABILITY_8BIT_DAC);

    dprintf(1, "VBE DISPI: lfb_addr=%x, size %d MB\n",
            lfb_addr, totalmem >> 20);

    // Validate modes
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    dispi_write(VBE_DISPI_INDEX_ENABLE, en | VBE_DISPI_GETCAPS);
    u16 max_xres = dispi_read(VBE_DISPI_INDEX_XRES);
    u16 max_bpp = dispi_read(VBE_DISPI_INDEX_BPP);
    dispi_write(VBE_DISPI_INDEX_ENABLE, en);
    struct bochsvga_mode *m = bochsvga_modes;
    for (; m < &bochsvga_modes[ARRAY_SIZE(bochsvga_modes)]; m++) {
        u16 width = GET_GLOBAL(m->info.width);
        u16 height = GET_GLOBAL(m->info.height);
        u8 depth = GET_GLOBAL(m->info.depth);
        u32 mem = (height * DIV_ROUND_UP(width * vga_bpp(&m->info), 8)
                   * stdvga_vram_ratio(&m->info));

        if (width > max_xres || depth > max_bpp || mem > totalmem) {
            dprintf(1, "Removing mode %x\n", GET_GLOBAL(m->mode));
            SET_VGA(m->mode, 0xffff);
        }
    }

    return 0;
}

