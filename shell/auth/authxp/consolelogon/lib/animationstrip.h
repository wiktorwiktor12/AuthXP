#pragma once

#include "pch.h"

#include "DirectUI/DirectUI.h"

/*class CDUIAnimationStrip : public DirectUI::Element
{
public:

	~CDUIAnimationStrip() override;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);

	static HRESULT Register();

	void OnPropertyChanged(const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld, DirectUI::Value* pvNew) override;
	void Paint(HDC hDC, const RECT* prcBounds, const RECT* prcInvalid, RECT* prcSkipBorder, RECT* prcSkipContent) override;
	HRESULT Start();
	HRESULT LoadFromResource(HINSTANCE a2, int resID, int cX);
	void OnHosted(DirectUI::Element* a2) override;
	void OnDestroy() override;

	static void CALLBACK RawActionProc(GMA_ACTIONINFO* a1);

	HANDLE m_action;
	HIMAGELIST m_imageList;
	DWORD m_imageCount;
	int m_index;
};*/
