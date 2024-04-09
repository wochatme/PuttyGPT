/* Get-Content log.txt -Wait */
#ifndef _ASKROB_H_
#define _ASKROB_H_

#include <stdint.h>
#include <fcntl.h>
#include <io.h>
#include "curl/curl.h"
#include "cjson/cJSON.h"
#include "Sci_Position.h"
#include "scintilla.h"

#define AR_ALIGN(size, boundary)   (((size) + ((boundary) -1)) & ~((boundary) - 1))
#define AR_ALIGN_DEFAULT32(size)   AR_ALIGN(size, 4)
#define AR_ALIGN_DEFAULT64(size)   AR_ALIGN(size, 8)      /** Default alignment */
#define AR_ALIGN_PAGE(size)        AR_ALIGN(size, (1<<16))

#pragma comment(lib, "crypt32.lib")

#define S8      int8_t
#define S16     int16_t
#define S32     int32_t
#define S64     int64_t
#define U8      uint8_t
#define U16     uint16_t
#define U32     uint32_t
#define U64     uint64_t

#define WT_OK       0
#define WT_FAIL     1

/* Some fundamental constants */
#define UNI_REPLACEMENT_CHAR		(U32)0x0000FFFD
#define UNI_MAX_BMP					(U32)0x0000FFFF
#define UNI_MAX_UTF16				(U32)0x0010FFFF
#define UNI_MAX_UTF32				(U32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32			(U32)0x0010FFFF
#define UNI_SUR_HIGH_START          (U32)0xD800
#define UNI_SUR_HIGH_END            (U32)0xDBFF
#define UNI_SUR_LOW_START           (U32)0xDC00
#define UNI_SUR_LOW_END             (U32)0xDFFF

static const int halfShift = 10; /* used for shifting by 10 bits */
static const U32 halfBase = 0x0010000UL;
static const U32 halfMask = 0x3FFUL;
/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const U8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static U32 wt_UTF16ToUTF8(U16* input, U32 input_len, U8* output, U32* output_len)
{
    U32 codepoint, i;
    U32 ret = WT_OK;
    U32 bytesTotal = 0;
    U8  BytesPerCharacter = 0;
    U16 leadSurrogate, tailSurrogate;
    const U32 byteMark = 0x80;
    const U32 byteMask = 0xBF;

    if (!output)  // the caller only wants to determine how many words in UTF16 string
    {
        for (i = 0; i < input_len; i++)
        {
            codepoint = input[i];
            /* If we have a surrogate pair, convert to UTF32 first. */
            if (codepoint >= UNI_SUR_HIGH_START && codepoint <= UNI_SUR_HIGH_END)
            {
                if (i < input_len - 1)
                {
                    if (input[i + 1] >= UNI_SUR_LOW_START && input[i + 1] <= UNI_SUR_LOW_END)
                    {
                        leadSurrogate = input[i];
                        tailSurrogate = input[i + 1];
                        codepoint = ((leadSurrogate - UNI_SUR_HIGH_START) << halfShift) 
                            + (tailSurrogate - UNI_SUR_LOW_START) + halfBase;
                        i += 1;
                    }
                    else /* it's an unpaired lead surrogate */
                    {
                        ret = WT_FAIL;
                        break;
                    }
                }
                else /* We don't have the 16 bits following the lead surrogate. */
                {
                    ret = WT_FAIL;
                    break;
                }
            }
            // TPN: substitute all control characters except for NULL, TAB, LF or CR
            if (codepoint && (codepoint != (U32)0x09) && (codepoint != (U32)0x0a) && (codepoint != (U32)0x0d) && (codepoint < (U32)0x20))
                codepoint = 0x3f;
            // TPN: filter out byte order marks and invalid character 0xFFFF
            if ((codepoint == (U32)0xFEFF) || (codepoint == (U32)0xFFFE) || (codepoint == (U32)0xFFFF))
                continue;

            /* Figure out how many bytes the result will require */
            if (codepoint < (U32)0x80)
                BytesPerCharacter = 1;
            else if (codepoint < (U32)0x800)
                BytesPerCharacter = 2;
            else if (codepoint < (U32)0x10000)
                BytesPerCharacter = 3;
            else if (codepoint < (U32)0x110000)
                BytesPerCharacter = 4;
            else
            {
                BytesPerCharacter = 3;
                codepoint = UNI_REPLACEMENT_CHAR;
            }
            bytesTotal += BytesPerCharacter;
        }
    }
    else
    {
        U8* p = output;
        for (i = 0; i < input_len; i++)
        {
            codepoint = input[i];
            /* If we have a surrogate pair, convert to UTF32 first. */
            if (codepoint >= UNI_SUR_HIGH_START && codepoint <= UNI_SUR_HIGH_END)
            {
                if (i < input_len - 1)
                {
                    if (input[i + 1] >= UNI_SUR_LOW_START && input[i + 1] <= UNI_SUR_LOW_END)
                    {
                        leadSurrogate = input[i];
                        tailSurrogate = input[i + 1];
                        codepoint = ((leadSurrogate - UNI_SUR_HIGH_START) << halfShift) + (tailSurrogate - UNI_SUR_LOW_START) + halfBase;
                        i += 1;
                    }
                    else /* it's an unpaired lead surrogate */
                    {
                        ret = WT_FAIL;
                        break;
                    }
                }
                else /* We don't have the 16 bits following the lead surrogate. */
                {
                    ret = WT_FAIL;
                    break;
                }
            }
            // TPN: substitute all control characters except for NULL, TAB, LF or CR
            if (codepoint && (codepoint != (U32)0x09) && (codepoint != (U32)0x0a) && (codepoint != (U32)0x0d) && (codepoint < (U32)0x20))
                codepoint = 0x3f;
            // TPN: filter out byte order marks and invalid character 0xFFFF
            if ((codepoint == (U32)0xFEFF) || (codepoint == (U32)0xFFFE) || (codepoint == (U32)0xFFFF))
                continue;

            /* Figure out how many bytes the result will require */
            if (codepoint < (U32)0x80)
                BytesPerCharacter = 1;
            else if (codepoint < (U32)0x800)
                BytesPerCharacter = 2;
            else if (codepoint < (U32)0x10000)
                BytesPerCharacter = 3;
            else if (codepoint < (U32)0x110000)
                BytesPerCharacter = 4;
            else
            {
                BytesPerCharacter = 3;
                codepoint = UNI_REPLACEMENT_CHAR;
            }

            p += BytesPerCharacter;
            switch (BytesPerCharacter) /* note: everything falls through. */
            {
            case 4: *--p = (U8)((codepoint | byteMark) & byteMask); codepoint >>= 6;
            case 3: *--p = (U8)((codepoint | byteMark) & byteMask); codepoint >>= 6;
            case 2: *--p = (U8)((codepoint | byteMark) & byteMask); codepoint >>= 6;
            case 1: *--p = (U8)(codepoint | firstByteMark[BytesPerCharacter]);
            }
            p += BytesPerCharacter;
            bytesTotal += BytesPerCharacter;
        }
    }

    if (WT_OK == ret && output_len)
        *output_len = bytesTotal;

    return ret;
}

