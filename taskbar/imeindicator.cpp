#include <precomp.h>
#include "imeindicator.h"
#include <windows.h>
#include <imm.h>
#include <string>

#pragma comment(lib, "imm32.lib")

CImeIndicator::CImeIndicator()
    : m_hParent(nullptr),
      m_hkl(nullptr),
      m_width(32)
{
    m_text[0] = L'\0';
}

bool CImeIndicator::Initialize(HWND hParent)
{
    m_hParent = hParent;

    Update();

    return true;
}

void CImeIndicator::Update()
{
    m_hkl = GetKeyboardLayout(0);

    UpdateText();
}

void CImeIndicator::OnInputLangChange()
{
    Update();
}

void CImeIndicator::Check()
{
    HWND hwnd = GetForegroundWindow();

    if (!hwnd)
        return;

    DWORD threadId = GetWindowThreadProcessId(hwnd, NULL);

    if (!threadId)
        return;

    HKL hkl = GetKeyboardLayout(threadId);

    if (hkl != m_hkl)
    {
        m_hkl = hkl;
        UpdateText();

        if (m_hParent)
            InvalidateRect(m_hParent, NULL, FALSE);
    }
}

void CImeIndicator::UpdateText()
{
    LANGID langid = LOWORD(m_hkl);

    WCHAR lang[LOCALE_NAME_MAX_LENGTH] = {};

    if (LCIDToLocaleName(
            MAKELCID(langid, SORT_DEFAULT),
            lang,
            ARRAYSIZE(lang),
            0))
    {
        if (PRIMARYLANGID(langid) == LANG_CHINESE)
        {
            lstrcpyW(m_text, L"CN");

        }
        else if (PRIMARYLANGID(langid) == LANG_ENGLISH)
        {
            lstrcpyW(m_text, L"EN");
        }
        else if (PRIMARYLANGID(langid) == LANG_JAPANESE)
        {
            lstrcpyW(m_text, L"JP");
        }
        else if (PRIMARYLANGID(langid) == LANG_KOREAN)
        {
            lstrcpyW(m_text, L"KO");
        }
        else
        {
            WCHAR shortName[16] = {};

            GetLocaleInfoEx(
                lang,
                LOCALE_SISO639LANGNAME,
                shortName,
                ARRAYSIZE(shortName));

            if (shortName[0])
            {
                CharUpperW(shortName);
                //wcscpy_s(m_text, shortName);
                lstrcpyW(m_text, shortName);
            }
            else
            {
                lstrcpyW(m_text, L"??");
            }
        }
    }
    else
    {
        lstrcpyW(m_text, L"??");
    }
}

int CImeIndicator::GetWidth() const
{
    return m_width;
}

void CImeIndicator::Paint(HDC hdc, const RECT& rc)
{
    RECT r = rc;

    HBRUSH hBrush = GetSysColorBrush(COLOR_BTNFACE);

    FillRect(hdc, &r, hBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HFONT oldFont =
        (HFONT)SelectObject(hdc, hFont);

    DrawTextW(
        hdc,
        m_text,
        -1,
        &r,
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE);

    SelectObject(hdc, oldFont);
}

bool CImeIndicator::HitTest(
    POINT pt,
    const RECT& rc) const
{
    return PtInRect(&rc, pt) != FALSE;
}

bool CImeIndicator::OnMouseMove(POINT pt)
{
    return false;
}

bool CImeIndicator::OnLButtonDown(POINT pt)
{
    /*
     * 第一版暂时不自己切换输入法。
     *
     * ctfmon / Windows 本身负责输入法切换。
     *
     * 后续如果需要，可以在这里加入
     * ActivateKeyboardLayout().
     */

    return false;
}