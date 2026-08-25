#pragma once

#include "pch.h"

#include "logonnativehwndhost.h"
#include "logonviewmanager.h"
#include "DirectUI/DirectUI.h"
#include "userlist.h"

/*class CLogonFrame7 : public DirectUI::HWNDElement
{
public:

	~CLogonFrame7() override;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(HWND hParent, bool fDblBuffer, UINT nCreate, Element* pParent, DWORD* pdwDeferCookie, DirectUI::Element** ppElement);

	static HRESULT Create(CLogonNativeHWNDHost* host);

	static HRESULT Register();

	static CLogonFrame7* GetSingleton();

	HRESULT CreateStyleParser(DirectUI::DUIXmlParser** outParser) override;
	void OnEvent(DirectUI::Event* pEvent) override;

	void SetOptions(MessageOptionFlag optionsFlag);

	void SetBackgroundGraphics();
	void ShowSecurityOptions(LC::LogonUISecurityOptions SecurityOptsFlag, WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion);
	HRESULT OnSecurityOptionSelected(LC::LogonUISecurityOptions SecurityOpt);
	HRESULT ConfirmEmergencyShutdown();

	void ShowStatusMessage(const wchar_t* message);

	void SwitchToUserList(class UserList* userList);
	void DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, WORD flags, WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
	void DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, WORD flags);
	HRESULT OnMessageOptionPressed(MessageOptionFlag flag);

	void ShowLockedScreen();

	CLogonNativeHWNDHost* m_nativeHost;
	DirectUI::Element* m_CurrentWindow = nullptr;
	DirectUI::DUIXmlParser* m_xmlParser;

	DirectUI::Element* m_Window = nullptr;
	DirectUI::Element* m_Options = nullptr;
	DirectUI::Element* m_SwitchUser = nullptr;
	DirectUI::Element* m_OtherTiles = nullptr;
	DirectUI::Element* m_Ok = nullptr;
	DirectUI::Element* m_Yes = nullptr;
	DirectUI::Element* m_No = nullptr;
	DirectUI::Element* m_Cancel = nullptr;
	DirectUI::Element* m_ShutDownFrame = nullptr;
	DirectUI::Element* m_ShowPLAP = nullptr;
	DirectUI::Element* m_DisconnectPLAP = nullptr;
	DirectUI::Element* m_Accessibility = nullptr;
	DirectUI::Element* m_MessageFrame = nullptr;
	DirectUI::Element* m_FullMessageFrame = nullptr;
	DirectUI::Element* m_ShortMessageFrame = nullptr;
	DirectUI::Element* m_ConnectMessageFrame = nullptr;
	DirectUI::Element* m_Status = nullptr;
	DirectUI::Element* m_StatusText = nullptr;
	DirectUI::Element* m_SecurityOptions = nullptr;
	DirectUI::Element* m_Locked = nullptr;
	DirectUI::Element* m_WaitAnimation = nullptr;

	UserList* m_activeUserList;
	UserList* m_LogonUserList;
	UserList* m_PLAPUserList;

	Microsoft::WRL::ComPtr<LogonViewManager> m_consoleUIManager;
	Microsoft::WRL::ComPtr<IShutdownChoices> m_shutdownChoices;
	LC::LogonUIRequestReason m_currentReason;

	bool isHighContrast = false;

private:
	HRESULT _Initialize(CLogonNativeHWNDHost* Host, DirectUI::Element* pParent, DWORD* DeferCookie);
	HRESULT _InitializeUserLists();
	bool _IsInstallUpdatesAndShutdownAllowed();
	bool _ShowBackgroundBitmap();
	bool _IsSwitchUserAllowed();
	void _SetSoftKeyboardAllowed(bool allowed);
	void _SetBrandingGraphic();
	void _SelectMode(DirectUI::Element* elementToHost, bool isVisible);
	void _ShowCursor(bool bShow);
	void _DisplayStatusMessage(const wchar_t* message, bool showSpinner);
	void _DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, UINT flags);
	void _OnEmergencyRestart();
	void _HandleShutdownChoices();
	void _ShutdownCommon(DWORD choice);

	static CLogonFrame7* _pSingleton;

	wistd::unique_ptr<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>>> m_SecurityOptionsCompletion;
	wistd::unique_ptr<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>>> m_MessageDisplayResultCompletion;
	bool m_bIsInEmergencyRestartDialog = false;
};*/

class LogonAccount;

#define LS_Pending      0
#define LS_Granted      1
#define LS_Denied       2

#define LAS_Initialized 0
#define LAS_PreStatus   1
#define LAS_Logon       2
#define LAS_PostStatus  3
#define LAS_Hide        4
#define LAS_Done        5

#define LASS_Email    0
#define LASS_LoggedOn 1

//TODO: move to nicer place
#define PF_TypeBits     0x03  // Map out for LocalOnly, Normal, or Trilevel
#define IsProp(p)       ((p##Prop() == ppi) && ((ppi->fFlags&PF_TypeBits) == iIndex))

#define RECTWIDTH(rc)   ((rc).right-(rc).left)

