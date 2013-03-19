#include "process.h"

Process::Process(DWORD workQueue, LONG priority):
        workQueue(workQueue), priority(priority), state(NULL), samples(NULL) {}

Process::~Process() {}

STDMETHODIMP Process::RuntimeClassInitialize() {
    return S_OK; }

STDMETHODIMP Process::GetParameters(DWORD *flags, DWORD *workQueue) {
    *flags = 0;
    *workQueue = this->workQueue;
    return S_OK; }

STDMETHODIMP Process::Invoke(IMFAsyncResult *result) {
    IUnknown *tmp = NULL;
    IUnknown *unknown = NULL;
    IMFAsyncResult *caller = result;
    HRESULT ret = result->GetState(&tmp);
    if(SUCCEEDED(ret)) {
        ret = tmp->QueryInterface(IID_PPV_ARGS(&caller)); }
    if(SUCCEEDED(ret)) {
        ret = caller->GetState(&unknown); }
    if(SUCCEEDED(ret)) {
        state = dynamic_cast<DecoderState *>(unknown);
        ret = caller->GetObject(&unknown); }
    if(SUCCEEDED(ret)) {
        samples = dynamic_cast<ProcessSamples *>(unknown); }
    if(SUCCEEDED(ret)) {
        ret = process(); }
    if(caller) {
        caller->SetStatus(ret);
        MFInvokeCallback(caller); }
    SafeRelease(tmp);
    SafeRelease(caller);
    return S_OK; }

HRESULT Process::BeginProcess(IMFSample *sample, IMFAsyncCallback *cb, const ComPtr<DecoderState> state) {
    ComPtr<ProcessSamples> samples = Make<ProcessSamples>(sample);
    HRESULT ret = E_OUTOFMEMORY;
    if(samples) {
        IMFAsyncResult *result = NULL;
        ret = MFCreateAsyncResult(samples.Get(), cb, state.Get(), &result);
        if(SUCCEEDED(ret)) {
            ret = MFPutWorkItem2(workQueue, priority, this, result); }
        SafeRelease(result); }
    return ret; }

HRESULT Process::EndProcess(IMFAsyncResult *result, IMFSample **sample, ComPtr<DecoderState> state) {
    IUnknown *unknown = NULL;
    ProcessSamples *samples = NULL;
    IMFSample *out = NULL;
    HRESULT ret = result->GetState(&unknown);
    if(SUCCEEDED(ret)) {
        state = ComPtr<DecoderState>(dynamic_cast<DecoderState *>(unknown));
        ret = result->GetObject(&unknown); }
    if(SUCCEEDED(ret)) {
        samples = dynamic_cast<ProcessSamples *>(unknown);
        ret = samples->GetOut(&out); }
    if(SUCCEEDED(ret)) {
        *sample = out; }
    SafeRelease(samples);
    SafeRelease(unknown);
    return result->GetStatus(); }

HRESULT Process::process() {
    HRESULT ret = S_OK;
    for(;SUCCEEDED(ret);) {
        if(state->finished) {
            ret = imgRead(); }
        else {
            switch(mpeg2_parse(state->mp2dec)) {
            case STATE_SLICE:
            case STATE_END:
            case STATE_INVALID_END:
                if(state->mp2info->display_fbuf) {
                    return imgWrite(); }
                break;
            case STATE_BUFFER:
                return imgDispose();
            default:
                break; } } }
    return ret; }

HRESULT Process::imgRead() {
    IMFMediaBuffer *buf = NULL;
    IMFSample *sample = NULL;
    BYTE *data = NULL;
    DWORD len = 0;
    HRESULT ret = samples->GetIn(&sample);
    if(SUCCEEDED(ret)) {
        sample->ConvertToContiguousBuffer(&buf); }
    if(SUCCEEDED(ret)) {
        ret = buf->Lock(&data, NULL, &len); }
    if(SUCCEEDED(ret)) {
        LONGLONG ts = 0;
        LONGLONG dur = 0;
        mpeg2_buffer(state->mp2dec, data, data + len);
        if(SUCCEEDED(sample->GetSampleTime(&ts)) && ts) {
            state->imgTS = ts; }
        if(SUCCEEDED(sample->GetSampleDuration(&dur)) && dur) {
            state->imgDuration = dur; }
        state->finished = FALSE; }
    SafeRelease(sample);
    SafeRelease(buf);
    return ret; }

