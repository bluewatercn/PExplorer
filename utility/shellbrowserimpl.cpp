/*
 * Copyright 2003 Martin Fuchs
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


//
// Explorer clone
//
// shellbrowserimpl.cpp
//
// Martin Fuchs, 28.09.2003
//
// Credits: Thanks to Leon Finker for his explorer cabinet window example
//


#include <precomp.h>


HRESULT IShellBrowserImpl::QueryInterface(REFIID iid, void **ppvObject)
{
    if (!ppvObject)
        return E_POINTER;

    if (iid == IID_IUnknown)
        *ppvObject = (IUnknown *)static_cast<IShellBrowser *>(this);
    else if (iid == IID_IOleWindow)
        *ppvObject = static_cast<IOleWindow *>(this);
    else if (iid == IID_IShellBrowser)
        *ppvObject = static_cast<IShellBrowser *>(this);
    else if (iid == IID_ICommDlgBrowser)
        *ppvObject = static_cast<ICommDlgBrowser *>(this);
    else if (iid == IID_IServiceProvider)
        *ppvObject = static_cast<IServiceProvider *>(this);
    else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    return S_OK;
}

HRESULT IShellBrowserImpl::QueryService(REFGUID guidService, REFIID riid, void **ppvObject)
{
    if (!ppvObject)
        return E_POINTER;

    ///@todo use guidService

    if (riid == IID_IUnknown)
        *ppvObject = (IUnknown *)static_cast<IShellBrowser *>(this);
    else if (riid == IID_IOleWindow)
        *ppvObject = static_cast<IOleWindow *>(this);
    else if (riid == IID_IShellBrowser)
        *ppvObject = static_cast<IShellBrowser *>(this);
    else if (riid == IID_ICommDlgBrowser)
        *ppvObject = static_cast<ICommDlgBrowser *>(this);
    else if (riid == IID_IServiceProvider)
        *ppvObject = static_cast<IServiceProvider *>(this);
    else if (riid == IID_IOleCommandTarget)
        *ppvObject = static_cast<IOleCommandTarget *>(this);
    else {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    return S_OK;
}

HRESULT IShellBrowserImpl::QueryStatus(const GUID *pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT *pCmdText)
{
    return E_FAIL;  ///@todo implement IOleCommandTarget
}

HRESULT IShellBrowserImpl::Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    return E_FAIL;  ///@todo implement IOleCommandTarget
}


// process default command: look for folders and traverse into them
HRESULT IShellBrowserImpl::OnDefaultCommand(IShellView *ppshv)
{
    IDataObject *selection;

    HRESULT hr = ppshv->GetItemObject(SVGIO_SELECTION, IID_IDataObject, (void **)&selection);
    if (FAILED(hr))
        return hr;

    PIDList pidList;

    hr = pidList.GetData(selection);
    if (FAILED(hr)) {
        selection->Release();
        return hr;
    }

    hr = OnDefaultCommand(pidList);

    selection->Release();

    return hr;
}

HRESULT IShellBrowserImpl::GetViewStateStream(
    DWORD grfMode,
    LPSTREAM* ppStrm)
{
    if (!ppStrm)
        return E_POINTER;

    *ppStrm = NULL;

    /*
     * STGM_READ      = 0x00000000
     * STGM_WRITE     = 0x00000001
     * STGM_READWRITE = 0x00000002
     *
     * 老项目中可能没有 STGM_ACCESS_MASK，
     * 这里直接取低两位。
     */
    DWORD accessMode =
        grfMode & 0x03;


    /*
     * DEBUG
     */
    TCHAR buf[512];

    wsprintf(
        buf,
        TEXT("GetViewStateStream\r\n")
        TEXT("====================\r\n\r\n")
        TEXT("grfMode        = 0x%08X\r\n")
        TEXT("accessMode     = 0x%08X\r\n\r\n")
        TEXT("STGM_READ      = 0x%08X\r\n")
        TEXT("STGM_WRITE     = 0x%08X\r\n")
        TEXT("STGM_READWRITE = 0x%08X"),
        grfMode,
        accessMode,
        STGM_READ,
        STGM_WRITE,
        STGM_READWRITE
    );

    MessageBox(
        NULL,
        buf,
        TEXT("GetViewStateStream"),
        MB_OK
    );


    const TCHAR* filename =
        TEXT("desktop_viewstate.dat");


    /*
     * =====================================================
     * WRITE
     * =====================================================
     */
    if (accessMode == STGM_WRITE)
    {
        /*
         * 如果之前存在旧 Stream，
         * 先释放。
         */
        if (_viewStateStream)
        {
            _viewStateStream->Release();
            _viewStateStream = NULL;
        }


        /*
         * 创建一个空的 HGLOBAL Stream。
         *
         * 后面由 Shell 自己调用
         * IStream::Write() 写入 View State。
         */
        HRESULT hr =
            CreateStreamOnHGlobal(
                NULL,
                TRUE,
                &_viewStateStream
            );

        if (FAILED(hr))
            return hr;


        /*
         * _viewStateStream：
         *
         * 一个引用由这里保存。
         *
         * 另一个引用交给 Shell。
         */
        _viewStateStream->AddRef();

        *ppStrm =
            _viewStateStream;

        return S_OK;
    }


    /*
     * =====================================================
     * READ
     * =====================================================
     *
     * 注意：
     *
     * STGM_READ == 0
     *
     * 所以不能写：
     *
     *     if (grfMode & STGM_READ)
     *
     * 必须通过 accessMode 判断。
     */
    if (accessMode == STGM_READ)
    {
        HANDLE hFile =
            CreateFile(
                filename,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

        if (hFile == INVALID_HANDLE_VALUE)
        {
            return HRESULT_FROM_WIN32(
                GetLastError()
            );
        }


        DWORD size =
            GetFileSize(
                hFile,
                NULL
            );

        if (size == INVALID_FILE_SIZE ||
            size == 0)
        {
            CloseHandle(hFile);
            return E_FAIL;
        }


        /*
         * 分配 HGLOBAL。
         */
        HGLOBAL hGlobal =
            GlobalAlloc(
                GMEM_MOVEABLE,
                size
            );

        if (!hGlobal)
        {
            CloseHandle(hFile);
            return E_OUTOFMEMORY;
        }


        /*
         * 锁定 HGLOBAL。
         */
        void* buffer =
            GlobalLock(hGlobal);

        if (!buffer)
        {
            GlobalFree(hGlobal);
            CloseHandle(hFile);
            return E_FAIL;
        }


        /*
         * 读取 desktop_viewstate.dat。
         */
        DWORD readSize = 0;

        BOOL ok =
            ReadFile(
                hFile,
                buffer,
                size,
                &readSize,
                NULL
            );


        GlobalUnlock(hGlobal);

        CloseHandle(hFile);


        if (!ok ||
            readSize != size)
        {
            GlobalFree(hGlobal);
            return E_FAIL;
        }


        /*
         * 释放旧 Stream。
         */
        if (_viewStateStream)
        {
            _viewStateStream->Release();
            _viewStateStream = NULL;
        }


        /*
         * 文件内容转换成 IStream。
         *
         * TRUE：
         * Stream 释放时自动释放 hGlobal。
         */
        HRESULT hr =
            CreateStreamOnHGlobal(
                hGlobal,
                TRUE,
                &_viewStateStream
            );

        if (FAILED(hr))
        {
            GlobalFree(hGlobal);
            return hr;
        }


        /*
         * 返回给 Shell。
         */
        _viewStateStream->AddRef();

        *ppStrm =
            _viewStateStream;

        return S_OK;
    }


    /*
     * =====================================================
     * READWRITE
     * =====================================================
     */
    if (accessMode == STGM_READWRITE)
    {
        return E_NOTIMPL;
    }


    return E_INVALIDARG;
}

