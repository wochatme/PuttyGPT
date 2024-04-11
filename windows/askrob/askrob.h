/* Get-Content log.txt -Wait */
#ifndef _ASKROB_H_
#define _ASKROB_H_

#include <stdint.h>
#include <fcntl.h>
#include <io.h>
#include <Commctrl.h>
#include <bcrypt.h>
#include "curl/curl.h"
#include "cjson/cJSON.h"
#include "Sci_Position.h"
#include "scintilla.h"

#include "askrob_util.h"

#pragma comment(lib, "crypt32.lib")

#if 0
static const char* tip_settings      = "change the configuration parameters";
static const char* tip_savechatlog   = "save the chat history into text file";
static const char* tip_emptychatlog  = "empty the chat history window";
static const char* tip_askquestion   = "ask a question to AI";
#endif 
static const char* tip_networkstatus = "Network Status: green is good, red is bad";

static const char* default_conf_json =
"{\n\"key\" : \"03339A1C8FDB6AFF46845E49D120E0400021E161B6341858585C2E25CA3D9C01CA\",\n"
"\"url\" : \"https://www.wochat.org/v1\",\n"
"\"font0\" : \"Courier New\",\n"
"\"font1\" : \"Courier New\",\n"
"\"fsize0\" : 11,\n"
"\"fsize1\" : 11,\n"
"\"startchat\" : 1,\n"
"\"screen\" : 1,\n"
"\"autologging\" : 1,\n"
"\"proxy_type\" : 0,\n"
"\"proxy\" : \"\"\n}\n";

/* the variables to save configuration information */
static const char* defaultFont = "Courier New";
static const char* defaultURL  = "https://www.wochat.org/v1";

static U8  g_appKey[67] = { 0 };
static U8  g_url[256] = { 0 };
static U8  g_font0[32] = { 0 };
static U8  g_font1[32] = { 0 };
static U32 g_fsize0 = 1100;
static U32 g_fsize1 = 1100;
static U8  g_AskRobAtStartUp = 1;
static U8  g_AutoLogging = 1;
static U8  g_screen = 1;

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

#define TIMER_ASKROB        666  /* 666 ms for timer */

#define INPUT_BUF_MAX       (1<<18) /* 256 KB should be big enough */
#define INPUT_BUF_64KB      (1<<16)

#define WM_BRING_TO_FRONT   (WM_USER + 1)
#define WM_NETWORK_STATUS   (WM_USER + 2)

static const LPCWSTR ASKROB_MAIN_CLASS_NAME = L"AskRobWin";
static const LPCWSTR ASKROB_MAIN_TITLE_NAME = L"X";

static const char* greeting = "\n--\nHi, I am your humble servant. You can ask me any question by typing in the below window.\n";

static volatile LONG g_threadCount = 0;
static volatile LONG g_Quit = 0;
static volatile LONG g_QuitAskRob = 0;

#define NETWORK_BAD     0
#define NETWORK_GOOD    1

static volatile LONG g_NetworkStatus = NETWORK_BAD;

static wchar_t g_logFile[MAX_PATH + 1] = { 0 };
static wchar_t g_cnfFile[MAX_PATH + 1] = { 0 };

static BOOL AskRobIsGood = FALSE;
static BOOL HasInputData = FALSE;

static HINSTANCE hInstAskRob = NULL;

static HWND hWndPutty  = NULL;
static HWND hWndAskRob = NULL; /* the window for AI chat */
static HWND hWndChat   = NULL;   /* the child window in hWndAskRob */
static HWND hWndEdit   = NULL;   /* the child window in hWndAskRob */
static HWND hWndToolTip = NULL;

#define WIN_SIZE_GAP    25
static int INPUT_WIN_HEIGHT = 140;
static int INPUT_BUF_OFFSET = 0;

/* 6 buttons */
static HBITMAP bmpQuestion = NULL;
static HBITMAP bmpSaveFile = NULL;
static HBITMAP bmpEmptyLog = NULL;
static HBITMAP bmpSettings = NULL;
static HBITMAP bmpNetwork0 = NULL;
static HBITMAP bmpNetwork1 = NULL;

