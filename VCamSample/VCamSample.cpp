#include "framework.h"
#include "tools.h"
#include "VCamSample.h"
#include <vector>
#include <string>

#define MAX_LOADSTRING 100

struct CameraConfig
{
	std::wstring id;
	std::wstring url;
	std::wstring friendlyName;
	UINT width;
	UINT height;
	bool enabled;
};

HINSTANCE _instance;
WCHAR _title[MAX_LOADSTRING];
WCHAR _windowClass[MAX_LOADSTRING];

// Runtime UI window handles
HWND _hwndMain = nullptr;
HWND _hwndListCameras = nullptr;
HWND _hwndEditUrl = nullptr;
HWND _hwndEditName = nullptr;
HWND _hwndComboResolution = nullptr;
HWND _hwndBtnAdd = nullptr;
HWND _hwndBtnRemove = nullptr;
HWND _hwndBtnSave = nullptr;
HWND _hwndStatus = nullptr;
HWND _hwndChkEnabled = nullptr;

std::vector<CameraConfig> _cameras;

ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
void GetSelectedResolution(UINT* width, UINT* height);
HRESULT LoadCamerasFromRegistry();
HRESULT SaveCamerasToRegistry();
void RefreshCameraList();
void PopulateFieldsFromSelectedCamera();
void GenerateUniqueCameraId(std::wstring& id);
std::wstring GenerateCameraClsidString(const std::wstring& cameraId);

// Helper to parse resolution from combo box
void GetSelectedResolution(UINT* width, UINT* height)
{
	auto index = SendMessage(_hwndComboResolution, CB_GETCURSEL, 0, 0);
	switch (index)
	{
	case 0: *width = 640; *height = 480; break;
	case 1: *width = 800; *height = 600; break;
	case 2: *width = 1024; *height = 768; break;
	case 3: *width = 1280; *height = 720; break;
	case 4: *width = 1920; *height = 1080; break;
	default: *width = 640; *height = 480; break;
	}
}

std::wstring GenerateCameraClsidString(const std::wstring& cameraId)
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
	
	// Convert to string
	wchar_t buffer[64];
	swprintf_s(buffer, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		clsid.Data1, clsid.Data2, clsid.Data3,
		clsid.Data4[0], clsid.Data4[1], clsid.Data4[2], clsid.Data4[3],
		clsid.Data4[4], clsid.Data4[5], clsid.Data4[6], clsid.Data4[7]);
	
	return std::wstring(buffer);
}

void GenerateUniqueCameraId(std::wstring& id)
{
	int counter = 1;
	while (true)
	{
		id = L"Camera" + std::to_wstring(counter);
		
		// Check if this ID already exists
		bool exists = false;
		for (const auto& camera : _cameras)
		{
			if (camera.id == id)
			{
				exists = true;
				break;
			}
		}
		
		if (!exists)
			break;
			
		counter++;
	}
}

