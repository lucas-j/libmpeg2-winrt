#pragma once

#include "common.h"

class Process: public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>, IMFAsyncCallback> {
public:
    Process(DWORD workQueue, LONG priority);
private:
    virtual ~Process();
public:
    STDMETHOD(RuntimeClassInitialize)();
    STDMETHODIMP GetParameters(DWORD *flags, DWORD *workQueue);
    STDMETHODIMP Invoke(IMFAsyncResult *result);
    HRESULT BeginProcess(IMFSample *sample, IMFAsyncCallback *cb, const ComPtr<DecoderState> state);
    HRESULT EndProcess(IMFAsyncResult *result, IMFSample **sample, ComPtr<DecoderState> state);

protected:
    HRESULT process();
    HRESULT imgRead();
    HRESULT imgDispose();
    static void interlace(BYTE *data, const BYTE *u, const BYTE *v, UINT32 count);
    HRESULT imgFill(BYTE *data, LONG stride);
    HRESULT imgCopy(IMFSample *sample);
    HRESULT imgWrite();

protected:
    DWORD workQueue;
    LONG priority;
    ComPtr<DecoderState> state;
    ComPtr<ProcessSamples> samples; };