static const char* default_conf_json =
"{\n\"key\" : \"03339A1C8FDB6AFF46845E49D120E0400021E161B6341858585C2E25CA3D9C01CA\",\n"
"\"url\" : \"https://www.wochat.org/v1\",\n"
"\"font0\" : \"Courier New\",\n"
"\"font1\" : \"Courier New\",\n"
"\"fsize0\" : 11,\n"
"\"fsize1\" : 11,\n"
"\"startchat\" : 1,\n"
"\"autologging\" : 1,\n"
"\"proxy_type\" : 0,\n"
"\"proxy\" : \"\"\n}\n";

/* the variables to save configuration information */
static const char* defaultFont = "Courier New";
static const char* defaultURL = "https://www.wochat.org/v1";
static U8  g_appKey[67] = { 0 };
static U8  g_url[128] = { 0 };
static U8  g_font0[32] = { 0 };
static U8  g_font1[32] = { 0 };
static U32 g_fsize0 = 1100;
static U32 g_fsize1 = 1100;
static U8  g_AskRobAtStartUp = 1;
static U8  g_AutoLogging = 1;

/*
 * libCurl Proxy type. Please check: https://curl.se/libcurl/c/CURLOPT_PROXYTYPE.html
 * 0 - No Proxy
 * 1 - CURLPROXY_HTTP
 * 2 - CURLPROXY_HTTPS
 * 3 - CURLPROXY_HTTPS2
 * 4 - CURLPROXY_HTTP_1_0
 * 5 - CURLPROXY_SOCKS4
 * 6 - CURLPROXY_SOCKS4A
 * 7 - CURLPROXY_SOCKS5
 * 8 - CURLPROXY_SOCKS5_HOSTNAME
 */
static U8  g_proxy_type = 0;
static U8  g_proxy[256] = { 0 };

typedef struct MessageTask
{
    struct MessageTask* next;
    volatile LONG  state;
    U8  message_type;
    U8* message;
    U32 message_length;
} MessageTask;

/* the message queue from the remote server */
static MessageTask* g_mtIncoming = NULL;
/* used to sync different threads */
static CRITICAL_SECTION     g_csSendMsg;
static CRITICAL_SECTION     g_csReceMsg;

#define IDM_ASKROB          0x01B0
#define IDM_COPYSCREEN      0x01C0

#define TIMER_ASKROB        999  /* 999 ms for timer */

#define CHATWIN_GAP         19

#define INPUT_BUF_MAX       (1<<18) /* 256 KB should be big enough */

#define WM_BRING_TO_FRONT   (WM_USER + 1)

static const LPCWSTR ASKROB_MAIN_CLASS_NAME = L"AskRobWin";
static const LPCWSTR ASKROB_MAIN_TITLE_NAME = L"X";

static const char* greeting = "\n--\nHi, I am your humble servant. You can ask me any question by typing in the below window.\n";

static volatile LONG g_threadCount = 0;
static volatile LONG g_Quit = 0;
static volatile LONG g_QuitAskRob = 0;

static wchar_t g_logFile[MAX_PATH + 1] = { 0 };
static wchar_t g_cnfFile[MAX_PATH + 1] = { 0 };

static BOOL AskRobIsGood = FALSE;
static BOOL HasInputData = FALSE;

static HINSTANCE hInstAskRob = NULL;

static HWND hWndPutty  = NULL;
static HWND hWndAskRob = NULL; /* the window for AI chat */
static HWND hWndChat   = NULL;   /* the child window in hWndAskRob */
static HWND hWndEdit   = NULL;   /* the child window in hWndAskRob */

/* 4 buttons */
static HBITMAP bmpQuestion = NULL;
static HBITMAP bmpSaveFile = NULL;
static HBITMAP bmpEmptyLog = NULL;
static HBITMAP bmpSettings = NULL;

static HCURSOR	hCursorHand = NULL;
static HCURSOR	hCursorNS   = NULL;

static RECT rectChat = { 0 }; /* to record the postion of the AskRob chat window */

static U8* inputBuffer = NULL;
static int inputBufPos = 0;

static wchar_t*  screenBuffer = NULL;
static int       screenBufPos = -1;

static U8* prev_chatdata = NULL;

/* this two functions are in askrob_terminal.h */
void term_copy_current_screen(Terminal* term, const int* clipboards, int n_clipboards);
wchar_t* term_copyScreen(Terminal* term, const int* clipboards, int n_clipboards);

/* copy the data in the current screen of Putty window and save to screenBuffer */
static void AR_CopyScreen(Terminal* term, const int* clipboards, int n_clipboards)
{
    wchar_t* sbuf;

    EnterCriticalSection(&g_csSendMsg);
    screenBufPos = -1; /* -1 means no data in the screen. 0 means no data, but the buffer is "", not NULL */
    LeaveCriticalSection(&g_csSendMsg);

    sbuf = term_copyScreen(term, clipboards, n_clipboards);
    if (sbuf)
    {
        U32 wlen = (U32)wcslen(sbuf);
        if (wlen < (INPUT_BUF_MAX / 2))
        {
            wmemcpy_s(screenBuffer, INPUT_BUF_MAX / 2, sbuf, wlen);
            EnterCriticalSection(&g_csSendMsg);
            screenBufPos = wlen;
            LeaveCriticalSection(&g_csSendMsg);
        }
        sfree(sbuf);
    }
}

