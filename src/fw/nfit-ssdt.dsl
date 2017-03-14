// asl test code
// 2017-2-3
// define a Operation Region of IO port 0x510 16 bits

ACPI_EXTRACT_ALL_CODE nfit_ssdt_aml

DefinitionBlock ("nfit-ssdt.aml", "SSDT", 0x01, "SIMICS", "NFITSSDT", 0x1)
{

/****************************************************************
 * CFG1_CTLW/ CFG2_DATA IO space
 ****************************************************************/
Scope(\_SB) {
    ACPI_EXTRACT_DEVICE_START nfit_ssdt_nvdr_start
    ACPI_EXTRACT_DEVICE_END nfit_ssdt_nvdr_end
    Device(NVDR) {
        Name(_HID, "ACPI0012") // _HID: Hardware ID
        OperationRegion(CFG1, SystemIO, 0x510, 0x2)
        Field(CFG1, WordAcc, NoLock, Preserve)
        {   
            CTLW, 16
        }
        
        OperationRegion(CFG2, SystemIO, 0x511, 0x1)
        Field(CFG2, ByteAcc, NoLock, Preserve)
        {
            DATA, 8
        }
        
        Name(BUFF, Buffer(4096) {0, 0 } )
        Name(SIZE, 0)
        Name(INDX, 0)
        
        /* Word write/read SIZE bytes from BUFF to CFG2.DATA IO port 
         * WRIT(BUFF, SIZE)
         * READ(BUFF, SIZE)
         */
        Method(WRIT, 2, Serialized)
        {
            BUFF = Arg0
            SIZE = Arg1
            INDX = Zero
            
            While(LGreater(SIZE, INDX))
            {                
                Local1 = DeRefOf(Index(BUFF, INDX))
                Increment(INDX)
                DATA = Local1
            }            
        }
        
        Method (READ, 2, Serialized)
        {
            BUFF = Arg0
            SIZE = Arg1
            INDX = Zero
            
            While(LGreater(SIZE, INDX))
            {
                Index(BUFF, INDX) = DATA
                Increment(INDX)
            }          
        }
        
        /* RECV, 1024 bytes receive buffer for result. */
        Name (RECV, Buffer(0x400) {})
        
        /* CDSM: common procedure to write parameters to DATA,
           then read result from DATA.
           Convert result to Arg5 and return.
         */
        Method (CDSM, 5, NotSerialized)
        {
            If (Arg0 == 0) 
            {
                Local0 = ToUUID ("2f10e7a4-9e91-11e4-89d3-123b93f75cba")
            }
            Else
            {
                Local0 = ToUUID ("4309ac30-0d11-11e4-9191-0800200c9a66")
            }
            
            If (Arg1 != Local0)
            {            
                If (Arg1 == Zero)
                {
                    Return (Buffer (One) { 0x0 })           
                }
                Else
                {
                    Return (Buffer (One) { 0x1 })
                }
            }
            
            WRIT(Arg0, 0x4)
            WRIT(Local0, SizeOf(Local0))
            WRIT(Arg2, 0x4)
            WRIT(Arg3, 0x4)
            
            If (((ObjectType (Arg4) == 0x4) & (SizeOf (Arg4) == One)))
            {
                Local1 = Arg4 [Zero]
                Local2 = DerefOf (Local1)
                WRIT(Local2, SizeOf(Local2))
            }
            
            Local3 = 0
            READ(Local3, 0x4)            
            READ(RECV, Local3)
            Local4 = Local3 << 0x3
            CreateField(RECV, Zero, Local4, OUTB)
            Concatenate (Buffer (Zero) {}, OUTB, Local5)
            Return (Local5)
        }
        
        /* _DSM: Device-Specific Method */
        Method (_DSM, 4, NotSerialized)
        {   
            /* write 0 for root device */
            Return(CDSM(0, Arg0, Arg1, Arg2, Arg3))
        }            
        
        ACPI_EXTRACT_DEVICE_START nfit_ssdt_nv00_start
        ACPI_EXTRACT_DEVICE_END nfit_ssdt_nv00_end
        ACPI_EXTRACT_DEVICE_STRING nfit_ssdt_nv00_name
        
        Device (NV00)
        {
            Name (_ADR, 0x2)
            Method (_DSM, 4, NotSerialized)
            {
                /* write _ADR for NVDIMM device */
                Return(CDSM(_ADR, Arg0, Arg1, Arg2, Arg3))
            }
        }        
    }
}
}
