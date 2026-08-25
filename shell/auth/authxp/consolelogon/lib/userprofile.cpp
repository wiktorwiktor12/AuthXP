#include "pch.h"
#include "userprofile.h"

#include <windows.h>

#include <sddl.h>
#include <lmaccess.h>
#include <lmapibuf.h>
#include <dsgetdc.h>

STDAPI_(BOOL) OpenEffectiveToken(IN DWORD dwDesiredAccess, OUT HANDLE* phToken)
{
	BOOL fResult;

	if (IsBadWritePtr(phToken, sizeof(*phToken)))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		fResult = FALSE;
	}
	else
	{
		*phToken = NULL;
		fResult = OpenThreadToken(GetCurrentThread(), dwDesiredAccess, FALSE, phToken);
		if (fResult == FALSE)
		{
			fResult = OpenProcessToken(GetCurrentProcess(), dwDesiredAccess, phToken);
		}
	}
	return (fResult);
}


CPrivilegeEnable::CPrivilegeEnable(const TCHAR* pszName) :
	_fSet(false),
	_hToken(NULL)

{
	if (OpenEffectiveToken(TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &_hToken) != FALSE)
	{
		TOKEN_PRIVILEGES newPrivilege;

		if (LookupPrivilegeValue(NULL, pszName, &newPrivilege.Privileges[0].Luid) != FALSE)
		{
			DWORD dwReturnTokenPrivilegesSize;

			newPrivilege.PrivilegeCount = 1;
			newPrivilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			_fSet = (AdjustTokenPrivileges(_hToken,
			                               FALSE,
			                               &newPrivilege,
			                               sizeof(newPrivilege),
			                               &_tokenPrivilegePrevious,
			                               &dwReturnTokenPrivilegesSize) != FALSE);
		}
	}
}

CPrivilegeEnable::~CPrivilegeEnable(void)

{
	if (_fSet)
	{
		AdjustTokenPrivileges(_hToken,
		                            FALSE,
		                            &_tokenPrivilegePrevious,
		                            0,
		                            NULL,
		                            NULL);
	}
	if (_hToken != NULL)
	{
		CloseHandle(_hToken);
		_hToken = NULL;
	}
}

const TCHAR CUserProfile::s_szUserHiveFilename[] = TEXT("ntuser.dat");

CUserProfile::CUserProfile(const TCHAR* pszUsername, const TCHAR* pszDomain) :
	_hKeyProfile(NULL),
	_pszSID(NULL),
	_fLoaded(false)

{
	if (!IsBadStringPtr(pszUsername, static_cast<UINT_PTR>(-1)))
	{
		PSID pSID = UsernameToSID(pszUsername, pszDomain);
		if (pSID != NULL)
		{
			if (ConvertSidToStringSid(pSID, &_pszSID) != FALSE)
			{
				if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_USERS,
				                                  _pszSID,
				                                  0,
				                                  KEY_ALL_ACCESS,
				                                  &_hKeyProfile))
				{
					TCHAR szProfilePath[MAX_PATH];

					if (SIDStringToProfilePath(_pszSID, szProfilePath))
					{
						if ((lstrlen(szProfilePath) + sizeof('\\') + ARRAYSIZE(s_szUserHiveFilename)) < ARRAYSIZE(
							szProfilePath))
						{
							CPrivilegeEnable privilege(SE_RESTORE_NAME);

							lstrcat(szProfilePath, TEXT("\\"));
							lstrcat(szProfilePath, s_szUserHiveFilename);
							if (ERROR_SUCCESS == RegLoadKey(HKEY_USERS, _pszSID, szProfilePath))
							{
								_fLoaded = true;
								RegOpenKeyEx(HKEY_USERS,
								                  _pszSID,
								                  0,
								                  KEY_ALL_ACCESS,
								                  &_hKeyProfile);
							}
						}
					}
				}
			}
			LocalFree(pSID);
		}
	}
}

CUserProfile::~CUserProfile(void)
{
	if (_hKeyProfile != NULL)
	{
		RegCloseKey(_hKeyProfile);
	}
	if (_fLoaded)
	{
		CPrivilegeEnable privilege(SE_RESTORE_NAME);

		RegUnLoadKey(HKEY_USERS, _pszSID);
		_fLoaded = false;
	}
	if (_pszSID != NULL)
	{
		LocalFree(_pszSID);
		_pszSID = NULL;
	}
}

CUserProfile::operator HKEY(void) const
{
	return (_hKeyProfile);
}

