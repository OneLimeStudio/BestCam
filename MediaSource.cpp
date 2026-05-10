// ============================================================================
// MediaSource.cpp — Implementation of IMFMediaSource
// ============================================================================
#include "MediaSource.h"
#include "MediaStream.h"
#include <ks.h>
#include <ksproxy.h>
#include <ksmedia.h>

VirtualCamMediaSource::VirtualCamMediaSource() : _state(MFMEDIASOURCE_STOPPED) {}
VirtualCamMediaSource::~VirtualCamMediaSource() {}

HRESULT VirtualCamMediaSource::RuntimeClassInitialize()
{
    Log("VirtualCamMediaSource::RuntimeClassInitialize");

    HRESULT hr = MFCreateEventQueue(&_eventQueue);
    if (FAILED(hr)) { Log("MFCreateEventQueue failed: 0x%08X", hr); return hr; }

    hr = MFCreateAttributes(&_sourceAttributes, 8);
    if (FAILED(hr)) return hr;

    // We are a software source, disable D3D integrations
    _sourceAttributes->SetUINT32(MF_SA_D3D_AWARE, FALSE);
    _sourceAttributes->SetUINT32(MF_SA_D3D11_AWARE, FALSE);

    _stream = Microsoft::WRL::Make<VirtualCamMediaStream>();
    if (!_stream) return E_OUTOFMEMORY;
    
    hr = _stream->RuntimeClassInitialize(this);
    if (FAILED(hr)) { Log("Stream initialization failed: 0x%08X", hr); return hr; }

    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::GetSourceAttributes(IMFAttributes** ppAttributes)
{
    if (!ppAttributes) return E_POINTER;
    _sourceAttributes.CopyTo(ppAttributes);
    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::GetStreamAttributes(DWORD dwStreamIdentifier, IMFAttributes** ppAttributes)
{
    if (!ppAttributes) return E_POINTER;

    Microsoft::WRL::ComPtr<IMFAttributes> streamAttrs;
    HRESULT hr = MFCreateAttributes(&streamAttrs, 8);
    if (FAILED(hr)) return hr;

    // PINNAME_VIDEO_CAPTURE dictates this is a camera capture stream
    static const GUID PINNAME_VIDEO_CAPTURE_GUID =
        { 0xFB6C4281, 0x0353, 0x11d1, { 0x90, 0x5F, 0x00, 0x00, 0xC0, 0xCC, 0x16, 0xBA } };

    streamAttrs->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, PINNAME_VIDEO_CAPTURE_GUID);
    streamAttrs->SetUINT32(MF_DEVICESTREAM_STREAM_ID, dwStreamIdentifier);
    streamAttrs->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, 1);
    streamAttrs->SetUINT32(MF_SA_D3D_AWARE, FALSE);
    streamAttrs->SetUINT32(MF_SA_D3D11_AWARE, FALSE);

    *ppAttributes = streamAttrs.Detach();
    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::SetD3DManager(IUnknown* /*pManager*/)
{
    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::GetCharacteristics(DWORD* characteristics)
{
    if (!characteristics) return E_POINTER;
    *characteristics = MFMEDIASOURCE_IS_LIVE;
    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor** ppPD)
{
    if (!ppPD) return E_POINTER;

    Microsoft::WRL::ComPtr<IMFStreamDescriptor> sd;
    HRESULT hr = _stream->GetStreamDescriptor(&sd);
    if (FAILED(hr)) return hr;

    IMFStreamDescriptor* sdArr[] = { sd.Get() };
    Microsoft::WRL::ComPtr<IMFPresentationDescriptor> pd;
    
    hr = MFCreatePresentationDescriptor(1, sdArr, &pd);
    if (FAILED(hr)) return hr;

    pd->SelectStream(0);
    *ppPD = pd.Detach();
    
    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::Start(IMFPresentationDescriptor* pPD, const GUID* pguidTimeFormat, const PROPVARIANT* pvarStartPosition)
{
    Log("VirtualCamMediaSource::Start called");
    _state = MFMEDIASOURCE_RUNNING;
    _stream->SetActive(true);

    // Provide the stream to the media pipeline
    Microsoft::WRL::ComPtr<IUnknown> streamUnk;
    _stream.As(&streamUnk);
    
    HRESULT hr = _eventQueue->QueueEventParamUnk(MENewStream, GUID_NULL, S_OK, streamUnk.Get());
    if (FAILED(hr)) return hr;

    hr = _eventQueue->QueueEventParamVar(MESourceStarted, GUID_NULL, S_OK, pvarStartPosition);
    if (FAILED(hr)) return hr;

    hr = _stream->FireStreamStarted(pvarStartPosition);
    return hr;
}

STDMETHODIMP VirtualCamMediaSource::Stop()
{
    Log("VirtualCamMediaSource::Stop called");
    _state = MFMEDIASOURCE_STOPPED;
    _stream->SetActive(false);
    _eventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, nullptr);
    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::Pause()
{
    _state = MFMEDIASOURCE_PAUSED;
    _stream->SetActive(false);
    _eventQueue->QueueEventParamVar(MESourcePaused, GUID_NULL, S_OK, nullptr);
    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::Shutdown()
{
    Log("VirtualCamMediaSource::Shutdown called");
    if (_eventQueue) _eventQueue->Shutdown();
    if (_stream)     _stream->Shutdown();
    return S_OK;
}

STDMETHODIMP VirtualCamMediaSource::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    return _eventQueue->GetEvent(dwFlags, ppEvent);
}

STDMETHODIMP VirtualCamMediaSource::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState)
{
    return _eventQueue->BeginGetEvent(pCallback, punkState);
}

STDMETHODIMP VirtualCamMediaSource::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    return _eventQueue->EndGetEvent(pResult, ppEvent);
}

STDMETHODIMP VirtualCamMediaSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    return _eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}

STDMETHODIMP VirtualCamMediaSource::GetService(REFGUID guidService, REFIID riid, LPVOID* ppvObject)
{
    return E_NOINTERFACE;
}

STDMETHODIMP VirtualCamMediaSource::KsProperty(PKSPROPERTY, ULONG, LPVOID, ULONG, ULONG*) { return E_NOTIMPL; }
STDMETHODIMP VirtualCamMediaSource::KsMethod(PKSMETHOD, ULONG, LPVOID, ULONG, ULONG*)     { return E_NOTIMPL; }
STDMETHODIMP VirtualCamMediaSource::KsEvent(PKSEVENT, ULONG, LPVOID, ULONG, ULONG*)       { return E_NOTIMPL; }

STDMETHODIMP VirtualCamMediaSource::QueryInterface(REFIID riid, void** ppv)
{
    return RuntimeClass::QueryInterface(riid, ppv);
}
