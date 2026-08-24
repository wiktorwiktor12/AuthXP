#include "pch.h"
#include "restrictededit.h"
#include <windowsx.h>

#include "logonaccount.h"
#include "usertileelement.h"

static BYTE _uidCDUIRestrictedEditCapsLockWarning = 0;
UID CDUIRestrictedEdit::s_CapsLockWarning = UID(&_uidCDUIRestrictedEditCapsLockWarning);

DirectUI::IClassInfo* CDUIRestrictedEdit::Class = nullptr;

CDUIRestrictedEdit::CDUIRestrictedEdit()
{
}

CDUIRestrictedEdit::~CDUIRestrictedEdit()
{
}

DirectUI::IClassInfo* CDUIRestrictedEdit::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* CDUIRestrictedEdit::GetClassInfoPtr()
{
	return Class;
}

HRESULT CDUIRestrictedEdit::Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie,
	DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<CDUIRestrictedEdit, int>(3, pParent, pdwDeferCookie, ppElement);
}

HRESULT CDUIRestrictedEdit::Register()
{
	return DirectUI::ClassInfo<CDUIRestrictedEdit, DirectUI::Edit>::RegisterGlobal(HINST_THISCOMPONENT, L"RestrictedEdit", 0, 0);
}

void CDUIRestrictedEdit::OnPropertyChanged(const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld,
	DirectUI::Value* pvNew)
{
	if (!GetVisible())
		return DirectUI::Edit::OnPropertyChanged(ppi,iIndex,pvOld,pvNew);

	if (ppi == DirectUI::Element::KeyWithinProp() && (ppi->fFlags & 3) == iIndex)
	{
		_CheckCapsLock();
	}
	else if (ppi == DirectUI::Element::ContentProp() && (ppi->fFlags & 3) == iIndex)
	{
		DirectUI::Value* contentValue;
		const wchar_t* content = GetContentString(&contentValue);
		if (content && *content)
			SetAccValue(content);
		else
		{
			RemoveLocalValue(DirectUI::Element::AccValueProp);
		}

		//@Mod, dont need this, this is actually for credui, not logonui
		/*if (m_scenario == LCPD::CredProvScenario_CredUI)
		{
			DirectUI::Value* ClassValue;
			const wchar_t* Class = GetClass(&ClassValue);
			if (Class && _wcsicmp(Class, L"PasswordEdit") == 0)
			{
				WCHAR buf[256];
				HRESULT hr = GetEncodedContentString(buf,256);
				if (SUCCEEDED(hr))
					content = buf;
			}
			ClassValue->Release();

			//here i would set the datasource's value to content
		}*/
		contentValue->Release();
	}
	return DirectUI::Edit::OnPropertyChanged(ppi, iIndex, pvOld, pvNew);
}

LRESULT CDUIRestrictedEdit::_sSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData)
{
	CDUIRestrictedEdit* edit = reinterpret_cast<CDUIRestrictedEdit*>(uIdSubclass);
	if (uMsg > WM_COPY)
	{
		if (uMsg == WM_PASTE)
		{
			if (edit->m_scenario == LCPD::CredProvScenario_CredUI)
				return DefSubclassProc(hWnd,uMsg,wParam,lParam);

			return 0;
		}

		if (uMsg > WM_UNDO && (uMsg <= EM_GETCUEBANNER || uMsg > EM_HIDEBALLOONTIP))
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);

		return 0;
	}
	switch (uMsg)
	{
	case WM_DESTROY:
		RemoveWindowSubclass(hWnd, (SUBCLASSPROC)CDUIRestrictedEdit::_sSubclassProc, dwRefData);
		edit->m_hwnd = 0;
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	case WM_SETTEXT:
		//do set text in datasource here

			return DefSubclassProc(hWnd, uMsg, wParam, lParam);

	case WM_CONTEXTMENU:
		if (edit->m_scenario == LCPD::CredProvScenario_CredUI)
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);

		return 0;
	}
	if (uMsg != WM_GETDLGCODE)
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);

	return DefSubclassProc(hWnd, WM_GETDLGCODE, wParam, lParam) | 2;
}