PSID CUserProfile::UsernameToSID(const TCHAR* pszUsername, const TCHAR* pszDomain)
{
	DWORD dwSIDSize, dwComputerNameSize, dwReferencedDomainSize;
	SID_NAME_USE eSIDUse;
	WCHAR* pszDomainControllerName;
	DOMAIN_CONTROLLER_INFO* pDCI;
	TCHAR szComputerName[CNLEN + sizeof('\0')];

	PSID pSIDResult = NULL;
	dwComputerNameSize = ARRAYSIZE(szComputerName);
	if (GetComputerName(szComputerName, &dwComputerNameSize) == FALSE)
	{
		szComputerName[0] = TEXT('\0');
	}
	if ((pszDomain != NULL) &&
		(lstrcmpi(szComputerName, pszDomain) != 0) &&
		(ERROR_SUCCESS == DsGetDcName(NULL,
		                              pszDomain,
		                              NULL,
		                              NULL,
		                              0,
		                              &pDCI)))
	{
		pszDomainControllerName = pDCI->DomainControllerName;
	}
	else
	{
		pDCI = NULL;
		pszDomainControllerName = NULL;
	}
	dwSIDSize = dwReferencedDomainSize = 0;
	LookupAccountName(pszDomainControllerName,
	                        pszUsername,
	                        NULL,
	                        &dwSIDSize,
	                        NULL,
	                        &dwReferencedDomainSize,
	                        &eSIDUse);
	PSID pSID = LocalAlloc(LMEM_FIXED, dwSIDSize);
	if (pSID != NULL)
	{
		TCHAR* pszReferencedDomain = static_cast<TCHAR*>(LocalAlloc(LMEM_FIXED, dwReferencedDomainSize * sizeof(TCHAR)));
		if (pszReferencedDomain != NULL)
		{
			if (LookupAccountName(pszDomainControllerName,
			                      pszUsername,
			                      pSID,
			                      &dwSIDSize,
			                      pszReferencedDomain,
			                      &dwReferencedDomainSize,
			                      &eSIDUse) != FALSE)
			{
				if (SidTypeUser == eSIDUse)
				{
					pSIDResult = pSID;
					pSID = NULL;
				}
			}
			(HLOCAL)LocalFree(pszReferencedDomain);
		}
		if (pSID != NULL)
		{
			(HLOCAL)LocalFree(pSID);
		}
	}
	if (pDCI != NULL)
	{
		(NET_API_STATUS)NetApiBufferFree(pDCI);
	}
	return (pSIDResult);
}

bool CUserProfile::SIDStringToProfilePath(const TCHAR* pszSIDString, TCHAR* pszProfilePath)
{
	bool fResult = false;
	if (!IsBadStringPtr(pszSIDString, static_cast<UINT_PTR>(-1)) && !IsBadWritePtr(
		pszProfilePath, MAX_PATH * sizeof(TCHAR)))
	{
		HKEY hKeyProfileList;

		pszProfilePath[0] = TEXT('\0');
		if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		                                  TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList"),
		                                  0,
		                                  KEY_QUERY_VALUE,
		                                  &hKeyProfileList))
		{
			HKEY hKeySID;

			if (ERROR_SUCCESS == RegOpenKeyEx(hKeyProfileList,
			                                  pszSIDString,
			                                  0,
			                                  KEY_QUERY_VALUE,
			                                  &hKeySID))
			{
				DWORD dwType, dwProfilePathSize;
				TCHAR szProfilePath[MAX_PATH];

				dwProfilePathSize = ARRAYSIZE(szProfilePath);
				if (ERROR_SUCCESS == RegQueryValueEx(hKeySID,
				                                     TEXT("ProfileImagePath"),
				                                     NULL,
				                                     &dwType,
				                                     reinterpret_cast<LPBYTE>(szProfilePath),
				                                     &dwProfilePathSize))
				{
					if (REG_EXPAND_SZ == dwType)
					{
						fResult = true;
						if (ExpandEnvironmentStrings(szProfilePath, pszProfilePath, MAX_PATH) == 0)
						{
							dwType = REG_SZ;
						}
					}
					if (REG_SZ == dwType)
					{
						fResult = true;
						(TCHAR*)lstrcpy(pszProfilePath, szProfilePath);
					}
				}
				RegCloseKey(hKeySID);
			}
			RegCloseKey(hKeyProfileList);
		}
	}
	return (fResult);
}
