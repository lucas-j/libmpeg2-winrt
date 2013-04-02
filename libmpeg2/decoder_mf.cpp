#include "decoder.h"
#include <strsafe.h>

STDMETHODIMP Decoder::RuntimeClassInitialize() {
    return S_OK; }

IFACEMETHODIMP Decoder::SetProperties(ABI::Windows::Foundation::Collections::IPropertySet *) {
    return S_OK; }

STDMETHODIMP Decoder::RegisterThreadsEx(DWORD *, LPCWSTR, LONG) {
    return S_OK; }

STDMETHODIMP Decoder::UnregisterThreads() {
    return S_OK; }

STDMETHODIMP Decoder::SetWorkQueueEx(DWORD workQueue, LONG priority) {
    this->workQueue = workQueue;
    this->priority = priority;
    return S_OK; }

STDMETHODIMP Decoder::GetParameters(DWORD *flags, DWORD *workQueue) {
    *flags = 0;
    *workQueue = this->workQueue;
    return S_OK; }

HRESULT Decoder::BeginGetEvent(IMFAsyncCallback *caller, IUnknown *state) {
    EnterCriticalSection(&crit);
    HRESULT ret = this->state == State_Shutdown ? MF_E_SHUTDOWN : events->BeginGetEvent(caller, state);
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::EndGetEvent(IMFAsyncResult *result, IMFMediaEvent **out) {
    EnterCriticalSection(&crit);
    HRESULT ret = state == State_Shutdown ? MF_E_SHUTDOWN : events->EndGetEvent(result, out);
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::GetEvent(DWORD flags, IMFMediaEvent **out) {
    IMFMediaEventQueue *events = NULL;
    EnterCriticalSection(&crit);
    HRESULT ret = state == State_Shutdown ? MF_E_SHUTDOWN : S_OK;
    if(SUCCEEDED(ret)) {
        events = this->events;
        events->AddRef(); }
    LeaveCriticalSection(&crit);
    if(SUCCEEDED(ret)) {
        ret = events->GetEvent(flags, out); }
    SafeRelease(events);
    return ret; }

HRESULT Decoder::QueueEvent(MediaEventType type, REFGUID guid, HRESULT status, const PROPVARIANT *val) {
    EnterCriticalSection(&crit);
    HRESULT ret = state == State_Shutdown ? MF_E_SHUTDOWN : events->QueueEventParamVar(type, guid, status, val);
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::GetShutdownStatus(MFSHUTDOWN_STATUS *status) {
    EnterCriticalSection(&crit);
    HRESULT ret = status ? (state == State_Shutdown ? S_OK : MF_E_INVALIDREQUEST) : E_INVALIDARG;
    if(SUCCEEDED(ret)) {
        *status = MFSHUTDOWN_COMPLETED; }
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::Shutdown() {
    EnterCriticalSection(&crit);
    HRESULT ret = events->Shutdown();
    state = State_Shutdown;
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::GetStreamLimits(DWORD *minIn, DWORD *maxIn, DWORD *minOut, DWORD *maxOut) {
    if(minIn && maxIn && minOut && maxOut) {
        *minIn = *maxIn = *minOut = *maxOut = 1;
        return S_OK; }
    else {
        return E_POINTER; } }

HRESULT Decoder::GetStreamCount(DWORD *in, DWORD *out) {
    if(in && out) {
        *in = *out = 1;
        return S_OK; }
    else {
        return E_POINTER; } }

HRESULT Decoder::GetStreamIDs(DWORD, DWORD *, DWORD, DWORD *) {
    return E_NOTIMPL; }

HRESULT Decoder::GetInputStreamInfo(DWORD id, MFT_INPUT_STREAM_INFO *info) {
    if(info) {
        if(id == 0) {
            EnterCriticalSection(&crit);
            /* these parameters may not be correct */
            info->hnsMaxLatency = inType ? img.duration * MAX_QUEUE_SIZE : 0;
            info->dwFlags = MFT_INPUT_STREAM_HOLDS_BUFFERS;
            info->cbSize = 1;
            info->cbMaxLookahead = 0;
            info->cbAlignment = 0;
            LeaveCriticalSection(&crit);
            return S_OK; }
        return MF_E_INVALIDSTREAMNUMBER; }
    return E_POINTER; }

HRESULT Decoder::GetOutputStreamInfo(DWORD id, MFT_OUTPUT_STREAM_INFO *info) {
    if(info) {
        if(id == 0) {
            EnterCriticalSection(&crit);
            info->dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES |
                MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
                MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE |
                MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
            info->cbAlignment = 0;
            info->cbSize = outType ? img.size : 0;
            LeaveCriticalSection(&crit);
            return S_OK; }
        return MF_E_INVALIDSTREAMNUMBER; }
    return E_POINTER; }

HRESULT Decoder::GetAttributes(IMFAttributes **attr) {
    IMFAttributes *out = NULL;
    HRESULT ret = MFCreateAttributes(&out, 3);
    if(SUCCEEDED(ret)) {
        ret = out->SetUINT32(MF_TRANSFORM_ASYNC, 1); }
    if(SUCCEEDED(ret)) {
        ret = out->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, 0); }
    if(SUCCEEDED(ret)) {
        ret = out->SetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, 1); }
    if(SUCCEEDED(ret)) {
        *attr = out; }
    return ret; }

HRESULT Decoder::GetInputStreamAttributes(DWORD id, IMFAttributes **attr) {
    return id == 0 ? GetAttributes(attr) : MF_E_INVALIDSTREAMNUMBER; }

HRESULT Decoder::GetOutputStreamAttributes(DWORD id, IMFAttributes **attr) {
    return id == 0 ? GetAttributes(attr) : MF_E_INVALIDSTREAMNUMBER; }

HRESULT Decoder::DeleteInputStream(DWORD) {
    return E_NOTIMPL; }

HRESULT Decoder::AddInputStreams(DWORD, DWORD *) {
    return E_NOTIMPL; }

HRESULT Decoder::GetInputAvailableType(DWORD id, DWORD index, IMFMediaType **type) {
    if(type) {
        if(id == 0) {
            if(index == 0) {
                EnterCriticalSection(&crit);
                IMFMediaType *out = NULL;
                HRESULT ret = MFCreateMediaType(&out);
                if(SUCCEEDED(ret)) {
                    ret = out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); }
                if(SUCCEEDED(ret)) {
                    ret = out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_MPEG2); }
                /* these three attribute calls may not be necessary, as the format can change dynamically */
                if(SUCCEEDED(ret) && img.width && img.height) {
                    ret = MFSetAttributeSize(out, MF_MT_FRAME_SIZE, img.width, img.height); }
                if(SUCCEEDED(ret) && img.fps.Numerator && img.fps.Denominator) {
                    ret = MFSetAttributeRatio(out, MF_MT_FRAME_RATE, img.fps.Numerator, img.fps.Denominator); }
                if(SUCCEEDED(ret) && img.aspect.Numerator && img.aspect.Denominator) {
                    ret = MFSetAttributeRatio(out, MF_MT_PIXEL_ASPECT_RATIO, img.aspect.Numerator, img.aspect.Denominator); }
                if(SUCCEEDED(ret)) {
                    *type = out;
                    (*type)->AddRef(); }
                SafeRelease(out);
                LeaveCriticalSection(&crit);
                return ret; }
            return MF_E_NO_MORE_TYPES; }
        return MF_E_INVALIDSTREAMNUMBER; }
    return E_POINTER; }

HRESULT Decoder::GetOutputAvailableType(DWORD id, DWORD index, IMFMediaType **type) {
    if(type) {
        if(id == 0) {
            if(index == 0) {
                EnterCriticalSection(&crit);
                HRESULT ret = inType ? S_OK : MF_E_TRANSFORM_TYPE_NOT_SET;
                IMFMediaType *out = NULL;
                if(SUCCEEDED(ret)) {
                    ret = MFCreateMediaType(&out); }
                if(SUCCEEDED(ret)) {
                    ret = out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); }
                if(SUCCEEDED(ret)) {
                    ret = out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12); }
                if(SUCCEEDED(ret)) {
                    ret = out->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE); }
                if(SUCCEEDED(ret)) {
                    ret = out->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE); }
                if(SUCCEEDED(ret) && img.size) {
                    ret = out->SetUINT32(MF_MT_SAMPLE_SIZE, img.size); }
                if(SUCCEEDED(ret) && img.width && img.height) {
                    ret = MFSetAttributeSize(out, MF_MT_FRAME_SIZE, img.width, img.height); }
                if(SUCCEEDED(ret) && img.fps.Numerator && img.fps.Denominator) {
                    ret = MFSetAttributeRatio(out, MF_MT_FRAME_RATE, img.fps.Numerator, img.fps.Denominator); }
                if(SUCCEEDED(ret) && img.aspect.Numerator && img.aspect.Denominator) {
                    ret = MFSetAttributeRatio(out, MF_MT_PIXEL_ASPECT_RATIO, img.aspect.Numerator, img.aspect.Denominator); }
                if(SUCCEEDED(ret)) {
                    /* not sure about this one */
                    ret = out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive); }
                if(SUCCEEDED(ret)) {
                    *type = out;
                    (*type)->AddRef(); }
                SafeRelease(out);
                LeaveCriticalSection(&crit);
                return ret; }
            return MF_E_NO_MORE_TYPES; }
        return MF_E_INVALIDSTREAMNUMBER; }
    return E_POINTER; }

