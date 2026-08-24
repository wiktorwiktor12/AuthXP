#pragma once

#include "pch.h"

#include "fieldcontainer.h"
#include "DirectUI/DirectUI.h"
#include "logoninterfaces.h"

//wrapper struct to wrap the field datasource so that you can insert fake fields such as for Locked text or the user name text
/*struct CFieldWrapper
{
	Microsoft::WRL::ComPtr<LCPD::ICredentialField> m_dataSourceCredentialField;

	// used when m_dataSourceCredentialField is null
	CoTaskMemNativeString m_label;
	LCPD::CredentialTextSize m_size;
	//

	bool m_isSelectorField;
};

class CDUIUserTileElement : public DirectUI::Button
{
public:

	~CDUIUserTileElement() override;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);

	static const DirectUI::PropertyInfo* WINAPI TileZoomedProp();
	static HRESULT Register();

	bool GetTileZoomed();
	HRESULT SetTileZoomed(bool v);
	HRESULT SetFieldInitialVisibility(DirectUI::Element* field, CFieldWrapper* fieldData);
	HRESULT SetFieldVisibility(int index, Microsoft::WRL::ComPtr<LCPD::ICredentialField> fieldData);


	void OnEvent(DirectUI::Event* pEvent) override;

	bool m_bTileZoomed;

	bool m_bHasMadeSelectedFields;

	LCPD::CredProvScenario m_scenario;
	Microsoft::WRL::ComPtr<LCPD::ICredential> m_dataSourceCredential;
	Microsoft::WRL::ComPtr<LCPD::IUser> m_dataSourceUser;
	DirectUI::Element** m_elementsArray;
	CDUIFieldContainer** m_containersArray;

	CCoSimpleArray<CFieldWrapper*> fieldsArray;

	class UserList* m_owningUserList;

	DirectUI::DUIXmlParser* m_xmlParser;

	DirectUI::Button* m_submitButton;
	DirectUI::Element* m_capsLockWarning;

private:

	HRESULT _CreateElementArrays();
	UINT _FindFieldInsertionIndex(int fieldIndex);
	HRESULT _AddFieldContainer(int index, DirectUI::Element* Parent, CDUIFieldContainer** a5);
	HRESULT _AddField(DirectUI::Element* a2, int index, DirectUI::Element* Parent, CDUIFieldContainer** OutContainer);

	HRESULT _CreateStringField(int index, DirectUI::Element* Parent, DirectUI::Element** outElement, CDUIFieldContainer** OutContainer); //
	HRESULT _CreateEditField(int index, DirectUI::Element* Parent, DirectUI::Element** outElement, CDUIFieldContainer** OutContainer); //
	HRESULT _CreateCommandLinkField(int index, DirectUI::Element* Parent, DirectUI::Element** outElement, CDUIFieldContainer** OutContainer); //
	HRESULT _CreateTileImageField(const wchar_t* pszLabel, Microsoft::WRL::ComPtr<LCPD::ICredentialImageField>& tileImageDataSource, DirectUI::Element** OutContainer);
	HRESULT _CreateSubmitButton(int index, DirectUI::Button** outButton, DirectUI::Element** outElement);
	HRESULT _CreateCheckboxField(int index, DirectUI::Element* Parent, DirectUI::Element** outElement, CDUIFieldContainer** OutContainer); //
	HRESULT _CreateComboBoxField(int index, DirectUI::Element* Parent, DirectUI::Element** outElement, CDUIFieldContainer** OutContainer);

	HRESULT _CreateFieldByIndex(int index);

	HRESULT _CreateFieldsForDeselected();
	HRESULT _CreateFieldsForSelected();

	friend class UserList;
	friend class CDUIZoomableElement;
	friend class CAdvisableButton;
};
*/
