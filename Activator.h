// ============================================================================
// Activator.h — Virtual Camera IMFActivate implementation
// Instantiates the VirtualCamMediaSource inside the Windows Frame Server.
// ============================================================================
#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A00000B
#endif

#include <windows.h>

#ifndef STATUS_WAIT_0
#define STATUS_WAIT_0 ((DWORD)0x00000000L)
#endif
#ifndef STATUS_ABANDONED_WAIT_0
#define STATUS_ABANDONED_WAIT_0 ((DWORD)0x00000080L)
#endif

#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include "MediaSource.h"
#include "Logger.h"

using namespace Microsoft::WRL;

class VirtualCamActivator :
    public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFActivate, IKsControl>
{
    ComPtr<IMFAttributes> _attrs;
    ComPtr<VirtualCamMediaSource> _source;

public:
    HRESULT RuntimeClassInitialize()
    {
        Log("VirtualCamActivator::RuntimeClassInitialize");
        return MFCreateAttributes(&_attrs, 8);
    }

    STDMETHODIMP ActivateObject(REFIID riid, void** ppv) override
    {
        Log("VirtualCamActivator::ActivateObject called");
        if (!ppv) return E_POINTER;
        
        if (!_source)
        {
            _source = Make<VirtualCamMediaSource>();
            if (!_source) return E_OUTOFMEMORY;
            
            HRESULT hr = _source->RuntimeClassInitialize();
            if (FAILED(hr)) { 
                _source.Reset(); 
                return hr; 
            }
        }
        return _source->QueryInterface(riid, ppv);
    }

    STDMETHODIMP ShutdownObject() override
    {
        if (_source) { 
            _source->Shutdown(); 
            _source.Reset(); 
        }
        return S_OK;
    }

    STDMETHODIMP DetachObject() override
    {
        _source.Reset();
        return S_OK;
    }

    // ------------------------------------------------------------------
    // IMFAttributes implementation (delegated)
    // ------------------------------------------------------------------
    STDMETHODIMP GetItem(REFGUID guidKey, PROPVARIANT* pValue) override { return _attrs->GetItem(guidKey, pValue); }
    STDMETHODIMP GetItemType(REFGUID guidKey, MF_ATTRIBUTE_TYPE* pType) override { return _attrs->GetItemType(guidKey, pType); }
    STDMETHODIMP CompareItem(REFGUID guidKey, REFPROPVARIANT Value, BOOL* pbResult) override { return _attrs->CompareItem(guidKey, Value, pbResult); }
    STDMETHODIMP Compare(IMFAttributes* pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL* pbResult) override { return _attrs->Compare(pTheirs, MatchType, pbResult); }
    STDMETHODIMP GetUINT32(REFGUID guidKey, UINT32* punValue) override { return _attrs->GetUINT32(guidKey, punValue); }
    STDMETHODIMP GetUINT64(REFGUID guidKey, UINT64* punValue) override { return _attrs->GetUINT64(guidKey, punValue); }
    STDMETHODIMP GetDouble(REFGUID guidKey, double* pfValue) override { return _attrs->GetDouble(guidKey, pfValue); }
    STDMETHODIMP GetGUID(REFGUID guidKey, GUID* pguidValue) override { return _attrs->GetGUID(guidKey, pguidValue); }
    STDMETHODIMP GetStringLength(REFGUID guidKey, UINT32* pcchLength) override { return _attrs->GetStringLength(guidKey, pcchLength); }
    STDMETHODIMP GetString(REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32* pcchLength) override { return _attrs->GetString(guidKey, pwszValue, cchBufSize, pcchLength); }
    STDMETHODIMP GetAllocatedString(REFGUID guidKey, LPWSTR* ppwszValue, UINT32* pcchLength) override { return _attrs->GetAllocatedString(guidKey, ppwszValue, pcchLength); }
    STDMETHODIMP GetBlobSize(REFGUID guidKey, UINT32* pcbBlobSize) override { return _attrs->GetBlobSize(guidKey, pcbBlobSize); }
    STDMETHODIMP GetBlob(REFGUID guidKey, UINT8* pBuf, UINT32 cbBufSize, UINT32* pcbBlobSize) override { return _attrs->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize); }
    STDMETHODIMP GetAllocatedBlob(REFGUID guidKey, UINT8** ppBuf, UINT32* pcbSize) override { return _attrs->GetAllocatedBlob(guidKey, ppBuf, pcbSize); }
    STDMETHODIMP GetUnknown(REFGUID guidKey, REFIID riid, LPVOID* ppv) override { return _attrs->GetUnknown(guidKey, riid, ppv); }
    STDMETHODIMP SetItem(REFGUID guidKey, REFPROPVARIANT Value) override { return _attrs->SetItem(guidKey, Value); }
    STDMETHODIMP DeleteItem(REFGUID guidKey) override { return _attrs->DeleteItem(guidKey); }
    STDMETHODIMP DeleteAllItems() override { return _attrs->DeleteAllItems(); }
    STDMETHODIMP SetUINT32(REFGUID guidKey, UINT32 unValue) override { return _attrs->SetUINT32(guidKey, unValue); }
    STDMETHODIMP SetUINT64(REFGUID guidKey, UINT64 unValue) override { return _attrs->SetUINT64(guidKey, unValue); }
    STDMETHODIMP SetDouble(REFGUID guidKey, double fValue) override { return _attrs->SetDouble(guidKey, fValue); }
    STDMETHODIMP SetGUID(REFGUID guidKey, REFGUID guidValue) override { return _attrs->SetGUID(guidKey, guidValue); }
    STDMETHODIMP SetString(REFGUID guidKey, LPCWSTR wszValue) override { return _attrs->SetString(guidKey, wszValue); }
    STDMETHODIMP SetBlob(REFGUID guidKey, const UINT8* pBuf, UINT32 cbBufSize) override { return _attrs->SetBlob(guidKey, pBuf, cbBufSize); }
    STDMETHODIMP SetUnknown(REFGUID guidKey, IUnknown* pUnknown) override { return _attrs->SetUnknown(guidKey, pUnknown); }
    STDMETHODIMP LockStore() override { return _attrs->LockStore(); }
    STDMETHODIMP UnlockStore() override { return _attrs->UnlockStore(); }
    STDMETHODIMP GetCount(UINT32* pcItems) override { return _attrs->GetCount(pcItems); }
    STDMETHODIMP GetItemByIndex(UINT32 unIndex, GUID* pguidKey, PROPVARIANT* pValue) override { return _attrs->GetItemByIndex(unIndex, pguidKey, pValue); }
    STDMETHODIMP CopyAllItems(IMFAttributes* pDest) override { return _attrs->CopyAllItems(pDest); }

    // ------------------------------------------------------------------
    // IKsControl stub
    // ------------------------------------------------------------------
    STDMETHODIMP KsProperty(PKSPROPERTY, ULONG, LPVOID, ULONG, ULONG*) override { return E_NOTIMPL; }
    STDMETHODIMP KsMethod(PKSMETHOD, ULONG, LPVOID, ULONG, ULONG*) override { return E_NOTIMPL; }
    STDMETHODIMP KsEvent(PKSEVENT, ULONG, LPVOID, ULONG, ULONG*) override { return E_NOTIMPL; }
};