void CDUIRestrictedEdit::SetKeyFocus()
{
	DirectUI::Edit::SetKeyFocus();
	//TODO:
	if (false ) // CKeyboardNavigationTracker::s_bInKeyboardNavigation
		SendMessageW(m_hwnd, EM_SETSEL, 0, -1);
}

HWND CDUIRestrictedEdit::CreateHWND(HWND parentHwnd)
{
	HWND newHwnd = DirectUI::Edit::CreateHWND(parentHwnd, false);
	if (newHwnd)
	{
		if (m_hintText)
			Edit_SetCueBannerTextFocused(newHwnd,m_hintText, true);

		Edit_LimitText(newHwnd, m_maxTextLength);

		if (SetWindowSubclass(newHwnd, CDUIRestrictedEdit::_sSubclassProc, (UINT_PTR)this, 0))
			m_hwnd = newHwnd;

		bool bIsPasswordEdit = false;

		DirectUI::Value* classValue;
		const wchar_t* Class = GetClass(&classValue);
		if (Class && _wcsicmp(Class, L"PasswordEdit") == 0)
			bIsPasswordEdit = true;

		if (bIsPasswordEdit)
			SetPasswordCharacter(9679);

		classValue->Release();
	}
	return newHwnd;
}

void CDUIRestrictedEdit::OnInput(DirectUI::InputEvent* inputEvent)
{
	DirectUI::Edit::OnInput(inputEvent);

	if (inputEvent->nDevice == DirectUI::GINPUT_KEYBOARD)
	{
		auto keyboardEvent = reinterpret_cast<DirectUI::KeyboardEvent*>(inputEvent);

		if (keyboardEvent->nStage == DirectUI::GMF_BUBBLED && inputEvent->nCode == DirectUI::GMOUSE_UP && keyboardEvent->ch != VK_RETURN// VK_RETURN
		|| inputEvent->nCode == DirectUI::GMOUSE_MOVE && keyboardEvent->ch == VK_DELETE// VK_DELETE
		|| inputEvent->nCode == DirectUI::GMOUSE_UP && keyboardEvent->ch != VK_TAB )
		{
			WCHAR inputText[256];
			inputText[0] = '\0';

			DirectUI::Value* classVal;
			auto editclass = GetClass(&classVal);
			if (editclass && wcscmp(editclass,L"PasswordEdit") == 0)
			{
				GetEncodedContentString(inputText,256);
			}
			else
			{
				DirectUI::Value* contVal;
				auto ContentString = GetContentString(&contVal);

				if (ContentString && *ContentString)
					wcscpy_s(inputText,ContentString);

				contVal->Release();
			}
			classVal->Release();

			if (m_fieldData.Get())
			{
				Microsoft::WRL::ComPtr<LCPD::ICredentialEditField> editField;
				if (SUCCEEDED(m_fieldData->QueryInterface(IID_PPV_ARGS(&editField))))
				{
					Microsoft::WRL::Wrappers::HString content;
					content.Set(inputText);

					LOG_IF_FAILED(editField->put_Content(content));

					LOG_HR_MSG(E_FAIL, "Put content %s",inputText);
				}
				else
				{
					LOG_HR(E_FAIL, "Failed to get editField");
				}
			}
		}

		if (keyboardEvent->nStage)
			return;

		if (keyboardEvent->nCode == DirectUI::GMOUSE_UP)
		{
			keyboardEvent->fHandled = keyboardEvent->ch == VK_RETURN;
			if (keyboardEvent->ch == VK_RETURN && m_owningElement->_tileData.Get())
			{
				/*if (m_owningElement->m_containersArray[m_index]->m_SubmitButton)
					LOG_IF_FAILED(m_owningElement->m_dataSourceCredential->Submit());
				else
				{
					//find the next edit field (previous in array)

					if ((m_index - 1) < 0 || (m_index - 1) >= m_owningElement->fieldsArray.GetSize())
						return;

					CFieldWrapper* nextField;
					if (SUCCEEDED(m_owningElement->fieldsArray.GetAt(m_index - 1,nextField)))
					{
						LCPD::CredentialFieldKind kind = LCPD::CredentialFieldKind_StaticText;
						if (nextField->m_dataSourceCredentialField)
						{
							LOG_IF_FAILED(nextField->m_dataSourceCredentialField->get_Kind(&kind));
						}

						//if (kind != LCPD::CredentialFieldKind_EditText)
						//	return;

						auto nextEdit = m_owningElement->m_elementsArray[m_index - 1];
						if (nextEdit)
							nextEdit->SetKeyFocus();
					}
				}*/
			}

			return;
		}

		WCHAR chr;

		if (keyboardEvent->nCode)
		{
			if (keyboardEvent->nCode != DirectUI::GMOUSE_DOWN)
				return;
			chr = VK_MENU;
		}
		else
		{
			chr = VK_CAPITAL;
		}
		if (chr == keyboardEvent->ch)
			_CheckCapsLock();
	}
	else if (inputEvent->nDevice == DirectUI::GINPUT_MOUSE)
	{
		auto mouseEvent = reinterpret_cast<DirectUI::MouseEvent*>(inputEvent);
		if (mouseEvent->bButton == 1 || mouseEvent->bButton == 2)
			SetKeyFocus();
	}
}

