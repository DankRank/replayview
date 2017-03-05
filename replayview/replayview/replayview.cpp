#include <wchar.h>
#include <tchar.h>
#include <Windows.h>
#include <Shlwapi.h>
#include <cstdlib>
#include "compat.h"
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

uint8_t *readFile(const TCHAR* fileName, DWORD* outFileSize) {
	HANDLE file = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return nullptr;
	}
	DWORD fileSize = GetFileSize(file, nullptr);
	uint8_t* buf = new uint8_t[fileSize];
	if (!buf) {
		CloseHandle(file);
		return nullptr;
	}
	ReadFile(file, buf, fileSize, &fileSize, nullptr);
	if (fileSize) *outFileSize = fileSize;
	CloseHandle(file);
	return buf;
}

int writeFile(const TCHAR* fileName, DWORD fileSize, uint8_t* buf) {
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

enum UserChunkType {
	UCT_GAMEINFO = 0,
	UCT_COMMENT = 1
};

struct UserChunk{
	uint32_t magic;
	uint32_t size;
	uint32_t type;
	char* text() const {
		return (char*)(this + 1);
	}
};

static constexpr TCHAR *REPLAY_VIEW_PROP = _T("ReplayViewProp");

class DialogData{
public:
	HINSTANCE hInstance;
	TCHAR *fileName;
	const uint8_t *buffer;
	const UserChunk* gameInfo;
	const UserChunk* comment;
	DWORD fileSize;
	wchar_t *errCaption;
	wchar_t *errText;
	
	explicit DialogData(HINSTANCE hInst);
	int locateSections();
	bool Save(TCHAR* wcomment, int wlen);
	void Cleanup();
	~DialogData();
	
	static inline void Prop(HWND hWnd, DialogData *d);
	static inline DialogData* Prop(HWND hWnd);
	static inline void DelProp(HWND hWnd);
private:
	TCHAR* GetResourceString(UINT sid);
};

size_t LoadStringSafe(HINSTANCE hInst, UINT sid, const wchar_t**outstr, WORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)) {
	*outstr = nullptr;

	HRSRC hResInfo = FindResourceEx(hInst, RT_STRING, MAKEINTRESOURCE(sid / 16 + 1), lang);
	if (!hResInfo) return 0;
	HGLOBAL hGlobal = LoadResource(hInst, hResInfo);
	if (!hGlobal) return 0;
	wchar_t* str = (wchar_t*)LockResource(hGlobal);
	if (!str) return 0;

	// Seek to our string
	for (unsigned i = 0; i<sid % 16; i++) {
		str += 1 + (WORD)*str;
	}

	WORD len = (WORD)*str;
	*outstr = ++str;
	return len;
}

wchar_t* DialogData::GetResourceString(UINT sid) {
	const wchar_t *str;
	int len = LoadStringSafe(hInstance, sid, &str);
	if (!len) {
		DebugBreak();
		return new wchar_t();
	}
	wchar_t* rvstr = new wchar_t[len+1];
	memcpy(rvstr, str, len * sizeof(wchar_t));
	rvstr[len] = 0;
	return rvstr;
}

DialogData::DialogData(HINSTANCE hInst){
	hInstance = hInst;
	fileName = nullptr;
	buffer = nullptr;
	gameInfo = nullptr;
	comment = nullptr;
	fileSize = 0;
	errCaption = GetResourceString(IDS_ERR_CAPTION);
	errText = GetResourceString(IDS_ERR_TEXT);
}

