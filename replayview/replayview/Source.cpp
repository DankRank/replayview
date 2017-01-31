#include <wchar.h>
#include <Windows.h>
#include <stdint.h>
#include "resource.h"
#include "Utility.h"

bool checkMagic(const uint8_t* buf) {
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

wchar_t* SJIS_to_WCHAR(const char* str, DWORD len) {
	int wlen = MultiByteToWideChar(932, 0, str, len, nullptr, 0);
	wchar_t *wstr = new wchar_t[wlen+1];
	MultiByteToWideChar(932, 0, str, len, wstr, wlen);
	wstr[wlen] = 0; // just in case original string didn't have one
	return wstr;
}

char* WCHAR_to_SJIS(const wchar_t* wstr, DWORD wlen) {
	BOOL b;
	int len = WideCharToMultiByte(932, 0, wstr, wlen, nullptr, 0, nullptr, &b);
	char *str = new char[len + 1];
	WideCharToMultiByte(932, 0, wstr, wlen, str, len, nullptr, &b);
	str[len] = 0; // just in case original string didn't have one
	return str;
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

class DialogData{
public:
	wchar_t fileName[MAX_PATH] = { 0 };
	const uint8_t *buffer = nullptr;
	const char* gameInfo = nullptr;
	const char* comment = nullptr;
	DWORD fileSize = 0;

	int locateSections();
	bool Save(wchar_t* wcomment, int wlen);
	void Cleanup();
	~DialogData();
};

int DialogData::locateSections() {
	if (buffer && fileSize >= 0x10 && checkMagic(buffer)) {
		uint32_t offset = *(uint32_t*)&buffer[0xC];
		int32_t bytesLeft = fileSize - offset;
		while (bytesLeft > 0xC) {
			if (TO_MAGIC('U', 'S', 'E', 'R') == *(uint32_t*)&buffer[offset]) {
				switch (buffer[offset + 8]) {
				case 0:
					gameInfo = (char*)&buffer[offset];
					break;
				case 1:
					comment = (char*)&buffer[offset];
					break;
				}
			}
			int32_t section_size = *(uint32_t*)&buffer[offset + 4];
			if (section_size > bytesLeft) {
				section_size = bytesLeft;
				// Modify the value inside the structure, so that the dialog doesn't read past the EOF
				*(uint32_t*)&buffer[offset + 4] = bytesLeft;
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
bool DialogData::Save(wchar_t* wcomment, int wlen) {
	if (!buffer) return false;

	uint32_t offset = *(uint32_t*)&buffer[0xC];
	int nbSize = fileSize + 0x1FFFE;
	// TODO: fix this ZUN hackery
	uint8_t *nbuffer = new uint8_t[nbSize];
	memset(nbuffer, 0, nbSize);
	// TODO: check if offset is sane using math.
	if (memcpy_s(nbuffer, nbSize, buffer, offset)) {
		delete[] nbuffer;
		return false;
	}

	uint32_t* nptr = (uint32_t*)&nbuffer[offset];

	// copy over the gameinfo section
	uint32_t gameInfoSize = 0;
	if (gameInfo) {
		gameInfoSize = *(uint32_t*)&gameInfo[0x4];
		if (memcpy_s(nptr, nbSize - offset, gameInfo, gameInfoSize)) {
			delete[] nbuffer;
			return false;
		}
		// TODO: make all of this ptr conversion magic less annoying to read
		nptr = (uint32_t*)((uint8_t*)nptr + *(uint32_t*)&gameInfo[0x4]);
	}

	// add comment section
	if (!comment) {
		// byte 7 is USER chunk count. In th095+ it's always 0.
		++nbuffer[7];
	}
	// NOTE: let's not remove the arbitrary 0xFFFF limit, so that we don't crash original implementation.
	char* acomment = WCHAR_to_SJIS(wcomment, wlen);
	if (strlen(acomment) >= 0xFFFF) {
		acomment[0xFFFF] = 0;
	}

	// TODO: add more buffer overflow checks
	nptr[0] = TO_MAGIC('U', 'S', 'E', 'R');
	nptr[1] = 12 + strlen(acomment) + 1;
	nptr[2] = 1;
	strcpy((char*)&nptr[3], acomment);
	writeFile(fileName, offset + gameInfoSize + 12 + strlen(acomment) + 1, nbuffer);
	delete[] acomment;
	delete[] nbuffer;
	return true;
}
void DialogData::Cleanup() {
	delete[] buffer;
	buffer = nullptr;
	gameInfo = nullptr;
	comment = nullptr;
	fileSize = 0;
}
DialogData::~DialogData() {
	Cleanup();
}
int __stdcall DialogFunc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	DialogData *d = reinterpret_cast<DialogData*>( GetWindowLongPtr(hWnd, GWL_USERDATA) );

	if (uMsg == WM_INITDIALOG) {
		SetWindowLongPtr(hWnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>( new DialogData() ));
		return 1;
	}

	else if (uMsg == WM_NCDESTROY) {
		delete d;
		return 1;
	}

	else if (uMsg == WM_COMMAND) {
		switch (LOWORD(wParam)) {
		case IDC_SAVE: {
			wchar_t* wcomment = new wchar_t[0xFFFF]();
			int wlen = GetDlgItemText(hWnd, IDC_COMMENT, wcomment, 0xFFFF);
			if (d->Save(wcomment, wlen)) {
				EndDialog(hWnd, IDC_SAVE);
			}
			delete[] wcomment;
			return 1;
		}
		case IDOK:
		case IDCANCEL:
		case IDC_CLOSE:
			EndDialog(hWnd, wParam);
		}
		return 1;
	}

	else if (uMsg == WM_DROPFILES) {
		SetForegroundWindow(hWnd);

		d->Cleanup();
		
		if (DragQueryFile((HDROP)wParam, -1, 0, 0) > 0) {
			DragQueryFile((HDROP)wParam, 0, d->fileName, MAX_PATH);
			d->buffer = readFile(d->fileName, &d->fileSize);
			if (d->locateSections()) {
				MessageBox(hWnd, L"これはリプレイファイルではないか\nバージョンが違う", L"失敗", 0);
				d->Cleanup();
			}
		}
		DragFinish((HDROP)wParam);
		if (!d->buffer)
			return 1;

		// filename without path
		wchar_t *fileName2 = wcsrchr(d->fileName, L'\\');
		if (!fileName2) {
			fileName2 = wcsrchr(d->fileName, L'/');
			if (!fileName2) {
				fileName2 = d->fileName;
			}
			else fileName2++;
		}
		else fileName2++;
		SetDlgItemText(hWnd, IDC_FILENAME, fileName2);
		if (d->gameInfo) {
			wchar_t* wgameInfo = SJIS_to_WCHAR(&d->gameInfo[12], *(DWORD*)&d->gameInfo[4] - 12);
			SetDlgItemText(hWnd, IDC_GAMEINFO, wgameInfo);
			delete[] wgameInfo;

		}
		if (d->comment) {
			wchar_t* wcomment = SJIS_to_WCHAR(&d->comment[12], *(DWORD*)&d->comment[4] - 12);
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