HRESULT CDUIRestrictedEdit::Advise(LCPD::ICredentialField* dataSource)
{
	m_fieldData = dataSource;

	RETURN_IF_FAILED(m_fieldData->add_FieldChanged(this, &m_token)); // 19
	return S_OK;
}

HRESULT CDUIRestrictedEdit::UnAdvise()
{
	if (m_fieldData)
	{
		RETURN_IF_FAILED(m_fieldData->remove_FieldChanged(m_token)); // 25

		m_fieldData.Reset();
	}

	return S_OK;
}

void CDUIRestrictedEdit::OnDestroy()
{
	Edit::OnDestroy();
	UnAdvise();
}

HRESULT CDUIRestrictedEdit::Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args)
{
	LOG_HR_MSG(E_FAIL,"CDUIRestrictedEdit::Invoke\n");
	if (m_owningElement&& args == LCPD::CredentialFieldChangeKind_State)
	{
		m_owningElement->SetFieldInitialVisibility(m_fieldData,GetParent());
		//m_owningElement->SetFieldInitialVisibility(m_owningElement->m_containersArray[m_index],fieldData);
		LOG_HR_MSG(E_FAIL,"CDUIRestrictedEdit::Invoke SetFieldVisibility\n");
	}

	return S_OK;
}

void CDUIRestrictedEdit::_CheckCapsLock()
{
	bool bIsPasswordEdit = false;

	DirectUI::Value* classValue;
	const wchar_t* Class = GetClass(&classValue);
	if (Class && wcscmp(Class, L"PasswordEdit") == 0)
		bIsPasswordEdit = true;

	CapsLockToggleEvent event;
	event.uidType = CDUIRestrictedEdit::s_CapsLockWarning;
	event.peTarget = this;
	event.nStage = DirectUI::GMF_EVENT;
	event.bFieldFocused = GetKeyFocused() && bIsPasswordEdit;

	FireEvent(&event,true,false);

	classValue->Release();
}

bool CDUIRestrictedEdit::_IsPasswordField()
{
	bool bIsPasswordEdit = false;
	DirectUI::Value* classValue;
	const wchar_t* Class = GetClass(&classValue);
	if (Class && wcscmp(Class, L"PasswordEdit") == 0)
		bIsPasswordEdit = true;
	classValue->Release();

	return bIsPasswordEdit;
}
