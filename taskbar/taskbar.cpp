/*
 * Copyright 2003, 2004, 2005 Martin Fuchs
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// Explorer clone
// taskbar.cpp
// Martin Fuchs, 16.08.2003

#include <precomp.h>
#include <WinUser.h>
#include "taskbar.h"
#include "traynotify.h"

#include <Uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

DynamicFct<BOOL (WINAPI *)(HWND hwnd)>
    g_SetTaskmanWindow(TEXT("user32"), "SetTaskmanWindow");

DynamicFct<BOOL (WINAPI *)(HWND hwnd)>
    g_RegisterShellHookWindow(TEXT("user32"), "RegisterShellHookWindow");

DynamicFct<BOOL (WINAPI *)(HWND hwnd)>
    g_DeregisterShellHookWindow(TEXT("user32"), "DeregisterShellHookWindow");

DynamicFct<BOOL (WINAPI*)(HWND hWnd, DWORD dwType)>
    g_RegisterShellHook(TEXT("shell32"), (LPCSTR)0xb5);

#define ID_TIMER_DESTORYTHUMBNAIL 101
#define ID_TIMER_TASKFLASH        102

extern void InitThumbnailWindow(HWND taskbar, HWND toolbar);
extern int DrawThumbnailWindow(HINSTANCE hInstance, HWND hWndSrc,
                               LPCTSTR lpClassName, LPCTSTR lpWindowName,
                               int id);
extern void DestoryThumbnailWindow();

extern void TaskbarTransparency(HWND hwnd,
                                 const TCHAR *mode,
                                 UINT transparency,
                                 COLORREF color);

// constants for RegisterShellHook()
#define RSH_UNREGISTER  0
#define RSH_REGISTER    1
#define RSH_PROGMAN     2
#define RSH_TASKMGR     3

#ifdef _WIN64
#define GCL_HICON   GCLP_HICON
#define GCL_HICONSM GCLP_HICONSM
#endif

static HBRUSH hbrTaskLine = NULL;


TaskBarEntry::TaskBarEntry()
{
    _id = 0;
    _hbmp = 0;
    _bmp_idx = 0;
    _btn_idx = 0;
    _fsState = 0;
    _flash = false;
    _flashOn = false;
}


TaskBarMap::~TaskBarMap()
{
    while (!empty()) {
        iterator it = begin();

        DeleteBitmap(it->second._hbmp);
        erase(it);
    }
}


RECT TaskBar::_icon_area = {
    1,
    0,
    TASKBAR_ICON_SIZE + 4,
    DESKTOPBARBAR_HEIGHT - 4
};


void TaskBar::InitTaskbarStyle()
{
    _no_task_title = false;
    _task_close_button = false;

    bool show_task_line = false;
    COLORREF clrTaskLine = TASKBAR_TASKLINECOLOR();

    if (JCFG2_DEF("JS_TASKBAR", "no_task_title", false).ToBool() != FALSE) {
        _no_task_title = true;

        _icon_area.right = TASKBAR_ICON_SIZE + 8 + 4;
        _icon_area.bottom = DESKTOPBARBAR_HEIGHT - 4;
    }
    else {
        if (JCFG2_DEF("JS_TASKBAR",
                      "task_close_button",
                      false).ToBool() != FALSE) {
            _task_close_button = true;
        }
    }

    if (clrTaskLine != MAXDWORD) {
        show_task_line = true;

        _icon_area.top = -1;
        _icon_area.bottom -= 3;
    }
}


TaskBar::TaskBar(HWND hwnd)
    : super(hwnd),
      WM_SHELLHOOK(RegisterWindowMessage(WINMSG_SHELLHOOK))
{
    _last_btn_width = 0;

    _mmMetrics_org.cbSize = sizeof(MINIMIZEDMETRICS);

    SystemParametersInfo(
        SPI_GETMINIMIZEDMETRICS,
        sizeof(_mmMetrics_org),
        &_mmMetrics_org,
        0
    );

    if (!(_mmMetrics_org.iArrange & ARW_HIDE)) {
        MINIMIZEDMETRICS _mmMetrics_new = _mmMetrics_org;

        _mmMetrics_new.iArrange |= ARW_HIDE;

        SystemParametersInfo(
            SPI_SETMINIMIZEDMETRICS,
            sizeof(_mmMetrics_new),
            &_mmMetrics_new,
            0
        );
    }

    InitTaskbarStyle();
}


TaskBar::~TaskBar()
{
    if (g_RegisterShellHook)
        (*g_RegisterShellHook)(_hwnd, RSH_UNREGISTER);

    if (g_DeregisterShellHookWindow)
        (*g_DeregisterShellHookWindow)(_hwnd);
    else
        KillTimer(_hwnd, 0);

    if (g_SetTaskmanWindow)
        (*g_SetTaskmanWindow)(0);

    SystemParametersInfo(
        SPI_SETMINIMIZEDMETRICS,
        sizeof(_mmMetrics_org),
        &_mmMetrics_org,
        0
    );
}


HWND TaskBar::Create(HWND hwndParent)
{
    ClientRect clnt(hwndParent);

    int taskbar_pos = 80;

    static BtnWindowClass wcTaskBar(CLASSNAME_TASKBAR);
    wcTaskBar.hbrBackground = TASKBAR_BRUSH();

    return Window::Create(
        WINDOW_CREATOR(TaskBar),
        0,
        wcTaskBar,
        TITLE_TASKBAR,
        WS_CHILD | WS_VISIBLE | CCS_TOP | CCS_NODIVIDER | CCS_NORESIZE,
        taskbar_pos,
        clnt.top + 1,
        clnt.right - taskbar_pos - (NOTIFYAREA_WIDTH_DEF + 1),
        clnt.bottom - 2,
        hwndParent
    );
}


LRESULT TaskBar::Init(LPCREATESTRUCT pcs)
{
    if (super::Init(pcs))
        return 1;

    COLORREF clrTaskLine = TASKBAR_TASKLINECOLOR();

    if (clrTaskLine != MAXDWORD)
        hbrTaskLine = CreateSolidBrush(clrTaskLine);

    DWORD ws =
        WS_CHILD |
        WS_VISIBLE |
        WS_CLIPSIBLINGS |
        WS_CLIPCHILDREN |
        CCS_TOP |
        TBSTYLE_TRANSPARENT |
        CCS_NODIVIDER |
        TBSTYLE_LIST |
        TBSTYLE_TOOLTIPS |
        TBSTYLE_WRAPABLE |
        TBSTYLE_FLAT;

    _htoolbar = CreateWindowEx(
        0,
        TOOLBARCLASSNAME,
        NULL,
        ws,
        0,
        0,
        DESKTOPBARBAR_HEIGHT - 4,
        DESKTOPBARBAR_HEIGHT,
        _hwnd,
        NULL,
        g_Globals._hInstance,
        NULL
    );

    SendMessage(
        _htoolbar,
        TB_BUTTONSTRUCTSIZE,
        (WPARAM)sizeof(TBBUTTON),
        0
    );

    SendMessage(
        _htoolbar,
        TB_SETBUTTONWIDTH,
        0,
        MAKELPARAM(TASKBUTTONWIDTH_MAX, TASKBUTTONWIDTH_MAX)
    );

    if (_no_task_title) {
        SendMessage(
            _htoolbar,
            TB_SETEXTENDEDSTYLE,
            0,
            TBSTYLE_EX_MIXEDBUTTONS
        );
    }
    else if (_task_close_button) {
        SendMessage(
            _htoolbar,
            TB_SETEXTENDEDSTYLE,
            0,
            TBSTYLE_EX_DRAWDDARROWS
        );
    }

    SendMessage(
        _htoolbar,
        TB_SETBITMAPSIZE,
        0,
        MAKELPARAM(_icon_area.right, _icon_area.bottom)
    );

    HWND hwndToolTip =
    
    (HWND)SendMessage(_htoolbar, TB_GETTOOLTIPS, 0, 0);

    SetWindowStyle(
        hwndToolTip,
        GetWindowStyle(hwndToolTip) | TTS_ALWAYSTIP
    );

    TBMETRICS metrics;

    metrics.cbSize = sizeof(TBMETRICS);
    metrics.dwMask = TBMF_BARPAD | TBMF_BUTTONSPACING;
    metrics.cxBarPad = 0;
    metrics.cyBarPad = JCFG_TB(2, "padding-top").ToInt();
    metrics.cxButtonSpacing = 1;
    metrics.cyButtonSpacing = 0;

    SendMessage(
        _htoolbar,
        TB_SETMETRICS,
        0,
        (LPARAM)&metrics
    );

    _next_id = IDC_FIRST_APP;

    if (g_SetTaskmanWindow)
        (*g_SetTaskmanWindow)(_hwnd);

    if (g_RegisterShellHookWindow) {
        LOG(TEXT("Using shell hooks for notification of shell events."));
        (*g_RegisterShellHookWindow)(_hwnd);
    }
    else {
        LOG(TEXT("Shell hooks not available."));
        SetTimer(_hwnd, 0, 200, NULL);
    }

    if (g_RegisterShellHook) {
        OSVERSIONINFO osInfo;

        osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&osInfo);

        (*g_RegisterShellHook)(NULL, RSH_REGISTER);

        if (osInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
            (*g_RegisterShellHook)(_hwnd, RSH_PROGMAN);
        else
            (*g_RegisterShellHook)(_hwnd, RSH_TASKMGR);
    }

    Refresh();

    _thumbnail = JCfg_TaskThumbnailEnabled();

    if (_thumbnail)
        InitThumbnailWindow(_hwnd, _htoolbar);

    ApplyBackgroundStyle();

    return 0;
}


LRESULT TaskBar::WndProc(UINT nmsg, WPARAM wparam, LPARAM lparam)
{
    switch (nmsg) {

    case WM_SIZE:
        SendMessage(_htoolbar, WM_SIZE, 0, 0);
        ResizeButtons();
        break;


    case WM_TIMER:

        if (wparam == 0) {

            Refresh();

        }
        else if (wparam == ID_TIMER_DESTORYTHUMBNAIL) {

            KillTimer(_hwnd, ID_TIMER_DESTORYTHUMBNAIL);
            DestoryThumbnailWindow();

        }
        else if (wparam == ID_TIMER_TASKFLASH) {

            bool flashing = false;

            for (TaskBarMap::iterator it = _map.begin();
                 it != _map.end();
                 ++it) {

                if (it->second._flash) {

                    it->second._flashOn =
                        !it->second._flashOn;

                    flashing = true;
                }
            }

            if (flashing) {

                InvalidateRect(
                    _htoolbar,
                    NULL,
                    FALSE
                );

            }
            else {

                KillTimer(
                    _hwnd,
                    ID_TIMER_TASKFLASH
                );
            }
        }

        return 0;


    case WM_CONTEXTMENU:
    {
        Point pt(lparam);

        ScreenToClient(_htoolbar, &pt);

        if ((HWND)wparam == _htoolbar &&
            SendMessage(
                _htoolbar,
                TB_HITTEST,
                0,
                (LPARAM)&pt
            ) >= 0) {
            break;
        }

        goto def;
    }


    case PM_GET_LAST_ACTIVE:
        return (LRESULT)(HWND)_last_foreground_wnd;


    case WM_SYSCOLORCHANGE:
        SendMessage(_htoolbar, WM_SYSCOLORCHANGE, 0, 0);
        break;


    default:
    def:

        if (nmsg == WM_SHELLHOOK) {

            switch (wparam) {

            case HSHELL_WINDOWCREATED:
            {
                HWND hwndCreated = (HWND)lparam;

                AddWindow(hwndCreated);

                break;
            }


            case HSHELL_WINDOWDESTROYED:
            {
                HWND hwndDestroyed = (HWND)lparam;

                RemoveWindow(hwndDestroyed);

                break;
            }


            case HSHELL_REDRAW:
            {
                HWND hwndRedraw = (HWND)lparam;

                TaskBarMap::iterator it =
                    _map.find(hwndRedraw);

                if (it != _map.end())
                    UpdateWindow(hwndRedraw);

                break;
            }

            case HSHELL_WINDOWACTIVATED:
            {
                HWND hwndActivated = (HWND)lparam;

                TaskBarMap::iterator it =
                    _map.find(hwndActivated);

                if (it != _map.end()) {

                    it->second._flash = false;
                    it->second._flashOn = false;

                    InvalidateRect(
                        _htoolbar,
                        NULL,
                        FALSE
                    );
                }

                UpdateActiveWindow(hwndActivated);

                break;
            }


#ifdef HSHELL_FLASH
            case HSHELL_FLASH:
            {
                HWND hwndFlash = (HWND)lparam;

                TaskBarMap::iterator it =
                    _map.find(hwndFlash);

                if (it != _map.end()) {

                    it->second._flash = true;
                    it->second._flashOn = true;

                    SetTimer(
                        _hwnd,
                        ID_TIMER_TASKFLASH,
                        500,
                        NULL
                    );

                    InvalidateRect(
                        _htoolbar,
                        NULL,
                        FALSE
                    );
                }

                break;
            }
#endif

            }

        }
        else {
            return super::WndProc(
                nmsg,
                wparam,
                lparam
            );
        }
    }

    return 0;
}


int TaskBar::Command(int id, int code)
{
    TaskBarMap::iterator found = _map.find_id(id);

    if (found != _map.end()) {

        if (code == WM_CLOSE) {

            PostMessage(
                found->first,
                WM_SYSCOMMAND,
                SC_CLOSE,
                0
            );

        }
        else {

            ActivateApp(found);
        }

        return 0;
    }

    return super::Command(id, code);
}


int TaskBar::Notify(int id, NMHDR *pnmh)
{
    if (pnmh->hwndFrom == _htoolbar) {

        if (g_Globals._isDebug) {
            _log_(
                FmtString(
                    TEXT("TaskBar::Notify(%d)"),
                    pnmh->code
                )
            );
        }

        switch (pnmh->code) {

        case NM_RCLICK:
        {
            TBBUTTONINFO btninfo;
            TaskBarMap::iterator it;

            Point pt(GetMessagePos());

            ScreenToClient(
                _htoolbar,
                &pt
            );

            btninfo.cbSize = sizeof(TBBUTTONINFO);
            btninfo.dwMask =
                TBIF_BYINDEX |
                TBIF_COMMAND;

            int idx =
                (int)SendMessage(
                    _htoolbar,
                    TB_HITTEST,
                    0,
                    (LPARAM)&pt
                );

            if (idx >= 0 &&
                SendMessage(
                    _htoolbar,
                    TB_GETBUTTONINFO,
                    idx,
                    (LPARAM)&btninfo
                ) != -1 &&
                (it = _map.find_id(
                    btninfo.idCommand
                )) != _map.end()) {

                static DynamicFct<DWORD(STDAPICALLTYPE *)(RESTRICTIONS)>
                    pSHRestricted(
                        TEXT("SHELL32"),
                        "SHRestricted"
                    );

                if (pSHRestricted &&
                    !(*pSHRestricted)(
                        REST_NOTRAYCONTEXTMENU
                    )) {

                    ShowAppSystemMenu(it);
                }
            }

            break;
        }


        case TBN_DROPDOWN:
        {
            Point pt(GetMessagePos());

            ScreenToClient(
                pnmh->hwndFrom,
                &pt
            );

            TBBUTTONINFO btninfo;

            btninfo.cbSize = sizeof(TBBUTTONINFO);
            btninfo.dwMask =
                TBIF_BYINDEX |
                TBIF_COMMAND;

            int idx =
                (int)SendMessage(
                    _htoolbar,
                    TB_HITTEST,
                    0,
                    (LPARAM)&pt
                );

            if (idx >= 0 &&
                SendMessage(
                    _htoolbar,
                    TB_GETBUTTONINFO,
                    idx,
                    (LPARAM)&btninfo
                ) != -1) {

                DestoryThumbnailWindow();

                Command(
                    btninfo.idCommand,
                    WM_CLOSE
                );
            }

            break;
        }


        case NM_CUSTOMDRAW:
        {
            LPNMTBCUSTOMDRAW lptbcd =
                (LPNMTBCUSTOMDRAW)pnmh;

            switch (lptbcd->nmcd.dwDrawStage) {

            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;


            case CDDS_ITEMPREPAINT:
            {
                TaskBarMap::iterator it =
                    _map.find_id(
                        (int)lptbcd->nmcd.dwItemSpec
                    );

                bool flashOn = false;

                if (it != _map.end()) {

                    flashOn =
                        it->second._flash &&
                        it->second._flashOn;
                }

#define CDRF_USECDCOLORS 0x00800000

                if (flashOn) {

                    RECT rc =
                        lptbcd->nmcd.rc;

                    HBRUSH hbr =
                        CreateSolidBrush(
                            RGB(255, 128, 0)
                        );

                    FillRect(
                        lptbcd->nmcd.hdc,
                        &rc,
                        hbr
                    );

                    DeleteObject(hbr);

                    lptbcd->clrText =
                        RGB(0, 0, 0);

                    return
                        CDRF_USECDCOLORS |
                        TBCDRF_NOBACKGROUND |
                        TBCDRF_NOEDGES |
                        TBCDRF_NOMARK;
                }

                lptbcd->clrText =
                    TASKBAR_TEXTCOLOR();

                return
                    CDRF_NOTIFYPOSTPAINT |
                    CDRF_USECDCOLORS;
            }


            case CDDS_ITEMPOSTPAINT:
            {
                if (hbrTaskLine) {

                    RECT rect =
                        lptbcd->nmcd.rc;

                    rect.top =
                        DESKTOPBARBAR_HEIGHT - 4;

                    rect.bottom =
                        rect.top + 2;

                    if (((lptbcd->nmcd.uItemState &
                          CDIS_CHECKED) != CDIS_CHECKED) &&
                        ((lptbcd->nmcd.uItemState &
                          CDIS_HOT) != CDIS_HOT)) {

                        rect.left += 4;
                        rect.right -= 4;

                    }
                    else {

                        if (_task_close_button) {
                            rect.left -= 2;
                            rect.right += 2;
                        }
                    }

                    if (g_Globals._isDebug) {

                        _log_(
                            FmtString(
                                TEXT(
                                    "TaskBar::Notify("
                                    "NM_CUSTOMDRAW) %d"
                                ),
                                lptbcd->nmcd.uItemState
                            )
                        );
                    }

                    if (lptbcd->nmcd.uItemState == 0 &&
                        _thumbnail) {

                        KillTimer(
                            _hwnd,
                            ID_TIMER_DESTORYTHUMBNAIL
                        );

                        SetTimer(
                            _hwnd,
                            ID_TIMER_DESTORYTHUMBNAIL,
                            500,
                            NULL
                        );
                    }

                    FillRect(
                        lptbcd->nmcd.hdc,
                        &rect,
                        hbrTaskLine
                    );
                }

                return CDRF_DODEFAULT;
            }


            default:
                return CDRF_DODEFAULT;
            }
        }


        case TBN_HOTITEMCHANGE:
        {
            if (_thumbnail) {

                TBBUTTONINFO btninfo;
                TaskBarMap::iterator it;

                Point pt(GetMessagePos());

                ScreenToClient(
                    _htoolbar,
                    &pt
                );

                btninfo.cbSize =
                    sizeof(TBBUTTONINFO);

                btninfo.dwMask =
                    TBIF_BYINDEX |
                    TBIF_COMMAND;

                int idx =
                    (int)SendMessage(
                        _htoolbar,
                        TB_HITTEST,
                        0,
                        (LPARAM)&pt
                    );

                if (idx >= 0 &&
                    SendMessage(
                        _htoolbar,
                        TB_GETBUTTONINFO,
                        idx,
                        (LPARAM)&btninfo
                    ) != -1 &&
                    (it = _map.find_id(
                        btninfo.idCommand
                    )) != _map.end()) {

                    _log_(
                        FmtString(
                            TEXT(
                                "TaskBar::Notify("
                                "TBN_HOTITEMCHANGE) %d"
                            ),
                            idx
                        )
                    );

                    HWND hTaskWindow =
                        it->first;

                    DrawThumbnailWindow(
                        g_Globals._hInstance,
                        hTaskWindow,
                        NULL,
                        NULL,
                        idx
                    );
                }
            }

            return super::Notify(id, pnmh);
        }


        case TBN_GETINFOTIPA:
        case TBN_GETINFOTIPW:

            if (_thumbnail)
                KillTimer(
                    _hwnd,
                    ID_TIMER_DESTORYTHUMBNAIL
                );

            break;


        default:

            _log_(
                FmtString(
                    TEXT("TaskBar::Notify(%d)"),
                    pnmh->code
                )
            );

            return super::Notify(
                id,
                pnmh
            );
        }
    }

    return 0;
}


void TaskBar::ActivateApp(
    TaskBarMap::iterator it,
    bool can_minimize,
    bool can_restore)
{
    HWND hwnd = it->first;

    it->second._flash = false;
    it->second._flashOn = false;

    bool minimize_it =
        can_minimize &&
        !IsIconic(hwnd) &&
        (hwnd == GetForegroundWindow() ||
         hwnd == _last_foreground_wnd);

    if (can_restore && !minimize_it) {

        if (IsIconic(hwnd))
            PostMessage(
                hwnd,
                WM_SYSCOMMAND,
                SC_RESTORE,
                0
            );
    }

    SetForegroundWindow(hwnd);

    if (minimize_it) {

        PostMessage(
            hwnd,
            WM_SYSCOMMAND,
            SC_MINIMIZE,
            0
        );

        _last_foreground_wnd = 0;
    }
    else {

        _last_foreground_wnd = hwnd;
    }

    Refresh();
}


#define ENABLESYSMENUITEM(m, item, cond) \
    EnableMenuItem( \
        (m), \
        (item), \
        MF_BYCOMMAND | ((cond) ? MF_ENABLED : MF_GRAYED) \
    )


void TaskBar::ShowAppSystemMenu(TaskBarMap::iterator it)
{
    HWND hTaskWindow = it->first;
    HMENU hmenu =
        GetSystemMenu(
            it->first,
            FALSE
        );

    if (hmenu) {

        POINT pt;

        GetCursorPos(&pt);

        WINDOWPLACEMENT wndpl;

        wndpl.length =
            sizeof(WINDOWPLACEMENT);

        GetWindowPlacement(
            hTaskWindow,
            &wndpl
        );

        UINT showCmd =
            wndpl.showCmd;

        DWORD taskbarThreadID =
            GetWindowThreadProcessId(
                _hwnd,
                NULL
            );

        DWORD taskWindowThreadID =
            GetWindowThreadProcessId(
                hTaskWindow,
                NULL
            );

        AttachThreadInput(
            taskbarThreadID,
            taskWindowThreadID,
            TRUE
        );

        SetWindowPos(
            hTaskWindow,
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOSIZE |
            SWP_NOMOVE |
            SWP_NOACTIVATE
        );

        AttachThreadInput(
            taskbarThreadID,
            taskWindowThreadID,
            FALSE
        );

        if (GetClassLongPtr(
                hTaskWindow,
                GCL_STYLE) & CS_NOCLOSE) {

            DeleteMenu(
                hmenu,
                SC_CLOSE,
                MF_BYCOMMAND
            );
        }

        ENABLESYSMENUITEM(
            hmenu,
            SC_RESTORE,
            showCmd != SW_SHOWNORMAL
        );

        ENABLESYSMENUITEM(
            hmenu,
            SC_MOVE,
            showCmd == SW_SHOWNORMAL
        );

        ENABLESYSMENUITEM(
            hmenu,
            SC_SIZE,
            showCmd == SW_SHOWNORMAL
        );

        ENABLESYSMENUITEM(
            hmenu,
            SC_MINIMIZE,
            showCmd != SW_SHOWMINIMIZED
        );

        ENABLESYSMENUITEM(
            hmenu,
            SC_MAXIMIZE,
            showCmd != SW_SHOWMAXIMIZED
        );

        SendMessage(
            hTaskWindow,
            WM_INITMENUPOPUP,
            (WPARAM)hmenu,
            MAKELPARAM(0, TRUE)
        );

        SendMessage(
            hTaskWindow,
            WM_INITMENU,
            (WPARAM)hmenu,
            0
        );

        int cmd =
            TrackPopupMenu(
                hmenu,
                TPM_RETURNCMD |
                TPM_RECURSE,
                pt.x,
                pt.y,
                0,
                _hwnd,
                NULL
            );

        if (cmd) {

            PostMessage(
                hTaskWindow,
                WM_SYSCOMMAND,
                cmd,
                0
            );
        }
    }
}


HICON get_window_icon_small(HWND hwnd)
{
    HICON hIcon = 0;

    SendMessageTimeout(
        hwnd,
        WM_GETICON,
        ICON_SMALL2,
        0,
        SMTO_ABORTIFHUNG,
        1000,
        (PDWORD_PTR)&hIcon
    );

    if (!hIcon)
        SendMessageTimeout(
            hwnd,
            WM_GETICON,
            ICON_SMALL,
            0,
            SMTO_ABORTIFHUNG,
            1000,
            (PDWORD_PTR)&hIcon
        );

    if (!hIcon)
        SendMessageTimeout(
            hwnd,
            WM_GETICON,
            ICON_BIG,
            0,
            SMTO_ABORTIFHUNG,
            1000,
            (PDWORD_PTR)&hIcon
        );

    if (!hIcon)
        hIcon =
            (HICON)GetClassLongPtr(
                hwnd,
                GCL_HICONSM
            );

    if (!hIcon)
        hIcon =
            (HICON)GetClassLongPtr(
                hwnd,
                GCL_HICON
            );

    if (!hIcon)
        SendMessageTimeout(
            hwnd,
            WM_QUERYDRAGICON,
            0,
            0,
            0,
            1000,
            (PDWORD_PTR)&hIcon
        );

    return hIcon;
}


HICON get_window_icon_big(
    HWND hwnd,
    bool allow_from_class)
{
    HICON hIcon = 0;

    SendMessageTimeout(
        hwnd,
        WM_GETICON,
        ICON_BIG,
        0,
        SMTO_ABORTIFHUNG,
        1000,
        (PDWORD_PTR)&hIcon
    );

    if (!hIcon)
        SendMessageTimeout(
            hwnd,
            WM_GETICON,
            ICON_SMALL2,
            0,
            SMTO_ABORTIFHUNG,
            1000,
            (PDWORD_PTR)&hIcon
        );

    if (!hIcon)
        SendMessageTimeout(
            hwnd,
            WM_GETICON,
            ICON_SMALL,
            0,
            SMTO_ABORTIFHUNG,
            1000,
            (PDWORD_PTR)&hIcon
        );

    if (allow_from_class) {

        if (!hIcon)
            hIcon =
                (HICON)GetClassLongPtr(
                    hwnd,
                    GCL_HICON
                );

        if (!hIcon)
            hIcon =
                (HICON)GetClassLongPtr(
                    hwnd,
                    GCL_HICONSM
                );
    }

    if (!hIcon)
        SendMessageTimeout(
            hwnd,
            WM_QUERYDRAGICON,
            0,
            0,
            0,
            1000,
            (PDWORD_PTR)&hIcon
        );

    return hIcon;
}


static int isTopWindow(HWND hwnd)
{
    if (GetParent(hwnd))
        return 0;

    if (GetWindow(hwnd, GW_OWNER))
        return 0;

    return 1;
}


// EnumWindows() callback
BOOL CALLBACK TaskBar::EnumWndProc(HWND hwnd, LPARAM lparam)
{
    TaskBar* pThis = (TaskBar*)lparam;

    if (!pThis)
        return TRUE;

    // EnumWindows 只负责枚举窗口，
    // 实际的任务栏窗口判断、创建和更新全部交给 AddWindow()
    pThis->AddWindow(hwnd);

    // 注意：无论 AddWindow() 是否处理这个窗口，
    // EnumWindows 都必须继续枚举下一个窗口。
    return TRUE;
}


void TaskBar::ApplyBackgroundStyle()
{
    static String bkmode =
        TEXT("-");

    int transparency = 100;
    COLORREF transparency_color = 0;

    if (bkmode == TEXT(""))
        return;

    if (bkmode == TEXT("-")) {

        bkmode =
            TASKBAR_GETBKMODE().ToString();

        if (bkmode == TEXT("opaque")) {

            bkmode = TEXT("");
            return;
        }

        transparency =
            TASKBAR_GETBKTRANSPARENCY(100);

        transparency_color =
            TASKBAR_GETBKTRANSPARENCYCOLOR();
    }

    TaskbarTransparency(
        GetParent(_hwnd),
        bkmode.c_str(),
        transparency,
        transparency_color
    );
}


void TaskBar::Refresh()
{
    EnumWindows(
        EnumWndProc,
        (LPARAM)this
    );

    HWND foreground =
        GetForegroundWindow();

    if (foreground)
        UpdateActiveWindow(foreground);
}


TaskBarMap::iterator TaskBarMap::find_id(int id)
{
    for (iterator it = begin();
         it != end();
         ++it) {

        if (it->second._id == id)
            return it;
    }

    return end();
}


void TaskBar::ResizeButtons()
{
    int btns =
        (int)_map.size();

    if (btns > 0) {

        int bar_width =
            ClientRect(_hwnd).right;

        if (_task_close_button)
            bar_width -=
                btns * 20;

        int btn_width =
            (bar_width / btns) - 3;

        if (btn_width <
            TASKBUTTONWIDTH_MIN) {

            btn_width =
                TASKBUTTONWIDTH_MIN;
        }
        else if (btn_width >
                 TASKBUTTONWIDTH_MAX) {

            btn_width =
                TASKBUTTONWIDTH_MAX;
        }

        if (btn_width !=
            _last_btn_width) {

            _last_btn_width =
                btn_width;

            SendMessage(
                _htoolbar,
                TB_SETBUTTONWIDTH,
                0,
                MAKELPARAM(
                    btn_width,
                    btn_width
                )
            );

            SendMessage(
                _htoolbar,
                TB_AUTOSIZE,
                0,
                0
            );
        }
    }
}


void TaskBar::UpdateActiveWindow(
    HWND hwndActivated)
{
    HWND target =
        hwndActivated;

    if (_map.find(target) ==
        _map.end()) {

        HWND owner =
            GetWindow(
                hwndActivated,
                GW_OWNER
            );

        if (owner &&
            _map.find(owner) !=
                _map.end()) {

            target = owner;
        }
    }

    HWND old =
        _last_foreground_wnd;

    if (old == target) {
        TaskBarMap::iterator it = _map.find(target);

        if (it != _map.end()) {
            BYTE activeState =
                TBSTATE_PRESSED |
                TBSTATE_CHECKED;

            if ((it->second._fsState & activeState) == activeState)
                return;
        }
    }

    TaskBarMap::iterator old_it =
        _map.find(old);

    if (old_it != _map.end()) {

        TaskBarEntry& entry =
            old_it->second;

        BYTE state =
            entry._fsState &
            ~(TBSTATE_PRESSED |
              TBSTATE_CHECKED);

        if (state !=
            entry._fsState) {

            SendMessage(
                _htoolbar,
                TB_SETSTATE,
                entry._id,
                MAKELONG(
                    state,
                    0
                )
            );

            entry._fsState =
                state;
        }
    }

    TaskBarMap::iterator new_it =
        _map.find(target);

    if (new_it != _map.end()) {

        TaskBarEntry& entry =
            new_it->second;

        BYTE state =
            entry._fsState |
            TBSTATE_PRESSED |
            TBSTATE_CHECKED;

        SendMessage(
            _htoolbar,
            TB_SETSTATE,
            entry._id,
            MAKELONG(
                state,
                0
            )
        );

        entry._fsState =
            state;

        _last_foreground_wnd =
            target;
    }

    InvalidateRect(
        _htoolbar,
        NULL,
    
        FALSE
    );
}


BOOL TaskBar::AddWindow(HWND hwnd)
{
    DWORD style =
        GetWindowStyle(hwnd);

    DWORD ex_style =
        GetWindowExStyle(hwnd);

    if (!((style & WS_VISIBLE) &&
          !(ex_style & WS_EX_TOOLWINDOW) &&
          ((ex_style & WS_EX_APPWINDOW) |
           isTopWindow(hwnd)))) {

        return FALSE;
    }

    TCHAR title[BUFFER_LEN] = { 0 };
    TCHAR strbuffer[BUFFER_LEN] = { 0 };

    if (!GetWindowText(
            hwnd,
            title,
            BUFFER_LEN)) {

        title[0] = '\0';
    }

    String str_title =
        title;

    if (str_title.find(
            TEXT("Windows Shell Experience ")) !=
        String::npos) {

        return TRUE;
    }

    if (!GetClassName(
            hwnd,
            strbuffer,
            BUFFER_LEN)) {

        strbuffer[0] = '\0';
    }

    str_title =
        strbuffer;

    if (str_title.find(
            TEXT("ApplicationFrameWindow")) !=
        String::npos ||
        str_title.find(
            TEXT("Windows.UI.Core.CoreWindow")) !=
        String::npos) {

        return TRUE;
    }

    TaskBarMap::iterator found =
        _map.find(hwnd);

    int last_id = 0;

    if (found != _map.end()) {

        last_id =
            found->second._id;

        if (!last_id)
            found->second._id =
                _next_id++;
    }
    else {

        HBITMAP hbmp = NULL;
        HICON hIcon = NULL;
        BOOL delete_icon = FALSE;

        if (str_title ==
            TEXT("ConsoleWindowClass")) {

            hIcon =
                g_Globals._icon_cache
                    .get_icon(ICID_CMDEXE)
                    .get_hicon();
        }

        if (!hIcon)
            hIcon =
                get_window_icon_big(hwnd);

        if (!hIcon) {

            hIcon =
                LoadIcon(
                    0,
                    IDI_APPLICATION
                );

            delete_icon = TRUE;
        }

        if (hIcon) {

            RECT rect =
                _icon_area;

#ifdef _DEBUG
            ICONINFO iconInfo;

            GetIconInfo(
                hIcon,
                &iconInfo
            );

            BITMAP biIcon;

            GetObject(
                iconInfo.hbmColor,
                sizeof(BITMAP),
                &biIcon
            );
#endif

            hbmp =
                create_bitmap_from_icon(
                    hIcon,
                    TASKBAR_BRUSH(),
                    WindowCanvas(_htoolbar),
                    TASKBAR_ICON_SIZE,
                    rect
                );

            if (delete_icon)
                DestroyIcon(hIcon);
        }
        else {
            hbmp = 0;
        }

        TBADDBITMAP ab = {
            0,
            (UINT_PTR)hbmp
        };

        int bmp_idx =
            (int)SendMessage(
                _htoolbar,
                TB_ADDBITMAP,
                1,
                (LPARAM)&ab
            );

        TaskBarEntry entry;

        entry._id =
            _next_id++;

        entry._hbmp =
            hbmp;

        entry._bmp_idx =
            bmp_idx;

        entry._title =
            title;

        _map[hwnd] =
            entry;

        found =
            _map.find(hwnd);

        _log_(
            FmtString(
                TEXT("TaskBar::AddButton %s"),
                str_title.c_str()
            )
        );
    }

    TBBUTTON btn = {
        -2,
        0,
        TBSTATE_ENABLED,
        BTNS_BUTTON,
        { 0, 0 },
        0,
        0
    };

    if (_task_close_button)
        btn.fsStyle =
            BTNS_DROPDOWN;

    TaskBarEntry& entry =
        found->second;

    btn.idCommand =
        entry._id;

    if (!last_id) {

        if (title[0])
            btn.iString =
                (INT_PTR)title;

        btn.iBitmap =
            entry._bmp_idx;

        entry._btn_idx =
            (int)SendMessage(
                _htoolbar,
                TB_BUTTONCOUNT,
                0,
                0
            );

        SendMessage(
            _htoolbar,
            TB_INSERTBUTTON,
            entry._btn_idx,
            (LPARAM)&btn
        );

        ResizeButtons();
    }
    else {

        if (btn.fsState !=
            entry._fsState) {

            SendMessage(
                _htoolbar,
                TB_SETSTATE,
                entry._id,
                MAKELONG(
                    btn.fsState,
                    0
                )
            );
        }

        if (entry._title != title) {

            TBBUTTONINFO info;

            info.cbSize =
                sizeof(TBBUTTONINFO);

            info.dwMask =
                TBIF_TEXT;

            info.pszText =
                title;

            SendMessage(
                _htoolbar,
                TB_SETBUTTONINFO,
                entry._id,
                (LPARAM)&info
            );

            entry._title =
                title;
        }
    }

    entry._fsState =
        btn.fsState;

    return TRUE;
}


BOOL TaskBar::RemoveWindow(HWND hwnd)
{
    TaskBarMap::iterator found =
        _map.find(hwnd);

    if (found == _map.end())
        return FALSE;

    TaskBarEntry& entry =
        found->second;

    if (!entry._id)
        return FALSE;

    int idx =
        entry._btn_idx;

    HBITMAP hbmp =
        entry._hbmp;

    if (!SendMessage(
            _htoolbar,
            TB_DELETEBUTTON,
            idx,
            0)) {

        MessageBoxW(
            NULL,
            L"failed to delete button",
            NULL,
            MB_OK
        );
    }

    for (TaskBarMap::iterator it =
             _map.begin();
         it != _map.end();
         ++it) {

        if (it == found)
            continue;

        TaskBarEntry& other =
            it->second;

        if (other._btn_idx > idx)
            --other._btn_idx;
    }

    if (hbmp)
        DeleteObject(hbmp);

    if (_last_foreground_wnd == hwnd)
        _last_foreground_wnd = 0;

    _map.erase(found);

    ResizeButtons();

    return TRUE;
}


BOOL TaskBar::UpdateWindow(HWND hwnd)
{
    TaskBarMap::iterator found =
        _map.find(hwnd);

    if (found == _map.end())
        return FALSE;

    TaskBarEntry& entry =
        found->second;

    TCHAR title[BUFFER_LEN] = { 0 };

    if (!GetWindowText(
            hwnd,
            title,
            BUFFER_LEN)) {

        title[0] = '\0';
    }

    if (entry._title != title) {

        TBBUTTONINFO info;

        info.cbSize =
            sizeof(TBBUTTONINFO);

        info.dwMask =
            TBIF_TEXT;

        info.pszText =
            title;

        SendMessage(
            _htoolbar,
            TB_SETBUTTONINFO,
            entry._id,
            (LPARAM)&info
        );

        entry._title =
            title;
    }

    return TRUE;
}