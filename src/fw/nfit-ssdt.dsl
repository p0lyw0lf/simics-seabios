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
        
        Name(BUFF, Buffer(0x02) { Zero, Zero } )
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
                CreateWordField(BUFF, INDX, DATA)
                Store(DATA, CONF)
                Add(INDX, 2, INDX)                
            }            
        }
        
        Method (READ, 2, Serialized)
        {
            BUFF = Arg0
            SIZe = Arg1
            INDX = Zero
            
            While(LGreater(SIZE, INDX))
            {
                CreateWordField(BUFF, INDX, DATA)
                Store(CONF, DATA)
                Add(INDX, 2, INDX)                
            }      
        }
        
        /* _DSM: Device-Specific Method */
        Method (_DSM, 4, NotSerialized)
        {
            Local0 = ToUUID ("2f10e7a4-9e91-11e4-89d3-123b93f75cba")
            WRIT(Local0, SizeOf(Local0))
            Local0 = ToUUID ("4309ac30-0d11-11e4-9191-0800200c9a66")
            WRIT(Local0, SizeOf(Local0))
            Return( Zero )
        }
    }
}
}
