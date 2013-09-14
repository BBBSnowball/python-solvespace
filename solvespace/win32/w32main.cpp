//-----------------------------------------------------------------------------
// Our WinMain() functions, and Win32-specific stuff to set up our windows
// and otherwise handle our interface to the operating system. Everything
// outside win32/... should be standard C++ and gl.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gl/gl.h> 
#include <gl/glu.h> 
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <si/si.h>
#include <si/siapp.h>

#include "solvespace.h"

#define FREEZE_SUBKEY "SolveSpace"
#include "freeze.h"

// For the edit controls
#define EDIT_WIDTH  220
#define EDIT_HEIGHT 21

HINSTANCE Instance;

HWND TextWnd;
HWND TextWndScrollBar;
HWND TextEditControl;
HGLRC TextGl;

HWND GraphicsWnd;
HGLRC GraphicsGl;
HWND GraphicsEditControl;
struct {
    int x, y;
} LastMousePos;

char RecentFile[MAX_RECENT][MAX_PATH];
HMENU SubMenus[100];
HMENU RecentOpenMenu, RecentImportMenu;

HMENU ContextMenu, ContextSubmenu;

int ClientIsSmallerBy;

HFONT FixedFont;

// The 6-DOF input device.
SiHdl SpaceNavigator = SI_NO_HANDLE;

//-----------------------------------------------------------------------------
// Routines to display message boxes on screen. Do our own, instead of using
// MessageBox, because that is not consistent from version to version and 
// there's word wrap problems.
//-----------------------------------------------------------------------------

HWND MessageWnd, OkButton;
BOOL MessageDone;
char *MessageString;

