#pragma once

#include "pch.h"

#include <windows.foundation.collections.h>
#include <windows.storage.streams.h>
#include "version.h"

typedef enum
{
	TRIBIT_UNDEFINED = 0,
	TRIBIT_TRUE,
	TRIBIT_FALSE,
} TRIBIT;

typedef struct {
	LANGID LangID;
	BOOL   bInstalled;
} MUIINSTALLLANG, * LPMUIINSTALLLANG;

static LANGID Mirror_GetUserDefaultUILanguage(void)
{
	return GetUserDefaultUILanguage();
}

static BOOL ConvertHexStringToIntA(CHAR* pszHexNum, int* piNum)
{
	int   n = 0L;
	CHAR* psz = pszHexNum;


	for (n = 0; ; psz = CharNextA(psz))
	{
		if ((*psz >= '0') && (*psz <= '9'))
			n = 0x10 * n + *psz - '0';
		else
		{
			CHAR ch = *psz;
			int n2;

			if (ch >= 'a')
				ch -= 'a' - 'A';

			n2 = ch - 'A' + 0xA;
			if (n2 >= 0xA && n2 <= 0xF)
				n = 0x10 * n + n2;
			else
				break;
		}
	}

	/*
	 * Update results
	 */
	*piNum = n;

	return (psz != pszHexNum);
}

static BOOL ConvertHexStringToIntW(WCHAR* pszHexNum, int* piNum)
{
	int   n = 0L;
	WCHAR* psz = pszHexNum;


	for (n = 0; ; psz = CharNextW(psz))
	{
		if ((*psz >= '0') && (*psz <= '9'))
			n = 0x10 * n + *psz - '0';
		else
		{
			WCHAR ch = *psz;
			int n2;

			if (ch >= 'a')
				ch -= 'a' - 'A';

			n2 = ch - 'A' + 0xA;
			if (n2 >= 0xA && n2 <= 0xF)
				n = 0x10 * n + n2;
			else
				break;
		}
	}

	/*
	 * Update results
	 */
	*piNum = n;

	return (psz != pszHexNum);
}

static BOOL CALLBACK Mirror_EnumUILanguagesProc(LPWSTR lpUILanguageString, LONG_PTR lParam)
{
	int langID = 0;

	ConvertHexStringToIntW(lpUILanguageString, &langID);

	if ((LANGID)langID == ((LPMUIINSTALLLANG)lParam)->LangID)
	{
		((LPMUIINSTALLLANG)lParam)->bInstalled = TRUE;
		return FALSE;
	}
	return TRUE;
}

static BOOL Mirror_IsUILanguageInstalled(LANGID langId)
{

	MUIINSTALLLANG MUILangInstalled = { 0 };
	EnumUILanguagesW(Mirror_EnumUILanguagesProc, 0, (LONG_PTR)&MUILangInstalled);

	return MUILangInstalled.bInstalled;
}

static BOOL IsBiDiLocalizedSystemEx(LANGID* pLangID)
{
    int           iLCID = 0L;
    static TRIBIT s_tbBiDi = TRIBIT_UNDEFINED;
    static LANGID s_langID = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

    if (s_tbBiDi == TRIBIT_UNDEFINED)
    {
        BOOL bRet = FALSE;
        if (IsOS(OS_WIN2000ORGREATER))
        {
            /*
             * Need to use NT5 detection method (Multiligual UI ID)
             */
            s_langID = Mirror_GetUserDefaultUILanguage();

            if (s_langID)
            {
                WCHAR wchLCIDFontSignature[16];
                iLCID = MAKELCID(s_langID, SORT_DEFAULT);

                /*
                 * Let's verify this is a RTL (BiDi) locale. Since reg value is a hex string, let's
                 * convert to decimal value and call GetLocaleInfo afterwards.
                 * LOCALE_FONTSIGNATURE always gives back 16 WCHARs.
                 */

                if (GetLocaleInfoW(iLCID,
                    LOCALE_FONTSIGNATURE,
                    (WCHAR*)&wchLCIDFontSignature[0],
                    (sizeof(wchLCIDFontSignature) / sizeof(WCHAR))))
                {

                    /* Let's verify the bits we have a BiDi UI locale */
                    if ((wchLCIDFontSignature[7] & (WCHAR)0x0800) && Mirror_IsUILanguageInstalled(s_langID))
                    {
                        bRet = TRUE;
                    }
                }
            }
        }
        else {

            /*
             * Check if BiDi-Memphis is running with Lozalized Resources (
             * i.e. Arabic/Hebrew systems) -It should be enabled ofcourse-.
             */
            if ((IsOS(OS_WIN98ORGREATER)) && (GetSystemMetrics(SM_MIDEASTENABLED)))
            {
                HKEY          hKey;

                if (RegOpenKeyExA(HKEY_CURRENT_USER,
                    "Control Panel\\Desktop\\ResourceLocale",
                    0,
                    KEY_READ, &hKey) == ERROR_SUCCESS)
                {
                    CHAR szResourceLocale[12];
                    DWORD dwSize = sizeof(szResourceLocale);
                    RegQueryValueExA(hKey, "", 0, NULL, (LPBYTE)szResourceLocale, &dwSize);
                    szResourceLocale[(sizeof(szResourceLocale) / sizeof(CHAR)) - 1] = 0;

                    RegCloseKey(hKey);

                    if (ConvertHexStringToIntA(szResourceLocale, &iLCID))
                    {
                        iLCID = PRIMARYLANGID(LANGIDFROMLCID(iLCID));
                        if ((LANG_ARABIC == iLCID) || (LANG_HEBREW == iLCID))
                        {
                            bRet = TRUE;
                            s_langID = LANGIDFROMLCID(iLCID);
                        }
                    }
                }
            }
        }

        //COMPILETIME_ASSERT(sizeof(s_tbBiDi) == sizeof(long));
        //  close multiproc race on startup
        InterlockedExchange((long*)&s_tbBiDi, bRet ? TRIBIT_TRUE : TRIBIT_FALSE);
    }

    if (s_tbBiDi == TRIBIT_TRUE && pLangID)
    {
        *pLangID = s_langID;
    }

    return (s_tbBiDi == TRIBIT_TRUE);
}

static BOOL IsBiDiLocalizedSystem(void)
{
    return IsBiDiLocalizedSystemEx(NULL);
}

#define IS_BIDI_LOCALIZED_SYSTEM()       IsBiDiLocalizedSystem()

namespace Windows::Internal
{
}

namespace WF = ABI::Windows::Foundation;
namespace WFC = ABI::Windows::Foundation::Collections;
namespace WI = Windows::Internal;

namespace Windows::Internal::UI::Logon
{
	namespace CredProvData
	{
		enum UserAccountKind
		{
			UserAccountKind_Unknown = 0,
			UserAccountKind_Local = 1,
			UserAccountKind_Domain = 2,
			UserAccountKind_Connected = 3,
			UserAccountKind_CloudDomain = 4,
		};

		//guid from 21h2
		MIDL_INTERFACE("683ef64c-d0ca-4a52-a299-1a22c22b26d3")
		ITelemetryDataProvider : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE GetUserAccountKind(HSTRING, UserAccountKind*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_CurrentLogonUIRequestReason(DWORD*) PURE;
		};

		enum DisplayStateFlags
		{
			DisplayStateFlags_None = 0,
			DisplayStateFlags_DisplayOn = 1,
			DisplayStateFlags_DisplayDim = 2,
			DisplayStateFlags_LockScreenVisible = 4,
			DisplayStateFlags_LockScreenManipulated = 8,
			DisplayStateFlags_AppOnLockScreen = 0x10,
			DisplayStateFlags_UserSwitching = 0x20,
			DisplayStateFlags_SessionLocked = 0x40,
			DisplayStateFlags_AcceptingCreds = 0x80,
			DisplayStateFlags_LidClosed = 0x100,
			DisplayStateFlags_DelayLocked = 0x200,
			DisplayStateFlags_LoggedOut = 0x400,
			DisplayStateFlags_ResumeFromHibernateOrScreenSaver = 0x800,
		};

		DEFINE_ENUM_FLAG_OPERATORS(DisplayStateFlags);

