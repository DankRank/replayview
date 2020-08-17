#include <stdlib.h>
#include <wchar.h>
#include <tchar.h>
#include <Windows.h>
#include "resource.h"

/* MSVC 6 replacements */
#if defined(_MSC_VER) && _MSC_VER <= 1200
typedef CHAR int8_t;
typedef SHORT int16_t;
typedef LONG int32_t;
typedef BYTE uint8_t;
typedef WORD uint16_t;
typedef DWORD uint32_t;
typedef LONG intptr_t;
#else
#include <stdint.h>
#endif

#ifndef DWLP_USER
#define GetWindowLongPtr GetWindowLong
#define SetWindowLongPtr SetWindowLong
#define DWLP_USER DWL_USER
#endif

// FIXME: remove this
#define memcpy_s(a,b,c,d) (memcpy(a,c,min(b,d)),0)

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define MAGIC4(a,b,c,d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))
#define MAGICBUF(x) MAGIC4((x)[0],(x)[1],(x)[2],(x)[3])
#define COPYMAGIC(x,a,b,c,d) ((x)[0]=(a), (x)[1]=(b), (x)[2]=(c), (x)[3]=(d))

#define alloc(type,count) malloc(sizeof(type)*(count))
#define fatal() ExitProcess(1)

int
checkMagic(const uint8_t *buf)
{
	static const uint32_t magic[] = {
		MAGIC4( 'T', '8', 'R', 'P' ), // eiyashou (in)
		MAGIC4( 'T', '9', 'R', 'P' ), // kaeidzuka (pofv)
		MAGIC4( 't', '9', '5', 'r' ), // bunkachou (stb)
		MAGIC4( 't', '1', '0', 'r' ), // fuujinroku (mof)
		MAGIC4( 'a', 'l', '1', 'r' ), // tasogare sakaba (algostg)
		MAGIC4( 't', '1', '1', 'r' ), // chireiden (sa)
		MAGIC4( 't', '1', '2', 'r' ), // seirensen (ufo)
		MAGIC4( 't', '1', '2', '5' ), // bunkachou (ds)
		MAGIC4( '1', '2', '8', 'r' ), // yousei daisensou
		MAGIC4( 't', '1', '3', 'r' ), // shinreibyou (td)
		// kishinjou (ddc) - has the same id as th13 for some reason
		MAGIC4( 't', '1', '4', '3' ), // danmaku amanojaku (isc)
		MAGIC4( 't', '1', '5', 'r' ), // kanjuden (lolk)
		MAGIC4( 't', '1', '6', 'r' ), // tenkuushou (hsifs)
		MAGIC4( 't', '1', '5', '6' ), // hiifu nightmare diary (vd) (for some reason this is not t165)
		MAGIC4( 't', '1', '7', 't' ), // kikeijuu (wbawc) trial
		MAGIC4( 't', '1', '7', 'r' ), // kikeijuu (wbawc)
		// TODO: do other trial versions have separate magics?
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(magic); i++) {
		if (magic[i] == MAGICBUF(buf)) {
			return 1;
		}
	}
	return 0;
}

wchar_t *
SJIS_to_WCHAR(const char *str, DWORD len)
{
	int wlen;
	wchar_t *wstr;

	wlen = MultiByteToWideChar(932, 0, str, len, NULL, 0);
	wstr = alloc(wchar_t, wlen + 1);
	if (!wstr)
		fatal();
	MultiByteToWideChar(932, 0, str, len, wstr, wlen);
	wstr[wlen] = 0; // just in case original string didn't have one
	return wstr;
}

char *
WCHAR_to_SJIS(const wchar_t *wstr, DWORD wlen)
{
	BOOL b;
	int len;
	char *str;

	len = WideCharToMultiByte(932, 0, wstr, wlen, NULL, 0, NULL, &b);
	str = alloc(char, len + 1);
	if (!str)
		fatal();
	WideCharToMultiByte(932, 0, wstr, wlen, str, len, NULL, &b);
	str[len] = 0; // just in case original string didn't have one
	return str;
}

uint8_t *
readFile(const TCHAR *fileName, DWORD *outFileSize)
{
	HANDLE file;
	DWORD fileSize;
	uint8_t *buf;

	file = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return NULL;
	fileSize = GetFileSize(file, NULL);
	buf = alloc(uint8_t, fileSize);
	if (!buf) {
		CloseHandle(file);
		return NULL;
	}
	ReadFile(file, buf, fileSize, &fileSize, NULL);
	if (fileSize)
		*outFileSize = fileSize;
	CloseHandle(file);
	return buf;
}

int
writeFile(const TCHAR *fileName, DWORD fileSize, uint8_t *buf)
{
	HANDLE file;
	DWORD bytesWritten;

	file = CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return -1;

	WriteFile(file, buf, fileSize, &bytesWritten, NULL);
	CloseHandle(file);
	if(fileSize == bytesWritten)
		return 0;
	else
		return -2;
}

