#include <wchar.h>
#include <Windows.h>
#include <stdint.h>
#include "resource.h"
#include "Utility.h"

wchar_t fileName[MAX_PATH] = {0};
uint8_t *buffer = nullptr;
char* gameInfo = nullptr;
char* comment = nullptr;

bool checkMagic(uint8_t* buf) {
	uint32_t magic[] = {
		TO_MAGIC( 'T', '8', 'R', 'P' ), // eiyashou (in)
		TO_MAGIC( 'T', '9', 'R', 'P' ), // kaeidzuka (pofv)
		TO_MAGIC( 't', '9', '5', 'r' ), // bunkachou (stb)
		TO_MAGIC( 't', '1', '0', 'r' ), // fuujinroku (mof)
		TO_MAGIC( 'a', 'l', '1', 'r' ), // tasogare sakaba (algostg)
		TO_MAGIC( 't', '1', '1', 'r' ), // chireiden (sa)
		TO_MAGIC( 't', '1', '2', 'r' ), // seirensen (ufo)
		TO_MAGIC( 't', '1', '2', '5' ), // bunkachou (ds)
		TO_MAGIC( '1', '2', '8', 'r' ), // yousei daisensou
		TO_MAGIC( 't', '1', '3', 'r' ), // shinreibyou (td)
		// kishinjou (ddc) - has the same id as th13 for some reason
		TO_MAGIC( 't', '1', '4', '3' ), // danmaku amanojaku (isc)
		TO_MAGIC( 't', '1', '5', 'r' ), // kanjuden (lolk)
	};
	for (int i = 0; i < ARRAY_SIZE(magic); i++) {
		if (magic[i] == *(uint32_t*)buf) {
			return true;
		}
	}
	return false;
}

wchar_t* SJIS_to_WCHAR(char* str, DWORD len) {
	int wlen = MultiByteToWideChar(932, 0, str, len, nullptr, 0);
	wchar_t *wstr = new wchar_t[wlen+1];
	MultiByteToWideChar(932, 0, str, len, wstr, wlen);
	wstr[wlen] = 0; // just in case original string didn't have one
	return wstr;
}

int locateSections(uint8_t* buf, DWORD fileSize, char** gameInfo, char** comment) {
	if (buf && fileSize >= 0x10 && checkMagic(buf)) {
		uint32_t offset = *(uint32_t*)&buf[0xC];
		int32_t bytesLeft = fileSize - offset;
		while (bytesLeft > 0xC) {
			if (TO_MAGIC('U','S','E','R') == *(uint32_t*)&buf[offset]) {
				switch (buf[offset + 8]) {
				case 0:
					*gameInfo = (char*)&buf[offset];
					break;
				case 1:
					*comment = (char*)&buf[offset];
					break;
				}
			}
			int32_t section_size = *(uint32_t*)&buf[offset+4];
			if (section_size > bytesLeft) {
				section_size = bytesLeft;
				// Modify the value inside the structure, so that the dialog doesn't read past the EOF
				*(uint32_t*)&buf[offset + 4] = bytesLeft;
			}
			offset += section_size;
			bytesLeft -= section_size;
		}
		return 0;
	}
	else {
		return 1;
	}
}

uint8_t *readFile(const wchar_t* fileName, DWORD* outFileSize) {
	HANDLE file = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return nullptr;
	}
	DWORD fileSize = GetFileSize(file, nullptr);
	uint8_t* buf = (uint8_t*)malloc(fileSize);
	if (!buf) {
		CloseHandle(file);
		return nullptr;
	}
	ReadFile(file, buf, fileSize, &fileSize, nullptr);
	if (fileSize) *outFileSize = fileSize;
	CloseHandle(file);
	return buf;
}

int writeFile(const wchar_t* fileName, DWORD fileSize, uint8_t* buf) {
	HANDLE file = CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return -1;
	}
	DWORD bytesWritten;
	WriteFile(file, buf, fileSize, &bytesWritten, nullptr);
	CloseHandle(file);
	if( fileSize == bytesWritten ){
		return 0;
	}
	else {
		return -2;
	}
}

int __stdcall DialogFunc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (uMsg == WM_INITDIALOG) {
		return 1;
	}

	else if (uMsg == WM_COMMAND) {
		switch (LOWORD(wParam)) {
		case IDC_SAVE:
			// NYI
			break;
		case IDOK:
		case IDCANCEL:
		case IDC_CLOSE:
			delete[] buffer;
			EndDialog(hWnd, wParam);
		}
		return 1;
	}

	else if (uMsg == WM_DROPFILES) {
		SetForegroundWindow(hWnd);

		delete[] buffer;
		buffer = nullptr;
		gameInfo = nullptr;
		comment = nullptr;
		DWORD fileSize;

		if (DragQueryFile((HDROP)wParam, -1, 0, 0) > 0) {
			DragQueryFile((HDROP)wParam, 0, fileName, MAX_PATH);
			buffer = readFile(fileName, &fileSize);
			if (locateSections(buffer, fileSize, &gameInfo, &comment)) {
				MessageBox(hWnd, L"これはリプレイファイルではないか\nバージョンが違う", L"失敗", 0);
				delete[] buffer;
				buffer = nullptr;
				gameInfo = nullptr;
				comment = nullptr;
			}
		}
		DragFinish((HDROP)wParam);
		if (!buffer)
			return 1;

		// filename without path
		wchar_t *fileName2 = wcsrchr(fileName, L'\\');
		if (!fileName2) {
			fileName2 = wcsrchr(fileName, L'/');
			if (!fileName2) {
				fileName2 = fileName;
			}
			else fileName2++;
		}
		else fileName2++;
		SetDlgItemText(hWnd, IDC_FILENAME, fileName2);
		if (gameInfo) {
			wchar_t* wgameInfo = SJIS_to_WCHAR(&gameInfo[12], *(DWORD*)&gameInfo[4]);
			SetDlgItemText(hWnd, IDC_GAMEINFO, wgameInfo);
			delete[] wgameInfo;

		}
		if (comment) {
			wchar_t* wcomment = SJIS_to_WCHAR(&comment[12], *(DWORD*)&comment[4]);
			SetDlgItemText(hWnd, IDC_COMMENT, wcomment);
			delete[] wcomment;
		}
		else {
			SetDlgItemText(hWnd, IDC_COMMENT, L"");
		}
		return 1;
	}

	return 0;
}
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), nullptr, DialogFunc);
	return 0;
}