static int DoAskQuestion(HWND hWnd)
{
    if (IsWindow(hWndEdit))
    {
        U32 input_len;
        U8* p = inputBuffer;
        p[0] = '\n'; p[1] = 0xF0; p[2] = 0x9F; p[3] = 0xA4; p[4] = 0x9A;
        p[5] = '\n';  p[6] = '-'; p[7] = '-'; p[8] = '\n';
        /*--------------------------------------------------*/
        p[9]  = 'H'; p[10] = 'o'; p[11] = 'w'; p[12] = ' '; p[13] = 't'; p[14] = 'o'; p[15] = ' ';
        p[16] = 'f'; p[17] = 'i'; p[18] = 'x'; p[19] = ' '; p[20] = 'i'; p[21] = 't'; p[22] = '?'; p[23] = '\n';
        input_len = 15;
        if (IsWindow(hWndChat))
        {
            int totalLines = (int)SendMessage(hWndChat, SCI_GETLINECOUNT, 0, 0);
            SendMessage(hWndChat, SCI_SETREADONLY, FALSE, 0);
            SendMessage(hWndChat, SCI_APPENDTEXT, 9 + input_len, (LPARAM)inputBuffer);
            SendMessage(hWndChat, SCI_SETREADONLY, TRUE, 0);
            SendMessage(hWndChat, SCI_LINESCROLL, 0, totalLines);
            HasInputData = TRUE; /* the user has inputed some text */
        }

        if (IsWindow(hWndPutty))
        {
            EnterCriticalSection(&g_csSendMsg);
            inputBufPos = input_len;
            LeaveCriticalSection(&g_csSendMsg);
            PostMessage(hWndPutty, WM_COMMAND, IDM_COPYSCREEN, 0);
        }
    }
    return 0;
}

static int DoSaveLog(HWND hWnd)
{
    if (IsWindow(hWndChat)) /* save the chat history */
    {
        U32 length = SendMessage(hWndChat, SCI_GETTEXTLENGTH, 0, 0);
        if (length)
        {
            OPENFILENAMEW ofn;       // common dialog box structure
            wchar_t szFile[MAX_PATH + 1];       // buffer for file name

            // Initialize OPENFILENAME
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.lpstrFile[0] = L'\0';
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;

            if (GetSaveFileNameW(&ofn) == TRUE) // Display the "Save As" dialog
            {
                char* buf = (char*)malloc(AR_ALIGN_DEFAULT64(length + 1));
                if (buf)
                {
                    int fd;
                    SendMessage(hWndChat, SCI_GETTEXT, length, (LPARAM)buf);
                    if (_wsopen_s(&fd, szFile, _O_WRONLY | _O_CREAT, _SH_DENYNO, _S_IWRITE) == 0)
                    {
                        _write(fd, buf, length);
                        _close(fd);
                    }
                    free(buf);
                }
            }
        }
    }
    return 0;
}

static int DoEmptyLog(HWND hWnd)
{
    if (IsWindow(hWndChat)) /* save the chat history */
    {
        U32 length = SendMessage(hWndChat, SCI_GETTEXTLENGTH, 0, 0);
        if (length)
        {
            int choice = MessageBoxW(hWnd, L"Do you want to clear the chat history?", L"Clear Chat History", MB_YESNO);
            if (choice == IDYES)
            {
                SendMessage(hWndChat, SCI_SETREADONLY, FALSE, 0);
                SendMessage(hWndChat, SCI_SETTEXT, 0, (LPARAM)"");
                SendMessage(hWndChat, SCI_SETREADONLY, TRUE, 0);
                HasInputData = FALSE;
            }
        }
    }
    return 0;
}

/* simply to create a new process to run notepad.exe to open conf.json */
static int DoSettings(HWND hWnd)
{
    STARTUPINFOW si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    wchar_t cmd[280 + 1] = { 0 };

    swprintf_s(cmd, 280, L"notepad.exe \"%s\"", g_cnfFile);

    si.cb = sizeof(STARTUPINFOW);
    CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    return 0;
}

// define useful macros from windowsx.h
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lParam)	((int)(short)LOWORD(lParam))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lParam)	((int)(short)HIWORD(lParam))
#endif