		struct DECLSPEC_NOVTABLE IDisplayStateProvider : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_DisplayState(DisplayStateFlags*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_DisplayStateChanged(WF::ITypedEventHandler<IDisplayStateProvider*, DisplayStateFlags>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_DisplayStateChanged(EventRegistrationToken) PURE;
		};

		enum BioFeedbackState
		{
			BioFeedbackState_None = 0,
			BioFeedbackState_Error = 1,
			BioFeedbackState_PowerOn = 2,
			BioFeedbackState_Searching = 3,
			BioFeedbackState_Authenticated = 4,
			BioFeedbackState_TooFar = 5,
			BioFeedbackState_TooClose = 6,
			BioFeedbackState_MessageOnly = 7,
		};

		enum SerializationResponse
		{
			SerializationResponse_NoCredentialIncomplete = 0,
			SerializationResponse_NoCredentialComplete = 1,
			SerializationResponse_ReturnCredentialComplete = 2,
			SerializationResponse_ReturnNoCredentialComplete = 3,
		};

		enum CredentialProviderStatusIcon
		{
			CredentialProviderStatusIcon_None = 0,
			CredentialProviderStatusIcon_Error = 1,
			CredentialProviderStatusIcon_Warning = 2,
			CredentialProviderStatusIcon_Success = 3,
		};

		struct DECLSPEC_NOVTABLE ICredentialSerialization : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_CredentialId(UINT*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SerializationResponse(SerializationResponse*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SerializationIcon(CredentialProviderStatusIcon*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_StatusMessage(HSTRING*) PURE;
		};

		MIDL_INTERFACE("ff3e0ba7-f55b-40db-91d9-58626f36aead")
		IComboBoxField : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Items(WFC::IObservableVector<HSTRING>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedIndex(int*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedIndex(int) PURE;
		};

		//TODO: verify iid
		MIDL_INTERFACE("5cad4c8f-6f35-4a41-ba52-bdf26571c77e")
		ICheckBoxField : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_BoxLabel(HSTRING* boxLabel) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Checked(BOOLEAN* isChecked) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_Checked(BOOLEAN isChecked) PURE;
		};

		//todo:verify, + this is updated to 21h2
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
		MIDL_INTERFACE("1a522c5f-5032-4c4e-8563-20c524dde117")
#else
		MIDL_INTERFACE("20486918-5147-4725-a0cb-12b952a5f0c3")
#endif
		ICommandLinkField : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Content(HSTRING* outContent) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE get_IsStyledAsButton(BOOLEAN*) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE Invoke() PURE;
		};

		//TODO: verify + UPDATED to 21h2
		MIDL_INTERFACE("48a0b4dd-74a6-4b03-9850-672ff62135fd")
		ICredentialEditField : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Content(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_Content(HSTRING) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsPasswordRevealEnabled(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsPasswordField(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_MaxPasswordLength(int*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsNumbersOnly(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsEmailAddress(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsEnglishOnly(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsLogonUserName(BOOLEAN*) PURE;
		};

		enum CredProvScenario
		{
			CredProvScenario_Logon = 0,
			CredProvScenario_Unlock = 1,
			CredProvScenario_ChangePassword = 2,
			CredProvScenario_CredUI = 3
		};

		enum SelectionMode
		{
			SelectionMode_UserOnly = 0,
			SelectionMode_PLAP = 1,
			SelectionMode_UserAndV1Aggregate = 2,
			SelectionMode_FlatCredentials = 3,
			SelectionMode_SingleSelectedProvider = 4
		};

		enum TileStreamType
		{
			TileStreamType_Image = 0,
			TileStreamType_Video = 1
		};

		enum UserTileImageSize
		{
			UserTileImageSize_Large = 0,
			UserTileImageSize_Small = 1,
			UserTileImageSize_ExtraSmall = 2
		};

		//TODO: add support for older versions of this iid
		MIDL_INTERFACE("33034271-7459-4b69-a4e7-e3112ad88933")
		DECLSPEC_NOVTABLE IUserTileImage : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_TileStream(ABI::Windows::Storage::Streams::IRandomAccessStream**) PURE;

			virtual HRESULT STDMETHODCALLTYPE put_TileStream(ABI::Windows::Storage::Streams::IRandomAccessStream*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_TileStreamInitialized(WF::ITypedEventHandler<IUserTileImage*, ABI::Windows::Storage::Streams::IRandomAccessStream*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_TileStreamInitialized(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_TileStreamState(/*Windows::Internal::UI::Logon::CredProvData::TileStreamInitializationState*/ int*) PURE;

			virtual HRESULT STDMETHODCALLTYPE get_TileStreamType(TileStreamType*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Size(UserTileImageSize*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_TilePath(HSTRING*) PURE;
		};

		MIDL_INTERFACE("8ef3b825-a92c-416e-820d-ed0fcf8d2296")
		ICredentialImageField : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_BitmapStream(ABI::Windows::Storage::Streams::IRandomAccessStream** ) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsLogoImageHidden(BOOL* ) PURE;
		};

		enum EmptyUserType
		{
		};

		class User;

#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_NI
		MIDL_INTERFACE("8c4b5cd8-e91d-43e8-bc9b-59b6e744ad5b")
#else
		MIDL_INTERFACE("837c3232-b75b-4f12-bf4c-d85f57d3882f")
#endif
		IUser : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_IsLoggedIn(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsBuiltInGuest(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_DisplayName(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_LogonStatus(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_EmailAddress(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_QualifiedUsername(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Sid(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_LargeUserTileImage(IUserTileImage**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SmallUserTileImage(IUserTileImage**) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_ImageChanged(WF::ITypedEventHandler<User*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_ImageChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ShouldShowEmail(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ShouldShowDomainName(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsLocalNoPasswordUser(BOOLEAN*) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_NI
			virtual HRESULT STDMETHODCALLTYPE get_EmptyUserType(EmptyUserType*) PURE;
#endif
		};

		class User
		{
		};

		enum CredentialFieldKind
		{
			CredentialFieldKind_Invalid = 0,
			CredentialFieldKind_StaticText = 1,
			CredentialFieldKind_CommandLink = 2,
			CredentialFieldKind_EditText = 3,
			CredentialFieldKind_TileImage = 4,
			CredentialFieldKind_CheckBox = 5,
			CredentialFieldKind_ComboBox = 6,
			CredentialFieldKind_SubmitButton = 7
		};

		enum CredentialFieldChangeKind
		{
			CredentialFieldChangeKind_State = 0,
			CredentialFieldChangeKind_InteractiveState = 1,
			CredentialFieldChangeKind_SetString = 2,
			CredentialFieldChangeKind_SetCheckbox = 3,
			CredentialFieldChangeKind_SetBitmap = 4,
			CredentialFieldChangeKind_SetComboBoxSelected = 5,
			CredentialFieldChangeKind_PasswordReveal = 6,
			CredentialFieldChangeKind_InputType = 7,
			CredentialFieldChangeKind_MaxLength = 8
		};

		//TODO: verify
		//MIDL_INTERFACE("546e206b-52e8-419c-a82f-6363971cdf29")
		MIDL_INTERFACE("ddc7731f-aaf1-4bd4-b20a-d125a3bc23d8")
		ICredentialField : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Label(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsVisibleInSelectedTile(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsVisibleInDeselectedTile(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsHidden(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsInteractiveStateFocused(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_IsInteractiveStateFocused(BOOLEAN) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsInteractiveStateDisabled(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsInteractiveStateReadOnly(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Kind(CredentialFieldKind*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ID(UINT*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_AccessibilityText(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsAccessibilityViewRaw(BOOLEAN*) PURE;;
			virtual HRESULT STDMETHODCALLTYPE add_FieldChanged(WF::ITypedEventHandler<ICredentialField*, CredentialFieldChangeKind>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_FieldChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_ShuttingDown(WF::ITypedEventHandler<ICredentialField*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_ShuttingDown(EventRegistrationToken) PURE;
		};

		enum CredentialUIMode
		{
			CredentialUIMode_ComplexLayout = 0,
			CredentialUIMode_BasicPin = 1
		};

		enum ConnectionStatus
		{
			ConnectionStatus_NotConnecting = 0,
			ConnectionStatus_Connecting = 1,
			ConnectionStatus_Connected = 2,
			ConnectionStatus_ConnectionFailed = 3
		};

		struct DECLSPEC_NOVTABLE ISerializationProgressInfo : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Status(ConnectionStatus*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_StatusMessage(HSTRING*) PURE;

		};

		class SerializationProgressInfo
		{
		};

		class Credential;

#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_NI
		MIDL_INTERFACE("1e5854d9-e7e1-4b99-b286-202a3c27cc05")
#else
		MIDL_INTERFACE("e818d511-9182-420e-b02f-0ca0c728d047")
#endif
		ICredential : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Fields(WFC::IVectorView<ICredentialField*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_LogoImageField(ICredentialField**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_LogoLabel(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SubmitButton(ICredentialField**) PURE;
			//virtual HRESULT STDMETHODCALLTYPE get_SubmitButtonAdjacentID(int*) PURE;
			//virtual HRESULT STDMETHODCALLTYPE add_SubmitButtonChanged(WF::ITypedEventHandler<Credential*, int>*, EventRegistrationToken*) PURE;
			//virtual HRESULT STDMETHODCALLTYPE remove_SubmitButtonChanged(EventRegistrationToken) PURE;
			//virtual HRESULT STDMETHODCALLTYPE get_SubmitButtonEnabled(BOOLEAN*) PURE;
			//virtual HRESULT STDMETHODCALLTYPE add_SubmitButtonEnabledChanged(WF::ITypedEventHandler<Credential*, BOOLEAN>*, EventRegistrationToken*) PURE;
			//virtual HRESULT STDMETHODCALLTYPE remove_SubmitButtonEnabledChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsPicturePassword(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ProviderId(GUID*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_UIMode(CredentialUIMode*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsSelected(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsTileVisible(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_User(IUser**) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_TileChanged(WF::ITypedEventHandler<Credential*, /*CredProvData::CredentialTileChangeKind*/int>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_TileChanged(EventRegistrationToken) PURE;
			//virtual HRESULT STDMETHODCALLTYPE get_SubmitButtonField(ICredentialField**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_HideUserTileImage(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_LargeUserTileImage(IUserTileImage**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SmallUserTileImage(IUserTileImage**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ExtraSmallUserTileImage(IUserTileImage**) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_UserImageChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_UserImageChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_WebDialogUrl(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsWebDialogVisible(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_IsWebDialogVisible(BOOLEAN) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_WebDialogVisibilityChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_WebDialogVisibilityChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE Submit() PURE;
			virtual HRESULT STDMETHODCALLTYPE PicturePasswordSubmit(IInspectable*) PURE;
			virtual HRESULT STDMETHODCALLTYPE CancelSubmission() PURE;
			virtual HRESULT STDMETHODCALLTYPE RefreshSelection() PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SerializationProgressChanged(WF::ITypedEventHandler<SerializationProgressInfo*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SerializationProgressChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_UIModeChanged(WF::ITypedEventHandler<Credential*, CredentialUIMode>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_UIModeChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectionStateChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectionStateChanged(EventRegistrationToken) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_NI
			virtual HRESULT STDMETHODCALLTYPE get_IsShowWebDialogRequestPending(BOOLEAN*) PURE;
#endif
		};
		// ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
		class Credential : ICredential
		{
		};
		// ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
		class CredentialSerialization : ICredentialSerialization
		{
		};

		struct DECLSPEC_NOVTABLE IReportResultInfo : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_ProviderCLSID(GUID*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_StatusCode(NTSTATUS*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SubstatusCode(NTSTATUS*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_StatusIcon(CredentialProviderStatusIcon*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_StatusMessage(HSTRING*) PURE;
		};
		// ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
		class ReportResultInfo : IReportResultInfo
		{
		};

		class CredProvDataModel;

		//14361 ICredProvDataModel
		/*
		 struct ICredProvDataModel : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE InitializeAsync(CredProvScenario, LANGID, SelectionMode, WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE InitializeWithContextAsync(CredProvScenario, LANGID, SelectionMode, UINT, BOOLEAN, IInspectable*, GUID, ABI::Windows::Storage::Streams::IBuffer*, WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectionMode(SelectionMode*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectionMode(SelectionMode) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Users(WFC::IObservableVector<User*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedUser(IUser**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedUser(IUser*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedUserChanged(WF::ITypedEventHandler<User*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedUserChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_PLAPCredentials(WFC::IObservableVector<Credential*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedPLAPCredential(ICredential**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedPLAPCredential(ICredential*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedPLAPCredentialChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedPLAPCredentialChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_V1Credentials(WFC::IObservableVector<Credential*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedV1Credential(ICredential**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedV1Credential(ICredential*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedV1CredentialChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedV1CredentialChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_UsersAndV1Credentials(WFC::IObservableVector<IInspectable*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedUserOrV1Credential(IInspectable**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedUserOrV1Credential(IInspectable*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedUserOrV1CredentialChanged(WF::ITypedEventHandler<IInspectable*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedUserOrV1CredentialChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_FlatCredentials(WFC::IObservableVector<Credential*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedFlatCredential(ICredential**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedFlatCredential(ICredential*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedFlatCredentialChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedFlatCredentialChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SerializationComplete(WF::ITypedEventHandler<CredProvDataModel*, CredentialSerialization*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SerializationComplete(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_CurrentBioFeedbackState(BioFeedbackState*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_BioFeedbackLabel(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_BioFeedbackStateChange(WF::ITypedEventHandler<CredProvDataModel*, BioFeedbackState>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_BioFeedbackStateChange(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE ReportResultAsync(NTSTATUS, NTSTATUS, HSTRING, WF::IAsyncOperation<ReportResultInfo*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE ClearState() PURE;
			virtual HRESULT STDMETHODCALLTYPE ResetAsync(CredProvScenario, WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE ResetSelection() PURE;
			virtual HRESULT STDMETHODCALLTYPE Shutdown() PURE;
			virtual HRESULT STDMETHODCALLTYPE DisconnectCredentials() PURE;
		};
		 */

		enum SupportedFeatureFlags
		{
			SupportedFeatureFlags_0 = 0,
		};

		struct DECLSPEC_NOVTABLE ICredProvDataModel : IInspectable
		{
			//virtual long __cdecl CCredProvDataModel::InitializeAsync(enum Windows::Internal::UI::Logon::CredProvData::CredProvScenario,enum Windows::Internal::UI::Logon::CredProvData::SupportedFeatureFlags,unsigned short,struct HSTRING__ * __ptr64,struct Windows::Foundation::IAsyncAction * __ptr64 * __ptr64) __ptr64
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE InitializeAsync(CredProvScenario, SupportedFeatureFlags, LANGID, HSTRING, WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE InitializeAsyncAndReportAutologon(CredProvScenario, SupportedFeatureFlags, LANGID, HSTRING, WF::IAsyncOperation<BOOLEAN>**) PURE;
#else
			virtual HRESULT STDMETHODCALLTYPE InitializeAsync(CredProvScenario, SupportedFeatureFlags, LANGID, WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE InitializeAsyncAndReportAutologon(CredProvScenario, SupportedFeatureFlags, LANGID, WF::IAsyncOperation<BOOLEAN>**) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE InitializeWithContextAsync(CredProvScenario, LANGID, SelectionMode, UINT, BOOLEAN, IInspectable*, GUID, ABI::Windows::Storage::Streams::IBuffer*, WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE InitializeForFingerprintOnlyAsync(LANGID, WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectionMode(SelectionMode*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectionMode(SelectionMode) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Users(WFC::IObservableVector<User*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedUser(IUser**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedUser(IUser*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedUserChanged(WF::ITypedEventHandler<User*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedUserChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_PLAPCredentials(WFC::IObservableVector<Credential*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedPLAPCredential(ICredential**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedPLAPCredential(ICredential*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedPLAPCredentialChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedPLAPCredentialChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_V1Credentials(WFC::IObservableVector<Credential*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedV1Credential(ICredential**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedV1Credential(ICredential*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedV1CredentialChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedV1CredentialChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_UsersAndV1Credentials(WFC::IObservableVector<IInspectable*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedUserOrV1Credential(IInspectable**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedUserOrV1Credential(IInspectable*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedUserOrV1CredentialChanged(WF::ITypedEventHandler<CredProvDataModel*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedUserOrV1CredentialChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_FlatCredentials(WFC::IObservableVector<Credential*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedFlatCredential(ICredential**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedFlatCredential(ICredential*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedFlatCredentialChanged(WF::ITypedEventHandler<Credential*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedFlatCredentialChanged(EventRegistrationToken) PURE;

			//NOTE: i am too lazy to put params for these
			virtual HRESULT STDMETHODCALLTYPE get_CredentialMethodBuckets() PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedCredentialMethodBucket() PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedCredentialMethodBucket() PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedCredentialMethodBucketChanged() PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedCredentialMethodBucketChanged() PURE;
			virtual HRESULT STDMETHODCALLTYPE add_LockScreenDismissRequested() PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_LockScreenDismissRequested() PURE;
			virtual HRESULT STDMETHODCALLTYPE add_OptionExpansionRequested() PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_OptionExpansionRequested() PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SignInUIRequested() PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SignInUIRequested() PURE;
			//END NOTE

			virtual HRESULT STDMETHODCALLTYPE add_SerializationComplete(WF::ITypedEventHandler<CredProvDataModel*, CredentialSerialization*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SerializationComplete(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_CurrentBioFeedbackState(BioFeedbackState*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_BioFeedbackLabel(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_BioFeedbackAccessibilityText(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_BioFeedbackStateChange(WF::ITypedEventHandler<CredProvDataModel*, BioFeedbackState>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_BioFeedbackStateChange(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE ReportResultAsync(NTSTATUS, NTSTATUS, HSTRING, WF::IAsyncOperation<ReportResultInfo*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE ReportResultWithNameAsync(NTSTATUS, NTSTATUS, HSTRING, HSTRING, HSTRING, WF::IAsyncOperation<ReportResultInfo*>**) PURE;

#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_VB
			virtual HRESULT STDMETHODCALLTYPE SignInOrUnlockUserWithUserManagerAsync(ICredentialSerialization*, WF::IAsyncOperation</*CredProvData::SignInOrUnlockResult**/void*>**) PURE;
#endif

#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE SetUserSuggestion(HSTRING) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE ClearState() PURE;
			//virtual HRESULT STDMETHODCALLTYPE ResetAsync(CredProvScenario, WF::IAsyncAction**) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE ResetAsync(CredProvScenario, SupportedFeatureFlags, HSTRING, WF::IAsyncAction**) PURE;
#else
			virtual HRESULT STDMETHODCALLTYPE ResetAsync(CredProvScenario, SupportedFeatureFlags, WF::IAsyncAction**) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE ResetSelection() PURE;
			virtual HRESULT STDMETHODCALLTYPE Shutdown() PURE;
			virtual HRESULT STDMETHODCALLTYPE DisconnectCredentials() PURE;
		};

		class CredProvDataModel
		{
		};

		struct DECLSPEC_NOVTABLE IDispatchEvent : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE Dispatch() PURE;
		};

		MIDL_INTERFACE("edc8d4ae-643f-4ebf-b869-b1463e38b829")
		IUIThreadEventDispatcher : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE DispatchEvent(IDispatchEvent*) PURE;
		};

		enum OptionalDependencyKind
		{
			OptionalDependencyKind_AutoLogonManager = 0,
			OptionalDependencyKind_WindowContainer = 1,
			OptionalDependencyKind_DefaultSelector = 2,
			OptionalDependencyKind_DisplayState = 3,
			OptionalDependencyKind_ViewGeneratedCredentialField = 4,
			OptionalDependencyKind_TelemetryData = 5,
			OptionalDependencyKind_UserTileImage = 6,
		};

		MIDL_INTERFACE("ef207a42-36ca-4e16-81c0-116cccc45deb")
		IOptionalDependencyProvider : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE GetOptionalDependency(OptionalDependencyKind, IInspectable**) PURE;
		};

		MIDL_INTERFACE("4b869909-e780-424e-ae03-210fe46f4527")
		ICredProvDataModelFactory : IInspectable
		{
			//CredProvDataModelFactory::CreateCredProvDataModel(enum Windows::Internal::UI::Logon::CredProvData::SelectionMode,struct Windows::Internal::UI::Logon::CredProvData::IUIThreadEventDispatcher* __ptr64,struct Windows::Internal::UI::Logon::CredProvData::IOptionalDependencyProvider* __ptr64,struct Windows::Internal::UI::Logon::CredProvData::ICredProvDataModel* __ptr64* __ptr64)
			virtual HRESULT CreateCredProvDataModel(SelectionMode,IUIThreadEventDispatcher*, IOptionalDependencyProvider*, ICredProvDataModel**) PURE;
		};

		MIDL_INTERFACE("ba6e72f5-f47d-4aad-8bb6-1bba3a750e9f")
		ICredentialGroup : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Credentials(WFC::IObservableVector<Credential*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SelectedCredential(ICredential**) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_SelectedCredential(ICredential*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SelectedCredentialChanged(WF::ITypedEventHandler<ICredentialGroup*, Credential*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SelectedCredentialChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE RefreshSelection() PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsAutoSubmitPending(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_IsAutoSubmitPendingChanged(WF::ITypedEventHandler<ICredentialGroup*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_IsAutoSubmitPendingChanged(EventRegistrationToken) PURE;
		};

		MIDL_INTERFACE("9a5f2170-ef01-4fe4-83c1-141ae408d40b")
		ICredProvDefaultSelector : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_UseLastLoggedOnProvider(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_PreferredProviders(WFC::IVectorView<GUID>**) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE get_OtherUserPreferredProviders(WFC::IVectorView<GUID>**) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE get_ExcludedProviders(WFC::IVectorView<GUID>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_DefaultUserSid(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE AllowAutoSubmitOnSelection(IUser*, BOOLEAN*) PURE;
		};

		enum CredentialTextSize
		{
			CredentialTextSize_Large = 0,
			CredentialTextSize_Small = 1
		};

		MIDL_INTERFACE("59c3c31e-3f86-4fa5-9c7b-dc7b542e2a70")
		ICredentialTextField : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Content(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_TextSize(CredentialTextSize*) PURE;
		};

		MIDL_INTERFACE("2ce19468-de1a-4cb5-9a6f-d09c8146f35a")
		ICredentialSubmitButtonField : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_AdjacentID(UINT*) PURE;
		};
	}

	namespace Controller
	{
		enum LogonErrorRedirectorResponse
		{
			LogonErrorRedirectorResponse_Invalid = 0,
			LogonErrorRedirectorResponse_NotHandled = 1,
			LogonErrorRedirectorResponse_HandledShowLocally = 2,
			LogonErrorRedirectorResponse_HandledDoNotShowLocally = 3,
			LogonErrorRedirectorResponse_HandledDoNotShowLocallyStartOver = 4,
		};

		enum LogonUIRequestReason
		{
			LogonUIRequestReason_LogonUILogon = 0,
			LogonUIRequestReason_LogonUIUnlock = 1,
			LogonUIRequestReason_LogonUIChange = 2,
			LogonUIRequestReason_LogonUISessionRecovery = 3,
			LogonUIRequestReason_LogonUIAlternate = 4,
			LogonUIRequestReason_CredUI = 5,
		};

		struct DECLSPEC_NOVTABLE IRedirectionManager : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE OnBeginPainting() PURE;
			virtual HRESULT STDMETHODCALLTYPE RedirectStatus(HSTRING, LogonErrorRedirectorResponse*) PURE;
			virtual HRESULT STDMETHODCALLTYPE RedirectMessage(HSTRING, HSTRING, UINT, UINT*, LogonErrorRedirectorResponse*) PURE;
			virtual HRESULT STDMETHODCALLTYPE RedirectLogonError(NTSTATUS, NTSTATUS, HSTRING, HSTRING, UINT, UINT*, LogonErrorRedirectorResponse*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsRedirectMode(BOOLEAN*) PURE;
		};

		enum UserTheme
		{
			UserTheme_Dark = 0,
			UserTheme_HCDark = 1,
			UserTheme_Light = 2,
			UserTheme_HCLight = 3,
		};

		enum EmbeddedSKUPolicyFlags
		{
			EmbeddedSKUPolicyFlags_DisableAll = 0,
			EmbeddedSKUPolicyFlags_EnableShutdownOption = 0x1,
			EmbeddedSKUPolicyFlags_EnableLanguageOption = 0x2,
			EmbeddedSKUPolicyFlags_EnableEaseOfAccessOption = 0x4,
			EmbeddedSKUPolicyFlags_EnableBackButton = 0x10,
			EmbeddedSKUPolicyFlags_EnableBSDR = 0x20,
			EmbeddedSKUPolicyFlags_EnableAll = 0x3F,
		};

		DEFINE_ENUM_FLAG_OPERATORS(EmbeddedSKUPolicyFlags);

		struct DECLSPEC_NOVTABLE IUserSettingManager : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE IsUserSwitchingAllowed(LogonUIRequestReason, BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE CancelUserSwitch() PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsLoggedOnUserSidPresent(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_LangID(LANGID*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_UserSid(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_UserSid(HSTRING) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_CaretWidth(int*) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE get_TextScale(int*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsTransparencyEnabled(BOOLEAN*) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE get_LastLoggedOnUserSid(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_TelemetryDataProvider(CredProvData::ITelemetryDataProvider**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsLockScreenAllowed(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_IsLockScreenAllowed(BOOLEAN) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsSwitchUser(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_UserTheme(UserTheme*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsLowMemoryDevice(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsFirstLogonAfterSignOutOrSwitchUser(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_EmbeddedSKUPolicy(EmbeddedSKUPolicyFlags*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ShowAccentColorInsteadOfLogonBackgroundImage(BOOLEAN*) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_CO
			virtual HRESULT STDMETHODCALLTYPE get_ShouldAnimateLogonBackgroundImage(BOOLEAN*) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE get_IsFirstBoot(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ShouldLaunchFirstLogonAnimation(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsOobeZDPRebootRequired(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ShouldLaunchFirstSignInAnimationInUserSession(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE put_ShouldLaunchFirstSignInAnimationInUserSession(BOOLEAN) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsAudioHIDEnabled(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_CurrentInputProfile(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsUserAssignedAccess(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsLowPowerState(BOOLEAN*) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_CO
			virtual HRESULT STDMETHODCALLTYPE get_BackgroundColor(ABI::Windows::UI::Color*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ForegroundColor(ABI::Windows::UI::Color*) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE add_LowPowerStateChanged(WF::ITypedEventHandler<IUserSettingManager*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_LowPowerStateChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_SessionDisconnected(WF::ITypedEventHandler<IUserSettingManager*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SessionDisconnected(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_ColorSetChanged(WF::ITypedEventHandler<IUserSettingManager*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_ColorSetChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_HighContrastChanged(WF::ITypedEventHandler<IUserSettingManager*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_HighContrastChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_CaretWidthChanged(WF::ITypedEventHandler<IUserSettingManager*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_CaretWidthChanged(EventRegistrationToken) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE add_TextScaleChanged(WF::ITypedEventHandler<IUserSettingManager*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_TextScaleChanged(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_IsTransparencyEnabledChanged(WF::ITypedEventHandler<IUserSettingManager*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_IsTransparencyEnabledChanged(EventRegistrationToken) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE add_SlideToShutdownDetected(WF::ITypedEventHandler<IUserSettingManager*, IInspectable*>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_SlideToShutdownDetected(EventRegistrationToken) PURE;
		};

		MIDL_INTERFACE("bd4fd664-fd5e-4ea2-8a71-48a75a4529c0")
		IUnlockTrigger : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE TriggerUnlock() PURE;
			virtual HRESULT STDMETHODCALLTYPE SyncBackstop() PURE;
			virtual HRESULT STDMETHODCALLTYPE CheckCompletion() PURE;
		};

		enum LogonUIFlags
		{
			LogonUIFlags_None = 0x0,
			LogonUIFlags_SecureGestureDisabled = 0x1,
			LogonUIFlags_PasswordExpired = 0x2,
			LogonUIFlags_AllowDirectUserSwitching = 0x4,
		};

		DEFINE_ENUM_FLAG_OPERATORS(LogonUIFlags);

		MIDL_INTERFACE("963ffa57-1043-4961-8bf0-d3966cd5b833")
		IBioFeedbackListener : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE OnBioFeedbackUpdate(CredProvData::BioFeedbackState, HSTRING) PURE;
		};

		enum LogonMessageMode
		{
			LogonMessageMode_None = 0,
			LogonMessageMode_Message = 1,
			LogonMessageMode_LogonError = 2,
			LogonMessageMode_NoCredentialNotFinished = 3,
			LogonMessageMode_NoCredentialFinished = 4,
			LogonMessageMode_EmergencyRestart = 5,
			LogonMessageMode_TerminalServiceDisconnectOptions = 6,
			LogonMessageMode_CredentialConnecting = 7,
		};

		enum LogonUIState
		{
			LogonUIState_Start = 0,
			LogonUIState_Welcome = 1,
			LogonUIState_Logon = 2,
			LogonUIState_Authenticate = 3,
			LogonUIState_LoggingOn = 4,
			LogonUIState_LoggingOff = 5,
			LogonUIState_StandingBy = 6,
			LogonUIState_ShuttingDown = 7,
			LogonUIState_Restarting = 8,
		};

		enum LogonUISecurityOptions
		{
			LogonUISecurityOptions_Cancel = 0x0,
			LogonUISecurityOptions_Lock = 0x1,
			LogonUISecurityOptions_LogOff = 0x2,
			LogonUISecurityOptions_ChangePassword = 0x4,
			LogonUISecurityOptions_TaskManager = 0x8,
			LogonUISecurityOptions_SwitchUser = 0x10,
			LogonUISecurityOptions_0x20 = 0x20,
		};

		DEFINE_ENUM_FLAG_OPERATORS(LogonUISecurityOptions);

		MIDL_INTERFACE("3b1f419e-0b45-42b1-a18c-3a2a8e1715db")
		IRequestCredentialsData : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Credential(CredProvData::ICredentialSerialization**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SaveCredential(BOOLEAN*) PURE;
		};

		// ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
		class RequestCredentialsData : public IRequestCredentialsData
		{
		};

		enum LogonUIShutdownChoice
		{
			LogonUIShutdownChoice_None = 0x0,
			LogonUIShutdownChoice_TurnOff = 0x1,
			LogonUIShutdownChoice_Restart = 0x2,
			LogonUIShutdownChoice_StandBy = 0x4,
			LogonUIShutdownChoice_AdvStandBy = 0x8,
			LogonUIShutdownChoice_Hibernate = 0x10,
			LogonUIShutdownChoice_EmergencyRestart = 0x20,
			LogonUIShutdownChoice_ForceCurrentSession = 0x40,
			LogonUIShutdownChoice_ForceOtherSessions = 0x80,
			LogonUIShutdownChoice_InstallUpdates = 0x100,
			LogonUIShutdownChoice_HybridMode = 0x200,
			LogonUIShutdownChoice_BootOptions = 0x400,
			LogonUIShutdownChoice_Disconnect = 0x800,
		};

		DEFINE_ENUM_FLAG_OPERATORS(LogonUIShutdownChoice);

		MIDL_INTERFACE("49931e22-78c2-41af-83de-4e02876bdb3c")
		IRequestCredentialsDataFactory : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE CreateRequestCredentialsData(CredProvData::ICredentialSerialization*, LogonUIShutdownChoice, HSTRING, IRequestCredentialsData**) PURE;
		};

		enum LogonUICredProvResponse
		{
			LogonUICredProvResponse_LogonUIResponseDefault = 0,
			LogonUICredProvResponse_LogonUIResponseRetry = 1,
			LogonUICredProvResponse_LogonUIResponseAbort = 2,
		};

		MIDL_INTERFACE("ba2419f9-f552-4e37-9145-3c1673104057")
		IReportCredentialsData : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Message(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Response(LogonUICredProvResponse*) PURE;
		};

		// ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
		class ReportCredentialsData : public IReportCredentialsData
		{
		};

		MIDL_INTERFACE("2e6a5994-4adf-4e53-80ea-106294d15f73")
		IReportCredentialsDataFactory : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE CreateReportCredentialsData(LogonUICredProvResponse, HSTRING, IReportCredentialsData**) PURE;
		};

		MIDL_INTERFACE("63ace640-87c6-4bda-ac7c-793c6e16cce1")
		IMessageDisplayResult : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_ResultCode(UINT*) PURE;
		};

		// ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
		class MessageDisplayResult : public IMessageDisplayResult
		{
		};

		MIDL_INTERFACE("a95d3ab1-c7b8-43a3-935d-26f432e72e0f")
		IMessageDisplayResultFactory : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE CreateMessageDisplayResult(UINT, IMessageDisplayResult**) PURE;
		};

		MIDL_INTERFACE("e1b3be00-a1e8-4ab4-a1b9-ea94cc55ed0d")
		ILogonUISecurityOptionsResult : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_SecurityOptionChoice(LogonUISecurityOptions*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ShutdownChoice(LogonUIShutdownChoice*) PURE;
		};

		// ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
		class LogonUISecurityOptionsResult : ILogonUISecurityOptionsResult
		{
		};

		MIDL_INTERFACE("d54e91cd-1605-4e92-a736-b18f5b26cedd")
		ILogonUISecurityOptionsResultFactory : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE CreateSecurityOptionsResult(LogonUISecurityOptions, LogonUIShutdownChoice, ILogonUISecurityOptionsResult**) PURE;
		};

		MIDL_INTERFACE("15be936a-4b14-4d87-bfeb-668c6d4aff2a")
		IWebDialogDismissTrigger : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE DismissWebDialog() PURE;
			virtual HRESULT STDMETHODCALLTYPE SyncBackstop() PURE;
		};

		//14361 ILogonUX
		/*MIDL_INTERFACE("fbc9fd2c-9b7e-4ed1-9b60-61cf17f4ec4c")
		ILogonUX : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE Start(IInspectable*, IRedirectionManager*, IUserSettingManager*, CredProvData::IDisplayStateProvider*, IBioFeedbackListener*) PURE;
			virtual HRESULT STDMETHODCALLTYPE DelayLock(BOOLEAN, IUnlockTrigger*) PURE;
			virtual HRESULT STDMETHODCALLTYPE HardLock(LogonUIRequestReason, BOOLEAN, IUnlockTrigger*) PURE;
			virtual HRESULT STDMETHODCALLTYPE RequestCredentialsAsync(LogonUIRequestReason, LogonUIFlags, WF::IAsyncOperation<RequestCredentialsData*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE ReportCredentialsAsync(LogonUIRequestReason, NTSTATUS, NTSTATUS, HSTRING, HSTRING, HSTRING, WF::IAsyncOperation<ReportCredentialsData*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE DisplayMessageAsync(LogonMessageMode, UINT, HSTRING, HSTRING, WF::IAsyncOperation<MessageDisplayResult*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE DisplayCredentialErrorAsync(NTSTATUS, NTSTATUS, UINT, HSTRING, HSTRING, WF::IAsyncOperation<MessageDisplayResult*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE DisplayStatusAsync(LogonUIState, HSTRING, WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE TriggerLogonAnimationAsync(WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE ResetCredentials() PURE;
			virtual HRESULT STDMETHODCALLTYPE ClearUIState(HSTRING) PURE;
			virtual HRESULT STDMETHODCALLTYPE RestoreFromFirstSignInAnimation() PURE;
			virtual HRESULT STDMETHODCALLTYPE ShowSecurityOptionsAsync(LogonUISecurityOptions, WF::IAsyncOperation<LogonUISecurityOptionsResult*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_WindowContainer(IInspectable**) PURE;
			virtual HRESULT STDMETHODCALLTYPE Hide() PURE;
			virtual HRESULT STDMETHODCALLTYPE Stop() PURE;
		};*/

#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
		MIDL_INTERFACE("0f689741-b89f-41f0-8f2b-19da66e5f54b")
#else
		MIDL_INTERFACE("b20f1813-3f69-4f1f-97a0-35f558178cd5")
#endif
		ILogonUX : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE Start(IInspectable*, IRedirectionManager*, IUserSettingManager*, CredProvData::IDisplayStateProvider*, IBioFeedbackListener*) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE DelayLock(BOOLEAN, BOOLEAN, BOOLEAN, HSTRING, IUnlockTrigger*) PURE;
			virtual HRESULT STDMETHODCALLTYPE HardLock(LogonUIRequestReason, BOOLEAN, BOOLEAN, BOOLEAN, HSTRING, IUnlockTrigger*) PURE;
			virtual HRESULT STDMETHODCALLTYPE RequestCredentialsAsync(LogonUIRequestReason, LogonUIFlags, HSTRING, WF::IAsyncOperation<RequestCredentialsData*>**) PURE;
#else
			virtual HRESULT STDMETHODCALLTYPE DelayLock(BOOLEAN, BOOLEAN, BOOLEAN, IUnlockTrigger*) PURE;
			virtual HRESULT STDMETHODCALLTYPE HardLock(LogonUIRequestReason, BOOLEAN, BOOLEAN, BOOLEAN, IUnlockTrigger*) PURE;
			virtual HRESULT STDMETHODCALLTYPE RequestCredentialsAsync(LogonUIRequestReason, LogonUIFlags, WF::IAsyncOperation<RequestCredentialsData*>**) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE ReportCredentialsAsync(LogonUIRequestReason, NTSTATUS, NTSTATUS, HSTRING, HSTRING, HSTRING, WF::IAsyncOperation<ReportCredentialsData*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE DisplayMessageAsync(LogonMessageMode, UINT, HSTRING, HSTRING, WF::IAsyncOperation<MessageDisplayResult*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE DisplayCredentialErrorAsync(NTSTATUS, NTSTATUS, UINT, HSTRING, HSTRING, WF::IAsyncOperation<MessageDisplayResult*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE DisplayStatusAsync(LogonUIState, HSTRING, WF::IAsyncAction**) PURE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
			virtual HRESULT STDMETHODCALLTYPE DisplayStatusAndForceCredentialPageAsync(LogonUIRequestReason, LogonUIFlags, HSTRING, LogonUIState, HSTRING, WF::IAsyncAction**) PURE;
#endif
			virtual HRESULT STDMETHODCALLTYPE TriggerLogonAnimationAsync(WF::IAsyncAction**) PURE;
			virtual HRESULT STDMETHODCALLTYPE ResetCredentials() PURE;
			virtual HRESULT STDMETHODCALLTYPE ClearUIState(HSTRING) PURE;
			virtual HRESULT STDMETHODCALLTYPE RestoreFromFirstSignInAnimation() PURE;
			virtual HRESULT STDMETHODCALLTYPE ShowSecurityOptionsAsync(LogonUISecurityOptions, WF::IAsyncOperation<LogonUISecurityOptionsResult*>**) PURE;
			virtual HRESULT STDMETHODCALLTYPE WebDialogDisplayed(IWebDialogDismissTrigger*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_WindowContainer(IInspectable**) PURE;
			virtual HRESULT STDMETHODCALLTYPE Hide() PURE;
			virtual HRESULT STDMETHODCALLTYPE Stop() PURE;
		};

		enum LockOptions
		{
			LockOptions_Default = 0,
			LockOptions_SecureDesktop = 0x1,
			LockOptions_TooManyFailedAttempts = 0x2,
			LockOptions_RequireSecureGesture = 0x4,
			LockOptions_DelayLock = 0x8,
		};

		//MIDL_INTERFACE("133bcdc9-89ab-4094-941e-9e8a69407eb4")
		MIDL_INTERFACE("716a5b05-6e30-4f65-8a89-97de0fda0852")
		ILockScreenHost : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE ShowWebDialogAsync(HSTRING, IWebDialogDismissTrigger**) PURE;
			virtual HRESULT STDMETHODCALLTYPE LockAsync(LockOptions, HSTRING, HSTRING, HSTRING, HSTRING, BOOLEAN*, IUnlockTrigger**) PURE;
			virtual HRESULT STDMETHODCALLTYPE Reset() PURE;
			virtual HRESULT STDMETHODCALLTYPE PreShutdown() PURE;
		};

		enum LockDisplayOwner
		{
			LockDisplayOwner_LockHost = 0,
			LockDisplayOwner_LogonUX = 1,
		};

		enum LockActivity
		{
			LockActivity_None = 0,
			LockActivity_Manipulation = 0x1,
			LockActivity_StartDismiss = 0x2,
			LockActivity_StopDismiss = 0x3,
			LockActivity_AboveLockApp = 0x4,
		};

		MIDL_INTERFACE("c1dee830-94c1-488c-82d0-3332c15a6f2d")
		ILockInfo : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_VisualOwner(LockDisplayOwner*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_DomainName(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_UserName(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_FriendlyName(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_RequireSecureGesture(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ShowSpeedBump(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_RequireSecureGestureString(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_SpeedBumpString(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsLostMode(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_LostModeMessage(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_UserActivity(WF::ITypedEventHandler<ILockInfo*, LockActivity>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_UserActivity(EventRegistrationToken) PURE;
		};
		MIDL_INTERFACE("00915c44-56ec-46c4-a0bd-154182b2a893")
		ILogonUIStateInfo : IInspectable
		{
			virtual HRESULT get_CurrentLogonUIState(LogonUIState*) PURE;
		};

		struct DECLSPEC_NOVTABLE IShutdownBlockingApp : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE get_Icon(ABI::Windows::Storage::Streams::IRandomAccessStream**) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Caption(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_BlockReason(HSTRING*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_IsBlocking(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_Id(UINT*) PURE;
		};

		enum BlockedShutdownResolution
		{
			BlockedShutdownResolution_None = 0,
			BlockedShutdownResolution_Force = 1,
			BlockedShutdownResolution_Cancel = 2,
		};

		MIDL_INTERFACE("36d8e713-0af2-428f-931c-346c1cd1a722")
		IBlockedShutdownResolverUX : IInspectable
		{
			virtual HRESULT STDMETHODCALLTYPE Start(IUserSettingManager*, ILogonUIStateInfo*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_ScaleFactor(UINT*) PURE;
			virtual HRESULT STDMETHODCALLTYPE get_WasClicked(BOOLEAN*) PURE;
			virtual HRESULT STDMETHODCALLTYPE AddApplication(IShutdownBlockingApp*) PURE;
			virtual HRESULT STDMETHODCALLTYPE RemoveApplication(UINT) PURE;
			virtual HRESULT STDMETHODCALLTYPE add_Resolved(WF::ITypedEventHandler<IBlockedShutdownResolverUX*, BlockedShutdownResolution>*, EventRegistrationToken*) PURE;
			virtual HRESULT STDMETHODCALLTYPE remove_Resolved(EventRegistrationToken) PURE;
			virtual HRESULT STDMETHODCALLTYPE Hide() PURE;
			virtual HRESULT STDMETHODCALLTYPE Stop() PURE;
		};
	}
}

MIDL_INTERFACE("fb69bc98-66a0-47ba-8b1d-f79b9e842bbc")
IUserTileStore : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE SaveUserTileToStream(IStream*, int) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetUserTileFromStream(int, IStream*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetLargePath(const WCHAR*, WCHAR**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSmallPath(const WCHAR*, WCHAR**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetExtraSmallPath(const WCHAR*, WCHAR**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDynamicPath(const WCHAR*, WCHAR**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSmallPicture(const WCHAR*, HBITMAP*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetLargePicture(const WCHAR*, HBITMAP*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUserTilePathBySize(const WCHAR*, UINT, WCHAR**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUserTilePathBySizeNoDefault(const WCHAR*, UINT, WCHAR**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDynamicPathBySize(const WCHAR*, UINT, WCHAR**) = 0;
	virtual HRESULT STDMETHODCALLTYPE ResetUserTile() = 0;
	virtual HRESULT STDMETHODCALLTYPE ClearHistory() = 0;
	virtual HRESULT STDMETHODCALLTYPE ShowErrorDialog(HRESULT, const WCHAR*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUserNames(WCHAR**, WCHAR**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUserNameW(UINT, WCHAR**) = 0;
	virtual void STDMETHODCALLTYPE SQMUserTileIsDefault() = 0;
	virtual HRESULT STDMETHODCALLTYPE SetImages(IStream*, IStream*, IStream*, const WCHAR*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetImageForUser(const WCHAR*, IStream*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetExtendedErrorInfo(WCHAR**, BYTE*) = 0;
	virtual HRESULT STDMETHODCALLTYPE FixCorruptedStoreIfNeeded() = 0;
};

enum class MessageOptionFlag
{
	None = 0x0,
	Ok = 0x1,
	Cancel = 0x2,
	Yes = 0x4,
	No = 0x8,
	SwitchUser = 16, //CUSTOM, NOT IN CONSOLELOGON
	OtherTiles = 32,
	ShutDownFrame = 64,
	ShowPLAP = 128,
	Accessibility = 256,
	DisconnectPLAP = 512
};

DEFINE_ENUM_FLAG_OPERATORS(MessageOptionFlag);

MIDL_INTERFACE("a0b16477-52f1-4cfe-b1cc-388cd0e3e23a")
IEnumShutdownChoices : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Next(DWORD, DWORD*, DWORD*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Skip(DWORD) = 0;
	virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
	virtual HRESULT STDMETHODCALLTYPE Clone(IEnumShutdownChoices**) = 0;
};

// We only implement the Windows 10 version of this one
MIDL_INTERFACE("bfdc5e2f-3402-49b3-8740-91d6dc5dbb15")
IShutdownChoices : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Refresh() = 0;
	virtual HRESULT STDMETHODCALLTYPE SetChoiceMask(DWORD) = 0;
	virtual void STDMETHODCALLTYPE GetChoiceMask(DWORD*) = 0;
	virtual void STDMETHODCALLTYPE GetDefaultUIChoiceMask(DWORD*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetShowBadChoices(int) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetChoiceEnumerator(IEnumShutdownChoices**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDefaultChoice(DWORD*) = 0;
	virtual int STDMETHODCALLTYPE UserHasShutdownRights() = 0;
	virtual HRESULT STDMETHODCALLTYPE GetChoiceName(DWORD, int, WCHAR*, UINT) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetChoiceDesc(DWORD, WCHAR*, UINT) = 0;
};

// Belongs to internal inputswitchserver.h
inline const GUID DECLSPEC_SELECTANY CLSID_InputSwitchControl = { 0xb9bc2a50, 0x43c3, 0x41aa, { 0xa0, 0x86, 0x5d, 0xb1, 0x4e, 0x18, 0x4b, 0xae } };

typedef enum __MIDL___MIDL_itf_inputswitchserver_0000_0000_0001
{
	ISCT_IDL_DESKTOP,
	ISCT_IDL_TOUCHKEYBOARD,
	ISCT_IDL_LOGONUI,
	ISCT_IDL_UAC,
	ISCT_IDL_SETTINGSPANE,
	ISCT_IDL_OOBE,
	ISCT_IDL_USEROOBE
} INPUT_SWITCH_IDL_CLIENT_TYPE;

typedef struct __MIDL___MIDL_itf_inputswitchserver_0000_0000_0002
{
	DWORD hkl;
	WCHAR* pszShortLanguageName;
	WCHAR* pszLanguageName;
	WCHAR* pszShortLayoutName;
	WCHAR* pszLayoutName;
	BOOL fShortLayoutNameIsDifferentiator;
	BOOL fIme;
	BOOL fDesktopIncompatibleIme;
	BOOL fImmersiveIncompatibleIme;
 #if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_NI
	 BYTE gap[8];
 #endif
	WCHAR* pszBcp47;
	WCHAR* pszFontFaceName;
	int nFontSizeAdjustment;
	int nFontBaselineAdjustment;
	BOOL fFontVertical;
	WCHAR* pszImeIconFileName;
	int nImeIconIndex;
} INPUT_SWITCH_IDL_PROFILE_DATA;

typedef struct __MIDL___MIDL_itf_inputswitchserver_0000_0000_0003
{
	WCHAR* pszTooltip;
	HICON hIcon;
	BOOL fDisabled;
	BOOL fHidden;
	WCHAR* pszIconGlyph;
} INPUT_SWITCH_IDL_IME_MODE_ITEM_DATA;

typedef enum __MIDL___MIDL_itf_inputswitchserver_0000_0000_0004
{
	INPUT_SWITCH_IDL_IME_CLICK_TYPE_LEFT,
	INPUT_SWITCH_IDL_IME_CLICK_TYPE_RIGHT,
	INPUT_SWITCH_IDL_IME_CLICK_TYPE_LEFT_DISABLED
} INPUT_SWITCH_IDL_IME_CLICK_TYPE;

typedef enum __MIDL___MIDL_itf_inputswitchserver_0000_0000_0005
{
	INPUT_SWITCH_IDL_MODALITY_STANDARDKEYBOARD = 0x1,
	INPUT_SWITCH_IDL_MODALITY_SPLITKEYBOARD = 0x2,
	INPUT_SWITCH_IDL_MODALITY_CLASSICKEYBOARD = 0x4,
	INPUT_SWITCH_IDL_MODALITY_HANDWRITING = 0x8,
	INPUT_SWITCH_IDL_MODALITY_HIDE = 0x10,
	INPUT_SWITCH_IDL_MODALITY_ONEHANDEDKEYBOARD = 0x20,
} INPUT_SWITCH_IDL_MODALITY;

DEFINE_ENUM_FLAG_OPERATORS(INPUT_SWITCH_IDL_MODALITY);

typedef enum __MIDL___MIDL_itf_inputswitchserver_0000_0000_0006
{
	INPUT_SWITCH_IDL_ALIGN_DEFAULT,
	INPUT_SWITCH_IDL_ALIGN_RIGHT_EDGE,
	INPUT_SWITCH_IDL_ALIGN_LEFT_EDGE,
} INPUT_SWITCH_IDL_ALIGNMENT;

typedef enum __MIDL___MIDL_itf_inputswitchserver_0000_0000_0008
{
	INPUT_SWITCH_IDL_CFOM_NO_OVERRIDE,
	INPUT_SWITCH_IDL_CFOM_DESKTOP,
	INPUT_SWITCH_IDL_CFOM_IMMERSIVE,
} INPUT_SWITCH_IDL_CFOM;

interface IInputSwitchCallback;

MIDL_INTERFACE("b9bc2a50-43c3-41aa-a082-5db14e184bae")
IInputSwitchControl : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Init(INPUT_SWITCH_IDL_CLIENT_TYPE) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCallback(IInputSwitchCallback*) = 0;
	virtual HRESULT STDMETHODCALLTYPE ShowInputSwitch(const RECT*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProfileCount(UINT*, BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentProfile(INPUT_SWITCH_IDL_PROFILE_DATA*) = 0;
	virtual HRESULT STDMETHODCALLTYPE RegisterHotkeys() = 0;
	virtual HRESULT STDMETHODCALLTYPE ClickImeModeItem(INPUT_SWITCH_IDL_IME_CLICK_TYPE, POINT, const RECT*) = 0;
 #if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_NI
	 virtual HRESULT STDMETHODCALLTYPE ClickImeModeItemWithAnchor(INPUT_SWITCH_IDL_IME_CLICK_TYPE, IUnknown*) = 0;
 #endif
	virtual HRESULT STDMETHODCALLTYPE ForceHide() = 0;
	virtual HRESULT STDMETHODCALLTYPE ShowTouchKeyboardInputSwitch(const RECT*, INPUT_SWITCH_IDL_ALIGNMENT, int, DWORD, INPUT_SWITCH_IDL_MODALITY) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetContextFlags(DWORD*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetContextOverrideMode(INPUT_SWITCH_IDL_CFOM) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentImeModeItem(INPUT_SWITCH_IDL_IME_MODE_ITEM_DATA*) = 0;
	virtual HRESULT STDMETHODCALLTYPE ActivateInputProfile(const WCHAR*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetUserSid(const WCHAR*) = 0;
};

MIDL_INTERFACE("b9bc2a50-43c3-41aa-a083-5db14e184bae")
IInputSwitchCallback : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnUpdateProfile(const INPUT_SWITCH_IDL_PROFILE_DATA* pData) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnUpdateTsfFloatingFlags(DWORD dwFlags) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnProfileCountChange(UINT nCount, BOOL bCertifiedImePresent) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnShowHide(BOOL bShown, BOOL a3, BOOL a4) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnImeModeItemUpdate(const INPUT_SWITCH_IDL_IME_MODE_ITEM_DATA* pData) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnModalitySelected(INPUT_SWITCH_IDL_MODALITY a2) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnContextFlagsChange(DWORD dwFlags) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnTouchKeyboardManualInvoke() = 0;
};

extern "C" inline const IID IID_IInputSwitchCallback = __uuidof(IInputSwitchCallback);

extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Windows_Internal_UI_Logon_CredProvData_CredProvDataModel[] = L"Windows.Internal.UI.Logon.CredProvData.CredProvDataModel";

extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Windows_Internal_UI_Logon_Controller_RequestCredentialsData[] = L"Windows.Internal.UI.Logon.Controller.RequestCredentialsData";
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Windows_Internal_UI_Logon_Controller_ReportCredentialsData[] = L"Windows.Internal.UI.Logon.Controller.ReportCredentialsData";
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Windows_Internal_UI_Logon_Controller_MessageDisplayResult[] = L"Windows.Internal.UI.Logon.Controller.MessageDisplayResult";
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Windows_Internal_UI_Logon_Controller_LogonUISecurityOptionsResult[] = L"Windows.Internal.UI.Logon.Controller.LogonUISecurityOptionsResult";

namespace LC = Windows::Internal::UI::Logon::Controller;
namespace LCPD = Windows::Internal::UI::Logon::CredProvData;

#pragma region "Specializations for templated types"
// ReSharper disable CppPolymorphicClassWithNonVirtualPublicDestructor

namespace ABI::Windows::Foundation
{
	template <>
	struct __declspec(uuid("ffffffff-ffff-ffff-ffff-ffffffffffff")) // @Note: Changes in CredProvDataModel mean this GUID changes too
	ITypedEventHandler<LCPD::CredProvDataModel*, LCPD::CredentialSerialization*>
		: ITypedEventHandler_impl<
			  Internal::AggregateType<LCPD::CredProvDataModel*, LCPD::ICredProvDataModel*>
			, Internal::AggregateType<LCPD::CredentialSerialization*, LCPD::ICredentialSerialization*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.TypedEventHandler`2<Windows.Internal.UI.Logon.CredProvData.CredProvDataModel, Windows.Internal.UI.Logon.CredProvData.CredentialSerialization>";
		}
	};

	template <>
	struct __declspec(uuid("ffffffff-ffff-ffff-ffff-ffffffffffff")) // @Note: Changes in CredProvDataModel mean this GUID changes too
	ITypedEventHandler<LCPD::CredProvDataModel*, LCPD::BioFeedbackState>
		: ITypedEventHandler_impl<
			Internal::AggregateType<LCPD::CredProvDataModel*, LCPD::ICredProvDataModel*>
			, LCPD::BioFeedbackState
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.TypedEventHandler`2<Windows.Internal.UI.Logon.CredProvData.CredProvDataModel, Windows.Internal.UI.Logon.CredProvData.BioFeedbackState>";
		}
	};

	template <>
	struct __declspec(uuid("ffffffff-ffff-ffff-ffff-ffffffffffff")) // @Note: Changes in CredProvDataModel mean this GUID changes too
	ITypedEventHandler<LCPD::CredProvDataModel*, IInspectable*>
		: ITypedEventHandler_impl<
			  Internal::AggregateType<LCPD::CredProvDataModel*, LCPD::ICredProvDataModel*>
			, IInspectable*
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.TypedEventHandler`2<Windows.Internal.UI.Logon.CredProvData.CredProvDataModel, Object>";
		}
	};

	template <>
	struct __declspec(uuid("bb2bff59-99c0-5e2c-8dd8-0eb219b6df5e"))
	ITypedEventHandler<LCPD::Credential*, IInspectable*>
		: ITypedEventHandler_impl<
			  Internal::AggregateType<LCPD::Credential*, LCPD::ICredential*>
			, IInspectable*
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.TypedEventHandler`2<Windows.Internal.UI.Logon.CredProvData.Credential, Object>";
		}
	};

	template <>
	struct __declspec(uuid("f341f931-a6ad-54a8-ae85-7cf24438ccf9"))
	ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind>
		: ITypedEventHandler_impl<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.TypedEventHandler`2<Windows.Internal.UI.Logon.CredProvData.ICredentialField, Windows.Internal.UI.Logon.CredProvData.CredentialFieldChangeKind>";
		}
	};

	template <>
	struct __declspec(uuid("ffffffff-ffff-ffff-ffff-ffffffffffff"))
	ITypedEventHandler<LCPD::ICredentialGroup*, LCPD::Credential*>
		: ITypedEventHandler_impl<
			  LCPD::ICredentialGroup*
			, Internal::AggregateType<LCPD::Credential*, LCPD::ICredential*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.TypedEventHandler`2<Windows.Internal.UI.Logon.CredProvData.ICredentialGroup, Windows.Internal.UI.Logon.CredProvData.Credential>";
		}
	};

	template <>
	struct __declspec(uuid("fdbc116e-6b5c-5474-b5c9-86eb542dec81"))
	ITypedEventHandler<LC::ILockInfo*, LC::LockActivity>
		: ITypedEventHandler_impl<LC::ILockInfo*, LC::LockActivity>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.TypedEventHandler`2<Windows.Internal.UI.Logon.Controller.ILockInfo, Windows.Internal.UI.Logon.Controller.LockActivity>";
		}
	};

	template <>
	struct __declspec(uuid("e2b942b6-affd-5dd4-bb99-47f5be2e1616"))
		ITypedEventHandler<LC::IBlockedShutdownResolverUX*, LC::BlockedShutdownResolution>
			: ITypedEventHandler_impl<LC::IBlockedShutdownResolverUX*, LC::BlockedShutdownResolution>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.TypedEventHandler`2<Windows.Internal.UI.Logon.Controller.IBlockedShutdownResolverUX, Windows.Internal.UI.Logon.Controller.BlockedShutdownResolution>";
		}
	};

	template <>
	struct
	IAsyncOperation<LCPD::ReportResultInfo*>
		: IAsyncOperation_impl<
			  Internal::AggregateType<LCPD::ReportResultInfo*, LCPD::IReportResultInfo*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.IAsyncOperation`1<Windows.Internal.UI.Logon.CredProvData.ReportResultInfo>";
		}
	};

	template <>
	struct __declspec(uuid("21b7b9a0-3262-5d33-bab9-93bf7441e227"))
	IAsyncOperation<LC::LogonUISecurityOptionsResult*>
		: IAsyncOperation_impl<
			  Internal::AggregateType<LC::LogonUISecurityOptionsResult*, LC::ILogonUISecurityOptionsResult*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.IAsyncOperation`1<Windows.Internal.UI.Logon.Controller.LogonUISecurityOptionsResult>";
		}
	};

	template <>
	struct __declspec(uuid("55a51b4d-565d-5406-a96c-330ce9523a75"))
	IAsyncOperation<LC::MessageDisplayResult*>
		: IAsyncOperation_impl<
			  Internal::AggregateType<LC::MessageDisplayResult*, LC::IMessageDisplayResult*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.IAsyncOperation`1<Windows.Internal.UI.Logon.Controller.MessageDisplayResult>";
		}
	};

	template <>
	struct __declspec(uuid("b1cba636-d8a4-5a7c-8ea5-73c457d4a53c"))
	IAsyncOperation<LC::ReportCredentialsData*>
		: IAsyncOperation_impl<
			  Internal::AggregateType<LC::ReportCredentialsData*, LC::IReportCredentialsData*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.IAsyncOperation`1<Windows.Internal.UI.Logon.Controller.ReportCredentialsData>";
		}
	};

	template <>
	struct __declspec(uuid("dfa58656-8538-5608-a288-2c63273a655e"))
	IAsyncOperation<LC::RequestCredentialsData*>
		: IAsyncOperation_impl<
			  Internal::AggregateType<LC::RequestCredentialsData*, LC::IRequestCredentialsData*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.IAsyncOperation`1<Windows.Internal.UI.Logon.Controller.RequestCredentialsData>";
		}
	};

	template <>
	struct __declspec(uuid("ab2e40b5-1d90-529c-a514-45d6219c2f75"))
	IAsyncOperationCompletedHandler<LCPD::ReportResultInfo*>
		: IAsyncOperationCompletedHandler_impl<
			  Internal::AggregateType<LCPD::ReportResultInfo*, LCPD::IReportResultInfo*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.AsyncOperationCompletedHandler`1<Windows.Internal.UI.Logon.CredProvData.ReportResultInfo>";
		}
	};

	template <>
	struct __declspec(uuid("db4ea180-872b-5393-96c5-f2617c05b8d5"))
	IAsyncOperationCompletedHandler<LC::LogonUISecurityOptionsResult*>
		: IAsyncOperationCompletedHandler_impl<
			  Internal::AggregateType<LC::LogonUISecurityOptionsResult*, LC::ILogonUISecurityOptionsResult*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.AsyncOperationCompletedHandler`1<Windows.Internal.UI.Logon.Controller.LogonUISecurityOptionsResult>";
		}
	};

	template <>
	struct __declspec(uuid("159cd2c8-ce8c-5397-9590-b2f0b17306e7"))
	IAsyncOperationCompletedHandler<LC::MessageDisplayResult*>
		: IAsyncOperationCompletedHandler_impl<
			  Internal::AggregateType<LC::MessageDisplayResult*, LC::IMessageDisplayResult*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.AsyncOperationCompletedHandler`1<Windows.Internal.UI.Logon.Controller.MessageDisplayResult>";
		}
	};

	template <>
	struct __declspec(uuid("bed8c32f-53c8-5309-a701-c81353ecc80d"))
	IAsyncOperationCompletedHandler<LC::ReportCredentialsData*>
		: IAsyncOperationCompletedHandler_impl<
			  Internal::AggregateType<LC::ReportCredentialsData*, LC::IReportCredentialsData*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.AsyncOperationCompletedHandler`1<Windows.Internal.UI.Logon.Controller.ReportCredentialsData>";
		}
	};

	template <>
	struct __declspec(uuid("613f6d50-f93e-568a-bd91-af580034347c"))
	IAsyncOperationCompletedHandler<LC::RequestCredentialsData*>
		: IAsyncOperationCompletedHandler_impl<
			  Internal::AggregateType<LC::RequestCredentialsData*, LC::IRequestCredentialsData*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.AsyncOperationCompletedHandler`1<Windows.Internal.UI.Logon.Controller.RequestCredentialsData>";
		}
	};
}

namespace ABI::Windows::Foundation::Collections
{
	template <>
	struct __declspec(uuid("b32bdca4-5e52-5b27-bc5d-d66a1a268c2a"))
	IVector<IInspectable*>
		: IVector_impl<IInspectable*>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.IVector`1<Object>";
		}
	};

	template <>
	//struct __declspec(uuid("9f15eb60-ec8f-5163-9d6e-dd6f3f7aa004"))
	//struct __declspec(uuid("85488cf7-2edf-541c-970b-0e9395940279"))
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_NI
	struct __declspec(uuid("8191239c-d019-5340-87d8-959a8fde15d3"))
#else
	struct __declspec(uuid("85488cf7-2edf-541c-970b-0e9395940279"))
#endif
	IVector<LCPD::Credential*>
		: IVector_impl<
			  Internal::AggregateType<LCPD::Credential*, LCPD::ICredential*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.IVector`1<Windows.Internal.UI.Logon.CredProvData.Credential>";
		}
	};

	template <>
	struct
	IVectorView<LCPD::ICredentialField*>
		: IVectorView_impl<LCPD::ICredentialField*>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.IVectorView`1<Windows.Internal.UI.Logon.CredProvData.ICredentialField>";
		}
	};

	template <>
	struct
	IObservableVector<HSTRING>
		: IObservableVector_impl<HSTRING>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.IObservableVector`1<String>";
		}
	};

	template <>
	struct
	IObservableVector<IInspectable*>
		: IObservableVector_impl<IInspectable*>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.IObservableVector`1<Object>";
		}
	};

	template <>
	struct
	IObservableVector<LCPD::Credential*>
		: IObservableVector_impl<
			  Internal::AggregateType<LCPD::Credential*, LCPD::ICredential*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.IObservableVector`1<Windows.Internal.UI.Logon.CredProvData.Credential>";
		}
	};

	template <>
	struct __declspec(uuid("cb6c396f-4861-5296-b14b-bd90b941a3e0"))
	VectorChangedEventHandler<HSTRING>
		: VectorChangedEventHandler_impl<HSTRING>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.VectorChangedEventHandler`1<String>";
		}
	};

	template <>
	struct __declspec(uuid("b423a801-d35e-56b9-813b-00889536cb98"))
	VectorChangedEventHandler<IInspectable*>
		: VectorChangedEventHandler_impl<IInspectable*>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.VectorChangedEventHandler`1<Object>";
		}
	};

	template <>
	struct __declspec(uuid("a1181e1b-89d9-5148-8b13-c202a0e595f4"))
	VectorChangedEventHandler<LCPD::Credential*>
		: VectorChangedEventHandler_impl<
			  Internal::AggregateType<LCPD::Credential*, LCPD::ICredential*>
		>
	{
		static const wchar_t* z_get_rc_name_impl()
		{
			return L"Windows.Foundation.Collections.VectorChangedEventHandler`1<Windows.Internal.UI.Logon.CredProvData.Credential>";
		}
	};
}

// ReSharper restore CppPolymorphicClassWithNonVirtualPublicDestructor
#pragma endregion "Specializations for templated types"
