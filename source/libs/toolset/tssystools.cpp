#include "toolset.h"

namespace ts
{
__declspec(thread) DWORD Time::threadCurrentTime = 0;

void	TSCALL hide_hardware_cursor(void)
{
    while (ShowCursor(false) >= 0)
    {
    }
}

void	TSCALL show_hardware_cursor(void)
{
    while (ShowCursor(true) < 0)
    {
    }
}

static BOOL CALLBACK calcmc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    int * cnt = (int *)dwData;
    *cnt = *cnt + 1;
    return TRUE;
}

int     TSCALL monitor_count(void)
{
    int cnt = 0;
    EnumDisplayMonitors(nullptr, nullptr, calcmc, (LPARAM)&cnt);
    return cnt;
}

static BOOL CALLBACK calcmrect_fs(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    int * mon = (int *)dwData;
    if (*mon == 0)
    {
        RECT * r = (RECT *)dwData;
        *r = *lprcMonitor;
        return FALSE;
    }
    *mon = *mon - 1;
    return TRUE;
}

void    TSCALL monitor_get_max_size_fs(RECT *r, int monitor)
{
    *(int *)r = monitor;
    EnumDisplayMonitors(nullptr, nullptr, calcmrect_fs, (LPARAM)r);
}

static BOOL CALLBACK calcmrect(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    int * mon = (int *)dwData;
    if (*mon == 0)
    {
        RECT * r = (RECT *)dwData;

        MONITORINFO mi;
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hMonitor, &mi);
        *r = mi.rcWork;

        return FALSE;
    }
    *mon = *mon - 1;
    return TRUE;
}

void    TSCALL monitor_get_max_size(RECT *r, int monitor)
{
    *(int *)r = monitor;
    EnumDisplayMonitors(nullptr, nullptr, calcmrect, (LPARAM)r);
}

ivec2   TSCALL wnd_get_center_pos( const ts::ivec2& size )
{
    ivec2 cp;
    GetCursorPos(&ref_cast<POINT>(cp));
    irect r = wnd_get_max_size(cp.x, cp.y);
    return r.center() - size/2;
}

void    TSCALL wnd_fix_rect(irect &r, int minw, int minh)
{
    int ww = r.width();
    int hh = r.height();
    if (ww < minw) ww = minw;
    if (hh < minh) hh = minh;

    int mc = monitor_count();

    ivec2 sdvig(65535, 65535);

    RECT min;
    min.left = 1000000;
    min.right = -1000000;
    min.top = 1000000;
    min.bottom = -1000000;
    RECT mr;
    for (int i = 0; i<mc; ++i)
    {
        monitor_get_max_size_fs(&mr, i);
        if (mr.left < min.left) min.left = mr.left;
        if (mr.right > min.right) min.right = mr.right;
        if (mr.top < min.top) min.top = mr.top;
        if (mr.bottom > min.bottom) min.bottom = mr.bottom;
    }

    int maxw = min.right - min.left;
    int maxh = min.bottom - min.top;
    if (ww > maxw) ww = maxw;
    if (hh > maxh) hh = maxh;

    for (int i = 0; i < mc; ++i)
    {
        monitor_get_max_size(&mr, i);
        ivec2 csdvig(0, 0);
        if (r.lt.y < mr.top) csdvig.y += (mr.top - r.lt.y);
        if ((r.lt.y + 20) > mr.bottom) csdvig.y -= (r.lt.y + 20) - mr.bottom;

        if ((r.lt.x + ww) < (mr.left + 100)) csdvig.x += ((mr.left + 100) - (r.lt.x + ww));
        if ((r.lt.x) > (mr.right - 50)) csdvig.x -= (r.lt.x - (mr.right - 50));

        uint a = csdvig.sqlen();
        uint b = sdvig.sqlen();

        if (a < b)
        {
            sdvig = csdvig;
        }
    }

    r.lt.x += sdvig.x;
    r.lt.y += sdvig.y;
    r.rb.x = r.lt.x + ww;
    r.rb.y = r.lt.y + hh;

}