/* the main window proc for AI chat window */
static LRESULT CALLBACK AskRobWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    bool bHandled = false;

    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 0;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            RECT rc;

            HDC hdc = BeginPaint(hWnd, &ps);
            GetClientRect(hWnd, &rc);
            
            HDC hdcMem = CreateCompatibleDC(hdc);
            if (hdcMem != NULL)
            {
                HDC hDCBitmap;
                HBITMAP bmpMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
                HBITMAP bmpOld = SelectObject(hdcMem, bmpMem);
                HBRUSH brushBkg = CreateSolidBrush(RGB(192, 192, 192));
                FillRect(hdcMem, &rc, brushBkg);

                hDCBitmap = CreateCompatibleDC(hdcMem);
                if (hDCBitmap)
                {
                    HBITMAP bmp;
                    if (bmpQuestion)
                    {
                        bmp = SelectObject(hDCBitmap, bmpQuestion);
                        BitBlt(hdcMem, 20 + 90, rc.bottom - 160, 15, 15, hDCBitmap, 0, 0, SRCCOPY);
                        SelectObject(hDCBitmap, bmp);
                    }
                    if (bmpSaveFile)
                    {
                        bmp = SelectObject(hDCBitmap, bmpSaveFile);
                        BitBlt(hdcMem, 20 + 30, rc.bottom - 160, 15, 15, hDCBitmap, 0, 0, SRCCOPY);
                        SelectObject(hDCBitmap, bmp);
                    }
                    if (bmpEmptyLog)
                    {
                        bmp = SelectObject(hDCBitmap, bmpEmptyLog);
                        BitBlt(hdcMem, 20 + 60, rc.bottom - 160, 15, 15, hDCBitmap, 0, 0, SRCCOPY);
                        SelectObject(hDCBitmap, bmp);
                    }
                    if (bmpSettings)
                    {
                        bmp = SelectObject(hDCBitmap, bmpSettings);
                        BitBlt(hdcMem, 20, rc.bottom - 161, 15, 15, hDCBitmap, 0, 0, SRCCOPY);
                        SelectObject(hDCBitmap, bmp);
                    }
                    DeleteDC(hDCBitmap);
                }

                BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);
                SelectObject(hdcMem, bmpOld);
                DeleteDC(hdcMem);
            }

            EndPaint(hWnd, &ps);
        }
        return 0;
    case WM_SETCURSOR:
        bHandled = false;
        if (((HWND)wParam == hWnd) && (LOWORD(lParam) == HTCLIENT))
        {
            RECT rc = { 0 };
            GetClientRect(hWnd, &rc);
            DWORD dwPos = GetMessagePos();
            POINT pt = { GET_X_LPARAM(dwPos), GET_Y_LPARAM(dwPos) };
            ScreenToClient(hWnd, &pt);
            if (pt.y >= rc.bottom - 160 && pt.y < rc.bottom - 160 + 15)
            {
                int xPos = pt.x;
                if ((xPos >= 20 && xPos < 20 + 15) 
                    || (xPos >= 20 + 30 && xPos < 20 + 30 + 15) 
                    || (xPos >= 20 + 60 && xPos < 20 + 60 + 15)
                    || (xPos >= 20 + 90 && xPos < 20 + 90 + 15))
                    bHandled = true;
            }
        }
        if (bHandled) return 0; 
        else return DefWindowProc(hWnd, uMsg, wParam, lParam);
    case WM_MOUSEMOVE:
        if (hCursorHand)
        {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            RECT rc;
            GetClientRect(hWnd, &rc);
            if (yPos >= rc.bottom - 160 && yPos < rc.bottom - 160 + 15)
            {
                if ((xPos >= 20 && xPos < 20 + 15) 
                    || (xPos >= 20 + 30 && xPos < 20 + 30 + 15) 
                    || (xPos >= 20 + 60 && xPos < 20 + 60 + 15)
                    || (xPos >= 20 + 90 && xPos < 20 + 90 + 15))
                    SetCursor(hCursorHand);
            }
        }
        return 0;
    case WM_LBUTTONDOWN:
        if(true)
        {
            int hit = -1;
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            RECT rc;
            GetClientRect(hWnd, &rc);
            if (yPos >= rc.bottom - 160 && yPos < rc.bottom - 160 + 15)
            {
                if ((xPos >= 20 && xPos < 20 + 15) 
                    || (xPos >= 20 + 30 && xPos < 20 + 30 + 15) 
                    || (xPos >= 20 + 60 && xPos < 20 + 60 + 15)
                    || (xPos >= 20 + 90 && xPos < 20 + 90 + 15))
                    hit = 0;
            }
            if (hit == -1)
                PostMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
        }
        return 0;
    case WM_LBUTTONUP:
        if (true)
        {
            int hit = -1;
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            RECT rc;
            GetClientRect(hWnd, &rc);
            if (yPos >= rc.bottom - 160 && yPos < rc.bottom - 160 + 15)
            {
                if (xPos >= 20 && xPos < 20 + 15)
                    hit = 0;
                else if (xPos >= 20 + 30 && xPos < 20 + 30 + 15)
                    hit = 1;
                else if (xPos >= 20 + 60 && xPos < 20 + 60 + 15)
                    hit = 2;
                else if (xPos >= 20 + 90 && xPos < 20 + 90 + 15)
                    hit = 3;
            }
            switch (hit)
            {
            case 0:
                DoSettings(hWnd);
                break;
            case 1:
                DoSaveLog(hWnd);
                break;
            case 2:
                DoEmptyLog(hWnd);
                break;
            case 3:
                DoAskQuestion(hWnd);
                break;
            default:
                break;
            }
        }
        return 0;
    case WM_BRING_TO_FRONT:
        SetForegroundWindow(hWnd);
        return 0;
    case WM_TIMER:
        {
            MessageTask* p;
            EnterCriticalSection(&g_csReceMsg);
            p = g_mtIncoming;
            while (p)
            {
                if (p->state == 0) /* this message is new message */
                {
                    if (IsWindow(hWndChat))
                    {
                        int totalLines = (int)SendMessage(hWndChat, SCI_GETLINECOUNT, 0, 0);
                        SendMessage(hWndChat, SCI_SETREADONLY, FALSE, 0);
                        SendMessage(hWndChat, SCI_APPENDTEXT, p->message_length, (LPARAM)p->message);
                        SendMessage(hWndChat, SCI_SETREADONLY, TRUE, 0);
                        SendMessage(hWndChat, SCI_LINESCROLL, 0, totalLines);
                    }
                    InterlockedIncrement(&(p->state));
                    break;
                }
                p = p->next;
            }
            LeaveCriticalSection(&g_csReceMsg);
        }
        return 0;
    case WM_NOTIFY: /* handle the event from the input window */
        {
            LPNMHDR lpnmhdr = (LPNMHDR)lParam;
            if (lpnmhdr->hwndFrom == hWndEdit && IsWindow(hWndEdit))
            {
                int currentPos;
                char ch;
                bool heldControl = (GetKeyState(VK_CONTROL) & 0x80) != 0; /* does the user hold Ctrl key? */
                switch (lpnmhdr->code)
                {
                case SCN_CHARADDED:
                    currentPos = SendMessage(hWndEdit, SCI_GETCURRENTPOS, 0, 0);
                    ch = SendMessage(hWndEdit, SCI_GETCHARAT, currentPos - 1, 0);
                    if (ch == '\n' && heldControl == false) /* the user hit the ENTER key */
                    {
                        U32 input_len  = SendMessage(hWndEdit, SCI_GETTEXTLENGTH, 0, 0);
                        if (input_len > 1)
                        {
                            U8* p = inputBuffer;
                            if (input_len > INPUT_BUF_MAX - 16)
                                input_len = INPUT_BUF_MAX - 16;
                            p[0] = '\n'; p[1] = 0xF0; p[2] = 0x9F; p[3] = 0xA4; p[4] = 0x9A; 
                            p[5] = '\n';  p[6] = '-'; p[7] = '-'; p[8] = '\n';
                            p = inputBuffer + 9;
                            SendMessage(hWndEdit, SCI_GETTEXT, input_len, (LPARAM)p);
                            if (IsWindow(hWndChat))
                            {
                                int totalLines = (int)SendMessage(hWndChat, SCI_GETLINECOUNT, 0, 0);
                                SendMessage(hWndChat, SCI_SETREADONLY, FALSE, 0);
                                SendMessage(hWndChat, SCI_APPENDTEXT, 9 + input_len, (LPARAM)inputBuffer);
                                SendMessage(hWndChat, SCI_SETREADONLY, TRUE, 0);
                                SendMessage(hWndChat, SCI_LINESCROLL, 0, totalLines);
                                HasInputData = TRUE; /* the user has inputed some text */
                            }

                            if (IsWindow(hWndPutty))
                            {
                                EnterCriticalSection(&g_csSendMsg);
                                inputBufPos = input_len;
                                LeaveCriticalSection(&g_csSendMsg);

                                PostMessage(hWndPutty, WM_COMMAND, IDM_COPYSCREEN, 0);
                            }
                        }
                        SendMessage(hWndEdit, SCI_SETTEXT, 0, (LPARAM)"");
                    }
                    break;
                default:
                    break;
                }
            }
        }
        return 0;
    case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
            lpMMI->ptMinTrackSize.x = 360;
            lpMMI->ptMinTrackSize.y = 600;
        }
        return 0;
    case WM_SIZE:
        if(IsWindow(hWndChat) && IsWindow(hWndEdit))
        {
            int width, height;
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            width = LOWORD(lParam);
            height = HIWORD(lParam);
            width = rcClient.right - rcClient.left;
            height = rcClient.bottom - rcClient.top;
            MoveWindow(hWndChat, 0, 0, width, height - 165, TRUE);
            MoveWindow(hWndEdit, 0, height - 140, width, 150, TRUE);
        }
        return 0;
    case WM_CREATE:
        {
            UINT_PTR id = SetTimer(hWnd, TIMER_ASKROB, TIMER_ASKROB, NULL);
            hWndChat = CreateWindowExW(0, L"Scintilla", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                0, 0, 16, 16, hWnd, NULL, hInstAskRob, NULL);
            hWndEdit = CreateWindowExW(0, L"Scintilla", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, 
                0, 0, 16, 16, hWnd, NULL, hInstAskRob, NULL);

            if (IsWindow(hWndChat))
            {
                U8 hello[128] = { 0 };
                U8 greeting_length = (U8)strlen(greeting);
                hello[0] = 0xF0; hello[1] = 0x9F; hello[2] = 0x99; hello[3] = 0x82;
                for (U8 i = 0; i < greeting_length; i++) hello[4 + i] = greeting[i];

                SendMessage(hWndChat, SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)g_font0);
                SendMessage(hWndChat, SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT, g_fsize0);
                SendMessage(hWndChat, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
                SendMessage(hWndChat, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);

                if(HasInputData == FALSE)
                    SendMessage(hWndChat, SCI_SETTEXT, 0, (LPARAM)hello);
                else if(prev_chatdata)
                    SendMessage(hWndChat, SCI_SETTEXT, 0, (LPARAM)prev_chatdata);

                SendMessage(hWndChat, SCI_SETREADONLY, TRUE, 0);
            }

            if (IsWindow(hWndEdit))
            {
                SendMessage(hWndEdit, SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)g_font1);
                SendMessage(hWndEdit, SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT, g_fsize1);
                SendMessage(hWndEdit, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
                SendMessage(hWndEdit, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
                SendMessage(hWndEdit, SCI_SETEOLMODE, SC_EOL_LF, 0);
                SendMessage(hWndEdit, EM_SETREADONLY, FALSE, 0);
            }
            HasInputData = FALSE;
        }
        return 0;
    case WM_DESTROY:
        {
            GetWindowRect(hWnd, &rectChat);
            KillTimer(hWnd, TIMER_ASKROB);
            g_QuitAskRob = 1;
            if (IsWindow(hWndChat)) /* save the chat history */
            {
                U32 length = SendMessage(hWndChat, SCI_GETTEXTLENGTH, 0, 0);
                if (length)
                {
                    if (prev_chatdata) /* free the memory block allocated in previous window */
                    {
                        free(prev_chatdata);
                        prev_chatdata = NULL;
                    }

                    prev_chatdata = (U8*)malloc(AR_ALIGN_DEFAULT64(length + 1));
                    if (prev_chatdata)
                    {
                        SendMessage(hWndChat, SCI_GETTEXT, length, (LPARAM)prev_chatdata);
                        prev_chatdata[length] = '\0';
                    }
                }
            }
        }
        break;
    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static const char reply_head[] = {'\n',0xF0,0x9F,0x99,0x82,'\n','-','-','\n',0 };

size_t Curl_Write_Callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t realsize = size * nmemb;
    if (ptr && realsize > 0)
    {
        U32 length;
        MessageTask* mt = NULL;
        if (g_AutoLogging)
        {
            int fd = 0;
            if (_wsopen_s(&fd, g_logFile, _O_WRONLY | _O_APPEND | _O_CREAT, _SH_DENYNO, _S_IWRITE) == 0)
            {
                SYSTEMTIME st = { 0 };
                char tmpbuf[128] = { 0 };
                GetSystemTime(&st);
                sprintf_s(tmpbuf, 128, "\n[%04d/%02d/%02d %02d:%02d:%02d]<=\n",
                    st.wYear,
                    st.wMonth,
                    st.wDay,
                    st.wHour,
                    st.wMinute,
                    st.wSecond
                );
                _write(fd, tmpbuf, strlen(tmpbuf));
                _write(fd, ptr, realsize);
                _close(fd);
            }
        }

        length = sizeof(MessageTask) + (U32)realsize + 9 + 1;
        mt = (MessageTask*)malloc(length);
        if (mt)
        {
            U8* p;
            MessageTask* mp;
            MessageTask* mq;

            mt->next  = NULL;
            mt->state = 0;
            mt->message_type = 0;
            mt->message = NULL;
            mt->message_length = (U32)realsize + 9;
            mt->message = ((U8*)mt) + sizeof(MessageTask);
            p = mt->message;
            for (U8 i = 0; i < 9; i++) *p++ = reply_head[i];
            memcpy(p, ptr, realsize);
            mt->message[mt->message_length - 1] = '\n';
            mt->message[mt->message_length] = 0;

            EnterCriticalSection(&g_csReceMsg);
            /////////////////////////////////////////////////
            mp = g_mtIncoming;
            while (mp) // scan the link to find the message that has been processed
            {
                mq = mp->next;
                if (mp->state == 0) // this task is not processed yet.
                    break;
                free(mp);
                mp = mq;
            }
            g_mtIncoming = mp;
            if (g_mtIncoming == NULL)
            {
                g_mtIncoming = mt;
            }
            else
            {
                while (mp->next) mp = mp->next;
                mp->next = mt;  // put task as the last node
            }
            /////////////////////////////////////////////////
            LeaveCriticalSection(&g_csReceMsg);
        }
    }
    return realsize;
}

