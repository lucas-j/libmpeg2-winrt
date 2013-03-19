#include "decoder.h"
#include <strsafe.h>

Decoder::Decoder(): img(), draining(FALSE), shutdown(FALSE), workQueue(0), priority(0),
        inEventCount(0), lastTS(0), process(NULL), inType(NULL), outType(NULL), mp2dec(NULL), mp2info(NULL) {
    InitializeCriticalSectionEx(&crit, 100, 0);
    state = Make<DecoderState>();
    inSamples = new queue<IMFSample *>();
    outSamples = new queue<IMFSample *>();
    pending = new vector<IMFSample *>();
    MFCreateEventQueue(&events); }

Decoder::~Decoder() {
    endStream();
    SafeRelease(events);
    DeleteCriticalSection(&crit);
    delete inSamples;
    delete outSamples;
    delete pending; }

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

STDMETHODIMP Decoder::Invoke(IMFAsyncResult *result) {
    EnterCriticalSection(&crit);
    IMFSample *sample = NULL;
    HRESULT ret = process->EndProcess(result, &sample, state);
    if(sample) {
        pending->push_back(sample);
        static ULONGLONG last = GetTickCount64();
        static UINT32 frames = 0;
        ULONGLONG curr = GetTickCount64();
        TCHAR foo[80];
        frames++;
        if(curr - last > 1000) {
            StringCchPrintf(foo, 80, TEXT("FPS: %f\n"), static_cast<double>(frames) / (curr - last) * 1000);
            OutputDebugString(foo);
            last = curr;
            frames = 0; }
        imgAdjust(); }
    process = nullptr;
    if(state->finished) {
        IMFSample *prev = inSamples->front();
        prev->Release();
        inSamples->pop();
        if(!draining) {
            QueueEvent(METransformNeedInput, GUID_NULL, S_OK, NULL);
            inEventCount++; }
        else if(inSamples->empty()) {
            QueueEvent(METransformDrainComplete, GUID_NULL, S_OK, NULL); } }
    if(!inSamples->empty() && outSamples->size() < MAX_QUEUE_SIZE) {
        StartProcess(); }
    LeaveCriticalSection(&crit);
    return S_OK; }

HRESULT Decoder::BeginGetEvent(IMFAsyncCallback *caller, IUnknown *state) {
    EnterCriticalSection(&crit);
    HRESULT ret = shutdown ? MF_E_SHUTDOWN : events->BeginGetEvent(caller, state);
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::EndGetEvent(IMFAsyncResult *result, IMFMediaEvent **out) {
    EnterCriticalSection(&crit);
    HRESULT ret = shutdown ? MF_E_SHUTDOWN : events->EndGetEvent(result, out);
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::GetEvent(DWORD flags, IMFMediaEvent **out) {
    IMFMediaEventQueue *events = NULL;
    EnterCriticalSection(&crit);
    HRESULT ret = shutdown ? MF_E_SHUTDOWN : S_OK;
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
    HRESULT ret = shutdown ? MF_E_SHUTDOWN : events->QueueEventParamVar(type, guid, status, val);
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::GetShutdownStatus(MFSHUTDOWN_STATUS *status) {
    EnterCriticalSection(&crit);
    HRESULT ret = status ? (shutdown ? S_OK : MF_E_INVALIDREQUEST) : E_INVALIDARG;
    if(SUCCEEDED(ret)) {
        *status = MFSHUTDOWN_COMPLETED; }
    LeaveCriticalSection(&crit);
    return ret; }

HRESULT Decoder::Shutdown() {
    EnterCriticalSection(&crit);
    HRESULT ret = events->Shutdown();
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
                return ret; } } }
    return MF_E_NO_MORE_TYPES; }

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
            HRESULT ret = allowChange() ? S_OK : MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;
            if(SUCCEEDED(ret) && type) {
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
            HRESULT ret = allowChange() ? S_OK : MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;
            if(SUCCEEDED(ret) && type) {
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
        *flags = allowOut() ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;
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
        ret = drain();
        break;
    case MFT_MESSAGE_SET_D3D_MANAGER:
        ret = E_NOTIMPL;
        break;
    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        ret = beginStream();
        break;
    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        ret = endStream();
        break;
    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
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
                    ret = AddSample(sample); }
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
                ret = GetSample(&samples->pSample); }
            if(SUCCEEDED(ret)) {
                if(ret == S_FALSE) {
                    ret = S_OK;
                    samples->dwStatus |= MFT_OUTPUT_DATA_BUFFER_INCOMPLETE; } }
            LeaveCriticalSection(&crit);
            return ret; }
        return E_INVALIDARG; }
    return E_POINTER; }

