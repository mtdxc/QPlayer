// Ini.h: interface for the CIni class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INI_H__82D19E50_EEFD_4348_A098_5D16E166B8C5__INCLUDED_)
#define AFX_INI_H__82D19E50_EEFD_4348_A098_5D16E166B8C5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <string>

const char* AppPath(char* path, const char* fName);
class CIni  
{
public:
	CIni(const char* path);
	virtual ~CIni();

	void SetPath(const char* path);
	const char* GetPath(){return cfgpath;}

	int GetInt(const char* sec, const char* key, int def);
	int GetStr(const char* sec, const char* key, char* value);

	bool SetInt(const char* sec, const char* key, int value);
	bool SetStr(const char* sec, const char* key, const char* value);
protected:
	char cfgpath[260];
};
int GetIniInt(const char* sec, const char* key, int def);
std::string GetIniStr(const char* sec, const char* key, const char* def);
bool SetIniInt(const char* sec, const char* key, int value);
bool SetIniStr(const char* sec, const char* key, const char* value);

#endif // !defined(AFX_INI_H__82D19E50_EEFD_4348_A098_5D16E166B8C5__INCLUDED_)