HRESULT IShellBrowserImpl::SaveViewStateStream()
{
    if (!_viewStateStream)
        return E_FAIL;

    LARGE_INTEGER zero;
    zero.QuadPart = 0;

    /*
     * 回到 Stream 开头
     */
    HRESULT hr =
        _viewStateStream->Seek(
            zero,
            STREAM_SEEK_SET,
            NULL
        );

    if (FAILED(hr))
        return hr;

    STATSTG stat;

    hr =
        _viewStateStream->Stat(
            &stat,
            STATFLAG_NONAME
        );

    if (FAILED(hr))
        return hr;

    if (stat.cbSize.HighPart != 0)
        return E_FAIL;

    ULONG size =
        (ULONG)stat.cbSize.LowPart;

    if (size == 0)
        return S_FALSE;

    BYTE* buffer =
        new BYTE[size];

    ULONG readSize = 0;

    hr =
        _viewStateStream->Read(
            buffer,
            size,
            &readSize
        );

    if (FAILED(hr) ||
        readSize != size)
    {
        delete[] buffer;
        return E_FAIL;
    }

    HANDLE hFile = 
        CreateFile(
            TEXT("desktop_viewstate.dat"),
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        delete[] buffer;
        return HRESULT_FROM_WIN32(
            GetLastError()
        );
    }

    DWORD written = 0;

    BOOL ok =
        WriteFile(
            hFile,
            buffer,
            size,
            &written,
            NULL
        );

    CloseHandle(hFile);

    delete[] buffer;

    if (!ok || written != size)
        return E_FAIL;

    return S_OK;
}