int DialogData::locateSections() {
	if (buffer && fileSize >= 0x10 && checkMagic(buffer)) {
		uint32_t offset = *(uint32_t*)&buffer[0xC];
		int32_t bytesLeft = fileSize - offset;
		while (bytesLeft > 0xC) {
			UserChunk* thisChunk = (UserChunk*)&buffer[offset];
			if (TO_MAGIC('U', 'S', 'E', 'R') == thisChunk->magic) {
				switch (thisChunk->type & 0xFF) {
				case UCT_GAMEINFO:
					gameInfo = thisChunk;
					break;
				case UCT_COMMENT:
					comment = thisChunk;
					break;
				}
			}
			int32_t section_size = thisChunk->size;
			if (section_size > bytesLeft) {
				section_size = bytesLeft;
				// Modify the value inside the structure, so that the dialog doesn't read past the EOF
				thisChunk->size = bytesLeft;
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
bool DialogData::Save(TCHAR* wcomment, int wlen) {
	if (!fileName || !buffer) return false;

	uint32_t offset = *(uint32_t*)&buffer[0xC];
	size_t nbSize = fileSize + 0xFFFF * 2;
	// TODO: fix this ZUN hackery
	uint8_t *nbuffer = new uint8_t[nbSize];
	memset(nbuffer, 0, nbSize);
	// TODO: check if offset is sane using math.
	if (memcpy_s(nbuffer, nbSize, buffer, offset)) {
		delete[] nbuffer;
		return false;
	}

	UserChunk* nptr = (UserChunk*)&nbuffer[offset];

	// copy over the gameinfo section
	uint32_t gameInfoSize = 0;
	if (gameInfo) {
		gameInfoSize = gameInfo->size;
		if (memcpy_s(nptr, nbSize - offset, gameInfo, gameInfo->size)) {
			delete[] nbuffer;
			return false;
		}
		nptr = (UserChunk*)&nbuffer[offset + gameInfo->size];
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
	nptr->magic = TO_MAGIC('U', 'S', 'E', 'R');
	nptr->size = (uint32_t)( 12 + strlen(acomment) + 1 );
	nptr->type = UCT_COMMENT;
	strcpy(nptr->text(), acomment);
	writeFile(fileName, (DWORD)( offset + gameInfoSize + nptr->size ), nbuffer);
	delete[] acomment;
	delete[] nbuffer;
	return true;
}
void DialogData::Cleanup() {
	delete[] fileName;
	fileName = nullptr;
	delete[] (void*)buffer; // TODO: figure out why VS6 doesn't like this
	buffer = nullptr;
	gameInfo = nullptr;
	comment = nullptr;
	fileSize = 0;
}
DialogData::~DialogData() {
	Cleanup();
	delete[] errCaption;
	delete[] errText;
}

inline void DialogData::Prop(HWND hWnd, DialogData *d) {
	SetProp(hWnd, REPLAY_VIEW_PROP, reinterpret_cast<HANDLE>(d));
}

inline DialogData* DialogData::Prop(HWND hWnd) {
	return reinterpret_cast<DialogData*>(GetProp(hWnd, REPLAY_VIEW_PROP));
}

inline void DialogData::DelProp(HWND hWnd) {
	RemoveProp(hWnd, REPLAY_VIEW_PROP);
}

intptr_t __stdcall DialogFunc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_INITDIALOG) {
		DialogData::Prop(hWnd, reinterpret_cast<DialogData*>(lParam));
		return 1;
	}

	else if (uMsg == WM_NCDESTROY) {
		// MSDN97 says that Prop should be removed in WM_DESTROY, but I think it wouldn't matter.
		DialogData::DelProp(hWnd);
		return 1;
	}

	else if (uMsg == WM_COMMAND) {
		switch (LOWORD(wParam)) {
		case IDC_SAVE: {
			DialogData *d = DialogData::Prop(hWnd);

			TCHAR* wcomment = new TCHAR[0xFFFF]();
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
		DialogData *d = DialogData::Prop(hWnd);
		d->Cleanup();

		SetForegroundWindow(hWnd);
		
		if (DragQueryFile((HDROP)wParam, -1, 0, 0) > 0) {
			UINT len = DragQueryFile((HDROP)wParam, 0, nullptr, 0);
			d->fileName = new TCHAR[len + 1];
			DragQueryFile((HDROP)wParam, 0, d->fileName, len+1);
			d->buffer = readFile(d->fileName, &d->fileSize);
			if (d->locateSections()) {
				MessageBox(hWnd, d->errText, d->errCaption, 0);
				d->Cleanup();
			}
		}
		DragFinish((HDROP)wParam);

		if (!d->buffer)
			return 1;

		// filename without path
		TCHAR *fileName2 = PathFindFileName(d->fileName);
		SetDlgItemText(hWnd, IDC_FILENAME, fileName2);

		if (d->gameInfo) {
			wchar_t* wgameInfo = SJIS_to_WCHAR(d->gameInfo->text(), d->gameInfo->size - 12);
			SetDlgItemText(hWnd, IDC_GAMEINFO, wgameInfo);
			delete[] wgameInfo;

		}
		if (d->comment) {
			wchar_t* wcomment = SJIS_to_WCHAR(d->comment->text(), d->comment->size - 12);
			SetDlgItemText(hWnd, IDC_COMMENT, wcomment);
			delete[] wcomment;
		}
		else {
			SetDlgItemText(hWnd, IDC_COMMENT, _T(""));
		}
		return 1;
	}

	return 0;
}
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	DialogData *d = new DialogData(hInstance);
	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), nullptr, &DialogFunc, reinterpret_cast<LPARAM>(d));
	delete d;
	return 0;
}