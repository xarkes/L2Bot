#include "EventSink.h"
#include "L2ProcessLister.h"

ULONG EventSink::AddRef()
{
    return InterlockedIncrement(&m_lRef);
}

ULONG EventSink::Release()
{
    LONG lRef = InterlockedDecrement(&m_lRef);
    if (lRef == 0)
        delete this;
    return lRef;
}

HRESULT EventSink::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_IWbemObjectSink)
    {
        *ppv = (IWbemObjectSink*)this;
        AddRef();
        return WBEM_S_NO_ERROR;
    }
    else return E_NOINTERFACE;
}


HRESULT EventSink::Indicate(long lObjectCount,
    IWbemClassObject** apObjArray)
{
    HRESULT hres = S_OK;

    for (int i = 0; i < lObjectCount; i++)
    {
		// Debugging purposes
		// obj->GetObjectText(0, &pstrObjectText);
		VARIANT TargetInstance, ProcName, ProcID;
		HRESULT hr;
		hr = apObjArray[i]->Get(_bstr_t(L"TargetInstance"), 0, &TargetInstance, 0, 0);
		if (FAILED(hr)) {
			continue;
		}
		IWbemClassObject* obj = (IWbemClassObject*)TargetInstance.punkVal;
		hr = obj->Get(_bstr_t(L"Name"), 0, &ProcName, 0, 0);
		if (FAILED(hr)) {
			continue;
		}
		hr = obj->Get(_bstr_t(L"ProcessID"), 0, &ProcID, 0, 0);
		if (FAILED(hr)) {
			continue;
		}

		// Communicate the process information to the main thread
		if (QtProxy) {
			QtProxy->sendMessage(ProcName.bstrVal, ProcID.uintVal);
		}
    }

    return WBEM_S_NO_ERROR;
}

HRESULT EventSink::SetStatus(
    /* [in] */ LONG lFlags,
    /* [in] */ HRESULT hResult,
    /* [in] */ BSTR strParam,
    /* [in] */ IWbemClassObject __RPC_FAR* pObjParam
)
{
    if (lFlags == WBEM_STATUS_COMPLETE)
    {
        printf("Call complete. hResult = 0x%X\n", hResult);
    }
    else if (lFlags == WBEM_STATUS_PROGRESS)
    {
        printf("Call in progress.\n");
    }

    return WBEM_S_NO_ERROR;
}

void EventSink::SetProxy(L2ProcessLister* proxy)
{
	this->QtProxy = proxy;
}

int WMIEventListener::StartListening()
{
	HRESULT hres;

	// 1. Initialize COM
	hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres)) {
		return 0x73811;
	}

	// 2. Set general COM security levels
	hres = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
	if (FAILED(hres)) {
		CoUninitialize();
		return 0x73812;
	}

	// 3. Obtain the initial locator to WMI
	hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
	if (FAILED(hres)) {
		CoUninitialize();
		return 0x73813;
	}

	// 4. Connect to WMI through the IWbemLocator::ConnectServer method
	hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
	if (FAILED(hres)) {
		pLoc->Release();
		CoUninitialize();
		return 0x73814;
	}

	// 5. Set security levels on the proxy
	hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
	if (FAILED(hres)) {
		pSvc->Release();
		pLoc->Release();
		CoUninitialize();
		return 0x73815;
	}

	// 6. Receive event notifications

	// Use an unsecured apartment for security
	hres = CoCreateInstance(CLSID_UnsecuredApartment, NULL, CLSCTX_LOCAL_SERVER, IID_IUnsecuredApartment, (void**)&pUnsecApp);
	pSink = new EventSink;
	pSink->AddRef();

	pUnsecApp->CreateObjectStub(pSink, &pStubUnk);

	pStubUnk->QueryInterface(IID_IWbemObjectSink, (void**)&pStubSink);

	// The ExecNotificationQueryAsync method will call
	// The EventQuery::Indicate method when an event occurs
	hres = pSvc->ExecNotificationQueryAsync(_bstr_t("WQL"),
		_bstr_t("SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process'"),
		WBEM_FLAG_SEND_STATUS,
		NULL,
		pStubSink);

	if (FAILED(hres)) {
		pSvc->Release();
		pLoc->Release();
		pUnsecApp->Release();
		pStubUnk->Release();
		pSink->Release();
		pStubSink->Release();
		CoUninitialize();
		return 0x73816;
	}

	initialized = true;
	return 0;
}

void WMIEventListener::StopListening()
{
	if (initialized) {
		pSvc->CancelAsyncCall(pStubSink);
		pSvc->Release();
		pLoc->Release();
		pUnsecApp->Release();
		pStubUnk->Release();
		pSink->Release();
		pStubSink->Release();
		CoUninitialize();
	}
}

void WMIEventListener::SetProxy(L2ProcessLister* proxy)
{
	pSink->SetProxy(proxy);
}
