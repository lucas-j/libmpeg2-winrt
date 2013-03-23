#include "common.h"

ImageInfo::ImageInfo() {
    SetSize(0, 0);
    SetAspect(0, 0);
    SetFPS(0, 0); }

STDMETHODIMP ImageInfo::RuntimeClassInitialize() {
    return S_OK; }

ImageInfo::~ImageInfo() {}

void ImageInfo::SetSize(UINT32 width, UINT32 height) {
    this->width = width;
    this->height = height; }

void ImageInfo::SetAspect(DWORD numerator, DWORD denominator) {
    aspect.Numerator = numerator;
    aspect.Denominator = denominator; }

void ImageInfo::SetFPS(DWORD numerator, DWORD denominator) {
    if(denominator != 0) {
        fps.Numerator = numerator;
        fps.Denominator = denominator;
        duration = static_cast<LONGLONG>(static_cast<DOUBLE>(fps.Denominator)
            / fps.Numerator * 10000000 + 0.5); } }

ProcessState::ProcessState(): img(), change(), imgTS(0), imgDuration(0),
        finished(FALSE), changed(FALSE), mp2dec(NULL), mp2info(NULL) {}

STDMETHODIMP ProcessState::RuntimeClassInitialize() {
    return S_OK; }

ProcessState::~ProcessState() {}

ProcessState::ProcessState(const ProcessState &copy) {
    img = copy.img;
    change = copy.change;
    imgTS = copy.imgTS;
    imgDuration = copy.imgDuration;
    finished = copy.finished;
    mp2dec = copy.mp2dec;
    mp2info = copy.mp2info; }

BOOL ProcessState::RequestChange(const ImageInfo &info) {
    if(info.width != img.width || info.height != img.height ||
       info.aspect.Numerator != img.aspect.Numerator || info.aspect.Denominator != img.aspect.Denominator ||
       info.fps.Numerator != img.fps.Numerator || info.fps.Denominator != img.fps.Denominator) {
        changed = TRUE;
        change = info;
        change.duration = img.duration;
        return TRUE; }
    return FALSE; }

BOOL ProcessState::OutputChanged() const {
    return changed ? (img.height != change.height || img.width != change.width ||
                      img.aspect.Numerator != change.aspect.Numerator ||
                      img.aspect.Denominator != change.aspect.Denominator ? TRUE : FALSE) : FALSE; }

void ProcessState::MakeChange() {
    changed = FALSE;
    img = change;
    change = ImageInfo(); }

ProcessSamples::ProcessSamples(IMFSample *sample): in(NULL), out(NULL) {
    SetIn(sample); }

STDMETHODIMP ProcessSamples::RuntimeClassInitialize() {
    return S_OK; }

ProcessSamples::~ProcessSamples() {
    SafeRelease(in);
    SafeRelease(out); }

HRESULT ProcessSamples::SetIn(IMFSample *sample) {
    SafeRelease(in);
    in = sample;
    if(in) {
        in->AddRef(); }
    return S_OK; }

HRESULT ProcessSamples::SetOut(IMFSample *sample) {
    SafeRelease(out);
    out = sample;
    if(out) {
        out->AddRef(); }
    return S_OK; }

HRESULT ProcessSamples::GetIn(IMFSample **sample) {
    if(sample) {
        if(in) {
            in->AddRef();
            *sample = in;
            return S_OK; }
        return E_FAIL; }
    return E_POINTER; }

HRESULT ProcessSamples::GetOut(IMFSample **sample) {
    if(sample) {
        if(out) {
            out->AddRef();
            *sample = out;
            return S_OK; }
        return E_FAIL; }
    return E_POINTER; }