static HCURSOR	hCursorHand = NULL;
static HCURSOR	hCursorNS   = NULL;

static RECT rectChat = { 0 }; /* to record the postion of the AskRob chat window */
static RECT g_rectClient = { 0 }; /* to cache the client area of the main chat window */

static U8*       inputBuffer = NULL;
static int       inputBufPos = 0;
static wchar_t*  screenBuffer = NULL;
static int       screenBufPos = 0;

static U8* prev_chatdata = NULL;

/* this two functions are in askrob_terminal.h */
void term_copy_current_screen(Terminal* term, const int* clipboards, int n_clipboards);
wchar_t* term_copyScreen(Terminal* term, const int* clipboards, int n_clipboards);

/* copy the data in the current screen of Putty window and save to screenBuffer */
static void AR_CopyScreen(Terminal* term, const int* clipboards, int n_clipboards)
{
    wchar_t* sbuf;

    EnterCriticalSection(&g_csSendMsg);
        screenBufPos = 0;
    LeaveCriticalSection(&g_csSendMsg);

    sbuf = term_copyScreen(term, clipboards, n_clipboards);
    if (sbuf)
    {
        U32 wlen = (U32)wcslen(sbuf);
        if (wlen && wlen < (INPUT_BUF_MAX / 2))
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
            INPUT_BUF_OFFSET = 9;
            inputBufPos = input_len;
        LeaveCriticalSection(&g_csSendMsg);

        if(g_screen)
            PostMessage(hWndPutty, WM_COMMAND, IDM_COPYSCREEN, 0);
    }

    return 0;
}

