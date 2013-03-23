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
        state = dynamic_cast<ProcessState *>(unknown);
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

HRESULT Process::BeginProcess(IMFSample *sample, IMFAsyncCallback *cb, const ComPtr<ProcessState> state) {
    ComPtr<ProcessSamples> samples = Make<ProcessSamples>(sample);
    HRESULT ret = E_OUTOFMEMORY;
    if(samples) {
        IMFAsyncResult *result = NULL;
        ret = MFCreateAsyncResult(samples.Get(), cb, state.Get(), &result);
        if(SUCCEEDED(ret)) {
            ret = MFPutWorkItem2(workQueue, priority, this, result); }
        SafeRelease(result); }
    return ret; }

HRESULT Process::EndProcess(IMFAsyncResult *result, IMFSample **sample, ComPtr<ProcessState> state) {
    IUnknown *unknown = NULL;
    ProcessSamples *samples = NULL;
    IMFSample *out = NULL;
    HRESULT ret = result->GetState(&unknown);
    if(SUCCEEDED(ret)) {
        state = ComPtr<ProcessState>(dynamic_cast<ProcessState *>(unknown));
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
            case STATE_SEQUENCE:
                if(possibleChange()) {
                    return MF_E_TRANSFORM_STREAM_CHANGE; }
                break;
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
    UINT32 dis = 0;
    HRESULT ret = samples->GetIn(&sample);
    if(SUCCEEDED(ret)) {
        sample->GetUINT32(MFSampleExtension_Discontinuity, &dis);
        if(dis != 0) {
            mpeg2_reset(state->mp2dec, 0); }
        sample->ConvertToContiguousBuffer(&buf);
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

void Process::interlace(BYTE *data, const BYTE *u, const BYTE *v, UINT32 count) {
    /* http://en.wikipedia.org/wiki/Duff's_device */
    UINT32 n = (count + 7) / 8;
    switch(count % 8) {
    case 0: do { *data++ = *u++;
    case 7:      *data++ = *v++;
    case 6:      *data++ = *u++;
    case 5:      *data++ = *v++;
    case 4:      *data++ = *u++;
    case 3:      *data++ = *v++;
    case 2:      *data++ = *u++;
    case 1:      *data++ = *v++; } while(--n > 0); } }

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
        UINT32 img_height = state->img.height;
        UINT32 img_width = state->img.width;
        for(UINT32 line = 0; line < img_height; line++, data += stride, luma += width) {
            memcpy_s(data, stride, luma, img_width); }
        if(chroma_height >= img_height) {
            chroma_width *= 2; }
        img_height /= 2;
        for(UINT32 line = 0; line < img_height; line++, data += stride, u += chroma_width, v += chroma_width) {
            interlace(data, u, v, img_width); } }
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

void Process::findClosest(const MFRatio *ratios, int size, double val, MFRatio *out) {
    int closest = 0;
    double close = ratios[closest].Numerator * 1.0 / ratios[closest].Denominator;
    for(int x = 0; x < size; x++) {
        double test = ratios[x].Numerator * 1.0 / ratios[x].Denominator;
        if(abs(test - val) < abs(close - val)) {
            closest = x;
            close = test; } }
    out->Numerator = ratios[closest].Numerator;
    out->Denominator = ratios[closest].Denominator; }

BOOL Process::possibleChange() {
    static const MFRatio speeds[] = {
        {24000, 1001}, {24, 1}, {25, 1}, {30000, 1001}, {30, 1}, {50, 1}, {60000, 1001}, {60, 1},
        {15, 1}, {5, 1}, {10, 1}, {12, 1}};
    ImageInfo info;
    UINT32 width, height;
    MFRatio aspect, fps;
    if(state->mp2info->sequence) {
        width = state->mp2info->sequence->display_width;
        height = state->mp2info->sequence->display_height;
        aspect.Numerator = state->mp2info->sequence->pixel_width;
        aspect.Denominator = state->mp2info->sequence->pixel_height;
        findClosest(speeds, 12, 27.0 * 1000 * 1000 / state->mp2info->sequence->frame_period, &fps);
        info.SetSize(width, height);
        info.SetAspect(aspect.Numerator, aspect.Denominator);
        info.SetFPS(fps.Numerator, fps.Denominator);
        return state->RequestChange(info); }
    return FALSE; }
