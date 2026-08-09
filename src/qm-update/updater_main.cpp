#if !defined(_WIN32)
#error QmClient-Updater is Windows-only
#endif

#include <engine/shared/qm_update.h>

#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <iterator>
#include <string>
#include <thread>

namespace
{
	constexpr wchar_t WINDOW_CLASS[] = L"QmClientUpdateWindow";
	constexpr wchar_t WINDOW_TITLE[] = L"QmClient 更新";

	struct SArguments
	{
		DWORD m_ParentPid = 0;
		std::wstring m_Package;
		std::wstring m_PackageSignature;
		std::wstring m_Manifest;
		std::wstring m_ManifestSignature;
		std::wstring m_Install;
		bool m_Elevated = false;
	};

	HWND g_hStatus = nullptr;
	HWND g_hProgress = nullptr;
	bool g_AllowClose = false;

	void SetStatus(const wchar_t *pText, bool Indeterminate)
	{
		if(g_hStatus)
			SetWindowTextW(g_hStatus, pText);
		if(g_hProgress)
		{
			LONG_PTR Style = GetWindowLongPtrW(g_hProgress, GWL_STYLE);
			if(Indeterminate)
				Style |= PBS_MARQUEE;
			else
				Style &= ~PBS_MARQUEE;
			SetWindowLongPtrW(g_hProgress, GWL_STYLE, Style);
			SendMessageW(g_hProgress, PBM_SETMARQUEE, Indeterminate ? TRUE : FALSE, 35);
			if(!Indeterminate)
				SendMessageW(g_hProgress, PBM_SETPOS, 100, 0);
		}
	}