static int DoSaveLog(HWND hWnd)
{
    if (IsWindow(hWndChat)) /* save the chat history */
    {
        U32 length = SendMessage(hWndChat, SCI_GETTEXTLENGTH, 0, 0);
        if (length) /* the chat window has some text */
        {
            OPENFILENAMEW ofn;       
            wchar_t szFile[MAX_PATH + 1];

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.lpstrFile[0] = L'\0';
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"Text Files(*.txt)\0*.txt\0All Files(*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

            if (GetSaveFileNameW(&ofn) == TRUE) /* Display the "Save As" dialog */
            {
                char* buf = (char*)malloc(AR_ALIGN_DEFAULT(length));
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

int DoPaint(HWND hWnd, HDC hdc)
{
    RECT rc;
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
            if (bmpNetwork0 && bmpNetwork1)
            {
                if(g_NetworkStatus == NETWORK_GOOD)
                    bmp = SelectObject(hDCBitmap, bmpNetwork0);
                else
                    bmp = SelectObject(hDCBitmap, bmpNetwork1);
                BitBlt(hdcMem, rc.right - 20, rc.bottom - 160, 15, 15, hDCBitmap, 0, 0, SRCCOPY);
                SelectObject(hDCBitmap, bmp);
            }
            DeleteDC(hDCBitmap);
        }

        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, bmpOld);
        DeleteDC(hdcMem);
    }

    return 0;
}

static int DoNotify(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    LPNMHDR lpnmhdr = (LPNMHDR)lParam;

    if (IsWindow(hWndEdit) && (lpnmhdr->hwndFrom == hWndEdit) && (lpnmhdr->code == SCN_CHARADDED))
    {
        bool heldControl = (GetKeyState(VK_CONTROL) & 0x80) != 0; /* does the user hold Ctrl key? */
        int  currentPos = SendMessage(hWndEdit, SCI_GETCURRENTPOS, 0, 0);
        char ch = SendMessage(hWndEdit, SCI_GETCHARAT, currentPos - 1, 0);

        INPUT_BUF_OFFSET = 0;

        if (ch == '\n' && heldControl == false) /* the user hit the ENTER key */
        {
            U32 input_len  = SendMessage(hWndEdit, SCI_GETTEXTLENGTH, 0, 0); /* in bytes */
            if (input_len > 1 && input_len < INPUT_BUF_MAX - 16)
            {
                bool allSpace = false;
                U8* p = inputBuffer;
                /* add the prefix to show in the chat window */
                p[0] = '\n'; p[1] = 0xF0; p[2] = 0x9F; p[3] = 0xA4; p[4] = 0x9A; 
                p[5] = '\n';  p[6] = '-'; p[7] = '-'; p[8] = '\n';

                p = inputBuffer + 9; /* skip the first 9 bytes */
                /* get the string from the input window */
                SendMessage(hWndEdit, SCI_GETTEXT, input_len, (LPARAM)p);

                allSpace = wt_IsAllSpace(p, input_len); /* check if every character is space character */

                if(allSpace == false) /* the string contains some non-space characters */
                {
                    INPUT_BUF_OFFSET = 9;
                    if(input_len >= 3)
                    {
                        /* if the first two characters are "--", the we do not send screen data to the server  */
                        if(p[0] == '-' && p[1] == '-')
                        {
                            allSpace = wt_IsAllSpace(p + 2, input_len - 2);
                            if (allSpace == false)
                            {
                                INPUT_BUF_OFFSET = 11;
                                for (int i = 2; i < input_len; i++)
                                {
                                    /* remove the leading space character */
                                    if (p[i] != ' ' && p[i] != '\t' && p[i] != '\n' && p[i] != '\n')
                                        break;
                                    INPUT_BUF_OFFSET++;
                                }
                            }
                            else INPUT_BUF_OFFSET = 0; /* the string contains only "--" and space characters, we do not send it */
                        }
                    }

                    if(allSpace == false) /* the user does input something */
                    {
                        if (IsWindow(hWndChat))
                        {
                            int totalLines = (int)SendMessage(hWndChat, SCI_GETLINECOUNT, 0, 0);
                            SendMessage(hWndChat, SCI_SETREADONLY, FALSE, 0);
                            SendMessage(hWndChat, SCI_APPENDTEXT, 9 + input_len, (LPARAM)inputBuffer);
                            SendMessage(hWndChat, SCI_SETREADONLY, TRUE, 0);
                            SendMessage(hWndChat, SCI_LINESCROLL, 0, totalLines);
                            HasInputData = TRUE; /* the user has inputed some text */
                        }

                        EnterCriticalSection(&g_csSendMsg);
                            /* update the inputBufPos */ 
                            inputBufPos = 0;
                            if (INPUT_BUF_OFFSET >= 9)
                                inputBufPos = input_len - (INPUT_BUF_OFFSET - 9);
                        LeaveCriticalSection(&g_csSendMsg);

                        if (g_screen && IsWindow(hWndPutty)) /* get the screen data */
                        {
                            PostMessage(hWndPutty, WM_COMMAND, IDM_COPYSCREEN, 0);
                        }
                    }
                }
            }
            SendMessage(hWndEdit, SCI_SETTEXT, 0, (LPARAM)"");
        }
    }
    return 0;
}

static RECT rcPaint = { 0 };

/* the main window proc for AI chat window */
static LRESULT CALLBACK AskRobWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static BOOL bCursorIsChanged = FALSE;

    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 0;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            DoPaint(hWnd, hdc);
            EndPaint(hWnd, &ps);
        }
        return 0;
    case WM_SETCURSOR:
        if (((HWND)wParam == hWnd) && (LOWORD(lParam) == HTCLIENT))
        {
            if (bCursorIsChanged) 
            {
                bCursorIsChanged = FALSE;
                return 0; 
            }
        }
        break;
    case WM_MOUSEMOVE:
        bCursorIsChanged = FALSE;
        if (hCursorHand && hCursorNS)
        {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);

            if (yPos >= g_rectClient.bottom - (INPUT_WIN_HEIGHT + 20) && yPos < g_rectClient.bottom - (INPUT_WIN_HEIGHT + 5))
            {
                if ((xPos >= 20 && xPos < 20 + 15) 
                    || (xPos >= 20 + 30 && xPos < 20 + 30 + 15) 
                    || (xPos >= 20 + 60 && xPos < 20 + 60 + 15)
                    || (xPos >= 20 + 90 && xPos < 20 + 90 + 15))
                {
                    SetCursor(hCursorHand);
                    bCursorIsChanged = TRUE;
                }
                else if(IsWindow(hWndToolTip))
                {
                    if (xPos >= g_rectClient.right - 20 && xPos < g_rectClient.right)
                        SendMessage(hWndToolTip, TTM_ACTIVATE, TRUE, 0);
                    else
                        SendMessage(hWndToolTip, TTM_ACTIVATE, FALSE, 0);
                }
            }
            else if((yPos > g_rectClient.bottom - (INPUT_WIN_HEIGHT + WIN_SIZE_GAP) && yPos <= g_rectClient.bottom - (INPUT_WIN_HEIGHT + WIN_SIZE_GAP - 2)) 
                 || (yPos >= g_rectClient.bottom - (INPUT_WIN_HEIGHT + 2) && yPos < g_rectClient.bottom - INPUT_WIN_HEIGHT))
            {
                SetCursor(hCursorNS);    
                bCursorIsChanged = TRUE;
            }
        }
        return 0;
    case WM_LBUTTONDOWN:
        if(true)
        {
            int hit = -1;
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            if (yPos >= g_rectClient.bottom - (INPUT_WIN_HEIGHT + 20) && yPos < g_rectClient.bottom - (INPUT_WIN_HEIGHT + 5))
            {
                if ((xPos >= 20 && xPos < 20 + 15) 
                    || (xPos >= 20 + 30 && xPos < 20 + 30 + 15) 
                    || (xPos >= 20 + 60 && xPos < 20 + 60 + 15)
                    || (xPos >= 20 + 90 && xPos < 20 + 90 + 15))
                    hit = 0;
            }
            if (hit == -1)
            {
                WINDOWPLACEMENT placement;
                if(GetWindowPlacement(hWnd, &placement))
                {
                    if(placement.showCmd != SW_SHOWMAXIMIZED) /* when the window is not in MAXIMIZED status */
                        PostMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
                }
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (true)
        {
            int hit = -1;
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            if (yPos >= g_rectClient.bottom - (INPUT_WIN_HEIGHT + 20) && yPos < g_rectClient.bottom - (INPUT_WIN_HEIGHT + 5))
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
    case WM_NETWORK_STATUS :
        InvalidateRect(hWnd, &rcPaint, FALSE); /* update the network status LED light */
        return 0;
    case WM_BRING_TO_FRONT :
        SetForegroundWindow(hWnd);
        return 0;
    case WM_TIMER:
        {
            MessageTask* p;  /* in each WM_TIMER call, we only pickup one message from the incomming queue */
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
        return DoNotify(hWnd, wParam, lParam);
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

            GetClientRect(hWnd, &g_rectClient);

            width = g_rectClient.right - g_rectClient.left;
            height = g_rectClient.bottom - g_rectClient.top;
            
            MoveWindow(hWndChat, 0, 0, width, height - (INPUT_WIN_HEIGHT + WIN_SIZE_GAP), TRUE);
            MoveWindow(hWndEdit, 0, height - INPUT_WIN_HEIGHT, width, INPUT_WIN_HEIGHT, TRUE);

            rcPaint.top    = height - (INPUT_WIN_HEIGHT + WIN_SIZE_GAP);
            rcPaint.bottom = height - (INPUT_WIN_HEIGHT);
            rcPaint.right  = g_rectClient.right;
            rcPaint.left = g_rectClient.right - 20; 
        }
        return 0;
    case WM_CREATE:
        {
            UINT_PTR id = SetTimer(hWnd, TIMER_ASKROB, TIMER_ASKROB, NULL);
            hWndChat = CreateWindowExW(0, L"Scintilla", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                0, 0, 16, 16, hWnd, NULL, hInstAskRob, NULL);
            hWndEdit = CreateWindowExW(0, L"Scintilla", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, 
                0, 0, 16, 16, hWnd, NULL, hInstAskRob, NULL);

            hWndToolTip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP, 
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hWnd, NULL, hInstAskRob, NULL);

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

                    prev_chatdata = (U8*)malloc(AR_ALIGN_DEFAULT(length + 1));
                    if (prev_chatdata)
                    {
                        SendMessage(hWndChat, SCI_GETTEXT, length, (LPARAM)prev_chatdata);
                        prev_chatdata[length] = '\0';
                    }
                }
            }
            hWndChat = NULL;
            hWndEdit = NULL;
            hWndToolTip = NULL;
        }
        break;
    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static const char reply_head[] = {'\n',0xF0,0x9F,0x99,0x82,'\n','-','-','\n',0 };