enum UserChunkType {
	UCT_GAMEINFO = 0,
	UCT_COMMENT = 1
};

typedef struct _tag_UserChunk {
	char magic[4];
	uint32_t size;
	uint32_t type;
	char text[1]; /* variable-length */
} UserChunk;

typedef struct _tag_DialogData {
	HINSTANCE hInstance;
	WORD langId;
	TCHAR *fileName;
	uint8_t *buffer;
	UserChunk* gameInfo;
	UserChunk* comment;
	DWORD fileSize;
	wchar_t *errCaption;
	wchar_t *errText;
} DialogData;

void *
LoadAndLockResource(HINSTANCE hinst, LPCTSTR type, LPCTSTR name, WORD langId)
{
	HRSRC hrsrc;
	HGLOBAL hglob;

	if (NULL != (hrsrc = FindResourceEx(hinst, type, name, langId))
	&& NULL != (hglob = LoadResource(hinst, hrsrc)))
		return LockResource(hglob);
	
	return NULL;
}

WORD
LoadStringSafe(HINSTANCE hinst, WORD langId, UINT uId, const wchar_t **outstr)
{
	wchar_t *pwsz;
	
	pwsz = (wchar_t*)LoadAndLockResource(hinst, RT_STRING, MAKEINTRESOURCE(uId / 16 + 1), langId);
	if (pwsz) {
		// Seek to our string
		unsigned i;
		for (i = 0; i < uId % 16; i++)
			pwsz += 1 + (WORD)*pwsz;
	}
	
	*outstr = pwsz;
	if (!pwsz)
		return 0;

	++*outstr; // now we know that this is not NULL, we can increment it
	return (WORD)*pwsz;
}

wchar_t *
GetResourceString(HINSTANCE hInstance, WORD langId, UINT sid)
{
	const wchar_t *str;
	size_t len;
	wchar_t *rvstr;
	
	len = LoadStringSafe(hInstance, langId, sid, &str);
	if (!len) {
		rvstr = alloc(wchar_t, 1);
		if (!rvstr)
			fatal();
		*rvstr = 0;
		return rvstr;
	}
	rvstr = alloc(wchar_t, len + 1);
	memcpy(rvstr, str, len * sizeof(wchar_t));
	rvstr[len] = 0;
	return rvstr;
}

int
locateSections(DialogData *d)
{
	const uint8_t *const buffer = d->buffer;

	if (buffer && d->fileSize >= 0x10 && checkMagic(buffer)) {
		uint32_t offset = *(uint32_t*)&buffer[0xC];
		uint32_t bytesLeft = d->fileSize - offset;
		while (bytesLeft > 0xC) {
			UserChunk* thisChunk = (UserChunk*)&buffer[offset];
			if (MAGIC4('U', 'S', 'E', 'R') == MAGICBUF(thisChunk->magic)) {
				switch (thisChunk->type & 0xFF) {
				case UCT_GAMEINFO:
					d->gameInfo = thisChunk;
					break;
				case UCT_COMMENT:
					d->comment = thisChunk;
					break;
				}
			}
			if (thisChunk->size > bytesLeft) {
				// Modify the value inside the structure, so that the dialog doesn't read past the EOF
				thisChunk->size = bytesLeft;
			}
			offset += thisChunk->size;
			bytesLeft -= thisChunk->size;
		}
		return 0;
	}
	else {
		return 1;
	}
}

int
save(DialogData *d, TCHAR *wcomment, int wlen)
{
	const uint8_t *const buffer = d->buffer;
	uint32_t offset;
	size_t nbSize;
	uint8_t *nbuffer;
	UserChunk *nptr;
	uint32_t gameInfoSize = 0;
	char *acomment;

	if (!d->fileName || !d->buffer)
		return 0;

	offset = *(uint32_t*)&buffer[0xC];
	nbSize = d->fileSize + 0xFFFF * 2;
	// TODO: fix this ZUN hackery
	nbuffer = alloc(uint8_t, nbSize);
	if(!nbuffer)
		fatal();
	memset(nbuffer, 0, nbSize);
	// TODO: check if offset is sane using math.
	if (memcpy_s(nbuffer, nbSize, buffer, offset)) {
		free(nbuffer);
		return 0;
	}

	nptr = (UserChunk*)&nbuffer[offset];

	// copy over the gameinfo section
	if (d->gameInfo) {
		gameInfoSize = d->gameInfo->size;
		if (memcpy_s(nptr, nbSize - offset, d->gameInfo, d->gameInfo->size)) {
			free(nbuffer);
			return 0;
		}
		nptr = (UserChunk*)&nbuffer[offset + d->gameInfo->size];
	}

	// add comment section
	if (!d->comment) {
		// byte 7 is USER chunk count. In th095+ it's always 0.
		++nbuffer[7];
	}
	// NOTE: let's not remove the arbitrary 0xFFFF limit, so that we don't crash original implementation.
	acomment = WCHAR_to_SJIS(wcomment, wlen);
	if (strlen(acomment) >= 0xFFFF) {
		acomment[0xFFFF] = 0;
	}

	// TODO: add more buffer overflow checks
	COPYMAGIC(nptr->magic, 'U', 'S', 'E', 'R');
	nptr->size = (uint32_t)( 12 + strlen(acomment) + 1 );
	nptr->type = UCT_COMMENT;
	strcpy(nptr->text, acomment);
	writeFile(d->fileName, (DWORD)( offset + gameInfoSize + nptr->size ), nbuffer);
	free(acomment);
	free(nbuffer);
	return 1;
}