inline COLORREF
ARGB(
	IN BYTE a,
	IN BYTE r,
	IN BYTE g,
	IN BYTE b)
{
	return ((a << 24) | RGB(r, g, b));   // Current A values may be 255 (opaque) or 0 (transparent)
}

inline COLORREF
ORGB(
	IN BYTE r,
	IN BYTE g,
	IN BYTE b)
{
	return ARGB(255, r, g, b);           // Current A values may be 255 (opaque) or 0 (transparent)
}

//

#define TIMER_REFRESHTIPS 1014
#define TIMER_ANIMATEFLAG 1015
#define TIMER_UPDATETIME 2002

#define MAX_FLAG_FRAMES 50
#define FLAG_ANIMATION_COUNT 100
#define TOTAL_FLAG_FRAMES (FLAG_ANIMATION_COUNT * MAX_FLAG_FRAMES)

extern UINT_PTR g_puTimerId;
extern UINT_PTR g_puFlagTimerId;
extern UINT_PTR g_puUpdateTimeId;

extern DWORD sTimerCount;

class LogonFrame : public DirectUI::HWNDElement, public DirectUI::IElementListener
{
public:
    static HRESULT Create(OUT Element** ppElement);  // Required for ClassInfo (always fails)
    static HRESULT Create(HWND hParent, BOOL fDblBuffer, UINT nCreate, DWORD* pdwDeferCookie, OUT Element** ppElement);

    // Generic events
    virtual void OnEvent(DirectUI::Event* pEvent);

    // System events
    virtual void OnInput(DirectUI::InputEvent* pEvent);
    virtual void OnPropertyChanged(DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld, DirectUI::Value* pvNew);

    virtual Element* GetAdjacent(Element* peFrom, int iNavDir, DirectUI::NavReference const* pnr, bool bKeyable);

    // Frame Callbacks
    HRESULT OnLogUserOn(LogonAccount* pla);
    HRESULT OnPower();
    HRESULT OnUndock();
    HRESULT OnTreeReady(DirectUI::DUIXmlParser* pParser);

    // Listener impl
    virtual void OnListenerAttach(Element* peFrom) { peFrom; }
    virtual void OnListenerDetach(Element* peFrom) { peFrom; }
    virtual bool OnListenedPropertyChanging(Element* peFrom, const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld, DirectUI::Value* pvNew) { peFrom; ppi; iIndex; pvOld; pvNew; return true; }
    virtual void OnListenedPropertyChanged(Element* peFrom, const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld, DirectUI::Value* pvNew);
    virtual void OnListenedInput(Element* peFrom, DirectUI::InputEvent* pInput) { peFrom; pInput; }
    virtual void OnListenedEvent(Element* peFrom, DirectUI::Event* pEvent) { peFrom; pEvent; }

    // Operations
    static int PointToPixel(int nPoint) { return MulDiv(nPoint, _nDPI, 72); }
    HINSTANCE GetHInstance() { return _pParser->GetHInstance(); }
    void HideAccountPanel() { _peRightPanel->SetLayoutPos(DirectUI::LP_None); _peLeftPanel->RemoveLocalValue(WidthProp); }
    void ShowAccountPanel() { _peRightPanel->SetLayoutPos(DirectUI::BLP_Left); _peLeftPanel->SetWidth(380); }
    void HideLogoArea() { _peLogoArea->SetLayoutPos(DirectUI::LP_None); }
    void ShowLogoArea() { _peLogoArea->SetLayoutPos(DirectUI::BLP_Client); }
    void HideDateTimeArea() { _peDateTimeArea->SetLayoutPos(DirectUI::LP_None); }
    void ShowDateTimeArea() { _peDateTimeArea->SetLayoutPos(DirectUI::BLP_Right); }
    void HideWelcomeArea() { _peMsgArea->SetLayoutPos(DirectUI::LP_None); }
    void ShowWelcomeArea() { _peMsgArea->SetLayoutPos(DirectUI::BLP_Client); }
    void HidePowerButton() { _pbPower->SetVisible(false); }
    void ShowPowerButton() { _pbPower->SetVisible(true); }
    void SetPowerButtonLabel(LPCWSTR psz) { Element* pe = _pbPower->FindDescendent(DirectUI::StrToID(L"label")); if (pe) pe->SetContentString(psz); }
    void InsertUndockButton() { _pbUndock->SetLayoutPos(DirectUI::BLP_Top); }
    void RemoveUndockButton() { _pbUndock->SetLayoutPos(DirectUI::LP_None); }
    void HideUndockButton() { _pbUndock->SetVisible(false); }
    void ShowUndockButton() { _pbUndock->SetVisible(true); }
    void SetUndockButtonLabel(LPCWSTR psz) { Element* pe = _pbUndock->FindDescendent(DirectUI::StrToID(L"label")); if (pe) pe->SetContentString(psz); }
    void SetStatus(LPCWSTR psz, bool bHideAccountPanel = true);
    void SetTitle(UINT uRCID);
    void SetTitle(LPCWSTR pszTitle);
    void SetButtonLabels();
    //HRESULT AddAccount(LPCWSTR pszPicture, BOOL fPicRes, LPCWSTR pszName, LPCWSTR pszUsername, LPCWSTR pszHint, BOOL fPwdNeeded, BOOL fLoggedOn, OUT LogonAccount** ppla);
    HRESULT AddAccountFromTile(const Microsoft::WRL::ComPtr<LCPD::ICredential>& tileData, const Microsoft::WRL::ComPtr<LCPD::IUser>& user, OUT LogonAccount** ppla);
    DirectUI::NativeHWNDHost* GetNativeHost() { return _pnhh; }
    void SetNativeHost(DirectUI::NativeHWNDHost* pnhh) { _pnhh = pnhh; }
    void UpdateUserStatus(BOOL fRefreshAll = false);
	void UpdateTime();
    LogonAccount* FindUserByCred(Microsoft::WRL::ComPtr<LCPD::ICredential>& cred);
    LogonAccount* FindNamedUser(LPCWSTR pszUsername);
    void SelectUser(LPCWSTR pszUsername);
    void ResetUserList();
    void Resize(BOOL fWorkArea);
    void SetAnimations(BOOL fAnimations);

