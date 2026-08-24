#include "pch.h"
#include "combobox.h"

#include "logonaccount.h"
#include "usertileelement.h"

DirectUI::IClassInfo* CDUIComboBox::Class = nullptr;

CDUIComboBox::CDUIComboBox() : m_selection(-1)
{

}

CDUIComboBox::~CDUIComboBox()
{
	for (size_t i = 0; i < m_stringArray.GetSize(); ++i)
	{
		DirectUI::HFree((void*)m_stringArray[i]);
	}

	DirectUI::Combobox::~Combobox();
}

DirectUI::IClassInfo* CDUIComboBox::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* CDUIComboBox::GetClassInfoPtr()
{
	return Class;
}

HRESULT CDUIComboBox::Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<CDUIComboBox, int>(3, pParent, pdwDeferCookie, ppElement);
}

HRESULT CDUIComboBox::Register()
{
	return DirectUI::ClassInfo<CDUIComboBox, DirectUI::Combobox>::RegisterGlobal(HINST_THISCOMPONENT, L"DUIComboBox", nullptr, 0);
}

HRESULT CDUIComboBox::Advise(LCPD::ICredentialField* dataSource)
{
	m_FieldInfo = dataSource;

	RETURN_IF_FAILED(m_FieldInfo->add_FieldChanged(this, &m_token)); // 19

	Microsoft::WRL::ComPtr<LCPD::IComboBoxField> comboBoxField;
	RETURN_IF_FAILED(dataSource->QueryInterface(IID_PPV_ARGS(&comboBoxField)));

	Microsoft::WRL::ComPtr<WFC::IObservableVector<HSTRING>> observableItems;
	RETURN_IF_FAILED(comboBoxField->get_Items(&observableItems));

	RETURN_IF_FAILED(observableItems->add_VectorChanged(this, &m_itemsChangedToken));

	return S_OK;
}

HRESULT CDUIComboBox::UnAdvise()
{
	if (m_FieldInfo)
	{
		RETURN_IF_FAILED(m_FieldInfo->remove_FieldChanged(m_token)); // 25

		m_FieldInfo.Reset();
	}

	return S_OK;
}

void CDUIComboBox::OnDestroy()
{
	Combobox::OnDestroy();
	UnAdvise();
}

void CDUIComboBox::OnEvent(DirectUI::Event* pEvent)
{
	if (pEvent->nStage == DirectUI::GMF_DIRECT && pEvent->uidType == DirectUI::Combobox::SelectionChange())
	{
		SelectionIndexChangeEvent* selEvent = (SelectionIndexChangeEvent*)pEvent;
		if (m_FieldInfo.Get())
		{
			Microsoft::WRL::ComPtr<LCPD::IComboBoxField> comboBoxField;
			if (SUCCEEDED(m_FieldInfo->QueryInterface(IID_PPV_ARGS(&comboBoxField))))
			{
				LOG_IF_FAILED(comboBoxField->put_SelectedIndex(selEvent->newIndex));
			}
			else
			{
				LOG_HR_MSG(E_FAIL,"Failed to query comboboxfield");
			}
		}
	}

	Combobox::OnEvent(pEvent);
}

HRESULT CDUIComboBox::Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args)
{
	LOG_HR_MSG(E_FAIL,"CDUIComboBox::Invoke\n");
	if (m_owningElement)
	{

		bool bShouldUpdateString = false;

		if (args == LCPD::CredentialFieldChangeKind_State)
		{
			bool bOldVisibility = GetVisible();
			m_owningElement->SetFieldInitialVisibility(m_FieldInfo,this);
			if (bOldVisibility != GetVisible())
				bShouldUpdateString = true;
		}
		else if (args == LCPD::CredentialFieldChangeKind_SetString || args == LCPD::CredentialFieldChangeKind_SetComboBoxSelected)
		{
			//m_owningElement->SetFieldVisibility(m_owningElement->m_containersArray[m_index],fieldData);
			bShouldUpdateString = true;
		}
		if (bShouldUpdateString)
		{
			RETURN_IF_FAILED(Rebuild());
		}
		//m_owningElement->SetFieldInitialVisibility(m_owningElement->m_containersArray[m_index],fieldData);
		//LOG_HR_MSG(E_FAIL,"CDUIComboBox::Invoke SetFieldInitialVisibility\n");
	}

	return S_OK;
}