HRESULT Process::imgDispose() {
    IMFMediaBuffer *buf = NULL;
    IMFSample *sample = NULL;
    HRESULT ret = samples->GetIn(&sample);
    if(SUCCEEDED(ret)) {
        ret = sample->GetBufferByIndex(0, &buf); }
    if(SUCCEEDED(ret)) {
        ret = buf->Unlock(); }
    if(SUCCEEDED(ret)) {
        samples->SetIn(NULL);
        state->finished = TRUE; }
    SafeRelease(buf);
    SafeRelease(sample);
    return ret; }

HRESULT Process::imgFill(BYTE *data, LONG stride) {
    const mpeg2_info_t *info = state->mp2info;
    HRESULT ret = info->display_fbuf && info->sequence ? S_OK : E_FAIL;
    if(SUCCEEDED(ret)) {
        const BYTE *luma = info->display_fbuf->buf[0];
        const BYTE *u = info->display_fbuf->buf[1];
        const BYTE *v = info->display_fbuf->buf[2];
        UINT32 width = info->sequence->width;
        UINT32 height = info->sequence->height;
        UINT32 chroma_width = info->sequence->chroma_width;
        UINT32 chroma_height = info->sequence->chroma_height;
        for(UINT32 line = 0; line < state->img.height; line++, data += stride, luma += width) {
            memcpy_s(data, stride, luma, state->img.width); }
        for(UINT32 line = 0; line < state->img.height / 2; line++, data += stride, u += chroma_width, v += chroma_width) {
            UINT16 *data_word = reinterpret_cast<UINT16 *>(data);
            for(UINT32 x = 0; x < state->img.width / 2; x++) {
                data_word[x] = (UINT16)(v[x] << 8) | (UINT16)u[x]; } } }
    return ret; }

HRESULT Process::imgCopy(IMFSample *sample) {
    IMFMediaBuffer *buf = NULL;
    IMF2DBuffer2 *buf2D = NULL;
    BYTE *data = NULL;
    LONG stride = 0;
    HRESULT ret = MFCreate2DMediaBuffer(state->img.width, state->img.height, FCC('NV12'), FALSE, &buf);
    if(SUCCEEDED(ret)) {
        ret = buf->QueryInterface(IID_PPV_ARGS(&buf2D)); }
    if(SUCCEEDED(ret)) {
        ret = buf2D->Lock2D(&data, &stride); }
    if(SUCCEEDED(ret)) {
        ret = imgFill(data, stride); }
    if(SUCCEEDED(ret)) {
        ret = buf2D->Unlock2D(); }
    if(SUCCEEDED(ret)) {
        ret = sample->AddBuffer(buf); }
    SafeRelease(buf);
    SafeRelease(buf2D);
    return ret; }

HRESULT Process::imgWrite() {
    IMFSample *sample = NULL;
    HRESULT ret = MFCreateSample(&sample);
    if(SUCCEEDED(ret)) {
        ret = imgCopy(sample); }
    if(SUCCEEDED(ret)) {
        ret = sample->SetUINT32(MFSampleExtension_CleanPoint, TRUE); }
    if(SUCCEEDED(ret)) {
        ret = sample->SetSampleTime(state->imgTS); }
    if(SUCCEEDED(ret)) {
        ret = sample->SetSampleDuration(state->imgDuration); }
    if(SUCCEEDED(ret)) {
        samples->SetOut(sample);
        state->imgTS = 0; }
    SafeRelease(sample);
    return ret; }
