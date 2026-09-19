#include "precomp.h"
#include "native_startmenu.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <powrprof.h>

#include <vector>
#include <algorithm>
#include <cstdlib>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "PowrProf.lib")

typedef void (WINAPI* RUNFILEDLG)(
    HWND,
    HICON,
    LPCTSTR,
    LPCTSTR,
    LPCTSTR,
    UINT);

// ============================================================
// NativeStartMenu
// ============================================================

NativeStartMenu::NativeStartMenu(
    HWND hwndOwner,
    HWND hwndStartButton)
    :
    _hwndOwner(hwndOwner),
    _hwndStartButton(hwndStartButton),
    _hwndMenu(NULL),
    _visible(false),
    _ignoreNextStartClick(false),
    _hMenu(NULL)
{
}


NativeStartMenu::~NativeStartMenu()
{
    Hide();

    if (_hMenu)
    {
        DestroyMenu(_hMenu);
        _hMenu = NULL;
    }

    if (_hwndMenu)
    {
        DestroyWindow(_hwndMenu);
        _hwndMenu = NULL;
    }
}


// ============================================================
// Create
// ============================================================

bool NativeStartMenu::Create()
{
    WNDCLASSW wc = {};

    wc.style =
        CS_HREDRAW |
        CS_VREDRAW;

    wc.lpfnWndProc =
        NativeStartMenu::WindowProc;

    wc.hInstance =
        GetModuleHandleW(NULL);

    wc.hCursor =
        LoadCursorW(
            NULL,
            IDC_ARROW);

    wc.hbrBackground =
        (HBRUSH)(COLOR_MENU + 1);

    wc.lpszClassName =
        L"NativeStartMenuWindow";


    static bool registered = false;

    if (!registered)
    {
        ATOM atom =
            RegisterClassW(&wc);

        if (!atom)
        {
            DWORD error =
                GetLastError();

            if (error !=
                ERROR_CLASS_ALREADY_EXISTS)
            {
                return false;
            }
        }

        registered = true;
    }


    _hwndMenu =
        CreateWindowExW(
            WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE,
            L"NativeStartMenuWindow",
            L"",
            WS_POPUP,
            0,
            0,
            NATIVE_STARTMENU_WIDTH,
            100,
            _hwndOwner,
            NULL,
            GetModuleHandleW(NULL),
            this);


    if (!_hwndMenu)
        return false;


    _hMenu =
        CreatePopupMenu();

    if (!_hMenu)
    {
        DestroyWindow(
            _hwndMenu);

        _hwndMenu = NULL;

        return false;
    }


    BuildMenu();

    return true;
}


// ============================================================
// SetStartButton
// ============================================================

void NativeStartMenu::SetStartButton(
    HWND hwndStartButton)
{
    _hwndStartButton =
        hwndStartButton;
}


// ============================================================
// Toggle
// ============================================================

void NativeStartMenu::Toggle()
{
    if (_visible)
        Hide();
    else
        Show();
}


// ============================================================
// Show
// ============================================================

void NativeStartMenu::Show()
{
    if (!_hwndMenu || !_hMenu)
        return;


    /*
     * 菜单已经显示
     */
    if (_visible)
        return;


    /*
     * 找开始按钮位置
     */
    RECT rcButton = {};

    if (_hwndStartButton &&
        IsWindow(_hwndStartButton))
    {
        GetWindowRect(
            _hwndStartButton,
            &rcButton);
    }
    else
    {
        SystemParametersInfoW(
            SPI_GETWORKAREA,
            0,
            &rcButton,
            0);
    }


    /*
     * 这里使用经典开始菜单的大致尺寸。
     *
     * 真正菜单高度由 Windows 菜单系统决定。
     */
    int menuWidth =
        NATIVE_STARTMENU_WIDTH;


    int menuHeight =
        420;


    int x =
        rcButton.left;


    int y =
        rcButton.top -
        menuHeight;


    /*
     * 工作区
     */
    RECT workArea = {};

    SystemParametersInfoW(
        SPI_GETWORKAREA,
        0,
        &workArea,
        0);


    /*
     * 如果上方放不下，就放到开始按钮下面。
     */
    if (y < workArea.top)
    {
        y =
            rcButton.bottom;
    }


    /*
     * 右边界
     */
    if (x + menuWidth >
        workArea.right)
    {
        x =
            workArea.right -
            menuWidth;
    }


    /*
     * 先设置一个窗口位置。
     *
     * 这个窗口本身只是 NativeStartMenu
     * 的宿主。
     */
    SetWindowPos(
        _hwndMenu,
        HWND_TOP,
        x,
        y,
        menuWidth,
        menuHeight,
        SWP_NOACTIVATE);


    _visible = true;


    /*
     * 显示菜单。
     */
    SetForegroundWindow(
        _hwndMenu);


    UINT flags =
        TPM_LEFTALIGN |
        TPM_BOTTOMALIGN |
        TPM_LEFTBUTTON |
        TPM_RETURNCMD;


    int command =
        TrackPopupMenuEx(
            _hMenu,
            flags,
            rcButton.left,
            rcButton.top,
            _hwndMenu,
            NULL);


    /*
     * TrackPopupMenuEx 返回以后，
     * 菜单已经关闭。
     */
    _ignoreNextStartClick = true;
    _visible = false;


    /*
     * 用户选择了一个项目。
     */
    if (command != 0)
    {
        ExecuteCommand(
            (UINT)command);
    }


    /*
     * 恢复前台窗口。
     */
    if (_hwndOwner &&
        IsWindow(_hwndOwner))
    {
        SetForegroundWindow(
            _hwndOwner);
    }
}


// ============================================================
// Hide
// ============================================================