static LRESULT CALLBACK MessageProc(HWND hwnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    switch (msg) {
        case WM_COMMAND:
            if((HWND)lParam == OkButton && wParam == BN_CLICKED) {
                MessageDone = TRUE;
            }
            break;

        case WM_CLOSE:
        case WM_DESTROY:
            MessageDone = TRUE;
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            int row = 0, col = 0, i;
            SelectObject(hdc, FixedFont);
            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            for(i = 0; MessageString[i]; i++) {
                if(MessageString[i] == '\n') {
                    col = 0;
                    row++;
                } else {
                    TextOut(hdc, col*SS.TW.CHAR_WIDTH + 10,
                                 row*SS.TW.LINE_HEIGHT + 10, 
                                 &(MessageString[i]), 1);
                    col++;
                }
            }
            EndPaint(hwnd, &ps);
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

HWND CreateWindowClient(DWORD exStyle, char *className, char *windowName,
    DWORD style, int x, int y, int width, int height, HWND parent,
    HMENU menu, HINSTANCE instance, void *param)
{
    HWND h = CreateWindowEx(exStyle, className, windowName, style, x, y,
        width, height, parent, menu, instance, param);

    RECT r;
    GetClientRect(h, &r);
    width = width - (r.right - width);
    height = height - (r.bottom - height);
    
    SetWindowPos(h, HWND_TOP, x, y, width, height, 0);

    return h;
}

void DoMessageBox(char *str, int rows, int cols, BOOL error)
{
    EnableWindow(GraphicsWnd, FALSE);
    EnableWindow(TextWnd, FALSE);
    HWND h = GetForegroundWindow();

    // Register the window class for our dialog.
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC;
    wc.lpfnWndProc      = (WNDPROC)MessageProc;
    wc.hInstance        = Instance;
    wc.hbrBackground    = (HBRUSH)COLOR_BTNSHADOW;
    wc.lpszClassName    = "MessageWnd";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 32, 32, 0);
    wc.hIconSm          = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 16, 16, 0);
    RegisterClassEx(&wc);

    // Create the window.
    MessageString = str;
    RECT r;
    GetWindowRect(GraphicsWnd, &r);
    char *title = error ? "SolveSpace - Error" : "SolveSpace - Message";
    int width  = cols*SS.TW.CHAR_WIDTH + 20,
        height = rows*SS.TW.LINE_HEIGHT + 60;
    MessageWnd = CreateWindowClient(0, "MessageWnd", title,
        WS_OVERLAPPED | WS_SYSMENU,
        r.left + 100, r.top + 100, width, height, NULL, NULL, Instance, NULL);

    OkButton = CreateWindowEx(0, WC_BUTTON, "OK",
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE | BS_DEFPUSHBUTTON,
        (width - 70)/2, rows*SS.TW.LINE_HEIGHT + 20,
        70, 25, MessageWnd, NULL, Instance, NULL); 
    SendMessage(OkButton, WM_SETFONT, (WPARAM)FixedFont, TRUE);

    ShowWindow(MessageWnd, TRUE);
    SetFocus(OkButton);

    MSG msg;
    DWORD ret;
    MessageDone = FALSE;
    while((ret = GetMessage(&msg, NULL, 0, 0)) && !MessageDone) {
        if((msg.message == WM_KEYDOWN &&
               (msg.wParam == VK_RETURN ||
                msg.wParam == VK_ESCAPE)) ||
            (msg.message == WM_KEYUP &&
               (msg.wParam == VK_SPACE)))
        {
            MessageDone = TRUE;
            break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    MessageString = NULL;
    EnableWindow(TextWnd, TRUE);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);
    DestroyWindow(MessageWnd);
}

void AddContextMenuItem(char *label, int id)
{
    if(!ContextMenu) ContextMenu = CreatePopupMenu();

    if(id == CONTEXT_SUBMENU) {
        AppendMenu(ContextMenu, MF_STRING | MF_POPUP, 
            (UINT_PTR)ContextSubmenu, label);
        ContextSubmenu = NULL;
    } else {
        HMENU m = ContextSubmenu ? ContextSubmenu : ContextMenu;
        if(id == CONTEXT_SEPARATOR) {
            AppendMenu(m, MF_SEPARATOR, 0, "");
        } else {
            AppendMenu(m, MF_STRING, id, label);
        }
    }
}

void CreateContextSubmenu(void)
{
    ContextSubmenu = CreatePopupMenu();
}

int ShowContextMenu(void)
{
    POINT p;
    GetCursorPos(&p);
    int r = TrackPopupMenu(ContextMenu, 
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_TOPALIGN,
        p.x, p.y, 0, GraphicsWnd, NULL);

    DestroyMenu(ContextMenu);
    ContextMenu = NULL;
    return r;
}

void CALLBACK TimerCallback(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    // The timer is periodic, so needs to be killed explicitly.
    KillTimer(GraphicsWnd, 1);
    SS.GW.TimerCallback();
    SS.TW.TimerCallback();
}
void SetTimerFor(int milliseconds)
{
    SetTimer(GraphicsWnd, 1, milliseconds, TimerCallback);
}

static void GetWindowSize(HWND hwnd, int *w, int *h)
{
    RECT r;
    GetClientRect(hwnd, &r);
    *w = r.right - r.left;
    *h = r.bottom - r.top;
}
void GetGraphicsWindowSize(int *w, int *h)
{
    GetWindowSize(GraphicsWnd, w, h);
}
void GetTextWindowSize(int *w, int *h)
{
    GetWindowSize(TextWnd, w, h);
}

void OpenWebsite(char *url) {
    ShellExecute(GraphicsWnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

void ExitNow(void) {
    PostQuitMessage(0);
}

//-----------------------------------------------------------------------------
// Helpers so that we can read/write registry keys from the platform-
// independent code.
//-----------------------------------------------------------------------------
void CnfFreezeString(char *str, char *name)
    { FreezeStringF(str, FREEZE_SUBKEY, name); }

void CnfFreezeDWORD(DWORD v, char *name)
    { FreezeDWORDF(v, FREEZE_SUBKEY, name); }

void CnfFreezeFloat(float v, char *name)
    { FreezeDWORDF(*((DWORD *)&v), FREEZE_SUBKEY, name); }

void CnfThawString(char *str, int maxLen, char *name)
    { ThawStringF(str, maxLen, FREEZE_SUBKEY, name); }

DWORD CnfThawDWORD(DWORD v, char *name)
    { return ThawDWORDF(v, FREEZE_SUBKEY, name); }

float CnfThawFloat(float v, char *name) {
    DWORD d = ThawDWORDF(*((DWORD *)&v), FREEZE_SUBKEY, name); 
    return *((float *)&d);
}

void SetWindowTitle(char *str) {
    SetWindowText(GraphicsWnd, str);
}

void SetMousePointerToHand(bool yes) {
    SetCursor(LoadCursor(NULL, yes ? IDC_HAND : IDC_ARROW));
}

static void PaintTextWnd(HDC hdc)
{
    wglMakeCurrent(GetDC(TextWnd), TextGl);

    SS.TW.Paint();
    SwapBuffers(GetDC(TextWnd));

    // Leave the graphics window context active, except when we're painting
    // this text window.
    wglMakeCurrent(GetDC(GraphicsWnd), GraphicsGl);
}

void MoveTextScrollbarTo(int pos, int maxPos, int page)
{
    SCROLLINFO si;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_DISABLENOSCROLL | SIF_ALL;
    si.nMin = 0;
    si.nMax = maxPos;
    si.nPos = pos;
    si.nPage = page;
    SetScrollInfo(TextWndScrollBar, SB_CTL, &si, TRUE);
}

void HandleTextWindowScrollBar(WPARAM wParam, LPARAM lParam)
{
    int maxPos, minPos, pos;
    GetScrollRange(TextWndScrollBar, SB_CTL, &minPos, &maxPos);
    pos = GetScrollPos(TextWndScrollBar, SB_CTL);

    switch(LOWORD(wParam)) {
        case SB_LINEUP:         pos--; break;
        case SB_PAGEUP:         pos -= 4; break;

        case SB_LINEDOWN:       pos++; break;
        case SB_PAGEDOWN:       pos += 4; break;

        case SB_TOP:            pos = 0; break;

        case SB_BOTTOM:         pos = maxPos; break;

        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:  pos = HIWORD(wParam); break;
    }
    
    SS.TW.ScrollbarEvent(pos);
}

static void MouseWheel(int thisDelta) {
    static int DeltaAccum;
    int delta = 0;
    // Handle mouse deltas of less than 120 (like from an un-detented mouse
    // wheel) correctly, even though no one ever uses those.
    DeltaAccum += thisDelta;
    while(DeltaAccum >= 120) {
        DeltaAccum -= 120;
        delta += 120;
    }
    while(DeltaAccum <= -120) {
        DeltaAccum += 120;
        delta -= 120;
    }
    if(delta == 0) return;

    POINT pt;
    GetCursorPos(&pt);
    HWND hw = WindowFromPoint(pt);
    
    // Make the mousewheel work according to which window the mouse is
    // over, not according to which window is active.
    bool inTextWindow;
    if(hw == TextWnd) {
        inTextWindow = true;
    } else if(hw == GraphicsWnd) {
        inTextWindow = false;
    } else if(GetForegroundWindow() == TextWnd) {
        inTextWindow = true;
    } else {
        inTextWindow = false;
    }

    if(inTextWindow) {
        int i;
        for(i = 0; i < abs(delta/40); i++) {
            HandleTextWindowScrollBar(delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        }
    } else {
        SS.GW.MouseScroll(LastMousePos.x, LastMousePos.y, delta);
    }
}

LRESULT CALLBACK TextWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_ERASEBKGND:
            break;

        case WM_CLOSE:
        case WM_DESTROY:
            SolveSpace::MenuFile(GraphicsWindow::MNU_EXIT);
            break;

        case WM_PAINT: {
            // Actually paint the text window, with gl.
            PaintTextWnd(GetDC(TextWnd));
            // And then just make Windows happy.
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;
        }
        
        case WM_SIZING: {
            RECT *r = (RECT *)lParam;
            int hc = (r->bottom - r->top) - ClientIsSmallerBy;
            int extra = hc % (SS.TW.LINE_HEIGHT/2);
            switch(wParam) {
                case WMSZ_BOTTOM:
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                    r->bottom -= extra;
                    break;

                case WMSZ_TOP:
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                    r->top += extra;
                    break;
            }
            int tooNarrow = (SS.TW.MIN_COLS*SS.TW.CHAR_WIDTH) -
                                                (r->right - r->left);
            if(tooNarrow >= 0) {
                switch(wParam) {
                    case WMSZ_RIGHT:
                    case WMSZ_BOTTOMRIGHT:
                    case WMSZ_TOPRIGHT:
                        r->right += tooNarrow;
                        break;

                    case WMSZ_LEFT:
                    case WMSZ_BOTTOMLEFT:
                    case WMSZ_TOPLEFT:
                        r->left -= tooNarrow;
                        break;
                }
            }
            break;
        }

        case WM_MOUSELEAVE:
            SS.TW.MouseLeave();
            break;

        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE: {
            // We need this in order to get the WM_MOUSELEAVE
            TRACKMOUSEEVENT tme;
            ZERO(&tme);
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = TextWnd;
            TrackMouseEvent(&tme);

            // And process the actual message
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            SS.TW.MouseEvent(msg == WM_LBUTTONDOWN, wParam & MK_LBUTTON, x, y);
            break;
        }
        
        case WM_SIZE: {
            RECT r;
            GetWindowRect(TextWndScrollBar, &r);
            int sw = r.right - r.left;
            GetClientRect(hwnd, &r);
            MoveWindow(TextWndScrollBar, r.right - sw, r.top, sw,
                (r.bottom - r.top), TRUE);
            // If the window is growing, then the scrollbar position may
            // be moving, so it's as if we're dragging the scrollbar.
            HandleTextWindowScrollBar(-1, -1);
            InvalidateRect(TextWnd, NULL, FALSE);
            break;
        }

        case WM_MOUSEWHEEL:
            MouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            break;

        case WM_VSCROLL:
            HandleTextWindowScrollBar(wParam, lParam);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

static BOOL ProcessKeyDown(WPARAM wParam)
{
    if(GraphicsEditControlIsVisible() && wParam != VK_ESCAPE) {
        if(wParam == VK_RETURN) {
            char s[1024];
            memset(s, 0, sizeof(s));
            SendMessage(GraphicsEditControl, WM_GETTEXT, 900, (LPARAM)s);
            SS.GW.EditControlDone(s);
            return TRUE;
        } else {
            return FALSE;
        }
    }
    if(TextEditControlIsVisible() && wParam != VK_ESCAPE) {
        if(wParam == VK_RETURN) {
            char s[1024];
            memset(s, 0, sizeof(s));
            SendMessage(TextEditControl, WM_GETTEXT, 900, (LPARAM)s);
            SS.TW.EditControlDone(s);
        } else {
            return FALSE;
        }
    }

    int c;
    switch(wParam) {
        case VK_OEM_PLUS:       c = '+';            break;
        case VK_OEM_MINUS:      c = '-';            break;
        case VK_ESCAPE:         c = 27;             break;
        case VK_OEM_1:          c = ';';            break;
        case VK_OEM_3:          c = '`';            break;
        case VK_OEM_4:          c = '[';            break;
        case VK_OEM_6:          c = ']';            break;
        case VK_OEM_5:          c = '\\';           break;
        case VK_OEM_PERIOD:     c = '.';            break;
        case VK_SPACE:          c = ' ';            break;
        case VK_DELETE:         c = 127;            break;
        case VK_TAB:            c = '\t';           break;

        case VK_BROWSER_BACK:
        case VK_BACK:           c = 1 + 'h' - 'a';  break;

        case VK_F1:
        case VK_F2:
        case VK_F3:
        case VK_F4:
        case VK_F5:
        case VK_F6:
        case VK_F7:
        case VK_F8:
        case VK_F9:
        case VK_F10:
        case VK_F11:
        case VK_F12:            c = (wParam - VK_F1) + 0xf1; break;

        // These overlap with some character codes that I'm using, so
        // don't let them trigger by accident.
        case VK_F16:
        case VK_INSERT:
        case VK_EXECUTE:
        case VK_APPS:
        case VK_LWIN:
        case VK_RWIN:           return FALSE;

        default:
            c = wParam;
            break;
    }
    if(GetAsyncKeyState(VK_SHIFT) & 0x8000)   c |= 0x100;
    if(GetAsyncKeyState(VK_CONTROL) & 0x8000) c |= 0x200;

    for(int i = 0; SS.GW.menu[i].level >= 0; i++) {
        if(c == SS.GW.menu[i].accel) {
            (SS.GW.menu[i].fn)((GraphicsWindow::MenuId)SS.GW.menu[i].id);
            break;
        }
    }

    if(SS.GW.KeyDown(c)) return TRUE;

    // No accelerator; process the key as normal.
    return FALSE;
}

void ShowTextWindow(BOOL visible)
{
    ShowWindow(TextWnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

static void CreateGlContext(HWND hwnd, HGLRC *glrc)
{
    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd;
    int pixelFormat;

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR); 
    pfd.nVersion = 1; 
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |  
                        PFD_DOUBLEBUFFER; 
    pfd.dwLayerMask = PFD_MAIN_PLANE; 
    pfd.iPixelType = PFD_TYPE_RGBA; 
    pfd.cColorBits = 32; 
    pfd.cDepthBits = 24; 
    pfd.cAccumBits = 0; 
    pfd.cStencilBits = 0; 
 
    pixelFormat = ChoosePixelFormat(hdc, &pfd); 
    if(!pixelFormat) oops();
 
    if(!SetPixelFormat(hdc, pixelFormat, &pfd)) oops();

    *glrc = wglCreateContext(hdc); 
    wglMakeCurrent(hdc, *glrc);
}

void PaintGraphics(void)
{
    SS.GW.Paint();
    SwapBuffers(GetDC(GraphicsWnd));
}
void InvalidateGraphics(void)
{
    InvalidateRect(GraphicsWnd, NULL, FALSE);
}

SDWORD GetMilliseconds(void)
{
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    LONGLONG d = t.QuadPart/(f.QuadPart/1000);
    return (SDWORD)d;
}

SQWORD GetUnixTime(void)
{
    __time64_t ret;
    _time64(&ret);
    return ret;
}

void InvalidateText(void)
{
    InvalidateRect(TextWnd, NULL, FALSE);
}

static void ShowEditControl(HWND h, int x, int y, char *s) {
    MoveWindow(h, x, y, EDIT_WIDTH, EDIT_HEIGHT, TRUE);
    ShowWindow(h, SW_SHOW);
    if(s) {
        SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
        SendMessage(h, EM_SETSEL, 0, strlen(s));
        SetFocus(h);
    }
}
void ShowTextEditControl(int x, int y, char *s)
{
    if(GraphicsEditControlIsVisible()) return;

    ShowEditControl(TextEditControl, x, y, s);
}
void HideTextEditControl(void)
{
    ShowWindow(TextEditControl, SW_HIDE);
}
BOOL TextEditControlIsVisible(void)
{
    return IsWindowVisible(TextEditControl);
}
void ShowGraphicsEditControl(int x, int y, char *s)
{
    if(GraphicsEditControlIsVisible()) return;

    RECT r;
    GetClientRect(GraphicsWnd, &r);
    x = x + (r.right - r.left)/2;
    y = (r.bottom - r.top)/2 - y;

    // (x, y) are the bottom left, but the edit control is placed by its
    // top left corner
    y -= 20;

    ShowEditControl(GraphicsEditControl, x, y, s);
}
void HideGraphicsEditControl(void)
{
    ShowWindow(GraphicsEditControl, SW_HIDE);
}
BOOL GraphicsEditControlIsVisible(void)
{
    return IsWindowVisible(GraphicsEditControl);
}

LRESULT CALLBACK GraphicsWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                            LPARAM lParam)
{
    switch (msg) {
        case WM_ERASEBKGND:
            break;

        case WM_SIZE:
            InvalidateRect(GraphicsWnd, NULL, FALSE);
            break;

        case WM_PAINT: {
            // Actually paint the window, with gl.
            PaintGraphics();
            // And make Windows happy.
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_MOUSELEAVE:
            SS.GW.MouseLeave();
            break;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // We need this in order to get the WM_MOUSELEAVE
            TRACKMOUSEEVENT tme;
            ZERO(&tme);
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = GraphicsWnd;
            TrackMouseEvent(&tme);

            // Convert to xy (vs. ij) style coordinates, with (0, 0) at center
            RECT r;
            GetClientRect(GraphicsWnd, &r);
            x = x - (r.right - r.left)/2;
            y = (r.bottom - r.top)/2 - y;

            LastMousePos.x = x;
            LastMousePos.y = y;

            if(msg == WM_LBUTTONDOWN) {
                SS.GW.MouseLeftDown(x, y);
            } else if(msg == WM_LBUTTONUP) {
                SS.GW.MouseLeftUp(x, y);
            } else if(msg == WM_LBUTTONDBLCLK) {
                SS.GW.MouseLeftDoubleClick(x, y);
            } else if(msg == WM_MBUTTONDOWN || msg == WM_RBUTTONDOWN) {
                SS.GW.MouseMiddleOrRightDown(x, y);
            } else if(msg == WM_RBUTTONUP) {
                SS.GW.MouseRightUp(x, y);
            } else if(msg == WM_MOUSEMOVE) {
                SS.GW.MouseMoved(x, y,
                    !!(wParam & MK_LBUTTON),
                    !!(wParam & MK_MBUTTON),
                    !!(wParam & MK_RBUTTON),
                    !!(wParam & MK_SHIFT),
                    !!(wParam & MK_CONTROL));
            } else {
                oops();
            }
            break;
        }
        case WM_MOUSEWHEEL:
            MouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            break;

        case WM_COMMAND: {
            if(HIWORD(wParam) == 0) {
                int id = LOWORD(wParam);
                if((id >= RECENT_OPEN && id < (RECENT_OPEN + MAX_RECENT))) {
                    SolveSpace::MenuFile(id);
                    break;
                }
                if((id >= RECENT_IMPORT && id < (RECENT_IMPORT + MAX_RECENT))) {
                    Group::MenuGroup(id);
                    break;
                }
                int i;
                for(i = 0; SS.GW.menu[i].level >= 0; i++) {
                    if(id == SS.GW.menu[i].id) {
                        (SS.GW.menu[i].fn)((GraphicsWindow::MenuId)id);
                        break;
                    }
                }
                if(SS.GW.menu[i].level < 0) oops();
            }
            break;
        }

        case WM_CLOSE:
        case WM_DESTROY:
            SolveSpace::MenuFile(GraphicsWindow::MNU_EXIT);
            return 1;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

//-----------------------------------------------------------------------------
// Common dialog routines, to open or save a file.
//-----------------------------------------------------------------------------
BOOL GetOpenFile(char *file, char *defExtension, char *selPattern)
{
    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = Instance;
    ofn.hwndOwner = GraphicsWnd;
    ofn.lpstrFilter = selPattern;
    ofn.lpstrDefExt = defExtension;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    EnableWindow(GraphicsWnd, FALSE);
    EnableWindow(TextWnd, FALSE);

    BOOL r = GetOpenFileName(&ofn);

    EnableWindow(TextWnd, TRUE);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);

    return r;
}
BOOL GetSaveFile(char *file, char *defExtension, char *selPattern)
{
    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = Instance;
    ofn.hwndOwner = GraphicsWnd;
    ofn.lpstrFilter = selPattern;
    ofn.lpstrDefExt = defExtension;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

    EnableWindow(GraphicsWnd, FALSE);
    EnableWindow(TextWnd, FALSE);

    BOOL r = GetSaveFileName(&ofn);

    EnableWindow(TextWnd, TRUE);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);

    return r;
}
int SaveFileYesNoCancel(void)
{
    EnableWindow(GraphicsWnd, FALSE);
    EnableWindow(TextWnd, FALSE);

    int r = MessageBox(GraphicsWnd, 
        "The program has changed since it was last saved.\r\n\r\n"
        "Do you want to save the changes?", "SolveSpace",
        MB_YESNOCANCEL | MB_ICONWARNING);

    EnableWindow(TextWnd, TRUE);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);

    return r;
}

void LoadAllFontFiles(void)
{
    WIN32_FIND_DATA wfd;
    char dir[MAX_PATH];
    GetWindowsDirectory(dir, MAX_PATH - 30);
    strcat(dir, "\\fonts\\*.ttf");

    HANDLE h = FindFirstFile(dir, &wfd);

    while(h != INVALID_HANDLE_VALUE) {
        TtfFont tf;
        ZERO(&tf);

        char fullPath[MAX_PATH];
        GetWindowsDirectory(fullPath, MAX_PATH - (30 + strlen(wfd.cFileName)));
        strcat(fullPath, "\\fonts\\");
        strcat(fullPath, wfd.cFileName);

        strcpy(tf.fontFile, fullPath);
        SS.fonts.l.Add(&tf);

        if(!FindNextFile(h, &wfd)) break;
    }
}

static void MenuById(int id, BOOL yes, BOOL check)
{
    int i;
    int subMenu = -1;

    for(i = 0; SS.GW.menu[i].level >= 0; i++) {
        if(SS.GW.menu[i].level == 0) subMenu++;
        
        if(SS.GW.menu[i].id == id) {
            if(subMenu < 0) oops();
            if(subMenu >= (sizeof(SubMenus)/sizeof(SubMenus[0]))) oops();

            if(check) {
                CheckMenuItem(SubMenus[subMenu], id,
                            yes ? MF_CHECKED : MF_UNCHECKED);
            } else {
                EnableMenuItem(SubMenus[subMenu], id,
                            yes ? MF_ENABLED : MF_GRAYED);
            }
            return;
        }
    }
    oops();
}
void CheckMenuById(int id, BOOL checked)
{
    MenuById(id, checked, TRUE);
}
void EnableMenuById(int id, BOOL enabled)
{
    MenuById(id, enabled, FALSE);
}
static void DoRecent(HMENU m, int base)
{
    while(DeleteMenu(m, 0, MF_BYPOSITION))
        ;
    int i, c = 0;
    for(i = 0; i < MAX_RECENT; i++) {
        char *s = RecentFile[i];
        if(*s) {
            AppendMenu(m, MF_STRING, base+i, s);
            c++;
        }
    }
    if(c == 0) AppendMenu(m, MF_STRING | MF_GRAYED, 0, "(no recent files)");
}
void RefreshRecentMenus(void)
{
    DoRecent(RecentOpenMenu,   RECENT_OPEN);
    DoRecent(RecentImportMenu, RECENT_IMPORT);
}

HMENU CreateGraphicsWindowMenus(void)
{
    HMENU top = CreateMenu();
    HMENU m;

    int i;
    int subMenu = 0;
    
    for(i = 0; SS.GW.menu[i].level >= 0; i++) {
        if(SS.GW.menu[i].level == 0) {
            m = CreateMenu();
            AppendMenu(top, MF_STRING | MF_POPUP, (UINT_PTR)m, 
                                                        SS.GW.menu[i].label);

            if(subMenu >= arraylen(SubMenus)) oops();
            SubMenus[subMenu] = m;
            subMenu++;
        } else if(SS.GW.menu[i].level == 1) {
            if(SS.GW.menu[i].label) {
                AppendMenu(m, MF_STRING, SS.GW.menu[i].id, SS.GW.menu[i].label);
            } else {
                AppendMenu(m, MF_SEPARATOR, SS.GW.menu[i].id, "");
            }
        } else if(SS.GW.menu[i].level == 10) {
            RecentOpenMenu = CreateMenu();
            AppendMenu(m, MF_STRING | MF_POPUP,
                (UINT_PTR)RecentOpenMenu, SS.GW.menu[i].label);
        } else if(SS.GW.menu[i].level == 11) {
            RecentImportMenu = CreateMenu();
            AppendMenu(m, MF_STRING | MF_POPUP,
                (UINT_PTR)RecentImportMenu, SS.GW.menu[i].label);
        } else oops();
    }
    RefreshRecentMenus();

    return top;
}

static void CreateMainWindows(void)
{
    WNDCLASSEX wc;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);

    // The graphics window, where the sketch is drawn and shown.
    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC |
                          CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)GraphicsWndProc;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); 
    wc.lpszClassName    = "GraphicsWnd";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 32, 32, 0);
    wc.hIconSm          = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 16, 16, 0);
    if(!RegisterClassEx(&wc)) oops();

    HMENU top = CreateGraphicsWindowMenus();
    GraphicsWnd = CreateWindowEx(0, "GraphicsWnd",
        "SolveSpace (not yet saved)",
        WS_OVERLAPPED | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX |
        WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX | WS_CLIPSIBLINGS,
        50, 50, 900, 600, NULL, top, Instance, NULL);
    if(!GraphicsWnd) oops();

    GraphicsEditControl = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
        WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS,
        50, 50, 100, 21, GraphicsWnd, NULL, Instance, NULL);
    SendMessage(GraphicsEditControl, WM_SETFONT, (WPARAM)FixedFont, TRUE);

    // The text window, with a comand line and some textual information
    // about the sketch.
    wc.style           &= ~CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)TextWndProc;
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName    = "TextWnd";
    wc.hCursor          = NULL;
    if(!RegisterClassEx(&wc)) oops();

    // We get the desired Alt+Tab behaviour by specifying that the text
    // window is a child of the graphics window.
    TextWnd = CreateWindowEx(0, 
        "TextWnd", "SolveSpace - Browser", WS_THICKFRAME | WS_CLIPCHILDREN,
        650, 500, 420, 300, GraphicsWnd, (HMENU)NULL, Instance, NULL);
    if(!TextWnd) oops();

    TextWndScrollBar = CreateWindowEx(0, WC_SCROLLBAR, "", WS_CHILD |
        SBS_VERT | SBS_LEFTALIGN | WS_VISIBLE | WS_CLIPSIBLINGS, 
        200, 100, 100, 100, TextWnd, NULL, Instance, NULL);
    // Force the scrollbar to get resized to the window,
    TextWndProc(TextWnd, WM_SIZE, 0, 0);

    TextEditControl = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
        WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS,
        50, 50, 100, 21, TextWnd, NULL, Instance, NULL);
    SendMessage(TextEditControl, WM_SETFONT, (WPARAM)FixedFont, TRUE);

    // Now that all our windows exist, set up gl contexts.
    CreateGlContext(TextWnd, &TextGl);
    CreateGlContext(GraphicsWnd, &GraphicsGl);

    RECT r, rc;
    GetWindowRect(TextWnd, &r);
    GetClientRect(TextWnd, &rc);
    ClientIsSmallerBy = (r.bottom - r.top) - (rc.bottom - rc.top);
}

