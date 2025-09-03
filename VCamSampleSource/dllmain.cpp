#include "pch.h"
#include "Undocumented.h"
#include "Tools.h"
#include "EnumNames.h"
#include "MFTools.h"
#include "FrameGenerator.h"
#include "MediaStream.h"
#include "MediaSource.h"
#include "Activator.h"
#include <map>
#include <string>

// Base GUID for virtual cameras - each camera will have a different GUID
// 3cad447d-f283-4af4-a3b2-6f5363309f52 (Camera1)
// 3cad447d-f283-4af4-a3b2-6f5363309f53 (Camera2)
// etc.
static GUID CLSID_VCamBase = { 0x3cad447d,0xf283,0x4af4,{0xa3,0xb2,0x6f,0x53,0x63,0x30,0x9f,0x52} };

// Map CLSID string ("{...}") to camera ID
std::map<std::wstring, std::wstring> g_cameraMap;

HMODULE _hModule;

// Forward declarations
GUID GenerateCameraClsid(LPCWSTR cameraId);
std::wstring GetCameraIdForClsid(REFCLSID clsid);
void LoadCameraRegistrations();

// Helper function to generate CLSID for a camera ID
GUID GenerateCameraClsid(LPCWSTR cameraId)
{
	// Simple hash-based approach: use the camera name to modify the last part of the GUID
	GUID clsid = CLSID_VCamBase;
	
	// Hash the camera ID and use it to modify the last few bytes
	DWORD hash = 0;
	for (int i = 0; cameraId[i] != 0; i++)
	{
		hash = hash * 31 + cameraId[i];
	}
	
	// Modify the last DWORD of the GUID using the hash
	clsid.Data4[4] = (hash & 0xFF);
	clsid.Data4[5] = ((hash >> 8) & 0xFF);
	clsid.Data4[6] = ((hash >> 16) & 0xFF);
	clsid.Data4[7] = ((hash >> 24) & 0xFF);
	
	return clsid;
}

// Helper function to get camera ID for a CLSID
std::wstring GetCameraIdForClsid(REFCLSID clsid)
{
    auto key = GUID_ToStringW(clsid, false);
    auto it = g_cameraMap.find(key);
    if (it != g_cameraMap.end())
    {
        return it->second;
    }
	
	// If not found in map, try to load from registry
	LoadCameraRegistrations();
	
	it = g_cameraMap.find(key);
	if (it != g_cameraMap.end())
	{
		return it->second;
	}
	
	return L"Camera1"; // Default fallback
}

