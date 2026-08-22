#pragma once
#include "pch.h"

#include "logonviewmanager.h"
#include "DirectUI/DirectUI.h"


class LogonAccountList : public DirectUI::Selector
{
public:
	static HRESULT Create(Element* pParent, DWORD* pdwDeferCookie,OUT Element** ppElement);  // Required for ClassInfo

	// System events
	virtual void OnPropertyChanged(const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld, DirectUI::Value* pvNew) override;

	// ClassInfo accessors (static and virtual instance-based)
	static DirectUI::IClassInfo* Class;
	virtual DirectUI::IClassInfo* GetClassInfo() { return Class; }
	static HRESULT Register();

	LogonAccountList() {}
	virtual ~LogonAccountList() {}
	HRESULT Initialize(Element* pParent, DWORD* pdwDeferCookie) { return Element::Initialize(0,pParent,pdwDeferCookie); }

#ifdef GADGET_ENABLE_GDIPLUS
	// Animations / Effects
	HRESULT FxMouseWithin(EFadeDirection dir);
#endif
};
