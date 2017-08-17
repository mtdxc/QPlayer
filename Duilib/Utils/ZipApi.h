#pragma once
///////////////////////////////////////////////////////////////////////////////////////
DECLARE_HANDLE(HZIP);	// An HZIP identifies a zip file that has been opened
typedef DWORD ZRESULT;
typedef struct
{
  int index;                 // index of this file within the zip
  char name[MAX_PATH];       // filename within the zip
  DWORD attr;                // attributes, as in GetFileAttributes.
  FILETIME atime, ctime, mtime;// access, create, modify filetimes
  long comp_size;            // sizes of item, compressed and uncompressed. These
  long unc_size;             // may be -1 if not yet known (e.g. being streamed in)
} ZIPENTRY;
typedef struct
{
  int index;                 // index of this file within the zip
  TCHAR name[MAX_PATH];      // filename within the zip
  DWORD attr;                // attributes, as in GetFileAttributes.
  FILETIME atime, ctime, mtime;// access, create, modify filetimes
  long comp_size;            // sizes of item, compressed and uncompressed. These
  long unc_size;             // may be -1 if not yet known (e.g. being streamed in)
} ZIPENTRYW;

#define OpenZip OpenZipU
#define CloseZip(hz) CloseZipU(hz)
extern HZIP OpenZipU(void *z, unsigned int len, DWORD flags);
extern ZRESULT CloseZipU(HZIP hz);
extern int GetZipFiles(HZIP hz);
extern UILIB_API int UnZipDir(LPCTSTR src, LPCTSTR dst);
extern UILIB_API int ZipDir(LPCTSTR lpszSrcPath, LPCTSTR lpszZipName, LPCTSTR lpszDestPath);

#ifdef _UNICODE
#define ZIPENTRY ZIPENTRYW
#define GetZipItem GetZipItemW
#define FindZipItem FindZipItemW
#else
#define GetZipItem GetZipItemA
#define FindZipItem FindZipItemA
#endif
extern ZRESULT GetZipItemA(HZIP hz, int index, ZIPENTRY *ze);
extern ZRESULT GetZipItemW(HZIP hz, int index, ZIPENTRYW *ze);
extern ZRESULT FindZipItemA(HZIP hz, const TCHAR *name, bool ic, int *index, ZIPENTRY *ze);
extern ZRESULT FindZipItemW(HZIP hz, const TCHAR *name, bool ic, int *index, ZIPENTRYW *ze);
extern ZRESULT UnzipItem(HZIP hz, int index, void *dst, unsigned int len, DWORD flags);
///////////////////////////////////////////////////////////////////////////////////////