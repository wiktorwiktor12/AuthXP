#pragma once
#include "pch.h"

class CPrivilegeEnable
{
private:
	CPrivilegeEnable(void);
	CPrivilegeEnable(const CPrivilegeEnable& copyObject);
	const CPrivilegeEnable& operator =(const CPrivilegeEnable& assignObject);

public:
	CPrivilegeEnable(const TCHAR* pszName);
	CPrivilegeEnable(ULONG ulPrivilegeValue);
	~CPrivilegeEnable(void);

private:
	bool _fSet;
	HANDLE _hToken;
	TOKEN_PRIVILEGES _tokenPrivilegePrevious;
};

class CUserProfile
{
private:
	CUserProfile(void);

public:
	CUserProfile(const TCHAR* pszUsername, const TCHAR* pszDomain);
	~CUserProfile(void);

	operator HKEY(void) const;

private:
	static PSID UsernameToSID(const TCHAR* pszUsername, const TCHAR* pszDomain);
	static bool SIDStringToProfilePath(const TCHAR* pszSIDString, TCHAR* pszProfilePath);

private:
	HKEY _hKeyProfile;
	TCHAR* _pszSID;
	bool _fLoaded;

	static const TCHAR s_szUserHiveFilename[];
};
