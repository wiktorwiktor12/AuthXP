#pragma once

#include "pch.h"
#include "DirectUI/DirectUI.h"
#include "usertileelement.h"

/*class UserList : public DirectUI::Element
{
public:

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);

	static HRESULT Register();

	HRESULT Configure(DirectUI::DUIXmlParser* parser);
	HRESULT AddTileFromData(const Microsoft::WRL::ComPtr<LCPD::ICredential>& credential, const Microsoft::WRL::ComPtr<LCPD::IUser> user, HSTRING userName);
	void DeselectAllTiles();
	void HideAllTiles();
	void FindAndSetKeyFocus();
	void EnableList();
	void DestroyAllTiles();

	CDUIUserTileElement* FindTileByCredential(const Microsoft::WRL::ComPtr<LCPD::ICredential>& credential);
	HRESULT ZoomTile(CDUIUserTileElement* userTile);
	HRESULT UnzoomList(CDUIUserTileElement* userTile);
	CDUIUserTileElement* GetZoomedTile();

private:
	void _SetUnzoomedWidth();
	void _ShowEnumeratedTilesWorker(int visibleIndex);

	static DirectUI::IClassInfo* s_pClassInfo;
public:
	DirectUI::DUIXmlParser* m_xmlParser;
	DirectUI::Element* m_UserListContainer;
	DirectUI::Selector* m_UserListSelector;
	LCPD::CredProvScenario m_scenario;

	bool m_bIsActive;

	friend class CLogonFrame7;
};*/