static DWORD WINAPI network_threadfunc(void* param)
{
    CURL* curl;
    InterlockedIncrement(&g_threadCount);

    g_mtIncoming = NULL;

    curl = curl_easy_init();

    if (curl)
    {
        U8* postBuf = NULL;
        U32 postLen = 0;

        curl_easy_setopt(curl, CURLOPT_URL, g_url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Curl_Write_Callback);

        if (g_proxy_type && strlen(g_proxy) > 0)
        {
            long pxtype = CURLPROXY_HTTP;
            switch (g_proxy_type)
            {
            case 1:
                pxtype = CURLPROXY_HTTP;
                break;
            case 2:
                pxtype = CURLPROXY_HTTP_1_0;
                break;
            case 3:
                pxtype = CURLPROXY_HTTPS;
                break;
            case 4:
                pxtype = CURLPROXY_HTTPS2;
                break;
            case 5:
                pxtype = CURLPROXY_SOCKS4;
                break;
            case 6:
                pxtype = CURLPROXY_SOCKS5;
                break;
            case 7:
                pxtype = CURLPROXY_SOCKS4A;
                break;
            case 8:
                pxtype = CURLPROXY_SOCKS5_HOSTNAME;
                break;
            default:
                break;
            }
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, pxtype);
            curl_easy_setopt(curl, CURLOPT_PROXY, g_proxy);
        }

        postBuf = (U8*)VirtualAlloc(NULL, INPUT_BUF_MAX + INPUT_BUF_MAX, MEM_COMMIT, PAGE_READWRITE);
        if (postBuf)
        {
            bool pickup;
            U32 status, i, utf8len;
            U8* p;
            U8* q;
            while (0 == g_Quit)
            {
                Sleep(500);
                pickup = false;
                EnterCriticalSection(&g_csSendMsg);
                if (inputBufPos > 0 && screenBufPos > (-1))
                {
                    p = postBuf;
                    q = g_appKey;
                    for (i = 0; i < 67; i++) *p++ = *q++;
                    q = inputBuffer + 9;
                    memcpy(p, q, inputBufPos);
                    p += inputBufPos;
                    postLen = 67 + inputBufPos;
                    if (screenBufPos > 0) /* there is some screen data */
                    {
                        *p++ = '"'; *p++ = '"'; *p++ = '"'; *p++ = '\n';
                        postLen += 4;
                        utf8len = 0;
                        status = wt_UTF16ToUTF8(screenBuffer, screenBufPos, NULL, &utf8len);
                        if (status == WT_OK)
                        {
                            status = wt_UTF16ToUTF8(screenBuffer, screenBufPos, p, NULL);
                            assert(status == WT_OK);
                            p += utf8len;
                            postLen += utf8len;
                        }
                        *p++ = '\n'; *p++ = '"'; *p++ = '"'; *p++ = '"'; *p++ = '\0';
                        postLen += 4;
                    }
                    inputBufPos = 0;
                    screenBufPos = -1;
                    pickup = true;
                }
                LeaveCriticalSection(&g_csSendMsg);

                if (pickup)
                {
                    CURLcode rc; 
                    if (g_AutoLogging)
                    {
                        int fd = 0;
                        if (_wsopen_s(&fd, g_logFile, _O_WRONLY | _O_APPEND | _O_CREAT, _SH_DENYNO, _S_IWRITE) == 0)
                        {
                            SYSTEMTIME st = { 0 };
                            char tmpbuf[128] = { 0 };
                            GetSystemTime(&st);
                            sprintf_s(tmpbuf, 128, "\n[%04d/%02d/%02d %02d:%02d:%02d]=>\n",
                                st.wYear,
                                st.wMonth,
                                st.wDay,
                                st.wHour,
                                st.wMinute,
                                st.wSecond
                            );

                            _write(fd, tmpbuf, strlen(tmpbuf));
                            _write(fd, postBuf + 67, postLen - 67);
                            _close(fd);
                        }
                    }
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBuf);
                    rc = curl_easy_perform(curl);
                }
            }
            VirtualFree(postBuf, 0, MEM_RELEASE);
        }
        curl_easy_cleanup(curl);
    }

    InterlockedDecrement(&g_threadCount);
    return 0;
}