	void PumpMessages()
	{
		MSG Message;
		while(PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
		{
			if(Message.message == WM_QUIT)
				return;
			TranslateMessage(&Message);
			DispatchMessageW(&Message);
		}
	}

	bool WaitForParent(DWORD ParentPid)
	{
		if(ParentPid == 0)
			return true;
		HANDLE Parent = OpenProcess(SYNCHRONIZE, FALSE, ParentPid);
		if(!Parent)
			return GetLastError() == ERROR_INVALID_PARAMETER;
		while(true)
		{
			PumpMessages();
			const DWORD Result = WaitForSingleObject(Parent, 50);
			if(Result == WAIT_OBJECT_0)
				break;
			if(Result == WAIT_FAILED)
			{
				CloseHandle(Parent);
				return false;
			}
		}
		CloseHandle(Parent);
		return true;
	}

	std::wstring CurrentExecutablePath()
	{
		wchar_t aPath[MAX_PATH * 4];
		const DWORD Length = GetModuleFileNameW(nullptr, aPath, static_cast<DWORD>(std::size(aPath)));
		return Length > 0 && Length < std::size(aPath) ? std::wstring(aPath, Length) : std::wstring();
	}

	bool ParseArguments(SArguments &Arguments)
	{
		int Argc = 0;
		wchar_t **ppArgv = CommandLineToArgvW(GetCommandLineW(), &Argc);
		if(!ppArgv)
			return false;
		bool Valid = true;
		for(int Index = 1; Index < Argc && Valid; ++Index)
		{
			const std::wstring Name = ppArgv[Index];
			const auto ReadValue = [&](std::wstring &Value) {
				if(Index + 1 >= Argc)
					return false;
				Value = ppArgv[++Index];
				return !Value.empty();
			};
			if(Name == L"--parent-pid")
			{
				std::wstring Value;
				if(!ReadValue(Value))
				{
					Valid = false;
					continue;
				}
				try
				{
					size_t ParsedLength = 0;
					const unsigned long Pid = std::stoul(Value, &ParsedLength);
					if(ParsedLength != Value.size() || Pid == 0 || Pid > MAXDWORD)
						Valid = false;
					else
						Arguments.m_ParentPid = static_cast<DWORD>(Pid);
				}
				catch(...)
				{
					Valid = false;
				}
			}
			else if(Name == L"--package")
			{
				if(!ReadValue(Arguments.m_Package))
					Valid = false;
			}
			else if(Name == L"--package-signature")
			{
				if(!ReadValue(Arguments.m_PackageSignature))
					Valid = false;
			}
			else if(Name == L"--manifest")
			{
				if(!ReadValue(Arguments.m_Manifest))
					Valid = false;
			}
			else if(Name == L"--manifest-signature")
			{
				if(!ReadValue(Arguments.m_ManifestSignature))
					Valid = false;
			}
			else if(Name == L"--install")
			{
				if(!ReadValue(Arguments.m_Install))
					Valid = false;
			}
			else if(Name == L"--qm-elevated")
				Arguments.m_Elevated = true;
			else
				Valid = false;
		}
		LocalFree(ppArgv);
		return Valid && Arguments.m_ParentPid != 0 && !Arguments.m_Package.empty() && !Arguments.m_PackageSignature.empty() &&
		       !Arguments.m_Manifest.empty() && !Arguments.m_ManifestSignature.empty() && !Arguments.m_Install.empty();
	}

	std::wstring QuoteArgument(const std::wstring &Value)
	{
		std::wstring Result = L"\"";
		unsigned Backslashes = 0;
		for(const wchar_t Character : Value)
		{
			if(Character == L'\\')
			{
				++Backslashes;
				continue;
			}
			if(Character == L'\"')
				Result.append(Backslashes * 2 + 1, L'\\');
			else
				Result.append(Backslashes, L'\\');
			Backslashes = 0;
			Result.push_back(Character);
		}
		Result.append(Backslashes * 2, L'\\');
		Result.push_back(L'\"');
		return Result;
	}

	bool RelaunchElevated()
	{
		const std::wstring Executable = CurrentExecutablePath();
		if(Executable.empty())
			return false;
		int Argc = 0;
		wchar_t **ppArgv = CommandLineToArgvW(GetCommandLineW(), &Argc);
		if(!ppArgv)
			return false;
		std::wstring Parameters;
		for(int Index = 1; Index < Argc; ++Index)
		{
			if(!Parameters.empty())
				Parameters.push_back(L' ');
			Parameters += QuoteArgument(ppArgv[Index]);
		}
		if(!Parameters.empty())
			Parameters.push_back(L' ');
		Parameters += L"--qm-elevated";
		LocalFree(ppArgv);
		SHELLEXECUTEINFOW Info{};
		Info.cbSize = sizeof(Info);
		Info.fMask = SEE_MASK_NOCLOSEPROCESS;
		Info.lpVerb = L"runas";
		Info.lpFile = Executable.c_str();
		Info.lpParameters = Parameters.c_str();
		Info.nShow = SW_SHOWNORMAL;
		if(!ShellExecuteExW(&Info))
			return false;
		if(Info.hProcess)
			CloseHandle(Info.hProcess);
		return true;
	}

	LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
	{
		if(Message == WM_CLOSE)
		{
			if(!g_AllowClose)
				return 0;
			DestroyWindow(hWnd);
			return 0;
		}
		if(Message == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(hWnd, Message, wParam, lParam);
	}

	HWND CreateStatusWindow(HINSTANCE Instance)
	{
		WNDCLASSW Class{};
		Class.hInstance = Instance;
		Class.lpfnWndProc = WindowProc;
		Class.lpszClassName = WINDOW_CLASS;
		Class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		if(!RegisterClassW(&Class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			return nullptr;
		HWND Window = CreateWindowExW(WS_EX_DLGMODALFRAME, WINDOW_CLASS, WINDOW_TITLE, WS_CAPTION | WS_SYSMENU,
			CW_USEDEFAULT, CW_USEDEFAULT, 460, 150, nullptr, nullptr, Instance, nullptr);
		if(!Window)
			return nullptr;
		g_hStatus = CreateWindowW(L"STATIC", L"正在等待客户端退出…", WS_CHILD | WS_VISIBLE,
			20, 20, 410, 24, Window, nullptr, Instance, nullptr);
		g_hProgress = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
			20, 60, 410, 22, Window, nullptr, Instance, nullptr);
		SendMessageW(g_hProgress, PBM_SETMARQUEE, TRUE, 35);
		ShowWindow(Window, SW_SHOWNORMAL);
		UpdateWindow(Window);
		return Window;
	}

	bool IsPermissionError(const std::string &Error)
	{
		std::string Lower = Error;
		std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
		return Lower.find("access is denied") != std::string::npos || Lower.find("permission denied") != std::string::npos || Lower.find("os error 5") != std::string::npos;
	}

	bool ToUtf8(const std::wstring &Value, std::string &Result)
	{
		const int Length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Value.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if(Length <= 0)
			return false;
		Result.resize(static_cast<size_t>(Length));
		if(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Value.c_str(), -1, Result.data(), Length, nullptr, nullptr) <= 0)
			return false;
		Result.resize(static_cast<size_t>(Length - 1));
		return true;
	}

	void CleanupPath(const std::wstring &Path, bool DelayIfLocked)
	{
		if(Path.empty())
			return;
		if(DeleteFileW(Path.c_str()) == 0 && DelayIfLocked)
			MoveFileExW(Path.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
	}

	void CleanupTemporaryFiles(const SArguments &Arguments)
	{
		CleanupPath(Arguments.m_Package, false);
		CleanupPath(Arguments.m_PackageSignature, false);
		CleanupPath(Arguments.m_Manifest, false);
		CleanupPath(Arguments.m_ManifestSignature, false);
		CleanupPath(CurrentExecutablePath(), true);
	}
}

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, PWSTR, int)
{
	INITCOMMONCONTROLSEX CommonControls{sizeof(CommonControls), ICC_PROGRESS_CLASS};
	InitCommonControlsEx(&CommonControls);
	SArguments Arguments;
	if(!ParseArguments(Arguments))
	{
		MessageBoxW(nullptr, L"更新参数无效。", WINDOW_TITLE, MB_OK | MB_ICONERROR);
		return 2;
	}
	HWND Window = CreateStatusWindow(Instance);
	if(!Window)
	{
		CleanupTemporaryFiles(Arguments);
		return 3;
	}
	if(!WaitForParent(Arguments.m_ParentPid))
	{
		SetStatus(L"无法等待客户端退出。", false);
		MessageBoxW(Window, L"无法确认客户端已退出，更新已取消。", WINDOW_TITLE, MB_OK | MB_ICONERROR);
		CleanupTemporaryFiles(Arguments);
		g_AllowClose = true;
		DestroyWindow(Window);
		return 1;
	}
	SetStatus(L"正在验证并安装更新…", true);
	char aError[1024] = "";
	std::string Package;
	std::string PackageSignature;
	std::string Manifest;
	std::string ManifestSignature;
	std::string Install;
	if(!ToUtf8(Arguments.m_Package, Package) || !ToUtf8(Arguments.m_PackageSignature, PackageSignature) ||
		!ToUtf8(Arguments.m_Manifest, Manifest) || !ToUtf8(Arguments.m_ManifestSignature, ManifestSignature) ||
		!ToUtf8(Arguments.m_Install, Install))
	{
		SetStatus(L"更新路径无效。", false);
		MessageBoxW(Window, L"更新路径不是有效的 UTF-8。", WINDOW_TITLE, MB_OK | MB_ICONERROR);
		CleanupTemporaryFiles(Arguments);
		g_AllowClose = true;
		DestroyWindow(Window);
		return 1;
	}
	std::atomic<bool> Finished = false;
	bool Success = false;
	std::thread Worker([&] {
		Success = qm_update_apply(Package.c_str(), PackageSignature.c_str(), Manifest.c_str(), ManifestSignature.c_str(), Install.c_str(), aError, sizeof(aError));
		Finished.store(true, std::memory_order_release);
	});
	while(!Finished.load(std::memory_order_acquire))
	{
		PumpMessages();
		Sleep(20);
	}
	Worker.join();
	if(!Success && !Arguments.m_Elevated && IsPermissionError(aError) && RelaunchElevated())
	{
		DestroyWindow(Window);
		return 0;
	}
	if(Success)
	{
		SetStatus(L"更新完成，客户端即将退出。", false);
		Sleep(450);
	}
	else
	{
		SetStatus(L"更新失败。", false);
		MessageBoxA(Window, aError[0] ? aError : "Update failed", "QmClient update", MB_OK | MB_ICONERROR);
	}
	CleanupTemporaryFiles(Arguments);
	g_AllowClose = true;
	DestroyWindow(Window);
	return Success ? 0 : 1;
}