HRESULT Decoder::StartProcess() {
    HRESULT ret = S_OK;
    process = Make<Process>(workQueue, priority);
    IMFSample *sample = inSamples->front();
    process->BeginProcess(sample, this, state);
    return ret; }

HRESULT Decoder::AddSample(IMFSample *sample) {
    HRESULT ret = allowIn() && inEventCount > 0 ? S_OK : MF_E_NOTACCEPTING;
    if(SUCCEEDED(ret)) {
        inSamples->push(sample);
        sample->AddRef();
        inEventCount--;
        if(process == NULL && outSamples->size() < MAX_QUEUE_SIZE) {
            state->finished = TRUE;
            StartProcess(); } }
    return ret; }

HRESULT Decoder::GetSample(IMFSample **sample) {
    if(sample) {
        HRESULT ret = allowOut() ? S_OK : E_UNEXPECTED;
        if(SUCCEEDED(ret)) {
            *sample = outSamples->front();
            outSamples->pop();
            ret = allowOut() ? S_OK : S_FALSE;
            if(process == NULL && !inSamples->empty()) {
                StartProcess(); } }
        return ret; }
    return E_POINTER; }

HRESULT Decoder::beginStream() {
    HRESULT ret = S_OK;
    if(mp2dec == NULL) {
        if(!(mp2dec = mpeg2_init())) {
            ret = E_OUTOFMEMORY; }
        if(SUCCEEDED(ret) && !(mp2info = mpeg2_info(mp2dec))) {
            ret = E_FAIL; } }
    if(SUCCEEDED(ret)) {
        ret = reset(); }
    if(SUCCEEDED(ret)) {
        for(UINT32 x = 0; x < MAX_QUEUE_SIZE; x++) {
            QueueEvent(METransformNeedInput, GUID_NULL, S_OK, NULL);
            inEventCount++; } }
    return ret; }

HRESULT Decoder::endStream() {
    reset();
    if(mp2dec) {
        mpeg2_close(mp2dec);
        mp2dec = NULL;
        mp2info = NULL; }
    state->mp2dec = NULL;
    state->mp2info = NULL;
    return S_OK; }

BOOL Decoder::allowIn() const {
    return inSamples->size() < MAX_QUEUE_SIZE && !draining; }

BOOL Decoder::allowOut() const {
    return !outSamples->empty(); }

BOOL Decoder::allowChange() const {
    return inSamples->empty() && outSamples->empty() && pending->empty(); }

HRESULT Decoder::drain() {
    draining = TRUE;
    return S_OK; }

HRESULT Decoder::reset() {
    for(;!inSamples->empty(); inSamples->pop()) {
        inSamples->front()->Release(); }
    for(;!outSamples->empty(); outSamples->pop()) {
        outSamples->front()->Release(); }
    for(vector<IMFSample *>::iterator itr = pending->begin(); itr != pending->end(); itr++) {
        (*itr)->Release(); }
    pending->clear();
    draining = FALSE;
    if(mp2dec) {
        mpeg2_reset(mp2dec, 0); }
    state->mp2dec = mp2dec;
    state->mp2info = mp2info;
    state->img = img;
    state->imgTS = 0;
    state->imgDuration = img.duration;
    lastTS = 0;
    inEventCount = 0;
    return S_OK; }

