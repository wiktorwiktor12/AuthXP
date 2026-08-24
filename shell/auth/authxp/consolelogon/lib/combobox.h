#pragma once

#include "pch.h"
#include "DirectUI/DirectUI.h"

struct SelectionIndexChangeEvent : DirectUI::Event
{
	int oldIndex;
	int newIndex;
};

class CDUIComboBox : public DirectUI::Combobox
	, public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>
			, WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind>
			, WFC::VectorChangedEventHandler<HSTRING>
		>
{
public:
	CDUIComboBox();
	CDUIComboBox(const CDUIComboBox& other) = delete;
	~CDUIComboBox() override;

	CDUIComboBox& operator=(const CDUIComboBox&) = delete;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);

	static HRESULT Register();

	HRESULT Advise(LCPD::ICredentialField* dataSource);
	HRESULT UnAdvise();

	virtual void OnDestroy() override;
	void OnEvent(DirectUI::Event* pEvent) override;

	//~ Begin WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface
	HRESULT STDMETHODCALLTYPE Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args) override;
	//~ End WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface

	//~ Begin WFC::VectorChangedEventHandler<HSTRING> Interface
    STDMETHODIMP Invoke(WFC::IObservableVector<HSTRING>* sender, WFC::IVectorChangedEventArgs* args) override;
    //~ End WFC::VectorChangedEventHandler<HSTRING> Interface

	HRESULT SetSelectionEx(int newSelection);
	int AddStringEx(const wchar_t* String);

	void ClearCombobox();
	HRESULT Build();
	HRESULT Rebuild();

	void OnHosted(DirectUI::Element* peNewHost) override;

	CCoSimpleArray<const wchar_t*> m_stringArray; //@mod, use CCoSimpleArray instead of hdpa
	int m_selection;

	//int m_index;
	class LogonAccount* m_owningElement;

private:
	EventRegistrationToken m_token;
	EventRegistrationToken m_itemsChangedToken;
	Microsoft::WRL::ComPtr<LCPD::ICredentialField> m_FieldInfo;
};
