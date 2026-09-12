#pragma once

#include <windows.h>

class CImeIndicator
{
public:
    CImeIndicator();

    bool Initialize(HWND hParent);
    void Update();

    

    int GetWidth() const;

    void Paint(HDC hdc, const RECT& rc);

    bool OnMouseMove(POINT pt);
    bool OnLButtonDown(POINT pt);

    void OnInputLangChange();
    void Check();

private:
    HWND m_hParent;
    HKL  m_hkl;
    WCHAR m_text[16];
    int  m_width;

    bool HitTest(POINT pt, const RECT& rc) const;
    void UpdateText();
};