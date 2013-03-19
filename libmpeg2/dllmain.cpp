#include <wrl/module.h>
#include "decoder.h"

namespace libmpeg2 {
	ActivatableClass(Decoder); };

BOOL APIENTRY DllMain(HINSTANCE hnd, DWORD reason, LPVOID) {
    if(reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hnd);
		Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Create();  }
	else if(reason == DLL_PROCESS_DETACH) {
		Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Terminate(); }
	return TRUE; }

HRESULT APIENTRY DllGetActivationFactory(HSTRING clsid, IActivationFactory **factory) {
	return Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().GetActivationFactory(clsid, factory); }

HRESULT APIENTRY DllCanUnloadNow() {
	return Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Terminate() ? S_OK : S_FALSE; }

STDAPI DllGetClassObject(_In_ REFCLSID clsid, _In_ REFIID riid, _Outptr_ LPVOID FAR *ppv) {
	return Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().GetClassObject(clsid, riid, ppv); }