void NativeStartMenu::Hide()
{
    if (!_hwndMenu)
        return;


    _visible = false;


    /*
     * TrackPopupMenuEx 正在运行时，
     * EndMenu 可以主动结束菜单跟踪。
     */
    EndMenu();


    ShowWindow(
        _hwndMenu,
        SW_HIDE);
}


// ============================================================
// IsVisible
// ============================================================

bool NativeStartMenu::IsVisible() const
{
    return _visible;
}

// ============================================================
// ConsumeStartClick
// ============================================================

bool NativeStartMenu::ConsumeStartClick()
{
    if (!_ignoreNextStartClick)
        return false;

    _ignoreNextStartClick = false;
    return true;
}

// ============================================================
// Run
// ============================================================

void NativeStartMenu::Run()
{
    RUNFILEDLG RunFileDlg =
        (RUNFILEDLG)GetProcAddress(
            GetModuleHandle(TEXT("shell32.dll")),
            MAKEINTRESOURCEA(61));

    if (!RunFileDlg)
        return;

    RECT rect = { 0 };

#ifndef TASKBAR_AT_TOP
    rect.top =
        GetSystemMetrics(SM_CYSCREEN) -
        DESKTOPBARBAR_HEIGHT;
#endif

    rect.right =
        GetSystemMetrics(SM_CXSCREEN);

    rect.bottom =
        rect.top +
        DESKTOPBARBAR_HEIGHT;

    Static dlgOwner(
        0,
        0,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        0,
        0);

    RunFileDlg(
        dlgOwner,
        NULL,
        NULL,
        NULL,
        NULL,
        0x4);

    DestroyWindow(dlgOwner);
}