irect    TSCALL wnd_get_max_size_fs(int x, int y)
{
    MONITORINFO mi;
    POINT p;
    p.x = x;
    p.y = y;
    HMONITOR m = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
    if (m)
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else
    {
        //SystemParametersInfo(SPI_GETWORKAREA,0,&mi.rcMonitor,0);
        mi.rcMonitor.left = 0;
        mi.rcMonitor.top = 0;
        mi.rcMonitor.right = GetSystemMetrics(SM_CXSCREEN);
        mi.rcMonitor.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    return ref_cast<irect>(mi.rcMonitor);
}


irect    TSCALL wnd_get_max_size(int x, int y)
{
    MONITORINFO mi;
    POINT p;
    p.x = x;
    p.y = y;
    HMONITOR m = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
    if (m)
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    return ref_cast<irect>(mi.rcWork);
}


irect    TSCALL wnd_get_max_size(const irect &rfrom)
{
    MONITORINFO mi;
    HMONITOR m = MonitorFromRect(&ref_cast<RECT>(rfrom), MONITOR_DEFAULTTONEAREST);
    if (m)
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    return ref_cast<irect>(mi.rcWork);
}

irect   TSCALL wnd_get_max_size_fs(const irect &rfrom)
{
    MONITORINFO mi;
    HMONITOR m = MonitorFromRect(&ref_cast<RECT>(rfrom), MONITOR_DEFAULTTONEAREST);
    if (m)
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else
    {
        //SystemParametersInfo(SPI_GETWORKAREA,0,&mi.rcMonitor,0);
        mi.rcMonitor.left = 0;
        mi.rcMonitor.top = 0;
        mi.rcMonitor.right = GetSystemMetrics(SM_CXSCREEN);
        mi.rcMonitor.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    return ref_cast<irect>(mi.rcMonitor);
}

irect   TSCALL wnd_get_max_size(HWND hwnd)
{
    //GetWindowRect(hwnd, r);
    //wnd_get_max_size(r, r);
    //return;

    MONITORINFO mi;
    HMONITOR m = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (m)
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    return ref_cast<irect>(mi.rcWork);
}


ivec2 TSCALL center_pos_by_window(HWND hwnd)
{
    RECT r;
    GetWindowRect(hwnd, &r);
    return wnd_get_max_size(ref_cast<irect>(r)).center();
}

void TSCALL set_clipboard_text(const wsptr &t)
{
    OpenClipboard(nullptr);
    EmptyClipboard();

    int len = t.l + 1;

    HANDLE text = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar));
    void *d = GlobalLock(text);
    memcpy(d, t.s, t.l * sizeof(wchar));
    *(((wchar *)d)+t.l) = 0;
    GlobalUnlock(text);

    SetClipboardData(CF_UNICODETEXT, text);
    CloseClipboard();
}

wstr_c TSCALL get_clipboard_text(void)
{
    wstr_c res;

    if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(nullptr))
    {
        HGLOBAL hg = GetClipboardData(CF_UNICODETEXT);
        if (hg)
        {
            const wchar *p = (const wchar*)GlobalLock(hg);
            res.set(sptr<wchar>(p));
            res.replace_all(wsptr(L"\r\n"), wsptr(L"\n"));
            GlobalUnlock(hg);
        }
        CloseClipboard();
    }

    return res;
}

void TSCALL set_clipboard_bitmap(const bitmap_c &bmp)
{
    OpenClipboard(nullptr);
    EmptyClipboard();

    int len = 0;

    HANDLE text = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar));
    /*void *d = */ GlobalLock(text);


    GlobalUnlock(text);

    SetClipboardData(CF_TIFF, text);
    CloseClipboard();
}

