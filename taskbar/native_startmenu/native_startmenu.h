#pragma once

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <vector>

#define NATIVE_STARTMENU_WIDTH          160
#define NATIVE_STARTMENU_ITEM_HEIGHT    32
#define NATIVE_STARTMENU_MARGIN         4

#define NATIVE_CMD_PROGRAMS             50001
#define NATIVE_CMD_SETTINGS             50002
#define NATIVE_CMD_DOCUMENTS            50003
#define NATIVE_CMD_PICTURES             50004
#define NATIVE_CMD_COMPUTER             50005
#define NATIVE_CMD_NETWORK              50006
#define NATIVE_CMD_SEARCH               50007
#define NATIVE_CMD_RUN                  50008
#define NATIVE_CMD_LOGOFF               50009
#define NATIVE_CMD_SHUTDOWN             50010
#define NATIVE_CMD_RESTART              50011
#define NATIVE_CMD_SLEEP                50012
#define NATIVE_CMD_HIBERNATE            50013

struct NativeMenuItemData
{
    LPCWSTR text;

    HICON hIcon;

    PIDLIST_ABSOLUTE pidl;

    LPCWSTR path;

    bool ownsText;

    bool hasSubMenu;

    bool ownerDraw;

    bool smallIcon;
};


struct NativeProgramEntry
{
    WCHAR name[MAX_PATH];

    WCHAR path[MAX_PATH];

    bool directory;

    std::vector<NativeProgramEntry> children;
};


class NativeStartMenu
{
public:

    NativeStartMenu(
        HWND hwndOwner,
        HWND hwndStartButton);

    ~NativeStartMenu();


    bool Create(
        HWND hwndOwner);


    void SetStartButton(
        HWND hwndStartButton);


    void Toggle();


    void Show();


    void Hide();


    bool IsVisible() const;
    bool ConsumeStartClick();

    static void Run();
    static void Logoff();

    LRESULT WndProc(
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);


private:

    HWND _hwndOwner;

    HWND _hwndStartButton;

    HWND _hwndMenu;


    bool _visible;
    bool _ignoreNextStartClick;

    HMENU _hMenu;


    void BuildMenu();


    void BuildMainMenu(
        HMENU hMenu);


    void BuildProgramsMenu(
        HMENU hMenu);


    bool CollectProgramsDirectory(
        LPCWSTR path,
        std::vector<NativeProgramEntry>& entries);


    void MergeProgramEntries(
        std::vector<NativeProgramEntry>& target,
        std::vector<NativeProgramEntry>& source);


    void SortProgramEntries(
        std::vector<NativeProgramEntry>& entries);


    void BuildProgramMenuEntries(
        HMENU hMenu,
        const std::vector<NativeProgramEntry>& entries);


    void AddProgramsItem(
        HMENU hMenu,
        LPCWSTR path,
        LPCWSTR name);


    void InsertProgramSubMenu(
        HMENU hMenu,
        HMENU hSubMenu,
        LPCWSTR text,
        HICON hIcon);


    void InsertSubMenu(
        HMENU hMenu,
        HMENU hSubMenu,
        UINT command,
        LPCWSTR text,
        HICON hIcon);


    void InsertCommand(
        HMENU hMenu,
        UINT command,
        LPCWSTR text);


    void InsertShellFolder(
        HMENU hMenu,
        int csidl,
        LPCWSTR text);


    void ExecuteProgram(
        LPCWSTR path);


    void ExecutePIDL(
        PIDLIST_ABSOLUTE pidl);


    void ExecuteCommand(
        UINT command);


    HICON GetCommandIcon(
        UINT command);

    void DrawMenuItem(
        DRAWITEMSTRUCT* dis);


    void MeasureMenuItem(
        MEASUREITEMSTRUCT* mis);


    void FreeMenuItemData(
        NativeMenuItemData* data);

    void ShowContextMenu(
        HMENU hMenu,
        UINT itemId);

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);


    static int CompareProgramEntry(
        const void* a,
        const void* b);
};