#include "sqlite3.h"
#include <time.h>
#include <stddef.h>

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    (void)max; (void)format; (void)tm;
    if (s) s[0] = '\0';
    return 0;
}

static int dummyClose(sqlite3_file *pFile) { (void)pFile; return SQLITE_OK; }
static int dummyRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst) { (void)pFile; (void)zBuf; (void)iAmt; (void)iOfst; return SQLITE_IOERR_READ; }
static int dummyWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst) { (void)pFile; (void)zBuf; (void)iAmt; (void)iOfst; return SQLITE_IOERR_WRITE; }
static int dummyTruncate(sqlite3_file *pFile, sqlite3_int64 size) { (void)pFile; (void)size; return SQLITE_IOERR_TRUNCATE; }
static int dummySync(sqlite3_file *pFile, int flags) { (void)pFile; (void)flags; return SQLITE_OK; }
static int dummyFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize) { (void)pFile; (void)pSize; return SQLITE_IOERR_FSTAT; }
static int dummyLock(sqlite3_file *pFile, int eLock) { (void)pFile; (void)eLock; return SQLITE_OK; }
static int dummyUnlock(sqlite3_file *pFile, int eLock) { (void)pFile; (void)eLock; return SQLITE_OK; }
static int dummyCheckReservedLock(sqlite3_file *pFile, int *pResOut) { (void)pFile; *pResOut = 0; return SQLITE_OK; }
static int dummyFileControl(sqlite3_file *pFile, int op, void *pArg) { (void)pFile; (void)op; (void)pArg; return SQLITE_NOTFOUND; }
static int dummySectorSize(sqlite3_file *pFile) { (void)pFile; return 512; }
static int dummyDeviceCharacteristics(sqlite3_file *pFile) { (void)pFile; return 0; }

static const sqlite3_io_methods dummyIoMethods = {
  1, dummyClose, dummyRead, dummyWrite, dummyTruncate, dummySync,
  dummyFileSize, dummyLock, dummyUnlock, dummyCheckReservedLock,
  dummyFileControl, dummySectorSize, dummyDeviceCharacteristics,
  0, 0, 0, 0, 0, 0
};

static int dummyOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags) {
    (void)pVfs; (void)zName; (void)pFile; (void)flags; (void)pOutFlags;
    return SQLITE_CANTOPEN; 
}
static int dummyDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir) { (void)pVfs; (void)zName; (void)syncDir; return SQLITE_OK; }
static int dummyAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut) { (void)pVfs; (void)zName; (void)flags; *pResOut = 0; return SQLITE_OK; }
static int dummyFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut) {
    (void)pVfs; (void)zName;
    if(nOut>0) zOut[0] = 0;
    return SQLITE_OK;
}
static void *dummyDlOpen(sqlite3_vfs *pVfs, const char *zFilename) { (void)pVfs; (void)zFilename; return 0; }
static void dummyDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg) { (void)pVfs; (void)nByte; (void)zErrMsg; }
static void (*dummyDlSym(sqlite3_vfs *pVfs, void *p, const char *zSymbol))(void) { (void)pVfs; (void)p; (void)zSymbol; return 0; }
static void dummyDlClose(sqlite3_vfs *pVfs, void *pHandle) { (void)pVfs; (void)pHandle; }
static int dummyRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut) {
    (void)pVfs;
    for(int i=0; i<nByte; i++) zOut[i] = (char)i;
    return nByte;
}
static int dummySleep(sqlite3_vfs *pVfs, int microseconds) { (void)pVfs; (void)microseconds; return SQLITE_OK; }

#ifdef SQLITE_OMIT_FLOATING_POINT
static int dummyCurrentTime(sqlite3_vfs *pVfs, sqlite3_int64 *prNow) { (void)pVfs; *prNow = 2440587; return SQLITE_OK; }
#else
static int dummyCurrentTime(sqlite3_vfs *pVfs, double *prNow) { (void)pVfs; *prNow = 2440587.5; return SQLITE_OK; }
#endif

static int dummyGetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf) { (void)pVfs; (void)nBuf; (void)zBuf; return 0; }

static sqlite3_vfs dummyVfs = {
  1, 0, 1024, 0, "zarqaa_dummy", 0,
  dummyOpen, dummyDelete, dummyAccess, dummyFullPathname,
  dummyDlOpen, dummyDlError, dummyDlSym, dummyDlClose,
  dummyRandomness, dummySleep, dummyCurrentTime, dummyGetLastError,
  0, 0, 0, 0
};

int sqlite3_os_init(void) {
    sqlite3_vfs_register(&dummyVfs, 1);
    return SQLITE_OK;
}
int sqlite3_os_end(void) {
    return SQLITE_OK;
}
