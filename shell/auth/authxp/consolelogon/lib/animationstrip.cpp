#include "pch.h"
#include "animationstrip.h"

/*DirectUI::IClassInfo* CDUIAnimationStrip::Class = nullptr;

CDUIAnimationStrip::~CDUIAnimationStrip()
{
	if (m_imageList)
	    ImageList_Destroy(m_imageList);

	DirectUI::Element::~Element();
}

DirectUI::IClassInfo* CDUIAnimationStrip::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* CDUIAnimationStrip::GetClassInfoPtr()
{
	return Class;
}

HRESULT CDUIAnimationStrip::Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie,
	DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<CDUIAnimationStrip, int>(0, pParent, pdwDeferCookie, ppElement);
}

HRESULT CDUIAnimationStrip::Register()
{
	return DirectUI::ClassInfo<CDUIAnimationStrip, DirectUI::Element>::RegisterGlobal(HINST_THISCOMPONENT, L"DUIAnimationStrip", nullptr, 0);
}

void CDUIAnimationStrip::OnPropertyChanged(const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld,
	DirectUI::Value* pvNew)
{
	if (m_imageList
		&& ppi == DirectUI::Element::VisibleProp()
		&& ppi->pData->_iIndex == iIndex)
	{
		if (pvNew->GetBool())
		{
			Start();
		}
		else if (m_action)
		{
			DeleteHandle(m_action);
			m_action = nullptr;
		}
	}
	DirectUI::Element::OnPropertyChanged(ppi, iIndex, pvOld, pvNew);
}

int g_fHighDPIAware = 0;
int g_fHighDPI = 0;
int g_iLPX = -1;
int g_iLPY = -1;
void InitDPI()
{
	int v0 = IsProcessDPIAware();
	if (g_iLPX == -1 || g_fHighDPIAware != v0)
	{
		g_fHighDPIAware = v0;
		HDC DC = GetDC(0LL);
		if (DC)
		{
			g_iLPX = GetDeviceCaps(DC, 88);
			int DeviceCaps = GetDeviceCaps(DC, 90);
			bool v2 = g_iLPX != 96;
			g_fHighDPI = v2;
			g_iLPY = DeviceCaps;
			ReleaseDC(nullptr, DC);
		}
	}
}

void __fastcall SHLogicalToPhysicalDPI(int* a1, int* a2)
{
	InitDPI();
	if (a1)
		*a1 = MulDiv(*a1, g_iLPX, 96);
	if (a2)
		*a2 = MulDiv(*a2, g_iLPY, 96);
}

void CDUIAnimationStrip::Paint(HDC hDC, const RECT* prcBounds, const RECT* prcInvalid, RECT* prcSkipBorder,
	RECT* prcSkipContent)
{
	if (!m_imageList)
	{
		int resID;

		int cx_1 = 20;
		SHLogicalToPhysicalDPI(&cx_1, nullptr);
		int cx = cx_1;
		if (cx_1 == 20)
		{
			resID = IDB_SPINNER3;
		}
		else if (cx_1 == 25)
		{
			resID = IDB_SPINNER4;
		}
		else
		{
			resID = IDB_SPINNER5;
			if (cx_1 != 30)
				cx = 30;
			cx_1 = cx;
		}
		LoadFromResource(HINST_THISCOMPONENT, resID, cx);
	}
	if (m_imageList)
	{
		LONG bottom = 0;
		LONG right = 0;
		LONG left = 0;
		LONG top = 0;
		if (HasPadding())
		{
			DirectUI::Value* Value = DirectUI::Element::GetValue(PaddingProp, 2, nullptr);
			RECT rect = *Value->GetRect();
			Value->Release();

			bottom = rect.bottom;
			right = rect.right;
			top = rect.top;
			left = rect.left;
		}

		int dx = prcBounds->right - prcBounds->left - right - left;
		int dy = prcBounds->bottom - prcBounds->top - bottom - top;
		if (dx > 0 && dy > 0)
			ImageList_DrawEx(
				m_imageList,
				m_index,
				hDC,
				left + prcBounds->left,
				prcBounds->top + top,
				dx,
				dy,
				0xFFFFFFFF,
				0xFFFFFFFF,
				0x2020u);
	}
}

HRESULT CDUIAnimationStrip::Start()
{
	int AnimationsEnabled = 1;

	SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &AnimationsEnabled, 0);
	if (AnimationsEnabled && !m_action && m_imageList)
	{
		GMA_ACTION v3;
		v3.cRepeat = -1;
		v3.cbSize = 40;
		v3.flDelay = 0.0;
		v3.flDuration = 10000.0f;
		v3.flPeriod = 0.0;
		v3.dwPause = 33;
		v3.pvData = this;
		v3.pfnProc = RawActionProc;
		m_action = CreateAction(&v3);
	}
	return m_action == nullptr ? E_FAIL : 0;
}

HRESULT CDUIAnimationStrip::LoadFromResource(HINSTANCE hInstance, int resID, int cX)
{
	if (hInstance && resID && cX > 0)
	{
		if (m_imageList)
			ImageList_Destroy(m_imageList);

		m_imageList = ImageList_LoadImageW(hInstance, MAKEINTRESOURCEW(resID), cX, 0, 0xFF000000, 0, 0x2000u);
		if (m_imageList)
		{
			m_imageCount = ImageList_GetImageCount(m_imageList);
			Start();
			return S_OK;
		}
		return E_OUTOFMEMORY;
	}
	return E_INVALIDARG;
}

void CDUIAnimationStrip::OnHosted(DirectUI::Element* peNewRoot)
{
	DirectUI::Element::OnHosted(peNewRoot);
	if (GetVisible())
		Start();
	else if (m_action)
	{
		DeleteHandle(m_action);
		m_action = nullptr;
	}
}

void CDUIAnimationStrip::OnDestroy()
{
	if (m_action)
	{
		DeleteHandle(m_action);
		m_action = nullptr;
	}
	DirectUI::Element::OnDestroy();
}

void CDUIAnimationStrip::RawActionProc(GMA_ACTIONINFO* a1)
{
	CDUIAnimationStrip* animStrip = (CDUIAnimationStrip*)a1->pvData;
	if (!a1->fFinished)
	{
		if (animStrip->m_imageList)
		{
			HGADGET gadget = animStrip->GetDisplayNode();
			animStrip->m_index = (animStrip->m_index + 1) % animStrip->m_imageCount;
			InvalidateGadget(gadget);
		}
	}
}*/
