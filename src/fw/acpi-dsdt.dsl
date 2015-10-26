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


/****************************************************************
 * PCI Bus definition
 ****************************************************************/

    Scope(\_SB) {
        Device(PCI0) {
            Name(_HID, EisaId("PNP0A03"))
            Name(_ADR, 0x00)
            Name(_UID, 1)
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
 * PIIX3 ISA bridge
 ****************************************************************/

    Scope(\_SB.PCI0) {
        Device(ISA) {
            Name(_ADR, 0x00010000)

            /* PIIX PCI to ISA irq remapping */
            OperationRegion(P40C, PCI_Config, 0x60, 0x04)

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
        Scope(PCI0) {
			Name(_PRT, Package() {
				// Slot 1
				Package() {0x0001FFFF, 0, 0, 0xa},
				Package() {0x0001FFFF, 1, 0, 0xa},
				Package() {0x0001FFFF, 2, 0, 0xb},
				Package() {0x0001FFFF, 3, 0, 0xb},

				// Slot 2
				Package() {0x0002FFFF, 0, 0, 0xa},
				Package() {0x0002FFFF, 1, 0, 0xa},
				Package() {0x0002FFFF, 2, 0, 0xb},
				Package() {0x0002FFFF, 3, 0, 0xb},

				// Slot 3
				Package() {0x0003FFFF, 0, 0, 0xa},
				Package() {0x0003FFFF, 1, 0, 0xa},
				Package() {0x0003FFFF, 2, 0, 0xb},
				Package() {0x0003FFFF, 3, 0, 0xb},

				// Slot 4
				Package() {0x0004FFFF, 0, 0, 0xa},
				Package() {0x0004FFFF, 1, 0, 0xa},
				Package() {0x0004FFFF, 2, 0, 0xb},
				Package() {0x0004FFFF, 3, 0, 0xb},

				// Slot 5
				Package() {0x0005FFFF, 0, 0, 0xa},
				Package() {0x0005FFFF, 1, 0, 0xa},
				Package() {0x0005FFFF, 2, 0, 0xb},
				Package() {0x0005FFFF, 3, 0, 0xb},

				// Slot 6
				Package() {0x0006FFFF, 0, 0, 0xa},
				Package() {0x0006FFFF, 1, 0, 0xa},
				Package() {0x0006FFFF, 2, 0, 0xb},
				Package() {0x0006FFFF, 3, 0, 0xb},

				// Slot 7
				Package() {0x0007FFFF, 0, 0, 0xa},
				Package() {0x0007FFFF, 1, 0, 0xa},
				Package() {0x0007FFFF, 2, 0, 0xb},
				Package() {0x0007FFFF, 3, 0, 0xb},

				// Slot 8
				Package() {0x0008FFFF, 0, 0, 0xa},
				Package() {0x0008FFFF, 1, 0, 0xa},
				Package() {0x0008FFFF, 2, 0, 0xb},
				Package() {0x0008FFFF, 3, 0, 0xb},

				// Slot 9
				Package() {0x0009FFFF, 0, 0, 0xa},
				Package() {0x0009FFFF, 1, 0, 0xa},
				Package() {0x0009FFFF, 2, 0, 0xb},
				Package() {0x0009FFFF, 3, 0, 0xb},

				// Slot 10
				Package() {0x000AFFFF, 0, 0, 0xa},
				Package() {0x000AFFFF, 1, 0, 0xa},
				Package() {0x000AFFFF, 2, 0, 0xb},
				Package() {0x000AFFFF, 3, 0, 0xb},

				// Slot 11
				Package() {0x000BFFFF, 0, 0, 0xa},
				Package() {0x000BFFFF, 1, 0, 0xa},
				Package() {0x000BFFFF, 2, 0, 0xb},
				Package() {0x000BFFFF, 3, 0, 0xb},

				// Slot 12
				Package() {0x000CFFFF, 0, 0, 0xa},
				Package() {0x000CFFFF, 1, 0, 0xa},
				Package() {0x000CFFFF, 2, 0, 0xb},
				Package() {0x000CFFFF, 3, 0, 0xb},

				// Slot 13
				Package() {0x000DFFFF, 0, 0, 0xa},
				Package() {0x000DFFFF, 1, 0, 0xa},
				Package() {0x000DFFFF, 2, 0, 0xb},
				Package() {0x000DFFFF, 3, 0, 0xb},

				// Slot 14
				Package() {0x000EFFFF, 0, 0, 0xa},
				Package() {0x000EFFFF, 1, 0, 0xa},
				Package() {0x000EFFFF, 2, 0, 0xb},
				Package() {0x000EFFFF, 3, 0, 0xb},

				// Slot 15
				Package() {0x000FFFFF, 0, 0, 0xa},
				Package() {0x000FFFFF, 1, 0, 0xa},
				Package() {0x000FFFFF, 2, 0, 0xb},
				Package() {0x000FFFFF, 3, 0, 0xb},

				// Slot 16
				Package() {0x0010FFFF, 0, 0, 0xa},
				Package() {0x0010FFFF, 1, 0, 0xa},
				Package() {0x0010FFFF, 2, 0, 0xb},
				Package() {0x0010FFFF, 3, 0, 0xb},

				// Slot 17
				Package() {0x0011FFFF, 0, 0, 0xa},
				Package() {0x0011FFFF, 1, 0, 0xa},
				Package() {0x0011FFFF, 2, 0, 0xb},
				Package() {0x0011FFFF, 3, 0, 0xb},

				// Slot 18
				Package() {0x0012FFFF, 0, 0, 0xa},
				Package() {0x0012FFFF, 1, 0, 0xa},
				Package() {0x0012FFFF, 2, 0, 0xb},
				Package() {0x0012FFFF, 3, 0, 0xb},

				// Slot 19
				Package() {0x0013FFFF, 0, 0, 0xa},
				Package() {0x0013FFFF, 1, 0, 0xa},
				Package() {0x0013FFFF, 2, 0, 0xb},
				Package() {0x0013FFFF, 3, 0, 0xb},

				// Slot 20
				Package() {0x0014FFFF, 0, 0, 0xa},
				Package() {0x0014FFFF, 1, 0, 0xa},
				Package() {0x0014FFFF, 2, 0, 0xb},
				Package() {0x0014FFFF, 3, 0, 0xb},

				// Slot 21
				Package() {0x0015FFFF, 0, 0, 0xa},
				Package() {0x0015FFFF, 1, 0, 0xa},
				Package() {0x0015FFFF, 2, 0, 0xb},
				Package() {0x0015FFFF, 3, 0, 0xb},

				// Slot 22
				Package() {0x0016FFFF, 0, 0, 0xa},
				Package() {0x0016FFFF, 1, 0, 0xa},
				Package() {0x0016FFFF, 2, 0, 0xb},
				Package() {0x0016FFFF, 3, 0, 0xb},

				// Slot 23
				Package() {0x0017FFFF, 0, 0, 0xa},
				Package() {0x0017FFFF, 1, 0, 0xa},
				Package() {0x0017FFFF, 2, 0, 0xb},
				Package() {0x0017FFFF, 3, 0, 0xb},

				// Slot 24
				Package() {0x0018FFFF, 0, 0, 0xa},
				Package() {0x0018FFFF, 1, 0, 0xa},
				Package() {0x0018FFFF, 2, 0, 0xb},
				Package() {0x0018FFFF, 3, 0, 0xb},

				// Slot 25
				Package() {0x0019FFFF, 0, 0, 0xa},
				Package() {0x0019FFFF, 1, 0, 0xa},
				Package() {0x0019FFFF, 2, 0, 0xb},
				Package() {0x0019FFFF, 3, 0, 0xb},

				// Slot 26
				Package() {0x001AFFFF, 0, 0, 0xa},
				Package() {0x001AFFFF, 1, 0, 0xa},
				Package() {0x001AFFFF, 2, 0, 0xb},
				Package() {0x001AFFFF, 3, 0, 0xb},

				// Slot 27
				Package() {0x001BFFFF, 0, 0, 0xa},
				Package() {0x001BFFFF, 1, 0, 0xa},
				Package() {0x001BFFFF, 2, 0, 0xb},
				Package() {0x001BFFFF, 3, 0, 0xb},

				// Slot 28
				Package() {0x001CFFFF, 0, 0, 0xa},
				Package() {0x001CFFFF, 1, 0, 0xa},
				Package() {0x001CFFFF, 2, 0, 0xb},
				Package() {0x001CFFFF, 3, 0, 0xb},

				// Slot 29
				Package() {0x001DFFFF, 0, 0, 0xa},
				Package() {0x001DFFFF, 1, 0, 0xa},
				Package() {0x001DFFFF, 2, 0, 0xb},
				Package() {0x001DFFFF, 3, 0, 0xb},

				// Slot 30
				Package() {0x001EFFFF, 0, 0, 0xa},
				Package() {0x001EFFFF, 1, 0, 0xa},
				Package() {0x001EFFFF, 2, 0, 0xb},
				Package() {0x001EFFFF, 3, 0, 0xb},

				// Slot 31
				Package() {0x001FFFFF, 0, 0, 0xa},
				Package() {0x001FFFFF, 1, 0, 0xa},
				Package() {0x001FFFFF, 2, 0, 0xb},
				Package() {0x001FFFFF, 3, 0, 0xb}
			})
        }

        Field(PCI0.ISA.P40C, ByteAcc, NoLock, Preserve) {
            PRQ0,   8,
            PRQ1,   8,
            PRQ2,   8,
            PRQ3,   8
        }

        Method(IQST, 1, NotSerialized) {
            // _STA method - get status
            If (And(0x80, Arg0)) {
                Return (0x09)
            }
            Return (0x0B)
        }
        Method(IQCR, 1, Serialized) {
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

        define_link(LNKA, 0, PRQ0)
        define_link(LNKB, 1, PRQ1)
        define_link(LNKC, 2, PRQ2)
        define_link(LNKD, 3, PRQ3)

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
