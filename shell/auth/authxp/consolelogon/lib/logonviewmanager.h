#pragma once

#include "pch.h"

#include "consoleuimanager.h"

class DECLSPEC_UUID("0bd5f9b3-c467-4545-a2c7-354647461905") LogonViewManager final
	: public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>
		, ConsoleUIManager
		, WF::ITypedEventHandler<LCPD::CredProvDataModel*, LCPD::CredentialSerialization*>
		, WF::ITypedEventHandler<LCPD::CredProvDataModel*, LCPD::BioFeedbackState>
		, WF::ITypedEventHandler<LCPD::CredProvDataModel*, IInspectable*> // Changed in RS5. Was WF::ITypedEventHandler<IInspectable*, IInspectable*>
		, WFC::VectorChangedEventHandler<IInspectable*>
		, WFC::VectorChangedEventHandler<LCPD::Credential*>
		, WF::ITypedEventHandler<LCPD::ICredentialGroup*, LCPD::Credential*> // New in RS5
		, WF::ITypedEventHandler<LCPD::Credential*, IInspectable*> // Changed in RS5, now for add_WebDialogVisibilityChanged. Previously for add_SelectedCredentialChanged
		, Microsoft::WRL::FtmBase
	>
{
public:
	LogonViewManager();

	// ReSharper disable once CppHidingFunction
	HRESULT RuntimeClassInitialize();

	// 14361
	/*//~ Begin WF::ITypedEventHandler<LCPD::Credential*, IInspectable*> Interface
	STDMETHODIMP Invoke(LCPD::ICredential* sender, IInspectable* args) override;
	//~ End WF::ITypedEventHandler<LCPD::Credential*, IInspectable*> Interface*/

	//~ Begin WF::ITypedEventHandler<LCPD::ICredentialGroup*, LCPD::Credential*> Interface
	STDMETHODIMP Invoke(LCPD::ICredentialGroup* sender, LCPD::ICredential* args) override;
	//~ End WF::ITypedEventHandler<LCPD::ICredentialGroup*, LCPD::Credential*> Interface

	//~ Begin WFC::VectorChangedEventHandler<LCPD::Credential*> Interface
	STDMETHODIMP Invoke(WFC::IObservableVector<LCPD::Credential*>* sender, WFC::IVectorChangedEventArgs* args) override;
	//~ End WFC::VectorChangedEventHandler<LCPD::Credential*> Interface

	//~ Begin WFC::VectorChangedEventHandler<IInspectable*> Interface
	STDMETHODIMP Invoke(WFC::IObservableVector<IInspectable*>* sender, WFC::IVectorChangedEventArgs* args) override;
	//~ End WFC::VectorChangedEventHandler<IInspectable*> Interface

	//~ Begin WF::ITypedEventHandler<LCPD::CredProvDataModel*, IInspectable*> Interface
	STDMETHODIMP Invoke(LCPD::ICredProvDataModel* sender, IInspectable* args) override;
	//~ End WF::ITypedEventHandler<LCPD::CredProvDataModel*, IInspectable*> Interface

	//~ Begin WF::ITypedEventHandler<LCPD::CredProvDataModel*, LCPD::BioFeedbackState> Interface
	STDMETHODIMP Invoke(LCPD::ICredProvDataModel* sender, LCPD::BioFeedbackState args) override;
	//~ End WF::ITypedEventHandler<LCPD::CredProvDataModel*, LCPD::BioFeedbackState> Interface

	//~ Begin WF::ITypedEventHandler<LCPD::CredProvDataModel*, LCPD::CredentialSerialization*> Interface
	STDMETHODIMP Invoke(LCPD::ICredProvDataModel* sender, LCPD::ICredentialSerialization* args) override;
	//~ End WF::ITypedEventHandler<LCPD::CredProvDataModel*, LCPD::CredentialSerialization*> Interface

	//~ Begin WF::ITypedEventHandler<LCPD::Credential*, IInspectable*> Interface
	STDMETHODIMP Invoke(LCPD::ICredential* sender, IInspectable* args) override;
	//~ End WF::ITypedEventHandler<LCPD::Credential*, IInspectable*> Interface

	HRESULT SetContext(
		IInspectable* autoLogonManager, LC::IUserSettingManager* userSettingManager,
		LC::IRedirectionManager* redirectionManager, LCPD::IDisplayStateProvider* displayStateProvider,
		LC::IBioFeedbackListener* bioFeedbackListener);
	HRESULT Lock(
		LC::LogonUIRequestReason reason, BOOLEAN allowDirectUserSwitching, HSTRING unk,
		LC::IUnlockTrigger* unlockTrigger);
	HRESULT RequestCredentials(
		LC::LogonUIRequestReason reason, LC::LogonUIFlags flags, HSTRING unk,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IRequestCredentialsData>> completion);
	HRESULT ReportResult(
		LC::LogonUIRequestReason reason, NTSTATUS ntStatus, NTSTATUS ntSubStatus, HSTRING samCompatibleUserName,
		HSTRING displayName, HSTRING userSid,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IReportCredentialsData>> completion);
	HRESULT ClearCredentialState();
	HRESULT DisplayStatus(LC::LogonUIState state, HSTRING status, WI::AsyncDeferral<WI::CNoResult> completion);
	HRESULT DisplayMessage(
		LC::LogonMessageMode messageMode, UINT messageBoxFlags, HSTRING caption, HSTRING message,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
	HRESULT DisplayCredentialError(
		NTSTATUS ntsStatus, NTSTATUS ntsSubStatus, UINT messageBoxFlags, HSTRING caption, HSTRING message,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
	HRESULT ShowSecurityOptions(
		LC::LogonUISecurityOptions options,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion);
	HRESULT WebDialogDisplayed(LC::IWebDialogDismissTrigger* dismissTrigger);
	HRESULT Cleanup(WI::AsyncDeferral<WI::CNoResult> completion);

private:
	HRESULT SetContextUIThread(
		IInspectable* autoLogonManager, LC::IUserSettingManager* userSettingManager,
		LC::IRedirectionManager* redirectionManager, LCPD::IDisplayStateProvider* displayStateProvider,
		LC::IBioFeedbackListener* bioFeedbackListener);
	HRESULT LockUIThread(
		LC::LogonUIRequestReason reason, BOOLEAN allowDirectUserSwitching, HSTRING unk,
		LC::IUnlockTrigger* unlockTrigger);
	HRESULT RequestCredentialsUIThread(
		LC::LogonUIRequestReason reason, LC::LogonUIFlags flags, HSTRING unk,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IRequestCredentialsData>> completion);
	HRESULT ReportResultUIThread(
		LC::LogonUIRequestReason reason, NTSTATUS ntStatus, NTSTATUS ntSubStatus, HSTRING samCompatibleUserName,
		HSTRING displayName, HSTRING userSid,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IReportCredentialsData>> completion);
	HRESULT ShowSecurityOptionsUIThread(
		LC::LogonUISecurityOptions options,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion);
	HRESULT DisplayStatusUIThread(LC::LogonUIState state, HSTRING status, WI::AsyncDeferral<WI::CNoResult> completion);
	HRESULT DisplayMessageUIThread(
		LC::LogonMessageMode messageMode, UINT messageBoxFlags, HSTRING caption, HSTRING message,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
	HRESULT DisplayCredentialErrorUIThread(
		NTSTATUS ntsStatus, NTSTATUS ntsSubStatus, UINT messageBoxFlags, HSTRING caption, HSTRING message,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
	HRESULT ClearCredentialStateUIThread();
	HRESULT WebDialogDisplayedUIThread(LC::IWebDialogDismissTrigger* dismissTrigger);
	HRESULT CleanupUIThread(WI::AsyncDeferral<WI::CNoResult> completion);
	HRESULT ShowCredentialView();
	HRESULT ShowStatusView(HSTRING status);
	HRESULT ShowMessageView(
		HSTRING caption, HSTRING message, UINT messageBoxFlags,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
	HRESULT ShowMessageCredErrorView(
		HSTRING caption, HSTRING message, UINT messageBoxFlags,
		WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion);
	HRESULT ShowSerializationFailedView(HSTRING caption, HSTRING message);
	HRESULT StartCredProvsIfNecessary(LC::LogonUIRequestReason reason, BOOLEAN allowDirectUserSwitching, HSTRING unk);
	HRESULT OnCredProvInitComplete();

	enum class LogonView
	{
		None = 0,
		UserSelection = 1,
		CredProvSelection = 2,
		SelectedCredential = 3,
		Status = 4,
		Message = 5,
		ComboBox = 6,
		Locked = 7,
		SecurityOptions = 8,
		SerializationFailed = 9,
	};

	LogonView m_currentViewType;

	Microsoft::WRL::ComPtr<IInspectable> m_autoLogonManager;
	Microsoft::WRL::ComPtr<LC::IUserSettingManager> m_userSettingManager;
	Microsoft::WRL::ComPtr<LC::IRedirectionManager> m_redirectionManager;
	Microsoft::WRL::ComPtr<LCPD::IDisplayStateProvider> m_displayStateProvider;
	Microsoft::WRL::ComPtr<LC::IBioFeedbackListener> m_bioFeedbackListener;
	Microsoft::WRL::ComPtr<LCPD::ICredProvDataModel> m_credProvDataModel;
	Microsoft::WRL::ComPtr<LCPD::ICredentialGroup> m_selectedGroup;
	Microsoft::WRL::ComPtr<LCPD::ICredential> m_selectedCredential;
	EventRegistrationToken m_serializationCompleteToken;
	EventRegistrationToken m_bioFeedbackStateChangeToken;
	EventRegistrationToken m_usersChangedToken;
	EventRegistrationToken m_selectedUserChangeToken;
	EventRegistrationToken m_credentialsChangedToken;
	EventRegistrationToken m_selectedCredentialChangedToken;
	EventRegistrationToken m_webDialogVisibilityChangedToken;
	bool m_isCredentialResetRequired;
	bool m_credProvInitialized;
	bool m_showCredentialViewOnInitComplete;
	LC::LogonUIRequestReason m_currentReason;
	Microsoft::WRL::ComPtr<LC::IUnlockTrigger> m_unlockTrigger;
	Microsoft::WRL::ComPtr<LC::IWebDialogDismissTrigger> m_webDialogDismissTrigger;
	wistd::unique_ptr<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IRequestCredentialsData>>> m_requestCredentialsComplete;
	Microsoft::WRL::ComPtr<LCPD::IReportResultInfo> m_lastReportResultInfo;
	Microsoft::WRL::ComPtr<LCPD::ICredentialSerialization> m_cachedSerialization;
	Microsoft::WRL::ComPtr<IInputSwitchControl> m_inputSwitchControl;

	friend class CLogonFrame7;
	friend class LogonFrame;
	friend class LogonAccount;
};
