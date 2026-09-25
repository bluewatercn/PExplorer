#include "precomp.h"

#include "LuaEngine.h"


EXTERN_C {
        int lua_desktop_call(lua_State* L, const char *funcname, int top, int base) {
        int ret = 0;
        Token v = { TOK_UNSET };
        char strPath[MAX_PATH + 1] = { 0 };

        std::string func = funcname;
        if (func == "desktop::getpath") {
            SHGetSpecialFolderPathA(0, strPath, CSIDL_DESKTOPDIRECTORY, FALSE);
            lua_pushstring(L, strPath);
            ret++;
        } else if (func == "desktop::updatewallpaper") {
            SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, NULL, SPIF_SENDWININICHANGE | SPIF_UPDATEINIFILE);
        } else if (func == "desktop::getwallpaper") {
            TCHAR wpPath[MAX_PATH] = { 0 };
            SystemParametersInfo(SPI_GETDESKWALLPAPER, MAX_PATH, wpPath, 0);
            v.str = wpPath;
            PUSH_STR(v);
        } else if (func == "desktop::setwallpaper") {
            v.str = s2w(lua_tostring(L, base + 2));
            SHSetValue(HKEY_CURRENT_USER, TEXT("Control Panel\\Desktop"), TEXT("Wallpaper"), REG_SZ, v.str.c_str(), (DWORD)(v.str.length()) * sizeof(TCHAR));
            SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, NULL, SPIF_SENDWININICHANGE | SPIF_UPDATEINIFILE);
        }
        return ret;
    }
}
