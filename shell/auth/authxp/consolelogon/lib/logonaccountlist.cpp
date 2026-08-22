#include "pch.h"
#include "logonaccountlist.h"

HRESULT LogonAccountList::Create(Element* pParent, DWORD* pdwDeferCookie, OUT Element** ppElement)
{
	*ppElement = NULL;

	LogonAccountList* plal = DirectUI::HNew<LogonAccountList>();
	if (!plal)
		return E_OUTOFMEMORY;

	HRESULT hr = plal->Initialize(pParent,pdwDeferCookie);
	if (FAILED(hr))
	{
		plal->Destroy();
		return hr;
	}

	*ppElement = plal;

	return S_OK;
}

void LogonAccountList::OnPropertyChanged(const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld,
	DirectUI::Value* pvNew)
{
#ifdef GADGET_ENABLE_GDIPLUS
	if (IsProp(MouseWithin))
	{
		if (pvNew->GetBool())
			FxMouseWithin(fdIn);
		else
			FxMouseWithin(fdOut);
	}
#endif // GADGET_ENABLE_GDIPLUS

	Selector::OnPropertyChanged(ppi, iIndex, pvOld, pvNew);
}

DirectUI::IClassInfo* LogonAccountList::Class = NULL;
HRESULT LogonAccountList::Register()
{
	return DirectUI::ClassInfo<LogonAccountList, Selector>::Register(L"LogonAccountList", NULL, 0);
}