HRESULT Decoder::SetInputType(DWORD id, IMFMediaType *type, DWORD flags) {
    if(id == 0) {
        if((flags & ~MFT_SET_TYPE_TEST_ONLY) == 0) {
            EnterCriticalSection(&crit);
            HRESULT ret = type ? S_OK : E_POINTER;
            if(SUCCEEDED(ret)) {
                ret = checkInputType(type); }
            if(SUCCEEDED(ret) && (flags & MFT_SET_TYPE_TEST_ONLY) == 0) {
                ret = setInputType(type); }
            LeaveCriticalSection(&crit);
            return ret; }
        return E_INVALIDARG; }
    return MF_E_INVALIDSTREAMNUMBER; }

HRESULT Decoder::SetOutputType(DWORD id, IMFMediaType *type, DWORD flags) {
    if(id == 0) {
        if((flags & ~MFT_SET_TYPE_TEST_ONLY) == 0) {
            EnterCriticalSection(&crit);
            HRESULT ret = type ? S_OK : E_POINTER;
            if(SUCCEEDED(ret)) {
                ret = checkOutputType(type); }
            if(SUCCEEDED(ret) && (flags & MFT_SET_TYPE_TEST_ONLY) == 0) {
                ret = setOutputType(type); }
            LeaveCriticalSection(&crit);
            return ret; }
        return E_INVALIDARG; }
    return MF_E_INVALIDSTREAMNUMBER; }

