// asl test code
// 2017-2-3
// define a Operation Region of IO port 0x510 16 bits

ACPI_EXTRACT_ALL_CODE nfit_ssdt_aml

DefinitionBlock ("nfit-ssdt.aml", "SSDT", 0x01, "SIMICS", "NFITSSDT", 0x1)
{

/****************************************************************
 * PC-CONF IO space
 ****************************************************************/
Scope(\_SB) {
	Device(NVDR) {
        Name(_HID, "ACPI0012") // _HID: Hardware ID
        OperationRegion(PC__, SystemIO, 0x510, 0x2)
        Field(PC__, WordAcc, NoLock, Preserve)
        {   
            CONF, 16
        }
        
        Name(BUFF, Buffer(4096) {0, 0 } )
        Name(SIZE, 0)
        Name(INDX, 0)
        
        /* Word write/read SIZE bytes from BUFF to PC__CONF IO port 
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
                Store(DeRefOf(Index(BUFF, INDX)), Local1)
                Add(INDX, 1, INDX)
                Store(DeRefOf(Index(BUFF, INDX)), Local2)
                Add(INDX, 1, INDX)
                ShiftLeft(Local2, 8, Local2)
                Or(Local1, Local2, Local1)
                Store(Local1, CONF)                                
            }            
        }
        
        Method (READ, 2, Serialized)
        {
            BUFF = Arg0
            SIZE = Arg1
            INDX = Zero
            Local3 = Zero
            Local4 = Zero
            While(LGreater(SIZE, INDX))
            {
                Store(CONF, Local3)
                Store(Local3, Index(BUFF, INDX))
                Add(INDX, 1, INDX)
                
                ShiftRight(Local3, 8, Local4)
                Store(Local4, Index(BUFF, INDX))
                Add(INDX, 1, INDX)                
            }             
        }
        
        /* _DSM: Device-Specific Method */
        Method (_DSM, 4, NotSerialized)
        {
            Local0 = ToUUID ("2f10e7a4-9e91-11e4-89d3-123b93f75cba")
            READ(Local0, 0x10)
            WRIT(Local0, SizeOf(Local0))          
            Return( Zero )
        }
        
        Device (NV00)
        {
            Name (_ADR, One)
            Method (_DSM, 4, NotSerialized)
            {
                Local0 = ToUUID ("4309ac30-0d11-11e4-9191-0800200c9a66")
                READ(Local0, 0x10)
                WRIT(Local0, SizeOf(Local0)) 
                Return( One )
            }
        }
    }
}
}
