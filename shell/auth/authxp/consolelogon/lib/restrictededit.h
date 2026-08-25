#pragma once

#include "pch.h"
#include "DirectUI/DirectUI.h"

struct CapsLockToggleEvent : DirectUI::Event
{
	bool bFieldFocused = false;
};

class CDUIRestrictedEdit : public DirectUI::Edit
	, public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>
			, WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind>
		>
{
public:

	CDUIRestrictedEdit();
	CDUIRestrictedEdit(const CDUIRestrictedEdit& other) = delete;
	~CDUIRestrictedEdit() override;

	CDUIRestrictedEdit& operator=(const CDUIRestrictedEdit&) = delete;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);

	static HRESULT Register();

	void OnPropertyChanged(const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld, DirectUI::Value* pvNew) override;

	static LRESULT CALLBACK _sSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

	void SetKeyFocus() override;

	HWND CreateHWND(HWND parentHwnd) override;

	void OnInput(DirectUI::InputEvent* inputEvent) override;

	HRESULT Advise(LCPD::ICredentialField* dataSource);
	HRESULT UnAdvise();

	virtual void OnDestroy() override;

	//~ Begin WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface
	STDMETHODIMP Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args) override;
	//~ End WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface

	HWND m_hwnd;
	const wchar_t* m_hintText = L"";
	DWORD m_maxTextLength;
	LCPD::CredProvScenario m_scenario;
	bool m_bIsCapslockPressed;

	Microsoft::WRL::ComPtr<LCPD::ICredentialField> m_fieldData;
	//int m_index;
	class LogonAccount* m_owningElement;

	static UID s_CapsLockWarning;

private:
	void _CheckCapsLock();
	bool _IsPasswordField();

	EventRegistrationToken m_token;
};