static DWORD WINAPI askrob_threadfunc(void* param)
{
    int xPos, yPos;
    int width, height;
    RECT rect = { 0 };
    MSG msg;

    InterlockedIncrement(&g_threadCount);
    g_QuitAskRob = 0;

    xPos = rectChat.left;
    yPos = rectChat.top;
    width = rectChat.right - rectChat.left;
    height = rectChat.bottom - rectChat.top;

    if (width <= 0) width = 480;
    if (height <= 0) height = 800;

    hWndAskRob = CreateWindowExW(
        0,                        // Optional window styles
        ASKROB_MAIN_CLASS_NAME,   // Window class
        L"",  // ASKROB_MAIN_TITLE_NAME,   // Window title
        WS_OVERLAPPEDWINDOW,     // Window style
        xPos, yPos, width, height,    // Size and position
        NULL,       // Parent window
        NULL,       // Menu
        hInstAskRob, // Instance handle
        NULL        // Additional application data
    );

    if (IsWindow(hWndAskRob))
    {
        ShowWindow(hWndAskRob, SW_SHOW);
        UpdateWindow(hWndAskRob);

        while (0 == g_Quit && 0 == g_QuitAskRob)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
            {
                GetMessage(&msg, NULL, 0, 0);
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    if (IsWindow(hWndAskRob))
    {
        DestroyWindow(hWndAskRob);
    }
    hWndAskRob = NULL;

    InterlockedDecrement(&g_threadCount);
    return 0;
}

static void StartupAskRob_UI_Thread(RECT* pos)
{
    DWORD in_threadid; /* required for Win9x */
    HANDLE hThread;
    RECT* rc = &rectChat; /* we will use the postion save in rectChat at first */

    if (pos) /* this is a position of Putty window so we set chat window right beside it */
    {
        int w = rc->right - rc->left;
        int h = rc->bottom - rc->top;
        rc->left = pos->right + 10;
        rc->top = pos->top;
        rc->right = rc->left + w;
        rc->bottom = rc->top + h;
    }

    hThread = CreateThread(NULL, 0, askrob_threadfunc, rc, 0, &in_threadid);

    if (hThread)
        CloseHandle(hThread);          /* we don't need the thread handle */
}

/*
 * Set all parameters to the default values except the "key"
 * The "key" must be obtained from some place
 */
static void SetDeaultSettings()
{
    U8 i;

    /* initialize the integer parameters to the default value */
    g_AutoLogging     = 1;
    g_AskRobAtStartUp = 1;
    g_proxy_type      = 0;
    g_fsize0          = 1100;
    g_fsize1          = 1100;

    for (i = 0; i < 67; i++) /* set the invalid key value */
    {
        g_appKey[i] = '*';
    }

    /* set the default font: Courier New */
    for (i = 0; i < 11; i++)
    {
        g_font0[i] = defaultFont[i];
        g_font1[i] = defaultFont[i];
    }
    g_font0[11] = g_font1[11] = '\0';

    /* set the default URL: https://www.wochat.org/v1 */
    for (i = 0; i < 25; i++)
    {
        g_url[i] = defaultURL[i];
    }
    g_url[25] = '\0';

    g_proxy[0] = '\0';
}

/*
 * The only value we need to get is the key value that is 66 bytes long.
 * Othere parameters have the default value
 */
static bool Json_Parsing(const char* jdata)
{
    bool bRet = false;
    assert(jdata);

    cJSON* json = cJSON_Parse(jdata); /* parse the jsondata */
    if (json)
    {
        U8 length;
        cJSON* key     = cJSON_GetObjectItemCaseSensitive(json, "key");
        cJSON* url     = cJSON_GetObjectItemCaseSensitive(json, "url");
        cJSON* font0   = cJSON_GetObjectItemCaseSensitive(json, "font0");
        cJSON* font1   = cJSON_GetObjectItemCaseSensitive(json, "font1");
        cJSON* fsize0  = cJSON_GetObjectItemCaseSensitive(json, "fsize0");
        cJSON* fsize1  = cJSON_GetObjectItemCaseSensitive(json, "fsize1");
        cJSON* startct = cJSON_GetObjectItemCaseSensitive(json, "startchat");
        cJSON* autolog = cJSON_GetObjectItemCaseSensitive(json, "autologging");
        cJSON* proxytp = cJSON_GetObjectItemCaseSensitive(json, "proxy_type");
        cJSON* proxy   = cJSON_GetObjectItemCaseSensitive(json, "proxy");

        /* at least we need to the gehe application's key from conf.json */
        if (cJSON_IsString(key))
        {
            length = (U8)strlen(key->valuestring);
            if (length == 66) /* the key has 66 characters */
            {
                U8 oneChar, i;
                U8* p = key->valuestring;
                /* check if the key is valid, only 0-9, A-Z is good */
                for (i = 0; i < 66; i++) 
                {
                    oneChar = (U8)p[i];
                    g_appKey[i] = oneChar;
                    if (oneChar >= '0' && oneChar <= '9') continue;
                    if (oneChar >= 'A' && oneChar <= 'F') continue;
                    break;
                }
                if (i == 66) /* yes, the key is good */
                {
                    g_appKey[66] = '|';
                    bRet = true;
                }
            }
        }

        if (cJSON_IsString(url)) /* try to get the URL */
        {
            length = (U8)strlen(url->valuestring);
            if (length)
            {
                if (length > 127) length = 127;
                memcpy(g_url, url->valuestring, length);
                g_url[length] = '\0';
            }
        }
        if (cJSON_IsString(font0)) /* try to get the font name of the chat window */
        {
            length = (U8)strlen(font0->valuestring);
            if (length)
            {
                if (length > 31) length = 31;
                memcpy(g_font0, font0->valuestring, length);
                g_font0[length] = '\0';
            }
        }
        if (cJSON_IsString(font1)) /* try to get the font name of the input window */
        {
            length = (U8)strlen(font1->valuestring);
            if (length)
            {
                if (length > 31) length = 31;
                memcpy(g_font1, font1->valuestring, length);
                g_font1[length] = '\0';
            }
        }
        if (cJSON_IsNumber(fsize0)) /* try to get the font size of the chat window */
        {
            g_fsize0 = (U32)(fsize0->valueint) * 100;
        }
        if (cJSON_IsNumber(fsize1)) /* try to get the font size of the input window */
        {
            g_fsize1 = (U32)(fsize1->valueint) * 100;
        }
        if (cJSON_IsNumber(startct)) /* should we start the chat window at startup? */
        {
            g_AskRobAtStartUp = (U8)(startct->valueint);
        }
        if (cJSON_IsNumber(autolog)) /* should we auto log all the chat data? */
        {
            g_AutoLogging = (U8)(autolog->valueint);
        }

        if (cJSON_IsNumber(proxytp)) /* the proxy type range from 0 to 8 */
        {
            g_proxy_type = (U8)(proxytp->valueint);
        }

        if (g_proxy_type) /* if g_proxy_type is not zero, then we need to check the proxy string */
        {
            if (cJSON_IsString(proxy)) /* try to get the proxy string */
            {
                length = (U8)strlen(proxy->valuestring);
                if (length)
                {
                    if (length > 255) length = 255;
                    memcpy(g_proxy, proxy->valuestring, length);
                    g_proxy[length] = '\0';
                }
            }
        }

        cJSON_Delete(json);
    }

    return bRet;
}

/* 
 * load the application configuration information 
 * from conf.json and set the log file name 
 */
static bool LoadConfiguration(HINSTANCE hInstance)
{
    bool bRet = false;
    DWORD len, idx;
    
    len = GetModuleFileNameW(hInstance, g_cnfFile, MAX_PATH);
    idx = 0;
    if (len > 0) /* we get the full path of the .exe file */
    {
        for (idx = len - 1; idx > 0; idx--) /* we scan from the tail to the head to find the first "\" */
        {
            if (g_cnfFile[idx] == L'\\')
            {
                g_cnfFile[idx] = L'\0'; /* we only need the path where the exe file is in */
                break;
            }
        }
    }
    /* well, this is a valid path, so we find conf.json and logfile here */
    if (idx > 0 && idx < (MAX_PATH - 20)) 
    {
        int fd = 0;
        DWORD pid = GetCurrentProcessId(); 
        /* we use the current process id as the postfix of the log file */
        swprintf_s(g_logFile, MAX_PATH, L"%s\\log_%u.txt", g_cnfFile, pid);
        /* make the file name of conf.json */
        g_cnfFile[idx +  0] = L'\\';
        g_cnfFile[idx +  1] = L'c';
        g_cnfFile[idx +  2] = L'o';
        g_cnfFile[idx +  3] = L'n';
        g_cnfFile[idx +  4] = L'f';
        g_cnfFile[idx +  5] = L'.';
        g_cnfFile[idx +  6] = L'j';
        g_cnfFile[idx +  7] = L's';
        g_cnfFile[idx +  8] = L'o';
        g_cnfFile[idx +  9] = L'n';
        g_cnfFile[idx + 10] = L'\0';

        if(TRUE)
        {  /* check if the conf.json file is available */
            BOOL bAvailable = FALSE;
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            if (GetFileAttributesExW(g_cnfFile, GetFileExInfoStandard, &fileInfo) != 0)
            {
                if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) 
                    bAvailable = TRUE;
            }
            if (!bAvailable) /* conf.json is not exsiting */
            {
                if (_wsopen_s(&fd, g_cnfFile, _O_WRONLY | _O_CREAT, _SH_DENYNO, _S_IWRITE) == 0)
                {
                    size_t json_length = strlen(default_conf_json);
                    _write(fd, default_conf_json, json_length);
                    _close(fd);
                    fd = 0;
                }
            }
        }

        /* set the default paramter values */
        SetDeaultSettings();

        if (_wsopen_s(&fd, g_cnfFile, _O_RDONLY | _O_BINARY, _SH_DENYWR, 0) == 0)
        {
            U32 size = (U32)_lseek(fd, 0, SEEK_END); /* get the file size */
            if (size >= 128 && size < INPUT_BUF_MAX)
            {
                char* jsondata = (char*)VirtualAlloc(NULL, INPUT_BUF_MAX, MEM_COMMIT, PAGE_READWRITE);
                if (jsondata)
                {
                    U32 bytes;

                    _lseek(fd, 0, SEEK_SET);  /* go to the begin of the file */
                    /* read the total file into the buffer of jsondata */
                    bytes = (U32)_read(fd, jsondata, size);
                    if (bytes == size) /* the read is good because the bytes we get is equal to the file size */
                    {
                        jsondata[bytes] = '\0'; /* make jsondata zero-terminated string */
                        bRet = Json_Parsing(jsondata);
                        if (!bRet) /* if we cannot parse the conf.json, we try to parse the default json settings */
                        {
                            bRet = Json_Parsing(default_conf_json);
                        }
                    }
                    else /* there is something wrong when trying to read the data */
                    {
                        bRet = Json_Parsing(default_conf_json);
                    }

                    VirtualFree(jsondata, 0, MEM_RELEASE);
                }
                else /* we cannot get enough memory, so we use the default configuration */
                {
                    bRet = Json_Parsing(default_conf_json);
                }
            }
            else /* the conf.json has small size that seems invalid */
            {
                bRet = Json_Parsing(default_conf_json);
            }
            _close(fd);
        }
        else /* if we cannot read the conf.json, we need to parse the default json data */
        {
            bRet = Json_Parsing(default_conf_json);
        }
    }

    return bRet;
}

/* 
 * the initialization of AskRob. You should call this function only once 
 * at the begining of WinMain()
 * 
 */
static void AR_Init(HINSTANCE hInstance)
{
    int  r = 0;

    hInstAskRob = hInstance;
    AskRobIsGood = FALSE;

    InitializeCriticalSection(&g_csSendMsg);
    InitializeCriticalSection(&g_csReceMsg);

    assert(prev_chatdata == NULL);

    prev_chatdata = NULL;

    if (LoadConfiguration(hInstance)) /* load the configuration information from conf.json */
    {
        WNDCLASSEXW wc = { 0 };

        if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) /* initialize libCURL */
            r++;

        if (Scintilla_RegisterClasses(hInstance) == 0) /* initalize Scintialla */
            r++;

        /* we set a default postion of the chat window */
        rectChat.left = 100;
        rectChat.right = rectChat.left + 480;
        rectChat.top = 100;
        rectChat.bottom = rectChat.top + 800;

        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;
        wc.hInstance = hInstance;
        wc.lpfnWndProc = AskRobWindowProc;
        wc.lpszClassName = ASKROB_MAIN_CLASS_NAME;
        wc.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(IDI_MAINICON));
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);

        if (RegisterClassExW(&wc) == 0)
            r++;

        assert(inputBuffer == NULL); /* 64KB input is big enough */
        inputBufPos = 0;
        inputBuffer = (U8*)VirtualAlloc(NULL, INPUT_BUF_MAX, MEM_COMMIT, PAGE_READWRITE);
        if (inputBuffer == NULL) r++;

        assert(screenBuffer == NULL); /* 32 KB for the screen should be good */
        screenBufPos = -1;
        screenBuffer = (wchar_t*)VirtualAlloc(NULL, INPUT_BUF_MAX, MEM_COMMIT, PAGE_READWRITE);
        if (screenBuffer == NULL) r++;

        /* load the bitmap resource from the resource */
        bmpQuestion = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_QUESTION));
        if (bmpQuestion == NULL) r++;

        bmpSaveFile = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SAVEFILE));
        if (bmpSaveFile == NULL) r++;

        bmpEmptyLog = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_EMPTYLOG));
        if (bmpEmptyLog == NULL) r++;

        bmpSettings = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_SETTINGS));
        if (bmpSettings == NULL) r++;

        hCursorHand = LoadCursor(NULL, IDC_HAND);

        if (r == 0)
        {
            DWORD in_threadid; /* required for Win9x */
            HANDLE hThread;

            AskRobIsGood = TRUE;

            /* start up the backend network thread */
            hThread = CreateThread(NULL, 0, network_threadfunc, NULL, 0, &in_threadid);
            if (hThread)
                CloseHandle(hThread);          /* we don't need the thread handle */
        }
    }
}