// Load camera registrations from registry
void LoadCameraRegistrations()
{
	g_cameraMap.clear();
	
	HKEY hKey;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinCamHTTP\\Cameras", 0, KEY_READ, &hKey);
	if (result != ERROR_SUCCESS)
	{
		// No cameras registered, add default
		GUID defaultClsid = CLSID_VCamBase;
		g_cameraMap[GUID_ToStringW(defaultClsid, false)] = L"Camera1";
		return;
	}
	
	wil::unique_hkey keyGuard(hKey);
	
	// Enumerate subkeys (camera IDs)
	DWORD index = 0;
	WCHAR cameraId[256];
	DWORD nameSize = sizeof(cameraId) / sizeof(WCHAR);
	
	while (RegEnumKeyExW(hKey, index, cameraId, &nameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
	{
		GUID clsid = GenerateCameraClsid(cameraId);
		auto key = GUID_ToStringW(clsid, false);
		g_cameraMap[key] = cameraId;
		WINTRACE(L"LoadCameraRegistrations: Mapped %s to CLSID %s", cameraId, GUID_ToStringW(clsid).c_str());
		
		index++;
		nameSize = sizeof(cameraId) / sizeof(WCHAR);
	}
	
	// If no cameras found, add default
	if (g_cameraMap.empty())
	{
		GUID defaultClsid = CLSID_VCamBase;
		g_cameraMap[GUID_ToStringW(defaultClsid, false)] = L"Camera1";
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		_hModule = hModule;
		WinTraceRegister();
		WINTRACE(L"DllMain DLL_PROCESS_ATTACH '%s'", GetCommandLine());
		DisableThreadLibraryCalls(hModule);

		wil::SetResultLoggingCallback([](wil::FailureInfo const& failure) noexcept
			{
				wchar_t str[2048];
				if (SUCCEEDED(wil::GetFailureLogString(str, _countof(str), failure)))
				{
					WinTrace(2, 0, str); // 2 => error
				}
			});
		break;

	case DLL_PROCESS_DETACH:
		WINTRACE(L"DllMain DLL_PROCESS_DETACH '%s'", GetCommandLine());
		WinTraceUnregister();
		break;
	}
	return TRUE;
}

struct ClassFactory : winrt::implements<ClassFactory, IClassFactory>
{
	GUID _clsid;
	
	ClassFactory(REFCLSID clsid) : _clsid(clsid) {}

	STDMETHODIMP CreateInstance(IUnknown* outer, GUID const& riid, void** result) noexcept final
	{
		RETURN_HR_IF_NULL(E_POINTER, result);
		*result = nullptr;
		if (outer)
			RETURN_HR(CLASS_E_NOAGGREGATION);

		// Get camera ID for this CLSID
		std::wstring cameraId = GetCameraIdForClsid(_clsid);
		WINTRACE(L"ClassFactory::CreateInstance for CLSID %s mapped to camera %s", GUID_ToStringW(_clsid).c_str(), cameraId.c_str());

	auto vcam = winrt::make_self<Activator>();
	// Pass camera ID first so Initialize can pick it up
	RETURN_IF_FAILED(vcam->SetCameraId(cameraId.c_str()));
	RETURN_IF_FAILED(vcam->Initialize());
		
		auto hr = vcam->QueryInterface(riid, result);
		if (FAILED(hr))
		{
			auto iid = GUID_ToStringW(riid);
			WINTRACE(L"ClassFactory QueryInterface failed on IID %s", iid.c_str());
		}
		return hr;
	}

	STDMETHODIMP LockServer(BOOL) noexcept final
	{
		return S_OK;
	}
};

// Use the standard COM export signatures (STDAPI) so the linkage matches
// the declarations in the platform headers (combaseapi.h).
STDAPI DllCanUnloadNow()
{
	if (winrt::get_module_lock())
	{
		WINTRACE(L"DllCanUnloadNow S_FALSE");
		return S_FALSE;
	}

	winrt::clear_factory_cache();
	WINTRACE(L"DllCanUnloadNow S_OK");
	return S_OK;
}

_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
	WINTRACE(L"DllGetClassObject rclsid:%s riid:%s", GUID_ToStringW(rclsid).c_str(), GUID_ToStringW(riid).c_str());
	RETURN_HR_IF_NULL(E_POINTER, ppv);
	*ppv = nullptr;

	// Check if this CLSID is registered for any camera
	LoadCameraRegistrations();
	auto key = GUID_ToStringW(rclsid, false);
	if (g_cameraMap.find(key) != g_cameraMap.end())
	{
		return winrt::make_self<ClassFactory>(rclsid)->QueryInterface(riid, ppv);
	}

	RETURN_HR(E_NOINTERFACE);
}

using registry_key = winrt::handle_type<registry_traits>;

STDAPI DllRegisterServer()
{
	std::wstring exePath = wil::GetModuleFileNameW(_hModule).get();
	WINTRACE(L"DllRegisterServer '%s'", exePath.c_str());
	
	// Load camera registrations and register each one
	LoadCameraRegistrations();
	
	for (const auto& pair : g_cameraMap)
	{
		const auto& clsid = pair.first; // already a string like "{...}"
		std::wstring path = L"Software\\Classes\\CLSID\\" + clsid + L"\\InprocServer32";

		// note: a vcam *must* be registered in HKEY_LOCAL_MACHINE
		// for the frame server to be able to talk with it.
		registry_key key;
		RETURN_IF_WIN32_ERROR(RegWriteKey(HKEY_LOCAL_MACHINE, path.c_str(), key.put()));
		RETURN_IF_WIN32_ERROR(RegWriteValue(key.get(), nullptr, exePath));
		RETURN_IF_WIN32_ERROR(RegWriteValue(key.get(), L"ThreadingModel", L"Both"));
		
		std::wstring friendlyName = pair.second + L" (WinCamHTTP)";
		RETURN_IF_WIN32_ERROR(RegWriteValue(key.get(), L"FriendlyName", friendlyName));
		
		WINTRACE(L"DllRegisterServer: Registered CLSID %s for camera %s", clsid.c_str(), pair.second.c_str());
	}
	
	return S_OK;
}

STDAPI DllUnregisterServer()
{
	std::wstring exePath = wil::GetModuleFileNameW(_hModule).get();
	WINTRACE(L"DllUnregisterServer '%s'", exePath.c_str());
	
	// Load camera registrations and unregister each one
	LoadCameraRegistrations();
	
	for (const auto& pair : g_cameraMap)
	{
		const auto& clsid = pair.first; // string key
		std::wstring path = L"Software\\Classes\\CLSID\\" + clsid;
		RETURN_IF_WIN32_ERROR(RegDeleteTree(HKEY_LOCAL_MACHINE, path.c_str()));
		
		WINTRACE(L"DllUnregisterServer: Unregistered CLSID %s for camera %s", clsid.c_str(), pair.second.c_str());
	}
	
	return S_OK;
}