HRESULT Decoder::clear() {
    HRESULT ret = reset();
    if(SUCCEEDED(ret)) {
        img.SetSize(0, 0);
        img.SetAspect(0, 0);
        img.SetFPS(0, 0);
        state->img = img;
        if(mp2dec) {
            mpeg2_reset(mp2dec, 1); } }
    return ret; }

void Decoder::imgInterpolate(vector<IMFSample *>::const_iterator itr) {
    LONGLONG ts1 = 0, ts2 = 0, rise = 0, run = 0, start = 0;
    double slope = 0;
    vector<IMFSample *>::const_iterator curr = itr;
    for(run = 0; itr != pending->end() && !ts2; itr++, run++) {
        (*itr)->GetSampleTime(&ts2); }
    if(ts2) {
        if(curr == pending->begin()) {
            ts1 = lastTS; }
        else {
            (*(curr - 1))->GetSampleTime(&ts1); }
        rise = ts2 - ts1;
        slope = static_cast<double>(rise) / run; }
    else {
        (*(curr - 1))->GetSampleTime(&ts1);
        if(curr - 1 != pending->begin()) {
            (*(curr - 2))->GetSampleTime(&ts2); }
        else {
            ts2 = ts1 - img.duration; }
        rise = ts1 - ts2;
        slope = static_cast<double>(rise); }
    for(DWORD i = 1; i < run; i++, curr++) {
        LONGLONG ts = 0;
        (*curr)->GetSampleTime(&ts);
        ts = static_cast<LONGLONG>(ts1 + slope * i);
        (*curr)->SetSampleTime(static_cast<LONGLONG>(ts1 + slope * i)); } }

void Decoder::imgAdjust() {
    if(pending->size() > MAX_QUEUE_SIZE / 2 || draining) {
        set<LONGLONG> timestamps;
        for(vector<IMFSample *>::iterator itr = pending->begin(); itr != pending->end(); itr++) {
            LONGLONG ts = 0;
            HRESULT ret = (*itr)->GetSampleTime(&ts);
            if(FAILED(ret) || ts == 0 || !timestamps.insert(ts).second) {
                (*itr)->SetSampleTime(0); } }
        for(vector<IMFSample *>::iterator itr = pending->begin(); itr != pending->end(); itr++) {
            LONGLONG ts = 0;
            (*itr)->GetSampleTime(&ts);
            if(ts) {
                set<LONGLONG>::iterator x = timestamps.begin();
                (*itr)->SetSampleTime(*x);
                timestamps.erase(x); } }
        vector<IMFSample *>::iterator end = pending->end() - MAX_QUEUE_SIZE / 4 + 1;
        for(vector<IMFSample *>::iterator itr = pending->begin(); itr != end; itr++) {
            LONGLONG ts = 0;
            (*itr)->GetSampleTime(&ts);
            if(ts == 0) {
                imgInterpolate(itr); } }
        end--;
        for(vector<IMFSample *>::iterator itr = pending->begin(); itr != end; itr++) {
            LONGLONG ts1 = 0, ts2 = 0;
            (*itr)->GetSampleTime(&ts1);
            (*(itr + 1))->GetSampleTime(&ts2);
            if(ts1 && ts2) {
                (*itr)->SetSampleDuration(ts2 - ts1); }
            else {
                (*itr)->SetSampleDuration(img.duration); } }
        (*end)->GetSampleTime(&lastTS);
        for(vector<IMFSample *>::iterator itr = pending->begin(); itr != end; itr++) {
            outSamples->push(*itr);
            QueueEvent(METransformHaveOutput, GUID_NULL, S_OK, NULL); }
        pending->erase(pending->begin(), end); } }
/*
        for(int i = 0; !pending->empty() && !timestamps.empty() && (draining || i < MAX_QUEUE_SIZE / 4); i++) {
            IMFSample *s = pending->front();
            set<LONGLONG>::iterator t = timestamps.begin(), t2 = t;
            t2++;
            s->SetSampleTime(*t);
            if(t2 != timestamps.end()) {
                s->SetSampleDuration(*t2 - *t); }
            outSamples->push(s);
            pending->pop();
            timestamps.erase(t); } } }
*/

