// Ini.cpp: implementation of the CIni class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Ini.h"

#define EMPTYSTR ""
const char* AppPath(char* tcPath, const char* fName)
{
	if(!tcPath) 
		return tcPath;
	tcPath[0] = 0;
	int nLen = GetModuleFileNameA( NULL, tcPath, MAX_PATH );
	while(nLen)
	{
		if(tcPath[nLen] == '\\')
		{
			if(fName && fName[0])
				strcpy(tcPath+nLen+1, fName);
			else
				tcPath[nLen]=0;
			break;
		}
		nLen--;
	}
	if(!nLen && fName)
		strcpy(tcPath, fName);
	return tcPath;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIni::CIni(const char* path)
{
	SetPath(path);
}

CIni::~CIni()
{

}

void CIni::SetPath(const char* path)
{
	cfgpath[0] = 0;
	if(path) strcpy(cfgpath, path);
	// 如果路径中没反斜杠
	if(!strchr(cfgpath,'\\')&&!strchr(cfgpath,_T('/')))
		AppPath(cfgpath, path);
}

/**
@brief Int配置项获取

@param sec Sec名
@param key 键名
@param def 缺省值
@return Int值
*/
int CIni::GetInt(const char* sec, const char* key, int def)
{
	char ret[MAX_PATH] = {0};
  GetPrivateProfileStringA(sec, key, EMPTYSTR, ret, sizeof ret, cfgpath);
	if(!ret[0])
	{//返回为空字符串
		sprintf(ret, "%d", def);
		WritePrivateProfileStringA(sec, key, ret, cfgpath);
		return def;
	}
	return atoi(ret);
}

/**
@brief Int配置项获取

@param sec Sec名
@param key 键名
@param value 值
@return Int值
*/
bool CIni::SetInt(const char* sec,const char* key,int value)
{
	//std::string szValue; szValue.Format(_T("%d"), value);
	char szValue[20]={0};
	sprintf(szValue, "%d", value);
	return WritePrivateProfileStringA(sec, key, szValue, cfgpath);
}

/**
@brief Str配置项获取

@param sec Sec名
@param key 键名
@param value 缺省值/返回值
@return 获取字节数
*/
int CIni::GetStr(const char* sec, const char* key, char* value)
{
	char ret[MAX_PATH] = {0};
  int i = GetPrivateProfileStringA(sec, key, EMPTYSTR, ret, sizeof ret, cfgpath);
    if (i){
        if (value)
            strcpy(value, ret);
        return i;
    }
    else if (value){
        WritePrivateProfileStringA(sec, key, value, cfgpath);
        //i = _tcslen(value);
    }
	return i;
}

/**
@brief Str配置项设置

@param sec Sec名
@param key 键名
@param value 缺省值
@return Str值
*/
bool CIni::SetStr(const char* sec, const char* key, const char* value)
{
	return WritePrivateProfileStringA(sec, key, value, cfgpath);
}

static CIni gIni("config.ini");
int GetIniInt(const char* sec, const char* key, int def)
{
	return gIni.GetInt(sec,key, def);
}

std::string GetIniStr(const char* sec, const char* key, const char* def)
{
    char szValue[MAX_PATH] = { 0 };
    if (def&&def[0])
        strcpy(szValue, def);
    gIni.GetStr(sec, key, szValue);
    return szValue;
}

bool SetIniInt(const char* sec, const char* key, int value)
{
	return gIni.SetInt(sec, key, value);
}

bool SetIniStr(const char* sec, const char* key, const char* value)
{
	return gIni.SetStr(sec, key, value);
}