void
dialogCleanup(DialogData *d)
{
	free(d->fileName);
	d->fileName = NULL;
	free(d->buffer);
	d->buffer = NULL;
	d->gameInfo = NULL;
	d->comment = NULL;
	d->fileSize = 0;
}

static void
setDialogData(HWND hWnd, DialogData *d)
{
	SetWindowLongPtr(hWnd, DWLP_USER, (intptr_t)d);
}

static DialogData *
getDialogData(HWND hWnd)
{
	return (DialogData *)GetWindowLongPtr(hWnd, DWLP_USER);
}

LPTSTR
PathFindFileNameSimple(LPCTSTR path)
{
	LPCTSTR rv = path;
	while (path && *path) {
		if (*path == _T('\\') || *path == _T('/') || *path == _T(':'))
			rv = path + 1;
		path = CharNext(path);
	}
	return (LPTSTR)rv;
}

intptr_t __stdcall
DialogFunc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_INITDIALOG) {
		setDialogData(hWnd, (DialogData*)lParam);
		return 1;
	}

	else if (uMsg == WM_COMMAND) {
		switch (LOWORD(wParam)) {
		case IDC_SAVE: {
			DialogData *d;
			TCHAR *wcomment;
			int wlen;

			d = getDialogData(hWnd);
			wcomment = alloc(TCHAR, 0xFFFF);
			wlen = GetDlgItemText(hWnd, IDC_COMMENT, wcomment, 0xFFFF);
			if (save(d, wcomment, wlen)) {
				EndDialog(hWnd, IDC_SAVE);
			}
			free(wcomment);
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
		DialogData *d;

		d = getDialogData(hWnd);
		dialogCleanup(d);

		SetForegroundWindow(hWnd);
		
		if (DragQueryFile((HDROP)wParam, -1, 0, 0) > 0) {
			UINT len = DragQueryFile((HDROP)wParam, 0, NULL, 0);
			d->fileName = alloc(TCHAR, len + 1);
			DragQueryFile((HDROP)wParam, 0, d->fileName, len+1);
			d->buffer = readFile(d->fileName, &d->fileSize);
			if (locateSections(d)) {
				MessageBoxEx(hWnd, d->errText, d->errCaption, 0, d->langId);
				dialogCleanup(d);
			}
		}
		DragFinish((HDROP)wParam);

		if (!d->buffer)
			return 1;

		/* filename without path */
		SetDlgItemText(hWnd, IDC_FILENAME, PathFindFileNameSimple(d->fileName));

		if (d->gameInfo) {
			wchar_t *wgameInfo = SJIS_to_WCHAR(d->gameInfo->text, d->gameInfo->size - 12);
			SetDlgItemText(hWnd, IDC_GAMEINFO, wgameInfo);
			free(wgameInfo);
		} else {
			SetDlgItemText(hWnd, IDC_GAMEINFO, _T(""));
		}
		if (d->comment) {
			wchar_t *wcomment = SJIS_to_WCHAR(d->comment->text, d->comment->size - 12);
			SetDlgItemText(hWnd, IDC_COMMENT, wcomment);
			free(wcomment);
		} else {
			SetDlgItemText(hWnd, IDC_COMMENT, _T(""));
		}
		return 1;
	}

	return 0;
}

intptr_t
runDialog(HINSTANCE hInstance, WORD langId)
{
	DialogData d = {hInstance, langId};
	DLGTEMPLATE *dlg;
	intptr_t rv;

	d.errCaption = GetResourceString(hInstance, langId, IDS_ERR_CAPTION);
	d.errText = GetResourceString(hInstance, langId, IDS_ERR_TEXT);

	dlg = (DLGTEMPLATE*)LoadAndLockResource(hInstance, RT_DIALOG, MAKEINTRESOURCE(IDD_DIALOG1), langId);
	if (!dlg) {
		MessageBoxEx(NULL, _T("Couldn't load dialog"), NULL, 0, langId);
		return -1;
	}
	rv = DialogBoxIndirectParam(hInstance, dlg, NULL, &DialogFunc, (LPARAM)&d);

	dialogCleanup(&d);
	free(d.errCaption);
	free(d.errText);

	return rv;
}

int __stdcall
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	runDialog(hInstance, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
	return 0;
}