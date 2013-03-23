#pragma once

#include <mfapi.h>
#include <mftransform.h>
#include <mfidl.h>
#include <mferror.h>
#include <wrl\implements.h>
#include <wrl\module.h>

#include <inttypes.h>
#include <queue>
#include <set>
#include <vector>
#define LIBMPEG2_NO_IMPORT
extern "C" {
#include "mpeg2.h"
#include "mpeg2convert.h"
}
#include "libmpeg2.h"

using std::queue;
using std::set;
using std::vector;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::RuntimeClassType;

#define MAX_QUEUE_SIZE 30

#define SafeRelease(x) \
    if(x) { \
        x->Release(); \
        x = NULL; }

class ImageInfo {
public:
    ImageInfo();
    STDMETHOD(RuntimeClassInitialize)();
    virtual ~ImageInfo();
    void SetSize(UINT32 width, UINT32 height);
    void SetAspect(DWORD numerator, DWORD denominator);
    void SetFPS(DWORD numerator, DWORD denominator);
public:
    UINT32 width;
    UINT32 height;
    UINT32 size;
    MFRatio aspect;
    MFRatio fps;
    LONGLONG duration; };

class ProcessState: public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>, IUnknown> {
public:
    ProcessState();
    STDMETHOD(RuntimeClassInitialize)();
    virtual ~ProcessState();
    ProcessState(const ProcessState &copy);
    BOOL RequestChange(const ImageInfo &info);
    BOOL OutputChanged() const;
    void MakeChange();
public:
    ImageInfo img, change;
    LONGLONG imgTS;
    LONGLONG imgDuration;
    BOOL finished, changed;
    mpeg2dec_t *mp2dec;
    const mpeg2_info_t *mp2info; };

class ProcessSamples: public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>, IUnknown> {
public:
    ProcessSamples(IMFSample *sample);
    STDMETHOD(RuntimeClassInitialize)();
    virtual ~ProcessSamples();
    HRESULT SetIn(IMFSample *sample);
    HRESULT SetOut(IMFSample *sample);
    HRESULT GetIn(IMFSample **sample);
    HRESULT GetOut(IMFSample **sample);
private:
    IMFSample *in;
    IMFSample *out; };
