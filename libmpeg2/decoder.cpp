#include "decoder.h"
#include <strsafe.h>

Decoder::Decoder(): img(), state(State_Stopped), draining(FALSE), changing(FALSE),
        workQueue(0), priority(0), requests(0), lastTS(0),
        frames(0), framesSent(0), lastFrame(0), changeFrame(0),
        process(NULL), inType(NULL), outType(NULL), mp2dec(NULL), mp2info(NULL) {
    InitializeCriticalSectionEx(&crit, 100, 0);
    procState = Make<ProcessState>();
    inSamples = new queue<IMFSample *>();
    outSamples = new queue<IMFSample *>();
    pending = new vector<IMFSample *>();
    MFCreateEventQueue(&events);
    lastTick = GetTickCount64(); }

Decoder::~Decoder() {
    /* handle edge-case where Process is still running */
    endStream();
    Shutdown();
    SafeRelease(events);
    DeleteCriticalSection(&crit);
    delete inSamples;
    delete outSamples;
    delete pending; }

STDMETHODIMP Decoder::Invoke(IMFAsyncResult *result) {
    EnterCriticalSection(&crit);
    IMFSample *sample = NULL;
    HRESULT ret = process->EndProcess(result, &sample, procState);
    if(sample && (state == State_Started || state == State_Starting)) {
        pending->push_back(sample);
        frames++;
        outputFPS();
        imgAdjust(); }
    process = nullptr;
    if(procState->finished) {
        if(!inSamples->empty()) {
            IMFSample *prev = inSamples->front();
            prev->Release();
            inSamples->pop(); }
        if(draining && inSamples->empty()) {
            endDrain(); }
        requestSamples(); }
    if(procState->changed) {
        makeChange(); }
    switch(state) {
    case State_Shutdown:
        endStream();
        state = State_Shutdown;
        break;
    case State_Stopping:
        if(mp2dec) {
            reset(); }
        else {
            endStream(); }
        break;
    case State_Starting:
        state = State_Stopped;
        beginStream();
    case State_Started:
        startProcess();
    default:
        break; }
    LeaveCriticalSection(&crit);
    return S_OK; }

void Decoder::startProcess() {
    if(allowProcess()) {
        process = Make<Process>(workQueue, priority);
        IMFSample *sample = inSamples->front();
        process->BeginProcess(sample, this, procState); } }

HRESULT Decoder::addSample(IMFSample *sample) {
    HRESULT ret = allowIn() ? S_OK : MF_E_NOTACCEPTING;
    if(SUCCEEDED(ret)) {
        inSamples->push(sample);
        sample->AddRef();
        requests--;
        if(process == NULL) {
            procState->finished = TRUE;
            startProcess(); } }
    return ret; }

HRESULT Decoder::getSample(IMFSample **sample) {
    if(sample) {
        HRESULT ret = outReady() ? S_OK : E_UNEXPECTED;
        if(SUCCEEDED(ret)) {
            *sample = outSamples->front();
            outSamples->pop();
            ret = outReady() ? S_OK : S_FALSE;
            startProcess();
            framesSent++; }
        return ret; }
    return E_POINTER; }

void Decoder::requestSamples() {
    if(state == State_Started || state == State_Starting) {
        QueueEvent(METransformNeedInput, GUID_NULL, S_OK, NULL);
        requests++; } }

void Decoder::beginDrain() {
    draining = TRUE;
    if(inSamples->size() == 0) {
        endDrain(); } }

void Decoder::endDrain() {
    QueueEvent(METransformDrainComplete, GUID_NULL, S_OK, NULL);
    draining = FALSE; }

HRESULT Decoder::beginStream() {
    HRESULT ret = S_OK;
    if(mp2dec == NULL) {
        if(!(mp2dec = mpeg2_init())) {
            ret = E_OUTOFMEMORY; }
        if(SUCCEEDED(ret) && !(mp2info = mpeg2_info(mp2dec))) {
            ret = E_FAIL; } }
    if(SUCCEEDED(ret)) {
        if(state == State_Stopped) {
            ret = reset(); }
        else {
            state = State_Starting;
            return ret; } }
    if(SUCCEEDED(ret)) {
        state = State_Started;
        requestSamples(); }
    return ret; }

HRESULT Decoder::endStream() {
    reset();
    if(process == NULL) {
        if(mp2dec) {
            mpeg2_close(mp2dec); }
        procState->mp2dec = NULL;
        procState->mp2info = NULL; }
    mp2dec = NULL;
    mp2info = NULL;
    return S_OK; }

BOOL Decoder::allowIn() const {
    return state == State_Started && inSamples->size() < MAX_QUEUE_SIZE && requests > 0 && !draining; }

BOOL Decoder::allowProcess() const {
    return state == State_Started && process == NULL && !inSamples->empty() && outSamples->size() < MAX_QUEUE_SIZE; }

BOOL Decoder::outReady() const {
    return !outSamples->empty(); }

BOOL Decoder::changeNeeded() const {
    return changing && framesSent >= changeFrame; }

HRESULT Decoder::reset() {
    if(process == NULL) {
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
        procState->mp2dec = mp2dec;
        procState->mp2info = mp2info;
        procState->img = img;
        procState->imgTS = 0;
        procState->imgDuration = img.duration;
        changing = FALSE;
        state = State_Stopped; 
        lastTS = 0;
        frames = 0;
        framesSent = 0;
        lastFrame = 0;
        lastTick = GetTickCount64();
        requests = 0; }
    else {
        state = State_Stopping; }
    return S_OK; }

void Decoder::makeChange() {
    TCHAR foo[80];
    changing = TRUE;
    changeFrame = frames;
    procState->MakeChange();
    img = procState->img;
    StringCchPrintf(foo, 80, TEXT("Changing to %dx%d (%d:%d) at %0.2f fps\n"), img.width, img.height,
                    img.aspect.Numerator, img.aspect.Denominator,
                    img.fps.Numerator * 1.0 / img.fps.Denominator);
    OutputDebugString(foo); }

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
        vector<IMFSample *>::iterator end = pending->end();
        if(pending->size() > MAX_QUEUE_SIZE / 4 + 2) {
            end = pending->end() - MAX_QUEUE_SIZE / 4 + 1; }
        for(vector<IMFSample *>::iterator itr = pending->begin(); itr != end; itr++) {
            LONGLONG ts = 0;
            (*itr)->GetSampleTime(&ts);
            if(ts == 0) {
                imgInterpolate(itr); } }
        if(end != pending->begin()) {
            end--; }
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

void Decoder::outputFPS() {
    TCHAR foo[80];
    ULONGLONG curr = GetTickCount64();
    if(curr - lastTick > 1000) {
        StringCchPrintf(foo, 80, TEXT("FPS: %f\n"),
                        static_cast<double>(frames - lastFrame) / (curr - lastTick) * 1000);
        OutputDebugString(foo);
        lastTick = curr;
        frames = 0; } }

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
        inType->AddRef();
        if(state != State_Started) {
            procState->RequestChange(img); } }
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
    changing = FALSE;
    changeFrame = 0;
    return S_OK; }
