#include "framework.h"
#include "tools.h"
#include "WinCamHTTP.h"
#include <shellapi.h>
#include <map>

#define MAX_LOADSTRING 100

struct CameraInfo
{
	std::wstring id;
	std::wstring friendlyName;
	std::wstring url;
	UINT width;
	UINT height;
	GUID clsid;
	wil::com_ptr_nothrow<IMFVirtualCamera> vcam;
};

HINSTANCE _instance;
WCHAR _title[MAX_LOADSTRING];
WCHAR _windowClass[MAX_LOADSTRING];
std::vector<CameraInfo> _cameras;
HWND _hwnd;
NOTIFYICONDATA _nid{};
bool _camerasStarted = false;

ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT RegisterVirtualCameras();
HRESULT UnregisterVirtualCameras();
HRESULT LoadCameraSettingsFromRegistry();
void CreateTrayIcon();
void RemoveTrayIcon();
void ShowTrayContextMenu(HWND hwnd, POINT pt);
GUID GenerateCameraClsid(const std::wstring& cameraId);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	// set tracing & CRT leak tracking
	WinTraceRegister();
	WINTRACE(L"WinMain starting '%s'", GetCommandLineW());
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);

	wil::SetResultLoggingCallback([](wil::FailureInfo const& failure) noexcept
		{
			wchar_t str[2048];
			if (SUCCEEDED(wil::GetFailureLogString(str, _countof(str), failure)))
			{
				WinTrace(2, 0, str); // 2 => error
#ifndef _DEBUG
				MessageBox(nullptr, str, L"WinCamHTTP Error", MB_OK | MB_ICONERROR);
#endif
			}
		});

	LoadStringW(hInstance, IDS_APP_TITLE, _title, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINCAMHTTP, _windowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	
	// Create hidden window for message processing
	_hwnd = InitInstance(hInstance, SW_HIDE);
	if (!_hwnd)
		return -1;

	winrt::init_apartment();
	if (FAILED(MFStartup(MF_VERSION)))
		return -1;

	// Load settings from registry
	auto settingsResult = LoadCameraSettingsFromRegistry();
	
	if (FAILED(settingsResult) || _cameras.empty())
	{
		MessageBox(nullptr, 
			L"No cameras configured. Please run WinCamHTTPSetup first to configure at least one camera.\n\n"
			L"WinCamHTTPSetup must be run as administrator to save configuration to the registry.",
			L"WinCamHTTP - Configuration Missing", 
			MB_OK | MB_ICONWARNING);
		MFShutdown();
		return -1;
	}

	// Create tray icon
	CreateTrayIcon();

	// Automatically start all cameras
	auto hr = RegisterVirtualCameras();
	if (SUCCEEDED(hr))
	{
		_camerasStarted = true;
		WINTRACE(L"All virtual cameras started automatically");
		
		// Update tray icon tooltip
		wcscpy_s(_nid.szTip, L"WinCamHTTP - All Cameras Active");
		Shell_NotifyIcon(NIM_MODIFY, &_nid);
	}
	else
	{
		WINTRACE(L"Failed to start one or more virtual cameras: 0x%08X", hr);
		MessageBox(nullptr, 
			L"One or more virtual cameras could not be started. Make sure the WinCamHTTPSource DLL is registered.\n\n"
			L"Run 'regsvr32 WinCamHTTPSource.dll' as administrator.",
			L"WinCamHTTP - Startup Error", 
			MB_OK | MB_ICONERROR);
		
		// Still show tray icon even if cameras failed to start
		wcscpy_s(_nid.szTip, L"WinCamHTTP - Camera Start Failed");
		Shell_NotifyIcon(NIM_MODIFY, &_nid);
	}

	// Message loop
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Cleanup
	if (_camerasStarted)
	{
		UnregisterVirtualCameras();
	}
	RemoveTrayIcon();
	_cameras.clear();
	MFShutdown();

	// cleanup & CRT leak checks
	_CrtDumpMemoryLeaks();
	WINTRACE(L"WinMain exiting '%s'", GetCommandLineW());
	WinTraceUnregister();
	return (int)msg.wParam;
}