//-----------------------------------------------------------------------------
// Test if a message comes from the SpaceNavigator device. If yes, dispatch
// it appropriately and return TRUE. Otherwise, do nothing and return FALSE.
//-----------------------------------------------------------------------------
static BOOL ProcessSpaceNavigatorMsg(MSG *msg) {
    if(SpaceNavigator == SI_NO_HANDLE) return FALSE;

    SiGetEventData sged;
    SiSpwEvent sse;

    SiGetEventWinInit(&sged, msg->message, msg->wParam, msg->lParam);
    int ret = SiGetEvent(SpaceNavigator, 0, &sged, &sse);
    if(ret == SI_NOT_EVENT) return FALSE;
    // So the device is a SpaceNavigator event, or a SpaceNavigator error.

    if(ret == SI_IS_EVENT) {
        if(sse.type == SI_MOTION_EVENT) {
            // The Z axis translation and rotation are both
            // backwards in the default mapping.
            double tx =  sse.u.spwData.mData[SI_TX]*1.0,
                   ty =  sse.u.spwData.mData[SI_TY]*1.0,
                   tz = -sse.u.spwData.mData[SI_TZ]*1.0,
                   rx =  sse.u.spwData.mData[SI_RX]*0.001,
                   ry =  sse.u.spwData.mData[SI_RY]*0.001,
                   rz = -sse.u.spwData.mData[SI_RZ]*0.001;
            SS.GW.SpaceNavigatorMoved(tx, ty, tz, rx, ry, rz,
                !!(GetAsyncKeyState(VK_SHIFT) & 0x8000));
        } else if(sse.type == SI_BUTTON_EVENT) {
            int button;
            button = SiButtonReleased(&sse);
            if(button == SI_APP_FIT_BUTTON) SS.GW.SpaceNavigatorButtonUp();
        }
    }
    return TRUE;
}