HRESULT LoadCamerasFromRegistry()
{
	_cameras.clear();
	
	HKEY hKey;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinCamHTTP\\Cameras", 0, KEY_READ, &hKey);
	if (result != ERROR_SUCCESS)
	{
		// No cameras found, start with empty list
		return S_OK;
	}
	
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
			
			CameraConfig camera;
			camera.id = cameraId;
			
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
					// Read Enabled flag
					dataSize = sizeof(DWORD);
					DWORD enabled = 1;
					if (RegQueryValueExW(hCameraKey, L"Enabled", nullptr, nullptr, (LPBYTE)&enabled, &dataSize) == ERROR_SUCCESS)
					{
						camera.enabled = (enabled != 0);
					}
					else
					{
						camera.enabled = true;
					}
			
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

HRESULT SaveCamerasToRegistry()
{
	// First, delete the existing Cameras key and recreate it
	RegDeleteTree(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinCamHTTP\\Cameras");
	
	if (_cameras.empty())
	{
		SetWindowTextW(_hwndStatus, L"All cameras removed from registry.");
		return S_OK;
	}
	
	for (const auto& camera : _cameras)
	{
		std::wstring regPath = L"SOFTWARE\\WinCamHTTP\\Cameras\\" + camera.id;
		HKEY hKey;
		LSTATUS result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, nullptr,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
		if (result == ERROR_SUCCESS)
		{
			wil::unique_hkey keyGuard(hKey);
			
			// Save URL (cast byte count to DWORD to avoid size_t -> DWORD conversion warning)
			RegSetValueExW(hKey, L"URL", 0, REG_SZ, (LPBYTE)camera.url.c_str(), (DWORD)((camera.url.length() + 1) * sizeof(WCHAR)));
			
			// Save Width/Height
			RegSetValueExW(hKey, L"Width", 0, REG_DWORD, (LPBYTE)&camera.width, sizeof(DWORD));
			RegSetValueExW(hKey, L"Height", 0, REG_DWORD, (LPBYTE)&camera.height, sizeof(DWORD));
			
			// Save Friendly Name (cast byte count to DWORD to avoid size_t -> DWORD conversion warning)
			RegSetValueExW(hKey, L"FriendlyName", 0, REG_SZ, (LPBYTE)camera.friendlyName.c_str(), (DWORD)((camera.friendlyName.length() + 1) * sizeof(WCHAR)));

			// Save Enabled flag
			DWORD enabled = camera.enabled ? 1 : 0;
			RegSetValueExW(hKey, L"Enabled", 0, REG_DWORD, (LPBYTE)&enabled, sizeof(DWORD));
		}
		else
		{
			wchar_t errorMsg[512];
			swprintf_s(errorMsg, L"Failed to save camera '%s': Error %d. Make sure to run as Administrator.", camera.id.c_str(), result);
			SetWindowTextW(_hwndStatus, errorMsg);
			return HRESULT_FROM_WIN32(result);
		}
	}
	
	SetWindowTextW(_hwndStatus, L"All camera settings saved successfully!");
	return S_OK;
}

void RefreshCameraList()
{
	SendMessage(_hwndListCameras, LB_RESETCONTENT, 0, 0);
	
	for (const auto& camera : _cameras)
	{
		std::wstring displayText = camera.id + L" - " + camera.friendlyName;
		SendMessage(_hwndListCameras, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
	}
}

void PopulateFieldsFromSelectedCamera()
{
	int selectedIndex = (int)SendMessage(_hwndListCameras, LB_GETCURSEL, 0, 0);
	if (selectedIndex == LB_ERR || selectedIndex >= (int)_cameras.size())
	{
		// Clear fields
		SetWindowTextW(_hwndEditUrl, L"");
		SetWindowTextW(_hwndEditName, L"");
		SendMessage(_hwndComboResolution, CB_SETCURSEL, 0, 0);
		return;
	}
	
	const CameraConfig& camera = _cameras[selectedIndex];
	
	// Populate URL
	SetWindowTextW(_hwndEditUrl, camera.url.c_str());
	
	// Populate friendly name
	SetWindowTextW(_hwndEditName, camera.friendlyName.c_str());
	
	// Populate resolution
	int comboIndex = 0; // default to 640x480
	if (camera.width == 800 && camera.height == 600) comboIndex = 1;
	else if (camera.width == 1024 && camera.height == 768) comboIndex = 2;
	else if (camera.width == 1280 && camera.height == 720) comboIndex = 3;
	else if (camera.width == 1920 && camera.height == 1080) comboIndex = 4;
	SendMessage(_hwndComboResolution, CB_SETCURSEL, comboIndex, 0);

	// Populate enabled checkbox
	if (_hwndChkEnabled)
	{
		SendMessage(_hwndChkEnabled, BM_SETCHECK, camera.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

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
				TaskDialog(nullptr, nullptr, _title, L"A fatal error has occurred. Press OK to terminate.", str, TDCBF_OK_BUTTON, TD_WARNING_ICON, nullptr);
#endif
			}
		});

	LoadStringW(hInstance, IDS_APP_TITLE, _title, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINCAMHTTPSETUP, _windowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	auto hwnd = InitInstance(hInstance, nCmdShow);
	if (hwnd)
	{
		// Normal message loop for the setup UI
		HACCEL accelerators = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINCAMHTTPSETUP));
		MSG msg{};
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			if (!TranslateAccelerator(msg.hwnd, accelerators, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	// cleanup & CRT leak checks
	_CrtDumpMemoryLeaks();
	WINTRACE(L"WinMain exiting '%s'", GetCommandLineW());
	WinTraceUnregister();
	return 0;
}

HRESULT SaveSettingsToRegistry()
{
	int selectedIndex = (int)SendMessage(_hwndListCameras, LB_GETCURSEL, 0, 0);
	if (selectedIndex == LB_ERR || selectedIndex >= (int)_cameras.size())
	{
		SetWindowTextW(_hwndStatus, L"Please select a camera to save settings for.");
		return E_FAIL;
	}
	
	// Get values from UI
	wchar_t url[2048]{};
	if (_hwndEditUrl) GetWindowTextW(_hwndEditUrl, url, _countof(url));
	wchar_t friendlyName[256]{};
	if (_hwndEditName) GetWindowTextW(_hwndEditName, friendlyName, _countof(friendlyName));
	if (wcslen(friendlyName) == 0)
	{
		swprintf_s(friendlyName, L"WinCamHTTP Virtual Camera %s", _cameras[selectedIndex].id.c_str());
	}
	
	UINT width, height;
	GetSelectedResolution(&width, &height);
	
	// Update the camera in our list
	_cameras[selectedIndex].url = url;
	_cameras[selectedIndex].friendlyName = friendlyName;
	_cameras[selectedIndex].width = width;
	_cameras[selectedIndex].height = height;
	// Enabled checkbox
	if (_hwndChkEnabled)
	{
		LRESULT check = SendMessage(_hwndChkEnabled, BM_GETCHECK, 0, 0);
		_cameras[selectedIndex].enabled = (check == BST_CHECKED);
	}
	
	// Save to registry
	HRESULT hr = SaveCamerasToRegistry();
	
	// Refresh the list display
	RefreshCameraList();
	SendMessage(_hwndListCameras, LB_SETCURSEL, selectedIndex, 0);
	
	return hr;
}

// Helper: load persisted settings from registry into UI controls
static void LoadPersistedSettingsFromRegistry()
{
	LoadCamerasFromRegistry();
	RefreshCameraList();
	
	// Select first camera if any exist
	if (!_cameras.empty())
	{
		SendMessage(_hwndListCameras, LB_SETCURSEL, 0, 0);
		PopulateFieldsFromSelectedCamera();
	}
}

ATOM MyRegisterClass(HINSTANCE instance)
{
	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = instance;
	wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_WINCAMHTTPSETUP));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINCAMHTTPSETUP);
	wcex.lpszClassName = _windowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	return RegisterClassExW(&wcex);
}

HWND InitInstance(HINSTANCE instance, int cmd)
{
	_instance = instance;
	// Make the main window slightly larger so buttons at the bottom are not cut off on smaller displays
	auto hwnd = CreateWindowW(_windowClass, _title, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
		0, 0, 760, 560, nullptr, nullptr, instance, nullptr);
	if (!hwnd)
		return nullptr;

	_hwndMain = hwnd;

	// Create controls for the setup UI
	// Left: Camera list with Add/Remove
	CreateWindowW(L"STATIC", L"Virtual Cameras:", WS_VISIBLE | WS_CHILD,
		20, 20, 200, 20, hwnd, nullptr, instance, nullptr);
	_hwndListCameras = CreateWindowW(L"LISTBOX", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
		20, 45, 260, 260, hwnd, (HMENU)2001, instance, nullptr);

	// Add/Remove buttons under the list (tab order follows creation order)
	_hwndBtnAdd = CreateWindowW(L"BUTTON", L"Add Camera", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
		20, 315, 120, 30, hwnd, (HMENU)1002, instance, nullptr);
	_hwndBtnRemove = CreateWindowW(L"BUTTON", L"Remove Camera", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
		160, 315, 120, 30, hwnd, (HMENU)1003, instance, nullptr);

	// Right: Configuration group box
	CreateWindowW(L"BUTTON", L"Configuration for Selected Camera:", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
		300, 20, 360, 300, hwnd, nullptr, instance, nullptr);

	// URL
	CreateWindowW(L"STATIC", L"HTTP URL:", WS_VISIBLE | WS_CHILD,
		320, 50, 80, 20, hwnd, nullptr, instance, nullptr);
	_hwndEditUrl = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
		410, 48, 240, 22, hwnd, nullptr, instance, nullptr);

	// Camera Name
	CreateWindowW(L"STATIC", L"Camera Name:", WS_VISIBLE | WS_CHILD,
		320, 85, 80, 20, hwnd, nullptr, instance, nullptr);
	_hwndEditName = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
		410, 83, 240, 22, hwnd, nullptr, instance, nullptr);

	// Resolution
	CreateWindowW(L"STATIC", L"Resolution:", WS_VISIBLE | WS_CHILD,
		320, 120, 80, 20, hwnd, nullptr, instance, nullptr);
	_hwndComboResolution = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
		410, 118, 150, 100, hwnd, nullptr, instance, nullptr);

	// Enabled checkbox
	_hwndChkEnabled = CreateWindowW(L"BUTTON", L"Enabled", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
		320, 155, 100, 22, hwnd, (HMENU)1004, instance, nullptr);

	// Populate resolution combo box
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"640 x 480");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"800 x 600");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"1024 x 768");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"1280 x 720");
	SendMessage(_hwndComboResolution, CB_ADDSTRING, 0, (LPARAM)L"1920 x 1080");
	SendMessage(_hwndComboResolution, CB_SETCURSEL, 0, 0); // default to 640x480

	// Save, OK, Cancel buttons
	_hwndBtnSave = CreateWindowW(L"BUTTON", L"Save Settings", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
		410, 190, 120, 30, hwnd, (HMENU)1001, instance, nullptr);

	CreateWindowW(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_TABSTOP,
		500, 430, 80, 28, hwnd, (HMENU)IDOK, instance, nullptr);
	CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
		590, 430, 80, 28, hwnd, (HMENU)IDCANCEL, instance, nullptr);

	// Increase status control height so it doesn't overlap with bottom buttons on certain DPI/settings
	_hwndStatus = CreateWindowW(L"STATIC", L"Configure camera settings and save to registry. Note: This program must be run as Administrator to save settings.", 
		WS_VISIBLE | WS_CHILD | SS_LEFT,
		20, 360, 700, 60, hwnd, nullptr, instance, nullptr);

	// Load any existing settings
	LoadPersistedSettingsFromRegistry();

	CenterWindow(hwnd);
	ShowWindow(hwnd, cmd);
	UpdateWindow(hwnd);
	return hwnd;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		auto wmId = LOWORD(wParam);
		auto wmEvent = HIWORD(wParam);
		
		switch (wmId)
		{
		case 1001: // Save Settings button
			SaveSettingsToRegistry();
			break;

		case 1002: // Add Camera button
		{
			std::wstring newId;
			GenerateUniqueCameraId(newId);
			
			CameraConfig newCamera;
			newCamera.id = newId;
			newCamera.url = L"";
			newCamera.friendlyName = L"WinCamHTTP Virtual Camera " + newId;
			newCamera.width = 640;
			newCamera.height = 480;
			newCamera.enabled = true;
			
			_cameras.push_back(newCamera);
			RefreshCameraList();
			
			// Select the new camera
			int newIndex = (int)_cameras.size() - 1;
			SendMessage(_hwndListCameras, LB_SETCURSEL, newIndex, 0);
			PopulateFieldsFromSelectedCamera();
			
			SetWindowTextW(_hwndStatus, L"New camera added. Configure settings and save.");
		}
		break;

		case 1003: // Remove Camera button
		{
			int selectedIndex = (int)SendMessage(_hwndListCameras, LB_GETCURSEL, 0, 0);
			if (selectedIndex != LB_ERR && selectedIndex < (int)_cameras.size())
			{
				_cameras.erase(_cameras.begin() + selectedIndex);
				RefreshCameraList();
				
				// Select previous camera if available
				if (!_cameras.empty())
				{
					int newIndex = (selectedIndex > 0) ? selectedIndex - 1 : 0;
					if (newIndex < (int)_cameras.size())
					{
						SendMessage(_hwndListCameras, LB_SETCURSEL, newIndex, 0);
						PopulateFieldsFromSelectedCamera();
					}
				}
				else
				{
					// Clear fields if no cameras left
					PopulateFieldsFromSelectedCamera();
				}
				
				SetWindowTextW(_hwndStatus, L"Camera removed. Save to update registry.");
			}
			else
			{
				SetWindowTextW(_hwndStatus, L"Please select a camera to remove.");
			}
		}
		break;

		case 2001: // Camera listbox
			if (wmEvent == LBN_SELCHANGE)
			{
				PopulateFieldsFromSelectedCamera();
			}
			break;

		case IDM_ABOUT:
			DialogBox(_instance, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, About);
			break;

		case IDM_EXIT:
			DestroyWindow(hwnd);
			break;

		case IDOK:
		{
			// Save settings for selected camera (if any) then exit
			SaveSettingsToRegistry();
			DestroyWindow(hwnd);
		}
		break;

		case IDCANCEL:
		{
			// Discard and exit
			DestroyWindow(hwnd);
		}
		break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		auto hdc = BeginPaint(hwnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hwnd, &ps);
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

INT_PTR CALLBACK About(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hwnd, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
