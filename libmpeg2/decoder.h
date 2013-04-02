#pragma once

/*
 * - split into different files (done)
 * - use state machine rather than booleans (done)
 * - use ComPtr and more WRL templates (done)
 * - try to find out if embedding gcc .obj optimizations is possible (done)
 * - allow for reverse play as well as forward
 * - test 1080p playback on high-power CPU (done)
 * - implement QoS frame dropping if CPU can't keep up
 * - test seeking (done)
 * - add rate control options
 * - gracefully handle format changes (e.g. resolution shifts in DVB) (untested)
 */

#include "common.h"
#include "process.h"

class Decoder: public RuntimeClass<RuntimeClassFlags<RuntimeClassType::WinRtClassicComMix>,
	    ABI::Windows::Media::IMediaExtension, IMFTransform, IMFMediaEventGenerator,
        IMFShutdown, IMFAsyncCallback, IMFRealTimeClientEx> {
    InspectableClass(RuntimeClass_libmpeg2_Decoder, BaseTrust)

    enum State {
        State_Stopped,
        State_Stopping,
        State_Starting,
        State_Started,
        State_Shutdown };

public:
    Decoder();
    virtual ~Decoder();
    STDMETHOD(RuntimeClassInitialize)();
    STDMETHODIMP SetProperties(ABI::Windows::Foundation::Collections::IPropertySet *config);
    STDMETHODIMP RegisterThreadsEx(DWORD *, LPCWSTR, LONG);
    STDMETHODIMP UnregisterThreads();
    STDMETHODIMP SetWorkQueueEx(DWORD workQueue, LONG priority);
    STDMETHODIMP GetParameters(DWORD *flags, DWORD *workQueue);
    STDMETHODIMP Invoke(IMFAsyncResult *result);
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback *caller, IUnknown *state);
    STDMETHODIMP EndGetEvent(IMFAsyncResult *result, IMFMediaEvent **out);
    STDMETHODIMP GetEvent(DWORD flags, IMFMediaEvent **out);
    STDMETHODIMP QueueEvent(MediaEventType type, REFGUID guid, HRESULT status, const PROPVARIANT *val);
    STDMETHODIMP GetShutdownStatus(MFSHUTDOWN_STATUS *status);
    STDMETHODIMP Shutdown();

    STDMETHODIMP GetStreamLimits(DWORD *minIn, DWORD *maxIn, DWORD *minOut, DWORD *maxOut);
    STDMETHODIMP GetStreamCount(DWORD *in, DWORD *out);
    STDMETHODIMP GetStreamIDs(DWORD countIn, DWORD *in, DWORD countOut, DWORD *out);
    STDMETHODIMP GetInputStreamInfo(DWORD id, MFT_INPUT_STREAM_INFO *info);
    STDMETHODIMP GetOutputStreamInfo(DWORD id, MFT_OUTPUT_STREAM_INFO *info);
    STDMETHODIMP GetAttributes(IMFAttributes **attr);
    STDMETHODIMP GetInputStreamAttributes(DWORD id, IMFAttributes **attr);
    STDMETHODIMP GetOutputStreamAttributes(DWORD id, IMFAttributes **attr);
    STDMETHODIMP DeleteInputStream(DWORD id);
    STDMETHODIMP AddInputStreams(DWORD count, DWORD *ids);
    STDMETHODIMP GetInputAvailableType(DWORD id, DWORD index, IMFMediaType **type);
    STDMETHODIMP GetOutputAvailableType(DWORD id, DWORD index, IMFMediaType **type);
    STDMETHODIMP SetInputType(DWORD id, IMFMediaType *type, DWORD flags);
    STDMETHODIMP SetOutputType(DWORD id, IMFMediaType *type, DWORD flags);
    STDMETHODIMP GetInputCurrentType(DWORD id, IMFMediaType **type);
    STDMETHODIMP GetOutputCurrentType(DWORD id, IMFMediaType **type);
    STDMETHODIMP GetInputStatus(DWORD id, DWORD *flags);
    STDMETHODIMP GetOutputStatus(DWORD *flags);
    STDMETHODIMP SetOutputBounds(LONGLONG lower, LONGLONG upper);
    STDMETHODIMP ProcessEvent(DWORD id, IMFMediaEvent *evt);
    STDMETHODIMP ProcessMessage(MFT_MESSAGE_TYPE msg, ULONG_PTR param);
    STDMETHODIMP ProcessInput(DWORD id, IMFSample *sample, DWORD flags);
    STDMETHODIMP ProcessOutput(DWORD flags, DWORD count, MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status);

protected:
    void startProcess();
    HRESULT addSample(IMFSample *sample);
    HRESULT getSample(IMFSample **sample);
    void requestSamples();
    void beginDrain();
    void endDrain();
    BOOL allowIn() const;
    BOOL allowProcess() const;
    BOOL allowAddPending() const;
    BOOL outReady() const;
    BOOL changeNeeded() const;
    HRESULT reset();
    HRESULT beginStream();
    HRESULT endStream();
    void makeChange();
    void imgInterpolate(vector<IMFSample *>::const_iterator itr);
    void imgAdjust();
    void outputFPS();
    static HRESULT info(IMFMediaType *type, GUID *major, GUID *minor, UINT32 *width, UINT32 *height,
                        MFRatio *fps, MFRatio *aspect);
    HRESULT checkInputType(IMFMediaType *type);
    HRESULT setInputType(IMFMediaType *type);
    HRESULT checkOutputType(IMFMediaType *type);
    HRESULT setOutputType(IMFMediaType *type);

protected:
    CRITICAL_SECTION crit;
    ImageInfo img;
    ComPtr<ProcessState> procState;
    State state;
    BOOL draining;
    BOOL changing;
    DWORD workQueue;
    LONG priority;
    UINT32 requests;
    LONGLONG lastTS;
    UINT32 frames;
    UINT32 framesSent;
    UINT32 lastFrame;
    UINT32 changeFrame;
    ULONGLONG lastTick;
    ComPtr<Process> process;
    IMFMediaType *inType;
    IMFMediaType *outType;
    queue<IMFSample *> *inSamples;
    queue<IMFSample *> *outSamples;
    vector<IMFSample *> *pending;
    IMFMediaEventQueue *events;
    mpeg2dec_t *mp2dec;
    const mpeg2_info_t *mp2info; };