HRESULT Decoder::GetInputCurrentType(DWORD id, IMFMediaType **type) {
    if(type) {
        if(id == 0) {
            EnterCriticalSection(&crit);
            HRESULT ret = inType ? S_OK : MF_E_TRANSFORM_TYPE_NOT_SET;
            if(SUCCEEDED(ret)) {
                *type = inType;
                (*type)->AddRef(); }
            LeaveCriticalSection(&crit);
            return ret; }
        return MF_E_INVALIDSTREAMNUMBER; }
    return E_POINTER; }

HRESULT Decoder::GetOutputCurrentType(DWORD id, IMFMediaType **type) {
    if(type) {
        if(id == 0) {
            EnterCriticalSection(&crit);
            HRESULT ret = outType ? S_OK : MF_E_TRANSFORM_TYPE_NOT_SET;
            if(SUCCEEDED(ret)) {
                *type = outType;
                (*type)->AddRef(); }
            LeaveCriticalSection(&crit);
            return ret; }
        return MF_E_INVALIDSTREAMNUMBER; }
    return E_POINTER; }

HRESULT Decoder::GetInputStatus(DWORD id, DWORD *flags) {
    if(flags) {
        if(id == 0) {
            EnterCriticalSection(&crit);
            *flags = allowIn() ? MFT_INPUT_STATUS_ACCEPT_DATA : 0;
            LeaveCriticalSection(&crit);
            return S_OK; }
        return MF_E_INVALIDSTREAMNUMBER; }
    return E_POINTER; }

HRESULT Decoder::GetOutputStatus(DWORD *flags) {
    if(flags) {
        EnterCriticalSection(&crit);
        *flags = outReady() ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;
        LeaveCriticalSection(&crit);
        return S_OK; }
    return E_POINTER; }

HRESULT Decoder::SetOutputBounds(LONGLONG, LONGLONG) {
    return E_NOTIMPL; }

HRESULT Decoder::ProcessEvent(DWORD, IMFMediaEvent *) {
    return E_NOTIMPL; }

HRESULT Decoder::ProcessMessage(MFT_MESSAGE_TYPE msg, ULONG_PTR) {
    EnterCriticalSection(&crit);
    HRESULT ret = S_OK;
    switch(msg) {
    case MFT_MESSAGE_COMMAND_FLUSH:
        ret = reset();
        break;
    case MFT_MESSAGE_COMMAND_DRAIN:
        beginDrain();
        break;
    case MFT_MESSAGE_SET_D3D_MANAGER:
        ret = E_NOTIMPL;
        break;
    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        break;
    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        break;
    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
        ret = beginStream();
        break;
    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
        ret = endStream();
        break; }
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::ProcessInput(DWORD id, IMFSample *sample, DWORD flags) {
    if(sample) {
        if(id == 0) {
            if(flags == 0) {
                EnterCriticalSection(&crit);
                HRESULT ret = inType && outType ? S_OK : MF_E_TRANSFORM_TYPE_NOT_SET;
                if(SUCCEEDED(ret)) {
                    ret = addSample(sample); }
                LeaveCriticalSection(&crit);
                return ret; }
            return E_INVALIDARG; }
        return MF_E_INVALIDSTREAMNUMBER; }
    return E_POINTER; }

HRESULT Decoder::ProcessOutput(DWORD flags, DWORD count, MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status) {
    if(samples && status) {
        if(flags == 0 && count == 1) {
            EnterCriticalSection(&crit);
            HRESULT ret = inType && outType ? S_OK : MF_E_TRANSFORM_TYPE_NOT_SET;
            if(SUCCEEDED(ret)) {
                if(changeNeeded()) {
                    ret = MF_E_TRANSFORM_STREAM_CHANGE;
                    samples->dwStatus |= MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE; }
                else {
                    ret = getSample(&samples->pSample); } }
            if(SUCCEEDED(ret)) {
                if(ret == S_FALSE) {
                    ret = S_OK;
                    samples->dwStatus |= MFT_OUTPUT_DATA_BUFFER_INCOMPLETE; } }
            LeaveCriticalSection(&crit);
            return ret; }
        return E_INVALIDARG; }
    return E_POINTER; }