GUID GenerateCameraClsid(const std::wstring& cameraId)
{
	// Base GUID: {3cad447d-f283-4af4-a3b2-6f5363309f52}
	GUID clsid = { 0x3cad447d,0xf283,0x4af4,{0xa3,0xb2,0x6f,0x53,0x63,0x30,0x9f,0x52} };
	
	// Hash the camera ID and use it to modify the last few bytes
	DWORD hash = 0;
	for (int i = 0; i < (int)cameraId.length(); i++)
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

HRESULT LoadCameraSettingsFromRegistry()
{
	_cameras.clear();
	
	HKEY hKey;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinCamHTTP\\Cameras", 0, KEY_READ, &hKey);
	if (result != ERROR_SUCCESS)
		return HRESULT_FROM_WIN32(result);

	wil::unique_hkey keyGuard(hKey);

	// Enumerate subkeys (camera IDs)
	DWORD index = 0;
	WCHAR cameraId[256];
	DWORD nameSize = sizeof(cameraId) / sizeof(WCHAR);
	
	while (RegEnumKeyExW(hKey, index, cameraId, &nameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
	{
		// Open this camera's key
		std::wstring subKeyPath = L"SOFTWARE\\WinCamHTTP\\Cameras\\" + std::wstring(cameraId);
		HKEY hCameraKey;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKeyPath.c_str(), 0, KEY_READ, &hCameraKey) == ERROR_SUCCESS)
		{
			wil::unique_hkey cameraKeyGuard(hCameraKey);
			
			CameraInfo camera;
			camera.id = cameraId;
			camera.clsid = GenerateCameraClsid(camera.id);
			
			// Read URL
			DWORD dataSize = 2048 * sizeof(WCHAR);
			std::vector<WCHAR> urlBuffer(2048);
			result = RegQueryValueExW(hCameraKey, L"URL", nullptr, nullptr, (LPBYTE)urlBuffer.data(), &dataSize);
			if (result == ERROR_SUCCESS)
			{
				camera.url = urlBuffer.data();
			}
			
			// Read Width/Height
			dataSize = sizeof(DWORD);
			DWORD width = 640, height = 480;
			RegQueryValueExW(hCameraKey, L"Width", nullptr, nullptr, (LPBYTE)&width, &dataSize);
			dataSize = sizeof(DWORD);
			RegQueryValueExW(hCameraKey, L"Height", nullptr, nullptr, (LPBYTE)&height, &dataSize);
			camera.width = width;
			camera.height = height;
			
			// Read Friendly Name
			dataSize = 256 * sizeof(WCHAR);
			std::vector<WCHAR> nameBuffer(256);
			result = RegQueryValueExW(hCameraKey, L"FriendlyName", nullptr, nullptr, (LPBYTE)nameBuffer.data(), &dataSize);
			if (result == ERROR_SUCCESS)
			{
				camera.friendlyName = nameBuffer.data();
			}
			else
			{
				camera.friendlyName = L"WinCamHTTP Virtual Camera " + camera.id;
			}
			
			_cameras.push_back(camera);
		}
		
		index++;
		nameSize = sizeof(cameraId) / sizeof(WCHAR);
	}

	return S_OK;
}

void CreateTrayIcon()
{
	_nid.cbSize = sizeof(NOTIFYICONDATA);
	_nid.hWnd = _hwnd;
	_nid.uID = 1;
	_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	_nid.uCallbackMessage = WM_TRAYICON;
	_nid.hIcon = LoadIcon(_instance, MAKEINTRESOURCE(IDI_WINCAMHTTP));
	wcscpy_s(_nid.szTip, L"WinCamHTTP - Starting...");

	Shell_NotifyIcon(NIM_ADD, &_nid);
}

void RemoveTrayIcon()
{
	Shell_NotifyIcon(NIM_DELETE, &_nid);
}

void ShowTrayContextMenu(HWND hwnd, POINT pt)
{
	HMENU hMenu = LoadMenu(_instance, MAKEINTRESOURCE(IDR_TRAY_MENU));
	if (hMenu)
	{
		HMENU hSubMenu = GetSubMenu(hMenu, 0);
		if (hSubMenu)
		{
			// Required for popup menus to work correctly
			SetForegroundWindow(hwnd);
			
			TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
				pt.x, pt.y, 0, hwnd, nullptr);
			
			// Required for popup menus to work correctly
			PostMessage(hwnd, WM_NULL, 0, 0);
		}
		DestroyMenu(hMenu);
	}
}

HRESULT RegisterVirtualCameras()
{
	for (auto& camera : _cameras)
	{
		auto clsid = GUID_ToStringW(camera.clsid);
		RETURN_IF_FAILED_MSG(MFCreateVirtualCamera(
			MFVirtualCameraType_SoftwareCameraSource,
			MFVirtualCameraLifetime_Session,
			MFVirtualCameraAccess_CurrentUser,
			camera.friendlyName.c_str(),
			clsid.c_str(),
			nullptr,
			0,
			&camera.vcam),
			"Failed to create virtual camera for %s", camera.id.c_str());

		WINTRACE(L"RegisterVirtualCamera '%s' for camera '%s' ok", clsid.c_str(), camera.id.c_str());
		RETURN_IF_FAILED_MSG(camera.vcam->Start(nullptr), "Cannot start VCam for %s", camera.id.c_str());
		WINTRACE(L"VCam for '%s' was started", camera.id.c_str());
	}
	
	return S_OK;
}

HRESULT UnregisterVirtualCameras()
{
	for (auto& camera : _cameras)
	{
		if (camera.vcam)
		{
			auto hr = camera.vcam->Remove();
			WINTRACE(L"Remove VCam for '%s' hr:0x%08X", camera.id.c_str(), hr);
		}
	}
	
	return S_OK;
}

ATOM MyRegisterClass(HINSTANCE instance)
{
	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = instance;
	wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_WINCAMHTTP));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = _windowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE instance, int cmd)
{
	_instance = instance;
	auto hwnd = CreateWindowW(_windowClass, _title, WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
	if (!hwnd)
		return nullptr;

	// Don't show the window - it's hidden for tray operation
	return hwnd;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_TRAYICON:
		switch (lParam)
		{
		case WM_RBUTTONUP:
		case WM_CONTEXTMENU:
		{
			POINT pt;
			GetCursorPos(&pt);
			ShowTrayContextMenu(hwnd, pt);
			break;
		}
		}
		break;

	case WM_COMMAND:
	{
		auto wmId = LOWORD(wParam);
		switch (wmId)
		{
		case IDM_TRAY_EXIT:
			PostQuitMessage(0);
			break;
		}
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}