//-----------------------------------------------------------------------------
// Entry point into the program.
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, INT nCmdShow)
{
    Instance = hInstance;

    InitCommonControls();

    // A monospaced font
    FixedFont = CreateFont(SS.TW.CHAR_HEIGHT, SS.TW.CHAR_WIDTH, 0, 0,
        FW_REGULAR, FALSE,
        FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");
    if(!FixedFont)
        FixedFont = (HFONT)GetStockObject(SYSTEM_FONT);

    // Create the root windows: one for control, with text, and one for
    // the graphics
    CreateMainWindows();

    ThawWindowPos(TextWnd);
    ThawWindowPos(GraphicsWnd);

    ShowWindow(TextWnd, SW_SHOWNOACTIVATE);
    ShowWindow(GraphicsWnd, SW_SHOW);
   
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT); 
    SwapBuffers(GetDC(GraphicsWnd));
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT); 
    SwapBuffers(GetDC(GraphicsWnd));

    // Create the heaps for all dynamic memory (AllocTemporary, MemAlloc)
    InitHeaps();

    // A filename may have been specified on the command line; if so, then
    // strip any quotation marks, and make it absolute.
    char file[MAX_PATH] = "";
    if(strlen(lpCmdLine)+1 < MAX_PATH) {
        char *s = lpCmdLine;
        while(*s == ' ' || *s == '"') s++;
        strcpy(file, s);
        s = strrchr(file, '"');
        if(s) *s = '\0';
    }
    if(*file != '\0') {
        GetAbsoluteFilename(file);
    }

    // Initialize the SpaceBall, if present. Test if the driver is running
    // first, to avoid a long timeout if it's not.
    HWND swdc = FindWindow("SpaceWare Driver Class", NULL);
    if(swdc != NULL) {
        SiOpenData sod;
        SiInitialize();
        SiOpenWinInit(&sod, GraphicsWnd);
        SpaceNavigator =
            SiOpen("GraphicsWnd", SI_ANY_DEVICE, SI_NO_MASK, SI_EVENT, &sod);
        SiSetUiMode(SpaceNavigator, SI_UI_NO_CONTROLS);
    }
    
    // Call in to the platform-independent code, and let them do their init
    SS.Init(file);

    // And now it's the message loop. All calls in to the rest of the code
    // will be from the wndprocs.
    MSG msg;
    DWORD ret;
    while(ret = GetMessage(&msg, NULL, 0, 0)) {
        // Is it a message from the six degree of freedom input device?
        if(ProcessSpaceNavigatorMsg(&msg)) goto done;

        // A message from the keyboard, which should be processed as a keyboard
        // accelerator?
        if(msg.message == WM_KEYDOWN) {
            if(ProcessKeyDown(msg.wParam)) goto done;
        }
        if(msg.message == WM_SYSKEYDOWN && msg.hwnd == TextWnd) {
            // If the user presses the Alt key when the text window has focus,
            // then that should probably go to the graphics window instead.
            SetForegroundWindow(GraphicsWnd);
        }

        // None of the above; so just a normal message to process.
        TranslateMessage(&msg);
        DispatchMessage(&msg);
done:
        SS.DoLater();
    }

    if(swdc != NULL) {
        if(SpaceNavigator != SI_NO_HANDLE) SiClose(SpaceNavigator);
        SiTerminate();
    }

    // Write everything back to the registry
    FreezeWindowPos(TextWnd);
    FreezeWindowPos(GraphicsWnd);
    return 0;
}