static bool bPinging = false;
size_t Curl_Write_Callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t realsize = size * nmemb;

    if (bPinging == false && ptr && realsize > 0)
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

        length = sizeof(MessageTask) + (U32)realsize + 9 ;
        mt = (MessageTask*)malloc(AR_ALIGN_DEFAULT(length + 1));
        if (mt)
        {
            U8* p;
            MessageTask* mp;
            MessageTask* mq;

            mt->next  = NULL;
            mt->state = 0;
            mt->message_type = 0;
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
    U32 pingCount = 0;
    CURL* curl;
    InterlockedIncrement(&g_threadCount);

    g_mtIncoming = NULL;

    curl = curl_easy_init();

    if (curl)
    {
        U8* postBuf = NULL;

        curl_easy_setopt(curl, CURLOPT_URL, g_url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Curl_Write_Callback);

        if (g_proxy_type && g_proxy[0])
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
            CURLcode rc;             
            bool pickup, update_network_status;
            U32 status, i, utf8len, postLen = 0;
            U8* p;
            while (0 == g_Quit)
            {
                Sleep(500);
                pingCount++;
                pickup = false;
                update_network_status = false;
                bPinging = false;
                EnterCriticalSection(&g_csSendMsg);
                    if (inputBufPos > 0 && inputBufPos < (INPUT_BUF_MAX - 70) && (INPUT_BUF_OFFSET >= 9))
                    {
                        pickup = true; /* there is some message need to send */
                        p = postBuf;
                        for (i = 0; i < 67; i++) *p++ = g_appKey[i];
                        memcpy(p, inputBuffer + INPUT_BUF_OFFSET, inputBufPos);
                        p += inputBufPos;
                        postLen = 67 + inputBufPos;
                        /* we need to include screen data */
                        if (g_screen && screenBufPos > 0 && (INPUT_BUF_OFFSET == 9)) 
                        {
                            utf8len = 0;
                            status = wt_UTF16ToUTF8(screenBuffer, screenBufPos, NULL, &utf8len);
                            if (status == WT_OK && utf8len && utf8len < INPUT_BUF_MAX)
                            {
                                *p++ = '"'; *p++ = '"'; *p++ = '"'; *p++ = '\n';
                                postLen += 4;
                                status = wt_UTF16ToUTF8(screenBuffer, screenBufPos, p, NULL);
                                assert(status == WT_OK);
                                p += utf8len;
                                postLen += utf8len;
                                *p++ = '\n'; *p++ = '"'; *p++ = '"'; *p++ = '"';
                                postLen += 4;
                            }
                        }
                        *p = '\0';
                        inputBufPos  = 0;    /* reset the length */
                        screenBufPos = 0;
                    }
                LeaveCriticalSection(&g_csSendMsg);

                if (pickup)
                {
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

                    update_network_status = true;
                    InterlockedExchange(&g_NetworkStatus, NETWORK_BAD);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBuf);
                    bPinging = false;
                    rc = curl_easy_perform(curl);
                    if(rc == CURLE_OK)
                    {
                        InterlockedExchange(&g_NetworkStatus, NETWORK_GOOD);
                    }
                }
                else if((pingCount % 10) == 0) /* every 5 seconds */
                {
                    update_network_status = true;
                    InterlockedExchange(&g_NetworkStatus, NETWORK_BAD);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, " ");
                    bPinging = true;
                    rc = curl_easy_perform(curl);
                    if(rc == CURLE_OK)
                    {
                        InterlockedExchange(&g_NetworkStatus, NETWORK_GOOD);
                    }
                }

                if(update_network_status && IsWindow(hWndAskRob))
                {
                    /* update the network status in UI thread */
                    PostMessage(hWndAskRob, WM_NETWORK_STATUS, 0, 0); 
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
    MSG msg;

    InterlockedIncrement(&g_threadCount);
    g_QuitAskRob = 0;

    xPos   = rectChat.left;
    yPos   = rectChat.top;
    width  = rectChat.right - rectChat.left;
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

    if(IsWindow(hWndAskRob))
    {
        if(IsWindow(hWndToolTip))
        {
            RECT clientRect;
            GetClientRect(hWndAskRob, &clientRect);
            TOOLINFO toolInfo = { 0 };
            toolInfo.cbSize = sizeof(TOOLINFO);
            toolInfo.hwnd = hWndAskRob;
            toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            toolInfo.uId = (UINT_PTR)hWndAskRob;
            toolInfo.lpszText = tip_networkstatus;
            toolInfo.rect = clientRect;
            SendMessage(hWndToolTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
        }

        ShowWindow(hWndAskRob, SW_SHOW);
        UpdateWindow(hWndAskRob);

        while (0 == g_Quit && 0 == g_QuitAskRob)
        {
            if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
            {
                GetMessage(&msg, NULL, 0, 0);
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (IsWindow(hWndAskRob))
        {
            DestroyWindow(hWndAskRob);
        }
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
    g_screen          = 1;

    g_proxy[0] = '\0';

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
        cJSON* screen  = cJSON_GetObjectItemCaseSensitive(json, "screen");
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
                if (length > 255) length = 255;
                memcpy(g_url, url->valuestring, length);
                g_url[length] = '\0';
            }
        }

        if (cJSON_IsNumber(screen)) 
        {
            g_screen = (U8)(screen->valueint); /* wether we need to send the screen data to the server? */
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
            WIN32_FILE_ATTRIBUTE_DATA fileInfo = { 0 };

            if (GetFileAttributesExW(g_cnfFile, GetFileExInfoStandard, &fileInfo) != 0) /* we find conf.json */
            {
                if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) /* it is not a directory */
                    bAvailable = TRUE;
            }

            if (bAvailable == FALSE) /* conf.json is not available, we will create a new one by using the default values */
            {
                if (_wsopen_s(&fd, g_cnfFile, _O_WRONLY | _O_CREAT, _SH_DENYNO, _S_IWRITE) == 0)
                {
                    size_t json_length = strlen(default_conf_json);
                    _write(fd, default_conf_json, (unsigned int)json_length);
                    _close(fd);
                }
            }
        }

        /* set the default paramter values */
        SetDeaultSettings();

        fd = 0;
        if (_wsopen_s(&fd, g_cnfFile, _O_RDONLY | _O_BINARY, _SH_DENYWR, 0) == 0) /* we open conf.json successfully */
        {
            U32 size = (U32)_lseek(fd, 0, SEEK_END); /* get the file size */
            if (size >= 128 && size < INPUT_BUF_64KB)
            {
                char* jsondata = (char*)VirtualAlloc(NULL, INPUT_BUF_64KB, MEM_COMMIT, PAGE_READWRITE);
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
    int  r = 0; /* after a serial initialization, if r is 0, then everything is good */

    hInstAskRob = hInstance;  /* cache the global HINSTANCE for some function calls */
    AskRobIsGood = FALSE;     /* a global variable to indicate AskRob is ready or not */

    g_Quit = 0;       /* this two are global signal to let some threads to quit gracefully */
    g_QuitAskRob = 0;
    g_threadCount = 0;
    g_NetworkStatus = NETWORK_BAD;

    INPUT_WIN_HEIGHT = 140;
    INPUT_BUF_OFFSET = 0;

    assert(prev_chatdata == NULL);
    prev_chatdata = NULL;  /* this buffer is used to save the previous chat data so we can restore it later */

    InitializeCriticalSection(&g_csSendMsg);  /* these two are Critial Sections to sync different threads */
    InitializeCriticalSection(&g_csReceMsg);


    if (LoadConfiguration(hInstance)) /* load the configuration information from conf.json */
    {
        WNDCLASSEXW wc = { 0 };

        if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) /* initialize libCURL */
            r++;

        if (Scintilla_RegisterClasses(hInstance) == 0) /* initalize Scintialla */
            r++;

        /* we set a default postion of the chat window */
        rectChat.left   = 100;
        rectChat.right  = rectChat.left + 480;
        rectChat.top    = 100;
        rectChat.bottom = rectChat.top + 800;

        /* register the window class for the chat window */
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;
        wc.hInstance     = hInstance;
        wc.lpfnWndProc   = AskRobWindowProc;
        wc.lpszClassName = ASKROB_MAIN_CLASS_NAME;
        wc.hIcon         = LoadIcon(hinst, MAKEINTRESOURCE(IDI_MAINICON));
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

        if (RegisterClassExW(&wc) == 0)
            r++;

        assert(inputBuffer == NULL); /* 256KB input is big enough */
        inputBufPos = 0;
        inputBuffer = (U8*)VirtualAlloc(NULL, INPUT_BUF_MAX, MEM_COMMIT, PAGE_READWRITE);
        if (inputBuffer == NULL) r++;

        assert(screenBuffer == NULL); /* 128 KB for the screen should be good */
        screenBufPos = 0;
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

        bmpNetwork0 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_NETWORK0));
        if (bmpNetwork0 == NULL) r++;

        bmpNetwork1 = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_NETWORK1));
        if (bmpNetwork1 == NULL) r++;

        hCursorHand = LoadCursor(NULL, IDC_HAND);
        if(hCursorHand == NULL) r++;
        hCursorNS   = LoadCursor(NULL, IDC_SIZENS);
        if(hCursorNS == NULL) r++;

        if (r == 0)
        {
            DWORD in_threadid; /* required for Win9x */
            HANDLE hThread;

            AskRobIsGood = TRUE; /* everything is good so far, set AskRobIsGood = TRUE; */

            /* start up the backend network thread */
            in_threadid = 0;
            hThread = CreateThread(NULL, 0, network_threadfunc, NULL, 0, &in_threadid);
            if (hThread)
            {
                CloseHandle(hThread);          /* we don't need the thread handle */
                hThread = NULL;
            }
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
        screenBufPos = 0;
    }
    if (AskRobIsGood)
    {
        curl_global_cleanup();
        Scintilla_ReleaseResources();
    }
    /* release the bitmap resource */
    if (bmpQuestion)
    {
        DeleteObject(bmpQuestion);
        bmpQuestion = NULL;
    }
    if (bmpSaveFile)
    {
        DeleteObject(bmpSaveFile);
        bmpSaveFile = NULL;
    }
    if (bmpEmptyLog)
    {
        DeleteObject(bmpEmptyLog);
        bmpEmptyLog = NULL;
    }
    if (bmpSettings)
    {
        DeleteObject(bmpEmptyLog);
        bmpSettings = NULL;
    }
    if (bmpNetwork0)
    {
        DeleteObject(bmpNetwork0);
        bmpNetwork0 = NULL;
    }
    if (bmpNetwork1)
    {
        DeleteObject(bmpNetwork1);
        bmpNetwork1 = NULL;
    }
    DeleteCriticalSection(&g_csSendMsg);
    DeleteCriticalSection(&g_csReceMsg);
}

#endif /* _ASKROB_H_ */