HRESULT CDUIComboBox::Invoke(WFC::IObservableVector<HSTRING>* sender, WFC::IVectorChangedEventArgs* args)
{
	LOG_HR_MSG(E_FAIL,"CDUIComboBox::Invoke VectorChangedEvent\n");
	RETURN_IF_FAILED(Rebuild());
	return S_OK;
}

HRESULT CDUIComboBox::SetSelectionEx(int newSelection)
{
	LOG_HR_MSG(E_FAIL,"CDUIComboBox::SetSelectionEx %i\n",newSelection);
	if (GetHWND())
	{
		SetSelection(9999);
		return SetSelection(newSelection);
	}

	m_selection = newSelection;

	return S_OK;
}

/*HRESULT StringStringAllocCopy(const wchar_t* Src, const wchar_t** a2)
{
	const wchar_t* newString = (const wchar_t*)DirectUI::HAlloc((wcslen(Src)+1)*2);
	memcpy((void*)newString,Src, (wcslen(Src) + 1) * 2);
	*a2 = newString;
	return S_OK;
}*/

int CDUIComboBox::AddStringEx(const wchar_t* String)
{
	if (GetHWND())
		return AddString((unsigned short const*)String);

	const wchar_t* newString = nullptr;
	if (SUCCEEDED(StringStringAllocCopy(String, &newString)))
	{
		if (FAILED(m_stringArray.Add(newString)))
			return -1;

		return m_stringArray.GetSize() - 1;
	}

	return -1;
}

void CDUIComboBox::ClearCombobox()
{
	for (size_t i = 0; i < m_stringArray.GetSize(); ++i)
	{
		DirectUI::HFree((void*)m_stringArray[i]);
	}
	m_stringArray.RemoveAll();

	if (HWND hwnd = GetHWND())
		SendMessage(hwnd,CB_RESETCONTENT,0,0);
}

HRESULT CDUIComboBox::Build()
{
	Microsoft::WRL::ComPtr<LCPD::IComboBoxField> comboBoxField;
	RETURN_IF_FAILED(m_FieldInfo->QueryInterface(IID_PPV_ARGS(&comboBoxField)));

	Microsoft::WRL::ComPtr<WFC::IObservableVector<HSTRING>> observableItems;
	RETURN_IF_FAILED(comboBoxField->get_Items(&observableItems));

	Microsoft::WRL::ComPtr<WFC::IVector<HSTRING>> items;
	RETURN_IF_FAILED(observableItems.As<WFC::IVector<HSTRING>>(&items));

	UINT numItems;
	RETURN_IF_FAILED(items->get_Size(&numItems));
	LOG_HR_MSG(E_FAIL,"Creating checkbox field");
	for (int i = 0; i < numItems; ++i)
	{
		Microsoft::WRL::Wrappers::HString item;
		RETURN_IF_FAILED(items->GetAt(i,item.ReleaseAndGetAddressOf()));
		RETURN_HR_IF(E_FAIL, AddStringEx(item.GetRawBuffer(nullptr)) == -1);

		LOG_HR_MSG(E_FAIL,"checkbox field item %i %s\n",i, item.GetRawBuffer(nullptr));
	}

	int initialSelection;
	RETURN_IF_FAILED(comboBoxField->get_SelectedIndex(&initialSelection));

	RETURN_IF_FAILED(SetSelectionEx(initialSelection));

	return S_OK;
}

HRESULT CDUIComboBox::Rebuild()
{
	ClearCombobox();
	RETURN_IF_FAILED(Build());
	return S_OK;
}

void CDUIComboBox::OnHosted(DirectUI::Element* peNewHost)
{
	bool initiallyNullHWND = GetHWND() == nullptr;
	DirectUI::Combobox::OnHosted(peNewHost);

	if (initiallyNullHWND && GetHWND())
	{
		for (size_t i = 0; i < m_stringArray.GetSize(); ++i)
		{
			if (AddStringEx(m_stringArray[i]) == -1)
				return;
		}
	}

	m_stringArray.RemoveAll();

	if (m_selection != -1)
		SetSelectionEx(m_selection);
}