    void ResetTheme();

	

    BOOL UserListAvailable() { return _fListAvailable; }
    void SetUserListAvailable(BOOL fListAvailable) { _fListAvailable = fListAvailable; }
    // ClassInfo accessors (static and virtual instance-based)
    static DirectUI::IClassInfo* Class;
    virtual DirectUI::IClassInfo* GetClassInfo() { return Class; }
    static HRESULT Register();

    // state management
    void SetState(UINT uNewState) { _nAppState = uNewState; }
    UINT GetState() { return _nAppState; }
    BOOL IsPreStatusLock() { return _fPreStatusLock; }

    void EnterPreStatusMode(BOOL fLock);
    void EnterLogonMode(BOOL fUnLock);
    void EnterPostStatusMode();
    void EnterHideMode();
    void EnterDoneMode();
    void EnterSecurityOptionsMode(LC::LogonUISecurityOptions options, WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion);
	HRESULT OnSecurityOptionSelected(LC::LogonUISecurityOptions SecurityOpt);
	HRESULT ConfirmEmergencyShutdown();
	HRESULT ShowLockedScreen();
	HRESULT DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, WORD flags,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
    LRESULT InteractiveLogonRequest(LPCWSTR pszUsername, LPCWSTR pszPassword);
    void NextFlagAnimate(DWORD dwFrame);

	void DisplaySerializationFailed(HSTRING caption, HSTRING message);

	void LoadSettings();

#ifdef GADGET_ENABLE_GDIPLUS
    // Animations / Effects
    HRESULT FxStartup();
#endif

    LogonFrame() {}
    virtual ~LogonFrame();
    HRESULT Initialize(HWND hParent, BOOL fDblBuffer, UINT nCreate);

#ifdef GADGET_ENABLE_GDIPLUS
    // Animations / Effects
    HRESULT FxFadeAccounts(EFadeDirection dir, float flCommonDelay = 0.0f);
    HRESULT FxLogUserOn(LogonAccount* pla);
    static void CALLBACK OnLoginCenterStage(GMA_ACTIONINFO* pmai);
#endif

    // References to key descendents
    DirectUI::Selector* _peAccountList;
    DirectUI::ScrollViewer* _peViewer;
    DirectUI::Element* _peRightPanel;
    DirectUI::Element* _peLeftPanel;
    DirectUI::Button* _pbPower;
    DirectUI::Button* _pbUndock;
    DirectUI::Element* _peHelp;
    DirectUI::Element* _peOptions;
    DirectUI::Element* _peMsgArea;
    DirectUI::Element* _peLogoArea;
    DirectUI::Element* _peDateTimeArea;
    DirectUI::Element* _peDateArea;
    DirectUI::Element* _peTimeArea;
	LogonAccount* _peLogonAccountFocused = NULL;
	Microsoft::WRL::ComPtr<LogonViewManager> m_consoleUIManager;
	Microsoft::WRL::ComPtr<IShutdownChoices> m_shutdownChoices;
	LC::LogonUIRequestReason m_currentReason;

	BOOL settingShouldShowDateAndTime = FALSE;
	BOOL settingShouldAnimateFlag = FALSE;

private:
    LogonAccount* InternalFindNamedUser(LPCWSTR pszUsername);

    static int _nDPI;
    DirectUI::DUIXmlParser* _pParser;

    BOOL _fListAvailable;
    BOOL _fPreStatusLock;
    HWND _hwndNotification;
    UINT _nStatusID;
    UINT _nAppState;
    DirectUI::NativeHWNDHost* _pnhh;
    DirectUI::Value* _pvHotList;
    DirectUI::Value* _pvList;
    HDC _hdcAnimation;
    HBITMAP _hbmpFlags;
    DWORD _dwFlagFrame;
    UINT _nColorDepth;
	WORD  _lastMinute;

	wistd::unique_ptr<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>>> m_SecurityOptionsCompletion;
};

extern LogonFrame* g_plf;
extern HANDLE g_rgH[3];
extern BOOL g_fNoAnimations;