HRESULT Decoder::info(IMFMediaType *type, GUID *major, GUID *minor, UINT32 *width, UINT32 *height,
                      MFRatio *fps, MFRatio *aspect) {
    HRESULT ret = type ? S_OK : E_POINTER;
    if(SUCCEEDED(ret) && major) {
        ret = type->GetGUID(MF_MT_MAJOR_TYPE, major); }
    if(SUCCEEDED(ret) && minor) {
        ret = type->GetGUID(MF_MT_SUBTYPE, minor); }
    if(SUCCEEDED(ret) && width && height) {
        ret = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, width, height); }
    if(SUCCEEDED(ret) && fps) {
        ret = MFGetAttributeRatio(type, MF_MT_FRAME_RATE,
            reinterpret_cast<UINT32 *>(&(fps->Numerator)), reinterpret_cast<UINT32 *>(&(fps->Denominator))); }
    if(SUCCEEDED(ret) && aspect) {
        ret = MFGetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO,
            reinterpret_cast<UINT32 *>(&(aspect->Numerator)), reinterpret_cast<UINT32 *>(&(aspect->Denominator))); }
    return ret; }

HRESULT Decoder::checkInputType(IMFMediaType *type) {
    GUID major, minor;
    UINT32 width, height;
    MFRatio fps, aspect;
    HRESULT ret = info(type, &major, &minor, &width, &height, &fps, &aspect);
    if(SUCCEEDED(ret)) {
        if(major != MFMediaType_Video || minor != MFVideoFormat_MPEG2) {
            ret = MF_E_INVALIDMEDIATYPE; }
        if(width == 0 || width > 4096 || height == 0 || height > 4096) {
            ret = MF_E_INVALIDMEDIATYPE; }
        if(fps.Numerator == 0 || fps.Denominator == 0) {
            ret = MF_E_INVALIDMEDIATYPE; }
        if(aspect.Numerator == 0 || aspect.Denominator == 0) {
            ret = MF_E_INVALIDMEDIATYPE; } }
    return ret; }

HRESULT Decoder::setInputType(IMFMediaType *type) {
    UINT32 width, height;
    MFRatio fps, aspect;
    HRESULT ret = reset();
    if(SUCCEEDED(ret)) {
        ret = info(type, NULL, NULL, &width, &height, &fps, &aspect); }
    if(SUCCEEDED(ret)) {
        SafeRelease(inType);
        img.SetSize(width, height);
        img.SetAspect(aspect.Numerator, aspect.Denominator);
        img.SetFPS(fps.Numerator, fps.Denominator);
        inType = type;
        inType->AddRef(); }
    return ret; }

HRESULT Decoder::checkOutputType(IMFMediaType *type) {
    GUID major, minor;
    UINT32 width, height;
    MFRatio fps, aspect;
    HRESULT ret = inType ? info(type, &major, &minor, &width, &height, &fps, &aspect) : MF_E_TRANSFORM_TYPE_NOT_SET;
    if(SUCCEEDED(ret)) {
        if(major != MFMediaType_Video || minor != MFVideoFormat_NV12) {
            ret = MF_E_INVALIDMEDIATYPE; }
        if(width != img.width || height != img.height) {
            ret = MF_E_INVALIDMEDIATYPE; }
        if(img.fps.Numerator != fps.Numerator || img.fps.Denominator != fps.Denominator) {
            ret = MF_E_INVALIDMEDIATYPE; }
        if(img.aspect.Numerator != aspect.Numerator || img.aspect.Denominator != aspect.Denominator) {
            ret = MF_E_INVALIDMEDIATYPE; } }
    return ret; }

HRESULT Decoder::setOutputType(IMFMediaType *type) {
    SafeRelease(outType);
    outType = type;
    outType->AddRef();
    return S_OK; }
