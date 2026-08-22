#pragma once
#include "pch.h"
#include "DirectUI/DirectUI.h"
#include "duiutil.h"

class LogonAccount : public DirectUI::Button
{
public:
    static HRESULT Create(Element* pParent, DWORD* pdwDeferCookie,OUT Element** ppElement);  // Required for ClassInfo

    // Generic events
    virtual void OnEvent(DirectUI::Event* pEvent) override;

    // System events
    virtual void OnInput(DirectUI::InputEvent* pEvent) override;
    virtual void OnPropertyChanged(const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld, DirectUI::Value* pvNew) override;

    // Account Callbacks
    void OnAuthenticatedUser();
    BOOL OnAuthenticateUser(LPCWSTR pszInPassword = NULL);
    void OnHintSelect();
    void OnStatusSelect(UINT nLine);
    HRESULT OnTreeReady(LPCWSTR pszPicture, BOOL fPicRes, LPCWSTR pszName, LPCWSTR pszUsername, LPCWSTR pszHint, BOOL fPwdNeeded, BOOL fLoggedOn, HINSTANCE hInst);

    // Operations
    void InitCredPanel(Element* pePwdPanel, DirectUI::Edit* pePwdEdit, Button* pbPwdInfo, Element* peKbdIcon) { _pePwdPanel = pePwdPanel; _pePwdEdit = pePwdEdit; _pbPwdInfo = pbPwdInfo; _peKbdIcon = peKbdIcon; }
    HRESULT CreateCredPanelElements();
    HRESULT CreateField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement);
    HRESULT _CreateCommandLinkField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement);
    HRESULT _CreateEditField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement);
    HRESULT _CreateCheckboxField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement);
    HRESULT _CreateComboBoxField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement);
    HRESULT _CreateSubmitButton(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement);
    HRESULT SetFieldInitialVisibility(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element* fieldElement);
    HRESULT SetFieldVisibility(DirectUI::Element* fieldElement, bool bIsVisible);
    HRESULT InsertCredPanel();
    HRESULT RemoveCredPanel();
    void InsertStatus(UINT nLine) { _pbStatus[nLine]->SetLayoutPos(DirectUI::BLP_Top); }
    void RemoveStatus(UINT nLine) { _pbStatus[nLine]->SetLayoutPos(DirectUI::LP_None); }
    void HideStatus(UINT nLine) { _pbStatus[nLine]->SetVisible(false); }
    void ShowStatus(UINT nLine) { _pbStatus[nLine]->SetVisible(true); }
    void SetStatus(UINT nLine, LPCWSTR psz);
    void DisableStatus(UINT nLine) { _pbStatus[nLine]->SetEnabled(false); }
    void ShowPasswordIncorrectMessage();
    void UpdateNotifications(BOOL fUpdateEverything);
    void AppRunningTip();
    void UnreadMailTip();
    BOOL IsPasswordBlank();

#ifdef GADGET_ENABLE_GDIPLUS
    void ShowEdit();
    void HideEdit();
#endif


    // Cached atoms for quicker identification
    static ATOM idPwdGo;
    static ATOM idPwdInfo;

    // Property definitions
    static DirectUI::PropertyInfo* LogonStateProp;

    // Quick property accessors
    int     GetLogonState()           DUIQuickGetter(int, GetInt(), LogonState, Specified)
        HRESULT SetLogonState(int v)  DUIQuickSetter(CreateInt(v), LogonState)
        void    SetRunningApps(DWORD dwRunningApps) { _dwRunningApps = dwRunningApps; }
    void    SetUnreadMail(DWORD dwUnreadMail) { _dwUnreadMail = dwUnreadMail; }
    LPCWSTR GetUsername() { return _pvUsername->GetString(); }
    static  LogonAccount* GetCandidate() { return _peCandidate; }
    static  void ClearCandidate() { _peCandidate = NULL; }
    void SetKeyboardIcon(HICON hIcon);
    // ClassInfo accessors (static and virtual instance-based)
    static DirectUI::IClassInfo* Class;
    virtual DirectUI::IClassInfo* GetClassInfo() { return Class; }
    static HRESULT Register();

#ifdef GADGET_ENABLE_GDIPLUS
    // Animations / Effects
    HRESULT FxLogUserOn();
#endif

    LogonAccount() {}
    virtual ~LogonAccount();
    HRESULT Initialize(Element* pParent, DWORD* pdwDeferCookie);

#ifdef GADGET_ENABLE_GDIPLUS
    // Animations / Effects
    HRESULT FxMouseWithin(EFadeDirection dir);
#endif

    // References to key descendents
    Button* _pbStatus[2];

    Element* _pePwdPanel;
    DirectUI::Edit* _pePwdEdit;
    Button* _pbPwdInfo;
    Element* _peKbdIcon;
    static LogonAccount* _peCandidate;

    DirectUI::Value* _pvUsername;
    DirectUI::Value* _pvHint;
    BOOL _fPwdNeeded;
    BOOL _fLoggedOn;
    BOOL _fHasPwdPanel;
    DWORD _dwUnreadMail;
    DWORD _dwRunningApps;

    DirectUI::DUIXmlParser* _pParser = nullptr;

    Microsoft::WRL::ComPtr<LCPD::ICredential> _tileData;
};
