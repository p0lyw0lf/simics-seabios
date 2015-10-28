/*
 * Bochs/QEMU ACPI DSDT ASL definition
 *
 * Copyright (c) 2006 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

ACPI_EXTRACT_ALL_CODE AmlCode

DefinitionBlock (
    "acpi-dsdt.aml",    // Output Filename
    "DSDT",             // Signature
    0x01,               // DSDT Compliance Revision
    "BXPC",             // OEMID
    "BXDSDT",           // TABLE ID
    0x1                 // OEM Revision
    )
{

#include "acpi-dsdt-dbug.dsl"

    Name (\GPIC, 0x00)
    Method (\_PIC, 1, NotSerialized)
    {
        Store (Arg0, GPIC)
    }

/****************************************************************
 * PCI Bus definition
 ****************************************************************/

    Scope(\_SB) {
        /* motherboard resource */
        Device(MBRS) {
            Name (_HID, EisaId ("PNP0C02"))
            Name (_CRS, ResourceTemplate() {
                /* MCFG */
                Memory32Fixed (ReadWrite, 0xE0000000, 0x10000000)
                /* hostfs */
                Memory32Fixed (ReadWrite, 0xFFE81000, 0x1000)
            })
        }

        Device(PCI0) {
            Name (_HID, EisaId ("PNP0A08"))
            Name (_CID, EisaId ("PNP0A03"))
            Name (_ADR, 0x00)
            Name (_UID, 1)
        }
    }

#include "acpi-dsdt-pci-crs.dsl"
#include "acpi-dsdt-hpet.dsl"


/****************************************************************
 * VGA
 ****************************************************************/

    Scope(\_SB.PCI0) {
        Device(VGA) {
            Name(_ADR, 0x00020000)
            Method(_S1D, 0, NotSerialized) {
                Return (0x00)
            }
            Method(_S2D, 0, NotSerialized) {
                Return (0x00)
            }
            Method(_S3D, 0, NotSerialized) {
                    Return (0x00)
            }
        }
    }


/****************************************************************
 * PIIX4 PM
 ****************************************************************/

    Scope(\_SB.PCI0) {
        Device(PX13) {
            Name(_ADR, 0x00010003)
            OperationRegion(P13C, PCI_Config, 0x00, 0xff)
        }
    }


/****************************************************************
 * ICH10 LPC ISA bridge
 ****************************************************************/

    Scope(\_SB.PCI0) {
        Device(ISA) {
            Name(_ADR, 0x001F0000)

            /* PCI to ISA irq remapping */
            OperationRegion(P40C, PCI_Config, 0x60, 0x0C)
            /* enable bits */
            Field(\_SB.PCI0.PX13.P13C, AnyAcc, NoLock, Preserve) {
                Offset(0x5f),
                , 7,
                LPEN, 1,         // LPT
                Offset(0x67),
                , 3,
                CAEN, 1,         // COM1
                , 3,
                CBEN, 1,         // COM2
            }
            Name(FDEN, 1)
        }
    }

#include "acpi-dsdt-isa.dsl"


/****************************************************************
 * PCI hotplug
 ****************************************************************/

    Scope(\_SB.PCI0) {
        OperationRegion(PCST, SystemIO, 0xae00, 0x08)
        Field(PCST, DWordAcc, NoLock, WriteAsZeros) {
            PCIU, 32,
            PCID, 32,
        }

        OperationRegion(SEJ, SystemIO, 0xae08, 0x04)
        Field(SEJ, DWordAcc, NoLock, WriteAsZeros) {
            B0EJ, 32,
        }

        /* Methods called by bulk generated PCI devices below */

        /* Methods called by hotplug devices */
        Method(PCEJ, 1, NotSerialized) {
            // _EJ0 method - eject callback
            Store(ShiftLeft(1, Arg0), B0EJ)
            Return (0x0)
        }

        /* Hotplug notification method supplied by SSDT */
        External(\_SB.PCI0.PCNT, MethodObj)

        /* PCI hotplug notify method */
        Method(PCNF, 0) {
            // Local0 = iterator
            Store(Zero, Local0)
            While (LLess(Local0, 31)) {
                Increment(Local0)
                If (And(PCIU, ShiftLeft(1, Local0))) {
                    PCNT(Local0, 1)
                }
                If (And(PCID, ShiftLeft(1, Local0))) {
                    PCNT(Local0, 3)
                }
            }
        }
    }


