/****************************************************************
 * Debugging
 ****************************************************************/

Scope(\) {
    /* Debug Output */
    OperationRegion(DBG, SystemIO, 0xb044, 0x04)
    Field(DBG, DWordAcc, NoLock, Preserve) {
        DBGL,   32,
    }
}