// ============================================================
// Logoff
// ============================================================
void NativeStartMenu::Logoff()
{
    if (MessageBoxW(
        NULL,
        L"Are you sure you want to log off?",
        L"Log Off Windows",
        MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        ExitWindowsEx(
            EWX_LOGOFF,
            0);
    }
}

// ============================================================
// BuildMenu
// ============================================================

void NativeStartMenu::BuildMenu()
{
    if (!_hMenu)
        return;


    /*
     * 如果重复 Build，
     * 先清空旧菜单。
     */
    while (GetMenuItemCount(_hMenu) > 0)
    {
        DeleteMenu(
            _hMenu,
            0,
            MF_BYPOSITION);
    }


    BuildMainMenu(
        _hMenu);
}


// ============================================================
// BuildMainMenu
// ============================================================

void NativeStartMenu::BuildMainMenu(
    HMENU hMenu)
{
    if (!hMenu)
        return;


    /*
     * 分隔线
     */
    /*
    InsertMenuW(
        hMenu,
        -1,
        MF_BYPOSITION |
        MF_SEPARATOR,
        0,
        NULL);
     */
    /*
 * Programs
 */
    HMENU hPrograms =
        CreatePopupMenu();

    if (hPrograms)
    {
        BuildProgramsMenu(
            hPrograms);


        if (GetMenuItemCount(
            hPrograms) > 0)
        {
            InsertSubMenu(
                hMenu,
                hPrograms,
                NATIVE_CMD_PROGRAMS,
                L"Programs",
                GetCommandIcon(
                    NATIVE_CMD_PROGRAMS));
        }
        else
        {
            DestroyMenu(
                hPrograms);
        }
    }


    /*
     * Documents
     */
    InsertShellFolder(
        hMenu,
        CSIDL_PERSONAL,
        L"Documents");

    /*
     * Pictures
     */
    InsertShellFolder(
        hMenu,
        CSIDL_MYPICTURES,
        L"Pictures");


    /*
     * Network
     */
        InsertShellFolder(
            hMenu,
            CSIDL_NETWORK,
            L"Network");


    /*
     * Settings
     */
    HMENU hSettings =
        CreatePopupMenu();

    if (hSettings)
    {
        InsertShellFolder(
            hSettings,
            CSIDL_CONTROLS,
            L"Control Panel");


        InsertShellFolder(
            hSettings,
            CSIDL_PRINTERS,
            L"Printers");


        /*
         * Settings 是一级菜单，
         * 但里面的项目属于内层菜单。
         *
         * 把 Settings 子菜单中的项目
         * 标记为小菜单样式。
         */
        int settingsCount =
            GetMenuItemCount(
                hSettings);

        for (int i = 0;
            i < settingsCount;
            ++i)
        {
            MENUITEMINFOW mii = {};

            mii.cbSize =
                sizeof(mii);

            mii.fMask =
                MIIM_DATA;

            if (GetMenuItemInfoW(
                hSettings,
                i,
                TRUE,
                &mii))
            {
                NativeMenuItemData* data =
                    (NativeMenuItemData*)
                    mii.dwItemData;

                if (data)
                {
                    data->smallIcon =
                        true;
                }
            }
        }

        if (settingsCount > 0)
        {
            InsertSubMenu(
                hMenu,
                hSettings,
                NATIVE_CMD_SETTINGS,
                L"Settings",
                GetCommandIcon(
                    NATIVE_CMD_SETTINGS));
        }
        else
        {
            DestroyMenu(
                hSettings);
        }
    }


    /*
     * Run
     */
    InsertCommand(
        hMenu,
        NATIVE_CMD_RUN,
        L"Run...");


    /*
     * 分隔线
     */
    InsertMenuW(
        hMenu,
        -1,
        MF_BYPOSITION |
        MF_SEPARATOR,
        0,
        NULL);


    /*
     * Log Off
     */
    InsertCommand(
        hMenu,
        NATIVE_CMD_LOGOFF,
        L"Log Off...");


    /*
     * Shut Down
     */
    HMENU hShutdown =
        CreatePopupMenu();

    if (hShutdown)
    {
        InsertCommand(
            hShutdown,
            NATIVE_CMD_SHUTDOWN,
            L"Shut Down");

        InsertCommand(
            hShutdown,
            NATIVE_CMD_RESTART,
            L"Restart");

        InsertCommand(
            hShutdown,
            NATIVE_CMD_SLEEP,
            L"Sleep");

        InsertCommand(
            hShutdown,
            NATIVE_CMD_HIBERNATE,
            L"Hibernate");

        InsertSubMenu(
            hMenu,
            hShutdown,
            NATIVE_CMD_SHUTDOWN,
            L"Shut Down",
            GetCommandIcon(
                NATIVE_CMD_SHUTDOWN));
    }
}


// ============================================================
// BuildProgramsMenu
// ============================================================

void NativeStartMenu::BuildProgramsMenu(
    HMENU hMenu)
{
    if (!hMenu)
        return;


    std::vector<NativeProgramEntry>
        entries;


    WCHAR userPath[MAX_PATH] = {};

    WCHAR commonPath[MAX_PATH] = {};


    /*
     * 当前用户 Programs
     */
    if (SUCCEEDED(
        SHGetFolderPathW(
            _hwndOwner,
            CSIDL_PROGRAMS,
            NULL,
            SHGFP_TYPE_CURRENT,
            userPath)))
    {
        std::vector<NativeProgramEntry>
            userEntries;


        CollectProgramsDirectory(
            userPath,
            userEntries);


        MergeProgramEntries(
            entries,
            userEntries);
    }


    /*
     * 所有用户 Programs
     */
    if (SUCCEEDED(
        SHGetFolderPathW(
            _hwndOwner,
            CSIDL_COMMON_PROGRAMS,
            NULL,
            SHGFP_TYPE_CURRENT,
            commonPath)))
    {
        std::vector<NativeProgramEntry>
            commonEntries;


        CollectProgramsDirectory(
            commonPath,
            commonEntries);


        MergeProgramEntries(
            entries,
            commonEntries);
    }


    /*
     * 排序
     */
    SortProgramEntries(
        entries);


    /*
     * 转换成真正的 HMENU
     */
    BuildProgramMenuEntries(
        hMenu,
        entries);
}


// ============================================================
// CollectProgramsDirectory
// ============================================================

bool NativeStartMenu::CollectProgramsDirectory(
    LPCWSTR path,
    std::vector<NativeProgramEntry>& entries)
{
    if (!path || !*path)
        return false;


    WCHAR searchPath[MAX_PATH] = {};


    StringCchCopyW(
        searchPath,
        MAX_PATH,
        path);


    StringCchCatW(
        searchPath,
        MAX_PATH,
        L"\\*");


    WIN32_FIND_DATAW fd = {};


    HANDLE hFind =
        FindFirstFileW(
            searchPath,
            &fd);


    if (hFind ==
        INVALID_HANDLE_VALUE)
    {
        return false;
    }


    do
    {
        /*
         * 排除 . 和 ..
         */
        if (!lstrcmpW(
                fd.cFileName,
                L".") ||
            !lstrcmpW(
                fd.cFileName,
                L".."))
        {
            continue;
        }


        NativeProgramEntry entry = {};


        StringCchCopyW(
            entry.name,
            MAX_PATH,
            fd.cFileName);


        StringCchCopyW(
            entry.path,
            MAX_PATH,
            path);


        StringCchCatW(
            entry.path,
            MAX_PATH,
            L"\\");


        StringCchCatW(
            entry.path,
            MAX_PATH,
            fd.cFileName);


        entry.directory =
            (fd.dwFileAttributes &
             FILE_ATTRIBUTE_DIRECTORY) != 0;


        /*
         * 文件夹
         */
        if (entry.directory)
        {
            CollectProgramsDirectory(
                entry.path,
                entry.children);


            /*
             * 空文件夹不显示
             */
            if (entry.children.empty())
                continue;
        }
        else
        {
            /*
             * 只处理这些入口
             */
            LPCWSTR ext =
                PathFindExtensionW(
                    entry.name);


            if (!ext)
                continue;


            if (lstrcmpiW(
                    ext,
                    L".lnk") != 0 &&
                lstrcmpiW(
                    ext,
                    L".exe") != 0 &&
                lstrcmpiW(
                    ext,
                    L".cpl") != 0)
            {
                continue;
            }
        }


        entries.push_back(
            entry);

    }
    while (FindNextFileW(
        hFind,
        &fd));


    FindClose(
        hFind);


    return !entries.empty();
}


// ============================================================
// MergeProgramEntries
// ============================================================

void NativeStartMenu::MergeProgramEntries(
    std::vector<NativeProgramEntry>& target,
    std::vector<NativeProgramEntry>& source)
{
    for (size_t i = 0;
         i < source.size();
         ++i)
    {
        NativeProgramEntry& src =
            source[i];


        bool merged = false;


        /*
         * 只有目录才进行合并。
         */
        if (src.directory)
        {
            for (size_t j = 0;
                 j < target.size();
                 ++j)
            {
                NativeProgramEntry& dst =
                    target[j];


                if (!dst.directory)
                    continue;


                if (lstrcmpiW(
                        dst.name,
                        src.name) != 0)
                {
                    continue;
                }


                /*
                 * 同名目录递归合并。
                 */
                MergeProgramEntries(
                    dst.children,
                    src.children);


                merged = true;

                break;
            }
        }


        /*
         * 不是同名目录，
         * 直接加入。
         */
        if (!merged)
        {
            target.push_back(
                src);
        }
    }
}


// ============================================================
// SortProgramEntries
// ============================================================

static bool NativeProgramEntryLess(
    const NativeProgramEntry& a,
    const NativeProgramEntry& b)
{
    /*
     * 文件夹优先。
     */
    if (a.directory !=
        b.directory)
    {
        return a.directory;
    }


    /*
     * 同类按照名称排序。
     */
    return lstrcmpiW(
        a.name,
        b.name) < 0;
}


void NativeStartMenu::SortProgramEntries(
    std::vector<NativeProgramEntry>& entries)
{
    std::sort(
        entries.begin(),
        entries.end(),
        NativeProgramEntryLess);


    /*
     * 子目录继续排序。
     */
    for (size_t i = 0;
         i < entries.size();
         ++i)
    {
        if (entries[i].directory)
        {
            SortProgramEntries(
                entries[i].children);
        }
    }
}


// ============================================================
// BuildProgramMenuEntries
// ============================================================

void NativeStartMenu::BuildProgramMenuEntries(
    HMENU hMenu,
    const std::vector<NativeProgramEntry>& entries)
{
    if (!hMenu)
        return;


    for (size_t i = 0;
         i < entries.size();
         ++i)
    {
        const NativeProgramEntry& entry =
            entries[i];


        /*
         * 文件夹
         */
        if (entry.directory)
        {
            HMENU hSubMenu =
                CreatePopupMenu();


            if (!hSubMenu)
                continue;


            BuildProgramMenuEntries(
                hSubMenu,
                entry.children);


            if (GetMenuItemCount(
                    hSubMenu) <= 0)
            {
                DestroyMenu(
                    hSubMenu);

                continue;
            }


            SHFILEINFOW sfi = {};


            SHGetFileInfoW(
                entry.path,
                FILE_ATTRIBUTE_DIRECTORY,
                &sfi,
                sizeof(sfi),
                SHGFI_ICON |
                SHGFI_SMALLICON);


            InsertProgramSubMenu(
                hMenu,
                hSubMenu,
                entry.name,
                sfi.hIcon);


            continue;
        }


        /*
         * 程序
         */
        AddProgramsItem(
            hMenu,
            entry.path,
            entry.name);
    }
}


// ============================================================
// AddProgramsItem
// ============================================================

void NativeStartMenu::AddProgramsItem(
    HMENU hMenu,
    LPCWSTR path,
    LPCWSTR name)
{
    if (!hMenu ||
        !path ||
        !name)
    {
        return;
    }


    SHFILEINFOW sfi = {};


    SHGetFileInfoW(
        path,
        0,
        &sfi,
        sizeof(sfi),
        SHGFI_ICON |
        SHGFI_SMALLICON);


    WCHAR displayName[MAX_PATH] = {};


    StringCchCopyW(
        displayName,
        MAX_PATH,
        name);


    /*
     * 去掉扩展名。
     */
    PathRemoveExtensionW(
        displayName);


    NativeMenuItemData* data =
        new NativeMenuItemData();


    data->text =
        _wcsdup(displayName);


    data->hIcon =
        sfi.hIcon;


    data->pidl =
        NULL;


    data->path =
        _wcsdup(path);


    data->ownsText =
        true;


    data->hasSubMenu =
        false;


    data->ownerDraw =
        true;

    data->smallIcon =
        true;

    MENUITEMINFOW mii = {};


    mii.cbSize =
        sizeof(mii);


    mii.fMask =
        MIIM_ID |
        MIIM_STRING |
        MIIM_DATA |
        MIIM_FTYPE;

    mii.fType =
        MFT_OWNERDRAW;

    /*
     * 给程序一个稳定 ID。
     *
     * 这里不依赖 ID，
     * 真正的路径放在 data->path。
     */
    static UINT nextProgramId =
        51000;


    mii.wID =
        nextProgramId++;


    if (nextProgramId >= 59999)
        nextProgramId = 51000;


    mii.dwTypeData =
        (LPWSTR)data->text;


    mii.dwItemData =
        (ULONG_PTR)data;


    if (!InsertMenuItemW(
            hMenu,
            -1,
            TRUE,
            &mii))
    {
        FreeMenuItemData(
            data);
    }
}


// ============================================================
// InsertProgramSubMenu
// ============================================================

void NativeStartMenu::InsertProgramSubMenu(
    HMENU hMenu,
    HMENU hSubMenu,
    LPCWSTR text,
    HICON hIcon)
{
    if (!hMenu ||
        !hSubMenu)
    {
        return;
    }


    NativeMenuItemData* data =
        new NativeMenuItemData();


    data->text =
        _wcsdup(text);


    data->hIcon =
        hIcon;


    data->pidl =
        NULL;


    data->path =
        NULL;


    data->ownsText =
        true;


    data->hasSubMenu =
        true;


    data->ownerDraw =
        true;

    data->smallIcon =
        true;

    MENUITEMINFOW mii = {};


    mii.cbSize =
        sizeof(mii);


    mii.fMask =
        MIIM_STRING |
        MIIM_SUBMENU |
        MIIM_DATA |
        MIIM_FTYPE;

    mii.fType =
        MFT_OWNERDRAW;

    mii.dwTypeData =
        (LPWSTR)data->text;


    mii.hSubMenu =
        hSubMenu;


    mii.dwItemData =
        (ULONG_PTR)data;


    if (!InsertMenuItemW(
            hMenu,
            -1,
            TRUE,
            &mii))
    {
        FreeMenuItemData(
            data);

        DestroyMenu(
            hSubMenu);
    }
}


// ============================================================
// InsertSubMenu
// ============================================================

void NativeStartMenu::InsertSubMenu(
    HMENU hMenu,
    HMENU hSubMenu,
    UINT command,
    LPCWSTR text,
    HICON hIcon)
{
    if (!hMenu ||
        !hSubMenu)
    {
        return;
    }


    NativeMenuItemData* data =
        new NativeMenuItemData();


    data->text =
        _wcsdup(text);


    data->hIcon =
        hIcon;


    data->pidl =
        NULL;


    data->path =
        NULL;


    data->ownsText =
        true;


    data->hasSubMenu =
        true;


    data->ownerDraw =
        true;

    data->smallIcon =
        false;

    MENUITEMINFOW mii = {};


    mii.cbSize =
        sizeof(mii);


    mii.fMask =
        MIIM_ID |
        MIIM_STRING |
        MIIM_SUBMENU |
        MIIM_DATA |
        MIIM_FTYPE;

    mii.fType =
        MFT_OWNERDRAW;

    mii.wID =
        command;


    mii.dwTypeData =
        (LPWSTR)data->text;


    mii.hSubMenu =
        hSubMenu;


    mii.dwItemData =
        (ULONG_PTR)data;


    if (!InsertMenuItemW(
            hMenu,
            -1,
            TRUE,
            &mii))
    {
        FreeMenuItemData(
            data);

        DestroyMenu(
            hSubMenu);
    }
}


// ============================================================
// InsertCommand
// ============================================================

void NativeStartMenu::InsertCommand(
    HMENU hMenu,
    UINT command,
    LPCWSTR text)
{
    if (!hMenu)
        return;


    NativeMenuItemData* data =
        new NativeMenuItemData();


    data->text =
        _wcsdup(text);


    data->hIcon =
        GetCommandIcon(command);


    data->pidl =
        NULL;


    data->path =
        NULL;


    data->ownsText =
        true;


    data->hasSubMenu =
        false;


    data->ownerDraw =
        true;

    data->smallIcon =
        false;

    MENUITEMINFOW mii = {};


    mii.cbSize =
        sizeof(mii);


    mii.fMask =
        MIIM_ID |
        MIIM_STRING |
        MIIM_DATA |
        MIIM_FTYPE;

    mii.fType =
        MFT_OWNERDRAW;

    mii.wID =
        command;


    mii.dwTypeData =
        (LPWSTR)data->text;


    mii.dwItemData =
        (ULONG_PTR)data;


    if (!InsertMenuItemW(
            hMenu,
            -1,
            TRUE,
            &mii))
    {
        FreeMenuItemData(
            data);
    }
}


// ============================================================
// InsertShellFolder
// ============================================================

void NativeStartMenu::InsertShellFolder(
    HMENU hMenu,
    int csidl,
    LPCWSTR text)
{
    if (!hMenu)
        return;


    PIDLIST_ABSOLUTE pidl =
        NULL;


    if (FAILED(
        SHGetSpecialFolderLocation(
            _hwndOwner,
            csidl,
            &pidl)))
    {
        return;
    }


    SHFILEINFOW sfi = {};


    SHGetFileInfoW(
        (LPCWSTR)pidl,
        0,
        &sfi,
        sizeof(sfi),
        SHGFI_PIDL |
        SHGFI_ICON |
        SHGFI_SMALLICON);


    NativeMenuItemData* data =
        new NativeMenuItemData();


    data->text =
        _wcsdup(text);


    data->hIcon =
        sfi.hIcon;


    data->pidl =
        pidl;


    data->path =
        NULL;


    data->ownsText =
        true;


    data->hasSubMenu =
        false;


    data->ownerDraw =
        true;

    data->smallIcon =
        false;

    MENUITEMINFOW mii = {};


    mii.cbSize =
        sizeof(mii);


    mii.fMask =
        MIIM_ID |
        MIIM_STRING |
        MIIM_DATA |
        MIIM_FTYPE;

    mii.fType =
        MFT_OWNERDRAW;

    /*
     * Shell folder使用固定 ID范围。
     */
    static UINT nextShellId =
        60000;


    mii.wID =
        nextShellId++;


    if (nextShellId >= 60999)
        nextShellId = 60000;


    mii.dwTypeData =
        (LPWSTR)data->text;


    mii.dwItemData =
        (ULONG_PTR)data;


    if (!InsertMenuItemW(
            hMenu,
            -1,
            TRUE,
            &mii))
    {
        FreeMenuItemData(
            data);
    }
}


// ============================================================
// GetCommandIcon
// ============================================================

HICON NativeStartMenu::GetCommandIcon(
    UINT command)
{
    int resourceId = 0;

    switch (command)
    {
    case NATIVE_CMD_PROGRAMS:
        resourceId = 326;
        break;

    case NATIVE_CMD_SETTINGS:
        resourceId = 330;
        break;

    case NATIVE_CMD_RUN:
        resourceId = 328;
        break;

    case NATIVE_CMD_LOGOFF:
        resourceId = 325;
        break;

    case NATIVE_CMD_SHUTDOWN:
        resourceId = 329;
        break;

    case NATIVE_CMD_RESTART:
        resourceId = 329;
        break;

    case NATIVE_CMD_SLEEP:
        resourceId = 329;
        break;

    case NATIVE_CMD_HIBERNATE:
        resourceId = 329;
        break;

    default:
        return NULL;
    }

    WCHAR path[MAX_PATH] = {};

    if (!ExpandEnvironmentStringsW(
        L"%SystemRoot%\\System32\\shell32.dll",
        path,
        MAX_PATH))
    {
        return NULL;
    }

    HMODULE hShell32 = LoadLibraryExW(
        path,
        NULL,
        LOAD_LIBRARY_AS_DATAFILE);

    if (!hShell32)
        return NULL;

    HICON hIcon = (HICON)LoadImageW(
        hShell32,
        MAKEINTRESOURCEW(resourceId),
        IMAGE_ICON,
        16,
        16,
        LR_DEFAULTCOLOR);

    FreeLibrary(hShell32);

    return hIcon;
}

// ============================================================
// ExecuteProgram
// ============================================================

void NativeStartMenu::ExecuteProgram(
    LPCWSTR path)
{
    if (!path || !*path)
        return;


    SHELLEXECUTEINFOW sei = {};


    sei.cbSize =
        sizeof(sei);


    sei.fMask =
        SEE_MASK_FLAG_NO_UI;


    sei.hwnd =
        _hwndOwner;


    sei.lpVerb =
        L"open";


    sei.lpFile =
        path;


    sei.nShow =
        SW_SHOWNORMAL;


    ShellExecuteExW(
        &sei);
}


// ============================================================
// ExecutePIDL
// ============================================================

void NativeStartMenu::ExecutePIDL(
    PIDLIST_ABSOLUTE pidl)
{
    if (!pidl)
        return;


    SHELLEXECUTEINFOW sei = {};


    sei.cbSize =
        sizeof(sei);


    sei.fMask =
        SEE_MASK_IDLIST |
        SEE_MASK_FLAG_NO_UI;


    sei.hwnd =
        _hwndOwner;


    sei.lpIDList =
        pidl;


    sei.nShow =
        SW_SHOWNORMAL;


    ShellExecuteExW(
        &sei);
}


// ============================================================
// ExecuteCommand
// ============================================================

void NativeStartMenu::ExecuteCommand(
    UINT command)
{
    /*
     * 先在当前主菜单和子菜单中查找
     * 对应的 NativeMenuItemData。
     */
    if (_hMenu)
    {
        std::vector<HMENU> menus;

        menus.push_back(
            _hMenu);


        while (!menus.empty())
        {
            HMENU menu =
                menus.back();

            menus.pop_back();


            int count =
                GetMenuItemCount(menu);


            for (int i = 0;
                 i < count;
                 ++i)
            {
                MENUITEMINFOW mii = {};

                mii.cbSize =
                    sizeof(mii);


                mii.fMask =
                    MIIM_ID |
                    MIIM_DATA |
                    MIIM_SUBMENU;


                if (!GetMenuItemInfoW(
                        menu,
                        i,
                        TRUE,
                        &mii))
                {
                    continue;
                }


                if (mii.wID ==
                    command &&
                    mii.dwItemData)
                {
                    NativeMenuItemData* data =
                        (NativeMenuItemData*)
                        mii.dwItemData;


                    if (data->path)
                    {
                        ExecuteProgram(
                            data->path);

                        return;
                    }


                    if (data->pidl)
                    {
                        ExecutePIDL(
                            data->pidl);

                        return;
                    }
                }


                if (mii.hSubMenu)
                {
                    menus.push_back(
                        mii.hSubMenu);
                }
            }
        }
    }


    /*
     * 固定命令。
     */
    switch (command)
    {
    case NATIVE_CMD_SEARCH:
    {
        ShellExecuteW(
            _hwndOwner,
            L"open",
            L"search-ms:",
            NULL,
            NULL,
            SW_SHOWNORMAL);

        break;
    }


    case NATIVE_CMD_RUN:
    
        Run();
        break;


    case NATIVE_CMD_LOGOFF:
        Logoff();
        break;


    case NATIVE_CMD_SHUTDOWN:
        ExitWindowsEx(
            EWX_SHUTDOWN |
            EWX_POWEROFF,
            0);
        break;

    case NATIVE_CMD_RESTART:
        ExitWindowsEx(
            EWX_REBOOT,
            0);
        break;

    case NATIVE_CMD_SLEEP:
        SetSuspendState(
            FALSE,
            FALSE,
            FALSE);
        break;

    case NATIVE_CMD_HIBERNATE:
        SetSuspendState(
            TRUE,
            FALSE,
            FALSE);
        break;


    default:
        break;
    }
}


// ============================================================
// DrawMenuItem
// ============================================================

void NativeStartMenu::DrawMenuItem(
    DRAWITEMSTRUCT* dis)
{
    if (!dis)
        return;


    NativeMenuItemData* data =
        (NativeMenuItemData*)
        dis->itemData;


    if (!data)
        return;


    HDC hdc =
        dis->hDC;


    RECT rc =
        dis->rcItem;


    bool selected =
        (dis->itemState &
         ODS_SELECTED) != 0;


    COLORREF bkColor =
        GetSysColor(
            selected
                ? COLOR_HIGHLIGHT
                : COLOR_MENU);


    COLORREF textColor =
        GetSysColor(
            selected
                ? COLOR_HIGHLIGHTTEXT
                : COLOR_MENUTEXT);


    HBRUSH brush =
        CreateSolidBrush(
            bkColor);


    if (brush)
    {
        FillRect(
            hdc,
            &rc,
            brush);

        DeleteObject(
            brush);
    }


    /*
     * 图标
     */
    const int iconSize =
        data->smallIcon
        ? 16
        : 20;


    int iconX =
        rc.left + 8;


    int iconY =
        rc.top +
        ((rc.bottom -
          rc.top -
          iconSize) / 2);


    if (data->hIcon)
    {
        DrawIconEx(
            hdc,
            iconX,
            iconY,
            data->hIcon,
            iconSize,
            iconSize,
            0,
            NULL,
            DI_NORMAL);
    }


    /*
    * 文字
    */
    RECT textRect =
        rc;

    textRect.left +=
        data->smallIcon
        ? 32
        : 36;

    if (data->hasSubMenu)
    {
        textRect.right -= 24;
    }
    else
    {
        textRect.right -= 8;
    }

    SetBkMode(
        hdc,
        TRANSPARENT);

    SetTextColor(
        hdc,
        textColor);

    /*
     * 内层菜单使用稍小的字体。
     *
     * 外层：
     * 使用系统当前菜单字体。
     *
     * 内层：
     * 在当前字体基础上缩小。
     */
    HFONT oldFont =
        (HFONT)GetCurrentObject(
            hdc,
            OBJ_FONT);

    HFONT smallFont =
        NULL;

    if (data->smallIcon &&
        oldFont)
    {
        LOGFONTW lf = {};

        if (GetObjectW(
            oldFont,
            sizeof(lf),
            &lf))
        {
            if (lf.lfHeight > 0)
            {
                lf.lfHeight =
                    -MulDiv(
                        lf.lfHeight,
                        90,
                        100);
            }
            else
            {
                lf.lfHeight =
                    MulDiv(
                        lf.lfHeight,
                        90,
                        100);
            }

            smallFont =
                CreateFontIndirectW(
                    &lf);

            if (smallFont)
            {
                SelectObject(
                    hdc,
                    smallFont);
            }
        }
    }

    DrawTextW(
        hdc,
        data->text,
        -1,
        &textRect,
        DT_SINGLELINE |
        DT_VCENTER |
        DT_LEFT |
        DT_NOPREFIX);

    /*
     * 恢复原来的字体。
     */
    if (smallFont)
    {
        SelectObject(
            hdc,
            oldFont);

        DeleteObject(
            smallFont);
    }

    /*
     * 焦点
     */
    if (dis->itemState &
        ODS_FOCUS)
    {
        DrawFocusRect(
            hdc,
            &rc);
    }
}


// ============================================================
// MeasureMenuItem
// ============================================================

void NativeStartMenu::MeasureMenuItem(
    MEASUREITEMSTRUCT* mis)
{
    if (!mis)
        return;

    NativeMenuItemData* data =
        (NativeMenuItemData*)mis->itemData;

    if (!data || !data->text)
    {
        mis->itemWidth = 0;
        mis->itemHeight =
            NATIVE_STARTMENU_ITEM_HEIGHT;
        return;
    }

    HDC hdc =
        GetDC(NULL);

    if (!hdc)
    {
        mis->itemWidth = 0;
        if (data->smallIcon)
        {
            mis->itemHeight =
                24;
        }
        else
        {
            mis->itemHeight =
                NATIVE_STARTMENU_ITEM_HEIGHT;
        }
        return;
    }
    
    SIZE size = {};

    GetTextExtentPoint32W(
        hdc,
        data->text,
        lstrlenW(data->text),
        &size);

    ReleaseDC(
        NULL,
        hdc);

    /*
     * 外层菜单：
     *
     * 8  左边距
     * 20 图标
     * 8  图标与文字间距
     * 12 右边距
     */
    if (!data->smallIcon)
    {
        int width =
            8 + 20 + 8;

        width +=
            size.cx;

        width +=
            12;

        if (data->hasSubMenu)
        {
            width +=
                16;
        }

        mis->itemWidth =
            width;
    }
    else
    {
        /*
         * 内层菜单：
         *
         * WinME 风格更紧凑。
         *
         * 8  左边距
         * 16 小图标
         * 8  图标与文字间距
         * 8  右边距
         */
        int width =
            8 + 16 + 8;

        /*
         * DrawMenuItem() 中内层字体
         * 是当前字体的 90%。
         *
         * 这里也按 90% 计算文字宽度，
         * 避免“实际文字变小了，
         * 菜单宽度却仍按大字体计算”。
         */
        size.cx =
            MulDiv(
                size.cx,
                90,
                100);

        width +=
            size.cx;

        width +=
            8;

        /*
         * 内层有子菜单时，
         * 给箭头留较小空间。
         */
        if (data->hasSubMenu)
        {
            width +=
                12;
        }

        mis->itemWidth =
            width;
    }

    if (data->smallIcon)
    {
        mis->itemHeight =
            24;
    }
    else
    {
        mis->itemHeight =
            NATIVE_STARTMENU_ITEM_HEIGHT;
    }
}

// ============================================================
// ShowContextMenu
// ============================================================

void NativeStartMenu::ShowContextMenu(
    HMENU hMenu,
    UINT itemIndex)
{
    /*
     * WM_MENURBUTTONUP:
     *
     * wParam = 菜单项的零基索引
     * lParam = 当前 HMENU
     */
    MENUITEMINFOW mii = {};

    mii.cbSize =
        sizeof(mii);

    mii.fMask =
        MIIM_DATA |
        MIIM_SUBMENU;

    if (!GetMenuItemInfoW(
        hMenu,
        itemIndex,
        TRUE,
        &mii))
    {
        return;
    }

    /*
     * 有子菜单的项目不处理。
     *
     * 例如：
     *
     * Programs
     * Accessories >
     */
    if (mii.hSubMenu)
    {
        return;
    }

    NativeMenuItemData* data =
        (NativeMenuItemData*)
        mii.dwItemData;

    if (!data)
        return;

    /*
     * 当前菜单项必须对应一个 Shell 对象。
     */
    if (!data->path &&
        !data->pidl)
    {
        return;
    }

    /*
     * --------------------------------------------------------
     * 得到完整 PIDL
     * --------------------------------------------------------
     */

    PIDLIST_ABSOLUTE pidl =
        NULL;

    if (data->path)
    {
        /*
         * Programs 中的 .lnk / .exe
         */
        HRESULT hr =
            SHParseDisplayName(
                data->path,
                NULL,
                &pidl,
                0,
                NULL);

        if (FAILED(hr) ||
            !pidl)
        {
            return;
        }
    }
    else
    {
        /*
         * Settings 中的 Shell Folder
         */
        pidl =
            ILClone(
                data->pidl);

        if (!pidl)
        {
            return;
        }
    }

    /*
     * --------------------------------------------------------
     * 找到父 Shell Folder
     * --------------------------------------------------------
     */

    IShellFolder* pParentFolder =
        NULL;

    LPCITEMIDLIST pidlChild =
        NULL;

    HRESULT hr =
        SHBindToParent(
            pidl,
            IID_IShellFolder,
            (void**)&pParentFolder,
            &pidlChild);

    if (FAILED(hr) ||
        !pParentFolder ||
        !pidlChild)
    {
        CoTaskMemFree(
            pidl);

        return;
    }

    /*
     * --------------------------------------------------------
     * 获取 Windows 原生 IContextMenu
     * --------------------------------------------------------
     */

    IContextMenu* pContextMenu =
        NULL;

    hr =
        pParentFolder->GetUIObjectOf(
            _hwndMenu,
            1,
            &pidlChild,
            IID_IContextMenu,
            NULL,
            (void**)&pContextMenu);

    pParentFolder->Release();

    if (FAILED(hr) ||
        !pContextMenu)
    {
        CoTaskMemFree(
            pidl);

        return;
    }

    /*
     * --------------------------------------------------------
     * 创建 Shell 原生菜单
     * --------------------------------------------------------
     */

    HMENU hContextMenu =
        CreatePopupMenu();

    if (!hContextMenu)
    {
        pContextMenu->Release();

        CoTaskMemFree(
            pidl);

        return;
    }

    hr =
        pContextMenu->QueryContextMenu(
            hContextMenu,
            0,
            1,
            0x7FFF,
            CMF_NORMAL);

    if (FAILED(hr))
    {
        DestroyMenu(
            hContextMenu);

        pContextMenu->Release();

        CoTaskMemFree(
            pidl);

        return;
    }

    /*
     * --------------------------------------------------------
     * 鼠标当前位置
     * --------------------------------------------------------
     */

    POINT pt = {};

    GetCursorPos(
        &pt);

    /*
     * --------------------------------------------------------
     * 在当前 Start Menu 正处于菜单跟踪时，
     * 嵌套弹出 Shell Context Menu。
     *
     * TPM_RECURSE 是关键。
     * --------------------------------------------------------
     */

    int command =
        TrackPopupMenuEx(
            hContextMenu,
            TPM_RETURNCMD |
            TPM_RIGHTBUTTON |
            TPM_LEFTALIGN |
            TPM_TOPALIGN |
            TPM_RECURSE,
            pt.x,
            pt.y,
            _hwndMenu,
            NULL);

    /*
     * --------------------------------------------------------
     * 执行 Shell 命令
     * --------------------------------------------------------
     */

    if (command)
    {
        CMINVOKECOMMANDINFO ici = {};

        ici.cbSize =
            sizeof(ici);

        ici.hwnd =
            _hwndMenu;

        ici.lpVerb =
            MAKEINTRESOURCEA(
                command - 1);

        ici.nShow =
            SW_SHOWNORMAL;

        pContextMenu->InvokeCommand(
            &ici);
    }

    /*
     * --------------------------------------------------------
     * 清理
     * --------------------------------------------------------
     */

    DestroyMenu(
        hContextMenu);

    pContextMenu->Release();

    CoTaskMemFree(
        pidl);
}


// ============================================================
// FreeMenuItemData
// ============================================================

void NativeStartMenu::FreeMenuItemData(
    NativeMenuItemData* data)
{
    if (!data)
        return;


    if (data->hIcon)
    {
        DestroyIcon(
            data->hIcon);

        data->hIcon =
            NULL;
    }


    if (data->pidl)
    {
        CoTaskMemFree(
            data->pidl);

        data->pidl =
            NULL;
    }


    if (data->ownsText &&
        data->text)
    {
        free(
            (void*)data->text);

        data->text =
            NULL;
    }


    if (data->path)
    {
        free(
            (void*)data->path);

        data->path =
            NULL;
    }


    delete data;
}


// ============================================================
// WndProc
// ============================================================

LRESULT NativeStartMenu::WndProc(
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_MEASUREITEM:
    {
        MeasureMenuItem(
            (MEASUREITEMSTRUCT*)lParam);

        return TRUE;
    }


    case WM_DRAWITEM:
    {
        DrawMenuItem(
            (DRAWITEMSTRUCT*)lParam);

        return TRUE;
    }


    case WM_MENURBUTTONUP:
    {
        ShowContextMenu(
            (HMENU)lParam,
            (UINT)wParam);

        return 0;
    }

    case WM_COMMAND:
    {
        UINT command =
            LOWORD(wParam);


        ExecuteCommand(
            command);

        return 0;
    }


    case WM_KILLFOCUS:
    {
        /*
         * 真正菜单由 TrackPopupMenuEx
         * 控制，这里暂时不主动关闭。
         */
        break;
    }


    case WM_DESTROY:
    {
        _visible =
            false;

        break;
    }
    }


    return DefWindowProcW(
        _hwndMenu,
        uMsg,
        wParam,
        lParam);
}


// ============================================================
// WindowProc
// ============================================================

LRESULT CALLBACK
NativeStartMenu::WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    NativeStartMenu* menu =
        (NativeStartMenu*)
        GetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA);


    if (uMsg ==
        WM_NCCREATE)
    {
        CREATESTRUCTW* cs =
            (CREATESTRUCTW*)lParam;


        menu =
            (NativeStartMenu*)
            cs->lpCreateParams;


        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            (LONG_PTR)menu);


        if (menu)
        {
            menu->_hwndMenu =
                hwnd;
        }
    }


    if (menu)
    {
        return menu->WndProc(
            uMsg,
            wParam,
            lParam);
    }


    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam);
}