/****************************************************************
 * PCI IRQs
 ****************************************************************/

    Scope(\_SB) {
        Field (\_SB.PCI0.ISA.P40C, ByteAcc, NoLock, Preserve) {
            PRQ0,   8, 
            PRQ1,   8, 
            PRQ2,   8, 
            PRQ3,   8, 
            Offset (0x08), 
            PIRE,   8, 
            PIRF,   8, 
            PIRG,   8, 
            PIRH,   8
        }

        Method(IQST, 1, NotSerialized) {
            // _STA method - get status
            If (And(0x80, Arg0)) {
                Return (0x09)
            }
            Return (0x0B)
        }
        Method(IQCR, 1, NotSerialized) {
            // _CRS method - get current settings
            Name(PRR0, ResourceTemplate() {
                Interrupt(, Level, ActiveHigh, Shared) { 0 }
            })
            CreateDWordField(PRR0, 0x05, PRRI)
            If (LLess(Arg0, 0x80)) {
                Store(Arg0, PRRI)
            }
            Return (PRR0)
        }

#define define_link(link, uid, reg)                             \
        Device(link) {                                          \
            Name(_HID, EISAID("PNP0C0F"))                       \
            Name(_UID, uid)                                     \
            Name(_PRS, ResourceTemplate() {                     \
                Interrupt(, Level, ActiveHigh, Shared) {        \
                    5, 10, 11                                   \
                }                                               \
            })                                                  \
            Method(_STA, 0, NotSerialized) {                    \
                Return (IQST(reg))                              \
            }                                                   \
            Method(_DIS, 0, NotSerialized) {                    \
                Or(reg, 0x80, reg)                              \
            }                                                   \
            Method(_CRS, 0, NotSerialized) {                    \
                Return (IQCR(reg))                              \
            }                                                   \
            Method(_SRS, 1, NotSerialized) {                    \
                CreateDWordField(Arg0, 0x05, PRRI)              \
                Store(PRRI, reg)                                \
            }                                                   \
        }

#define A_ROUTE(v, p0, p1, p2, p3)                      \
        Package () {v##ffff, 0x00, 0x00, p0 },          \
        Package () {v##ffff, 0x01, 0x00, p1 },          \
        Package () {v##ffff, 0x02, 0x00, p2 },          \
        Package () {v##ffff, 0x03, 0x00, p3 },

#define P_ROUTE(v, l0, l1, l2, l3)                      \
        Package () {v##ffff, 0x00, l0, 0x00 },          \
        Package () {v##ffff, 0x01, l1, 0x00 },          \
        Package () {v##ffff, 0x02, l2, 0x00 },          \
        Package () {v##ffff, 0x03, l3, 0x00 },

#define PIRQ_A  16
#define PIRQ_B  17
#define PIRQ_C  18
#define PIRQ_D  19
#define PIRQ_E  20
#define PIRQ_F  21
#define PIRQ_G  22
#define PIRQ_H  23

        /* buffer used to return current setting */
        Name (IRET, ResourceTemplate () {
            IRQ (Level, ActiveLow, Shared, )
                {0}
        })
        CreateWordField (IRET, One, IRQV)

        /* valid PCI IRQ targets */
        Name (PRSx, ResourceTemplate () {
            IRQ (Level, ActiveLow, Shared, )
                {9, 10, 11}
        })

        /* links */
        define_link(LNKA, 0, PRQ0)
        define_link(LNKB, 1, PRQ1)
        define_link(LNKC, 2, PRQ2)
        define_link(LNKD, 3, PRQ3)

        //LINK(LNKE, 5, PIRE, PRSx)
        //LINK(LNKF, 6, PIRF, PRSx)
        //LINK(LNKG, 7, PIRG, PRSx)
        //LINK(LNKH, 8, PIRH, PRSx)

        /* legacy IRQ routing */
        Name (PR00, Package ()
        {
            // DMI, PCI_E port 1-10
            P_ROUTE(0x0000, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0001, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0002, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0003, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0004, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0005, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0006, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0007, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0008, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0009, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x000a, LNKA, LNKB, LNKC, LNKD)

            // unused slots
            P_ROUTE(0x000b, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x000c, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x000d, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x000e, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x000f, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0010, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0011, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0012, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0013, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0014, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0015, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0016, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0017, LNKA, LNKB, LNKC, LNKD)
            P_ROUTE(0x0018, LNKA, LNKB, LNKC, LNKD)

            P_ROUTE(0x001E, LNKA, LNKB, LNKC, LNKD)
            
            // D25: GbE LAN
            P_ROUTE(0x0019, LNKA, LNKB, LNKC, LNKD)

            // D26: EHCI #2, UHCI #6,#5,#4
            P_ROUTE(0x001A, LNKA, LNKB, LNKC, LNKD)

            // D27: Audio
            P_ROUTE(0x001B, LNKA, LNKB, LNKC, LNKD)

            // D28: port1-6
            P_ROUTE(0x001C, LNKA, LNKB, LNKC, LNKD)

            // D29: EHCI, UCHI #6,#3,#2,#1
            P_ROUTE(0x001D, LNKA, LNKB, LNKC, LNKD)

            // D31: SATA1,2 SMBus, Thermal Throttle
            P_ROUTE(0x001F, LNKA, LNKB, LNKC, LNKD)
        })
        Name (AR00, Package ()
        {
            // DMI, PCI_E port 1-10
            A_ROUTE(0x0000, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0001, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0002, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0003, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0004, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0005, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0006, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0007, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0008, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0009, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x000a, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)

            // unused slots
            A_ROUTE(0x000b, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x000c, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x000d, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x000e, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x000f, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0010, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0011, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0012, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0013, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0014, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0015, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0016, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0017, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            A_ROUTE(0x0018, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)

            A_ROUTE(0x001E, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
            
            // D25: GbE LAN
            A_ROUTE(0x0019, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)

            // D26: EHCI #2, UHCI #6,#5,#4
            A_ROUTE(0x001A, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)

            // D27: Audio
            A_ROUTE(0x001B, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)

            // D28: port1-6
            A_ROUTE(0x001C, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)

            // D29: EHCI, UCHI #6,#3,#2,#1
            A_ROUTE(0x001D, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)

            // D31: SATA1,2 SMBus, Thermal Throttle
            A_ROUTE(0x001F, PIRQ_A, PIRQ_B, PIRQ_C, PIRQ_D)
        })
        Scope(\_SB.PCI0) {
            Method (_PRT, 0, NotSerialized) {
                If (GPIC) { 
                    Return (AR00)
                }
                Return (PR00)
            }
        }
    }


/****************************************************************
 * General purpose events
 ****************************************************************/

    Scope(\_GPE) {
        Name(_HID, "ACPI0006")

        Method(_L00) {
        }
        Method(_E01) {
            // PCI hotplug event
            \_SB.PCI0.PCNF()
        }
        Method(_E02) {
        }
        Method(_L03) {
        }
        Method(_L04) {
        }
        Method(_L05) {
        }
        Method(_L06) {
        }
        Method(_L07) {
        }
        Method(_L08) {
        }
        Method(_L09) {
        }
        Method(_L0A) {
        }
        Method(_L0B) {
        }
        Method(_L0C) {
        }
        Method(_L0D) {
        }
        Method(_L0E) {
        }
        Method(_L0F) {
        }
    }
}