bitmap_c TSCALL get_clipboard_bitmap(void)
{
    bitmap_c res;

    OpenClipboard(nullptr);
    wstr_c fmtt;
    wstrings_c sa;
    UINT fmt = 0;
    UINT adobe = 0;

    for (; 0 != (fmt = EnumClipboardFormats(fmt));)
    {
        fmtt.set_length(2048);
        int l = GetClipboardFormatNameW(fmt, fmtt.str(), 2048);
        fmtt.set_length(l);
        if (l)
        {
            sa.add(fmtt);
            if (fmtt.find_pos(L"Adobe") >= 0)
            {
                adobe = fmt;
            }
        }
        else
        {
            switch (fmt)
            {
            case CF_BITMAP: sa.add(L"CF_BITMAP"); break;
            case CF_DIB: sa.add(L"CF_DIB"); break;
            case CF_DIBV5: sa.add(L"CF_DIBV5"); break;
            case CF_DIF: sa.add(L"CF_DIF"); break;
            case CF_DSPBITMAP: sa.add(L"CF_DSPBITMAP"); break;
            case CF_DSPENHMETAFILE: sa.add(L"CF_DSPENHMETAFILE"); break;
            case CF_DSPMETAFILEPICT: sa.add(L"CF_DSPMETAFILEPICT"); break;
            case CF_DSPTEXT: sa.add(L"CF_DSPTEXT"); break;
            case CF_ENHMETAFILE: sa.add(L"CF_ENHMETAFILE"); break;
            case CF_HDROP: sa.add(L"CF_HDROP"); break;
            case CF_LOCALE: sa.add(L"CF_LOCALE"); break;
            case CF_METAFILEPICT: sa.add(L"CF_METAFILEPICT"); break;
            case CF_OEMTEXT: sa.add(L"CF_OEMTEXT"); break;
            case CF_OWNERDISPLAY: sa.add(L"CF_OWNERDISPLAY"); break;
            case CF_PALETTE: sa.add(L"CF_PALETTE"); break;
            case CF_PENDATA: sa.add(L"CF_PENDATA"); break;
            case CF_RIFF: sa.add(L"CF_RIFF"); break;
            case CF_SYLK: sa.add(L"CF_SYLK"); break;
            case CF_TEXT: sa.add(L"CF_TEXT"); break;
            case CF_WAVE: sa.add(L"CF_WAVE"); break;
            case CF_TIFF: sa.add(L"CF_TIFF"); break;
            case CF_UNICODETEXT: sa.add(L"CF_UNICODETEXT"); break;
            default:
                if (fmt >= CF_PRIVATEFIRST && fmt <= CF_PRIVATELAST)
                {
                    sa.add(wstr_c(L"CF_PRIVATEFIRST #").append_as_int(fmt - CF_PRIVATEFIRST));
                }
                else if (fmt >= CF_GDIOBJFIRST && fmt <= CF_GDIOBJLAST)
                {
                    sa.add(wstr_c(L"CF_GDIOBJFIRST #").append_as_int(fmt - CF_GDIOBJFIRST));
                }
            }
        }
    }

    buf_c b;
    BITMAPV5HEADER hdr;

    if (IsClipboardFormatAvailable(adobe))
    {
        HGLOBAL hg = GetClipboardData(adobe);
        if (hg)
        {
            uint sz = sz_cast<uint>(GlobalSize(hg));
            byte *p = (byte*)GlobalLock(hg);
            b.append_buf(p, sz);
            memcpy(&hdr, p, sizeof(BITMAPV5HEADER));

            GlobalUnlock(hg);
        }
    }
    CloseClipboard();
    MessageBoxW(nullptr, sa.join(L"\r\n"), L"x", MB_OK);
    return res;
}


bool start_app( const wsptr &cmdline, HANDLE *hProcess)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcessW(nullptr, tmp_wstr_c(cmdline).str(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        if (hProcess)
        {
            *hProcess = pi.hProcess;
        }
        return true;

    }
    else
    {
        return false;
    }
}



}