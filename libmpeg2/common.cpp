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

DecoderState::DecoderState(): img(), imgTS(0), imgDuration(0), finished(FALSE),
        mp2dec(NULL), mp2info(NULL) {}

STDMETHODIMP DecoderState::RuntimeClassInitialize() {
    return S_OK; }

DecoderState::~DecoderState() {}

DecoderState::DecoderState(const DecoderState &copy) {
    img = copy.img;
    imgTS = copy.imgTS;
    imgDuration = copy.imgDuration;
    finished = copy.finished;
    mp2dec = copy.mp2dec;
    mp2info = copy.mp2info; }

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