/* 
 * you should call this function only once at the end of 
 * WinMain to release the resource used by AskRob 
 * 
 */
static void AR_Term()
{
    UINT tries = 10;
    /* tell all threads to quit gracefully */
    InterlockedIncrement(&g_Quit); 
    /* wait the threads to quit */
    while (g_threadCount && tries > 0) 
    {
        Sleep(1000);
        tries--;
    }

    if (prev_chatdata)
    {
        free(prev_chatdata);
        prev_chatdata = NULL;
    }

    if (inputBuffer)
    {
        VirtualFree(inputBuffer, 0, MEM_RELEASE);
        inputBuffer = NULL;
        inputBufPos = 0;
    }

    if (screenBuffer)
    {
        VirtualFree(screenBuffer, 0, MEM_RELEASE);
        screenBuffer = NULL;
        screenBufPos = -1;
    }

    if (AskRobIsGood)
    {
        Scintilla_ReleaseResources();
    }

    if (bmpQuestion)
    {
        DeleteObject(bmpQuestion);
    }

    if (bmpSaveFile)
    {
        DeleteObject(bmpSaveFile);
    }

    if (bmpEmptyLog)
    {
        DeleteObject(bmpEmptyLog);
    }

    DeleteCriticalSection(&g_csSendMsg);
    DeleteCriticalSection(&g_csReceMsg);
}

#endif /* _ASKROB_H_ */