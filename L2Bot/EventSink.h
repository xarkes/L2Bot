#pragma once

#define _WIN32_DCOM
#include <iostream>
using namespace std;
#include <comdef.h>
#include <Wbemidl.h>

class L2ProcessLister;

#pragma comment(lib, "wbemuuid.lib")

class EventSink : public IWbemObjectSink
{
private:
    LONG m_lRef;
    bool bDone;
    L2ProcessLister* QtProxy = nullptr;

public:
    EventSink() { bDone = false;  m_lRef = 0; }
    ~EventSink() { bDone = true; QtProxy = nullptr; }

    void SetProxy(L2ProcessLister*);

    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();
    virtual HRESULT
        STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

    virtual HRESULT STDMETHODCALLTYPE Indicate(
        LONG lObjectCount,
        IWbemClassObject __RPC_FAR* __RPC_FAR* apObjArray
    );

    virtual HRESULT STDMETHODCALLTYPE SetStatus(
        /* [in] */ LONG lFlags,
        /* [in] */ HRESULT hResult,
        /* [in] */ BSTR strParam,
        /* [in] */ IWbemClassObject __RPC_FAR* pObjParam
    );
};

class WMIEventListener
{
public:
    int StartListening();
    void StopListening();
    void SetProxy(L2ProcessLister*);

private:
    bool initialized = false;
    L2ProcessLister* proxy = nullptr;

    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    EventSink* pSink = nullptr;
    IUnsecuredApartment* pUnsecApp = nullptr;
    IWbemObjectSink* pStubSink = nullptr;
    IUnknown* pStubUnk = nullptr;
};