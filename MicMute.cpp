#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <Windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include <iostream>
#include <stdio.h>
#include <thread>
#include <chrono>
#include <iphlpapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include "Defs.h"
#include "resource.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

HANDLE hKeyboardHook;
IMMDeviceEnumerator* pDeviceEnumerator;
IMMDevice* pMicDevice;
IAudioEndpointVolume* pMicVolume;
CHAR buffer[256];
SOCKET client;
sockaddr_in addr;
BOOL bAbortThreads = FALSE;
BOOL bLastMute;

DWORD WaitForServerStart(PUSHORT pLocalPort) {
	DWORD dwPID = -1;

	while (dwPID == -1) {
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		if (hSnapshot == INVALID_HANDLE_VALUE) {
			return SNAPSHOT_FAILED;
		}

		PROCESSENTRY32 procEntry;
		procEntry.dwSize = sizeof(PROCESSENTRY32);

		BOOL bResult = Process32FirstW(hSnapshot, &procEntry);

		while (bResult) {
			if (_tcscmp(SERVER_PROC_NAME, procEntry.szExeFile) == 0) {
				dwPID = procEntry.th32ProcessID;
				break;
			}
			bResult = Process32Next(hSnapshot, &procEntry);
		}

		CloseHandle(hSnapshot);
		std::this_thread::sleep_for(std::chrono::milliseconds(3000));
	}

	std::cout << "Server PID: " << dwPID << "\n";

	DWORD dwSize = 0;
	DWORD dwRetVal = GetExtendedTcpTable(NULL, &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

	PMIB_TCPTABLE_OWNER_PID pTcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(new char[dwSize]);

	dwRetVal = GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

	USHORT uLocalPort = 0;

	if (dwRetVal == NO_ERROR) {
		for (int i = 0; i < (INT)pTcpTable->dwNumEntries; i++) {
			if (pTcpTable->table[i].dwOwningPid == dwPID) {
				USHORT localPort = ntohs((USHORT) pTcpTable->table[i].dwLocalPort);
				uLocalPort = localPort;
				break;
			}
		}
	}

	if (pTcpTable != NULL) {
		delete[] pTcpTable;
		pTcpTable = NULL;
	}

	if (!uLocalPort) {
		return PORT_FIND_FAILED;
	}

	*pLocalPort = uLocalPort;
	return SUCCESSFUL;
}

VOID SendCommand(PCCH szCommand) {
	strcpy_s(buffer, szCommand);
	send(client, buffer, strlen(buffer), 0);
}

VOID StartKeepalive() {
	std::cout << "Starting keepalive messaging..\n";
	std::thread([]() {
		while (!bAbortThreads) {
			auto x = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
			SendCommand(KEEPALIVE_MSG);
			std::this_thread::sleep_until(x);
		}
	}).detach();
}

VOID StartMuteWatchdog() {
	if (!pMicVolume) {
		return;
	}
	std::cout << "Starting mute watchdog..\n";
	std::thread([]() {
		while (!bAbortThreads) {
			auto x = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
			BOOL bMute;
			pMicVolume->GetMute(&bMute);
			if (bMute != bLastMute) {
				bLastMute = bMute;
				if (bMute) {
					SendCommand(MODE_COLOR_MSG);
					SendCommand(COLOR_RED_MSG);
				} else {
					SendCommand(MODE_AUDIO_MSG);
				}
			}
			std::this_thread::sleep_until(x);
		}
	}).detach();
}

DWORD StartFocusriteIntegration() {
	USHORT uLocalPort;
	DWORD dwError = WaitForServerStart(&uLocalPort);
	if (dwError != SUCCESSFUL) {
		return dwError;
	}
	std::cout << "Local port: " << uLocalPort << "\n";

	WSADATA wsaData;
	INT result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		return WSA_START_FAILED;
	}

	client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client == INVALID_SOCKET) {
		WSACleanup();
		return WSA_INVALID_SOCKET;
	}

	addr = {
		.sin_family = AF_INET,
		.sin_port = htons(uLocalPort)
	};
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	result = connect(client, (SOCKADDR*)&addr, sizeof(addr));
	if (result == SOCKET_ERROR) {
		closesocket(client);
		WSACleanup();
		return WSA_SOCKET_ERROR;
	}

	SendCommand(INIT_MSG);
	StartKeepalive();
	StartMuteWatchdog();

	return SUCCESSFUL;
}

LRESULT CALLBACK KeyboardHookCallback(int code, WPARAM wParam, LPARAM lParam) {
	if (code != 0 || lParam == 0 || (wParam != WM_KEYDOWN && wParam != WM_KEYUP)) {
		return CallNextHookEx(0, code, wParam, lParam);
	}
	KBDLLHOOKSTRUCT* pHookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
	if (pHookStruct->vkCode == FUNCTION_KEY && wParam == WM_KEYUP) {
		BOOL bMuted;
		pMicVolume->GetMute(&bMuted);
		if (bMuted) {
			pMicVolume->SetMute(FALSE, NULL);
#ifdef USE_SOUND
			PlaySound((PWCH) IDR_WAVE1, NULL, SND_RESOURCE | SND_ASYNC);
#endif
		} else {
			pMicVolume->SetMute(TRUE, NULL);
#ifdef USE_SOUND
			PlaySound((PWCH)IDR_WAVE2, NULL, SND_RESOURCE | SND_ASYNC);
#endif
		}
		
	}
	return CallNextHookEx(NULL, code, wParam, lParam);
}

DWORD StartMicIntegration() {
	HRESULT hResult = CoInitialize(NULL);
	if (hResult != S_OK) {
		return COM_INIT_FAILED;
	}
	if (CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (PVOID*)&pDeviceEnumerator) != S_OK) {
		return COCREATE_FAILED;
	}
	pDeviceEnumerator->GetDefaultAudioEndpoint(EDataFlow::eCapture, ERole::eCommunications, &pMicDevice);
	pMicDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (PVOID*)&pMicVolume);
	hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, NULL, 0);
	return SUCCESSFUL;
}

INT APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ INT nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

#ifdef USE_CONSOLE
	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
#endif

	CreateMutex(NULL, FALSE, MUTEX_NAME);
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return ALREADY_RUNNING;
	}

	DWORD dwError = StartMicIntegration();
	if (dwError) {
		return dwError;
	}

#ifdef USE_FOCUSRITE
	dwError = StartFocusriteIntegration();
	if (dwError) {
		return dwError;
	}
#endif

	BOOL bRet;
	MSG msg;
	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	closesocket(client);
	WSACleanup();
	bAbortThreads = TRUE;

	return SUCCESSFUL;
}
