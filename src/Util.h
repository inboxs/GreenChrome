﻿#include <cctype>
#include <algorithm>

#define MAX_SIZE 32767

// 打印调试信息
void DebugLog(const wchar_t* format, ...)
{
    va_list args;

    va_start(args, format);
    size_t length = _vscwprintf(format, args);
    va_end(args);

    wchar_t* buffer = (wchar_t*)malloc((length + 1) * sizeof(wchar_t));
    va_start(args, format);
    _vsnwprintf_s(buffer, length + 1, length, format, args);
    va_end(args);

    OutputDebugStringW(buffer);
    free(buffer);
}

// 如果需要给路径加引号
inline std::wstring QuotePathIfNeeded(const std::wstring &path)
{
    std::vector<wchar_t> buffer(path.length() + 1 /* null */ + 2 /* quotes */);
    wcscpy(&buffer[0], path.c_str());

    PathQuoteSpaces(&buffer[0]);

    return std::wstring(&buffer[0]);
}

// 展开环境路径比如 %windir%
std::wstring ExpandEnvironmentPath(const std::wstring &path)
{
    std::vector<wchar_t> buffer(MAX_PATH);
    size_t expandedLength = ::ExpandEnvironmentStrings(path.c_str(), &buffer[0], (DWORD)buffer.size());
    if (expandedLength > buffer.size())
    {
        buffer.resize(expandedLength);
        expandedLength = ::ExpandEnvironmentStrings(path.c_str(), &buffer[0], (DWORD)buffer.size());
    }
    return std::wstring(&buffer[0], 0, expandedLength);
}

// 替换字符串
void ReplaceStringInPlace(std::wstring& subject, const std::wstring& search, const std::wstring& replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::wstring::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

void ReplaceStringInPlace(std::string& subject, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

// 压缩HTML
std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}
std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}
std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}
std::vector<std::string> split(const std::string &text, char sep) {
    std::vector<std::string> tokens;
    std::size_t start = 0, end = 0;
    while ((end = text.find(sep, start)) != std::string::npos) {
        std::string temp = text.substr(start, end - start);
        tokens.push_back(temp);
        start = end + 1;
    }
    std::string temp = text.substr(start);
    tokens.push_back(temp);
    return tokens;
}
void compression_html(std::string& html)
{
    auto lines = split(html, '\n');
    html.clear();
    for ( auto &line : lines)
    {
        html += trim(line);
    }
}

// 运行外部程序
HANDLE RunExecute(const wchar_t *command, WORD show = SW_SHOW)
{
    std::vector <std::wstring> command_line;

    int nArgs;
    LPWSTR *szArglist = CommandLineToArgvW(command, &nArgs);
    for (int i = 0; i < nArgs; i++)
    {
        command_line.push_back(QuotePathIfNeeded(szArglist[i]));
    }
    LocalFree(szArglist);

    SHELLEXECUTEINFO ShExecInfo = { 0 };
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.lpFile = command_line[0].c_str();
    ShExecInfo.nShow = show;

    std::wstring parameter;
    for (size_t i = 1; i < command_line.size(); i++)
    {
        parameter += command_line[i];
        parameter += L" ";
    }
    if ( command_line.size() > 1 )
    {
        ShExecInfo.lpParameters = parameter.c_str();
    }

    if (ShellExecuteEx(&ShExecInfo))
    {
        return ShExecInfo.hProcess;
    }

    return 0;
}


// 加载资源内容
template<typename Function>
bool LoadFromResource(const wchar_t *type, const wchar_t *name, Function f)
{
    bool result = false;
    HRSRC res = FindResource(hInstance, name, type);
    if (res)
    {
        HGLOBAL header = LoadResource(hInstance, res);
        if (header)
        {
            const char *data = (const char*)LockResource(header);
            DWORD size = SizeofResource(hInstance, res);
            if (data)
            {
                f(data, size);
                result = true;
                UnlockResource(header);
            }
        }
        FreeResource(header);
    }

    return result;
}

// 释放配置文件
EXPORT ReleaseIni(const wchar_t *exePath, wchar_t *iniPath)
{
    // ini路径
    wcscpy(iniPath, exePath);
    wcscat(iniPath, L"\\GreenChrome.ini");

    // 已经存在则跳过
    if (PathFileExistsW(iniPath))
    {
        return;
    }

    // 从资源中释放默认配置文件
    LoadFromResource(L"INI", L"CONFIG", [&](const char *data, DWORD size)
    {
        FILE *fp = _wfopen(iniPath, L"wb");
        if (fp)
        {
            fwrite(data, size, 1, fp);
            fclose(fp);
        }
        else
        {
            //不能写入当前目录，写%appdata%
            std::wstring path = ExpandEnvironmentPath(L"%appdata%\\GreenChrome.ini");
            wcscpy(iniPath, path.c_str());
            fp = _wfopen(iniPath, L"wb");
            if (fp)
            {
                fwrite(data, size, 1, fp);
                fclose(fp);
            }
            else
            {
                DebugLog(L"ReleaseIni failed");
            }
        }
    });
}

// 搜索内存
uint8_t* memmem(uint8_t* src, int n, const uint8_t* sub, int m)
{
    if (m > n)
    {
        return NULL;
    }

    short skip[256];
    for (int i = 0; i < 256; i++)
    {
        skip[i] = m;
    }
    for (int i = 0; i < m - 1; i++)
    {
        skip[sub[i]] = m - i - 1;
    }

    int pos = 0;
    while (pos <= n - m)
    {
        int j = m - 1;
        while (j >= 0 && src[pos + j] == sub[j])
        {
            j--;
        }
        if (j < 0)
        {
            return src + pos;
        }
        pos = pos + skip[src[pos + m - 1]];
    }

    return NULL;
}

bool GetVersion(wchar_t *vinfo)
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    vinfo[0] = '\0';

    bool ret = false;
    DWORD dummy;
    DWORD infoSize = GetFileVersionInfoSize(exePath, &dummy);
    if (infoSize > 0)
    {
        char *buffer = (char*)malloc(infoSize);

        if (GetFileVersionInfo(exePath, dummy, infoSize, buffer))
        {
            VS_FIXEDFILEINFO *InfoStructure;
            unsigned int TextSize = 0;
            if( VerQueryValue(buffer, L"\\", (VOID **) &InfoStructure, &TextSize) )
            {
                wsprintf(vinfo, L"%d.%d.%d.%d",
                         HIWORD(InfoStructure->dwFileVersionMS), LOWORD(InfoStructure->dwFileVersionMS),
                         HIWORD(InfoStructure->dwFileVersionLS), LOWORD(InfoStructure->dwFileVersionLS));
                ret = true;
            }
        }

        free(buffer);
    }
    return ret;
}

// 搜索模块，自动加载
uint8_t* SearchModule(const wchar_t *path, const uint8_t* sub, int m)
{
    HMODULE module = LoadLibraryW(path);

    if(!module)
    {
        // dll存在于版本号文件夹中
        wchar_t version[MAX_PATH];
        GetVersion(version);
        wcscat(version, L"/");
        wcscat(version, path);

        module = LoadLibraryW(version);
    }

    if(module)
    {
        uint8_t* buffer = (uint8_t*)module;

        PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)(buffer + ((PIMAGE_DOS_HEADER)buffer)->e_lfanew);
        PIMAGE_SECTION_HEADER section = (PIMAGE_SECTION_HEADER)((char*)nt_header + sizeof(DWORD) +
            sizeof(IMAGE_FILE_HEADER) + nt_header->FileHeader.SizeOfOptionalHeader);

        for(int i=0; i<nt_header->FileHeader.NumberOfSections; i++)
        {
            if(strcmp((const char*)section[i].Name,".text")==0)
            {
                return memmem(buffer + section[i].PointerToRawData, section[i].SizeOfRawData, sub, m);
                break;
            }
        }
    }
    return NULL;
}

template<typename String, typename Char, typename Function>
void StringSplit(String *str, Char delim, Function f)
{
    String *ptr = str;
    while (*str)
    {
        if (*str == delim)
        {
            *str = 0;           // 截断字符串

            if (str - ptr)      // 非空字符串
            {
                f(ptr);
            }

            *str = delim;       // 还原字符串
            ptr = str + 1;      // 移动下次结果指针
        }
        str++;
    }

    if (str - ptr)  // 非空字符串
    {
        f(ptr);
    }
}

void SendKey(std::wstring &keys)
{
    std::vector <INPUT> inputs;

    wchar_t *temp = _tcsdup(keys.c_str());
    StringSplit(temp, L'+', [&]
        (wchar_t *key)
    {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

        WORD vk = 0;

        // 解析控制键
        if (_tcsicmp(key, _T("Shift")) == 0) vk = VK_SHIFT;
        else if (_tcsicmp(key, _T("Ctrl")) == 0) vk = VK_CONTROL;
        else if (_tcsicmp(key, _T("Alt")) == 0) vk = VK_MENU;
        else if (_tcsicmp(key, _T("Win")) == 0) vk = VK_LWIN;
        // 解析方向键
        else if (_tcsicmp(key, _T("←")) == 0) vk = VK_LEFT;
        else if (_tcsicmp(key, _T("→")) == 0) vk = VK_RIGHT;
        else if (_tcsicmp(key, _T("↑")) == 0) vk = VK_UP;
        else if (_tcsicmp(key, _T("↓")) == 0) vk = VK_DOWN;
        // 解析单个字符A-Z、0-9等
        else if (_tcslen(key) == 1)
        {
            if (isalnum(key[0])) vk = toupper(key[0]);
            else vk = LOWORD(VkKeyScan(key[0]));
        }
        // 解析F1-F24功能键
        else if ((key[0] == 'F' || key[0] == 'f') && isdigit(key[1]) )
        {
            int FX = _ttoi(&key[1]);
            if (FX >= 1 && FX <= 24) vk = VK_F1 + FX - 1;
        }
        // 解析其他按键
        else
        {
            if (_tcsicmp(key, _T("Left")) == 0) vk = VK_LEFT;
            else if (_tcsicmp(key, _T("Right")) == 0) vk = VK_RIGHT;
            else if (_tcsicmp(key, _T("Up")) == 0) vk = VK_UP;
            else if (_tcsicmp(key, _T("Down")) == 0) vk = VK_DOWN;

            else if (_tcsicmp(key, _T("Esc")) == 0) vk = VK_ESCAPE;
            else if (_tcsicmp(key, _T("Tab")) == 0) vk = VK_TAB;

            else if (_tcsicmp(key, _T("Backspace")) == 0) vk = VK_BACK;
            else if (_tcsicmp(key, _T("Enter")) == 0) vk = VK_RETURN;
            else if (_tcsicmp(key, _T("Space")) == 0) vk = VK_SPACE;

            else if (_tcsicmp(key, _T("PrtSc")) == 0) vk = VK_SNAPSHOT;
            else if (_tcsicmp(key, _T("Scroll")) == 0) vk = VK_SCROLL;
            else if (_tcsicmp(key, _T("Pause")) == 0) vk = VK_PAUSE;
            else if (_tcsicmp(key, _T("Break")) == 0) vk = VK_PAUSE;

            else if (_tcsicmp(key, _T("Insert")) == 0) vk = VK_INSERT;
            else if (_tcsicmp(key, _T("Delete")) == 0) vk = VK_DELETE;

            else if (_tcsicmp(key, _T("End")) == 0) vk = VK_END;
            else if (_tcsicmp(key, _T("Home")) == 0) vk = VK_HOME;

            else if (_tcsicmp(key, _T("PageUp")) == 0) vk = VK_PRIOR;
            else if (_tcsicmp(key, _T("PageDown")) == 0) vk = VK_NEXT;
            else if (_tcsicmp(key, _T("PgUp")) == 0) vk = VK_PRIOR;
            else if (_tcsicmp(key, _T("PgDn")) == 0) vk = VK_NEXT;

            // 浏览器快捷键
            else if (_tcsicmp(key, _T("Back")) == 0) vk = VK_BROWSER_BACK;
            else if (_tcsicmp(key, _T("Forward")) == 0) vk = VK_BROWSER_FORWARD;
            else if (_tcsicmp(key, _T("Refresh")) == 0) vk = VK_BROWSER_REFRESH;

            // 音量相关快捷键
            else if (_tcsicmp(key, _T("VolumeMute")) == 0) vk = VK_VOLUME_MUTE;
            else if (_tcsicmp(key, _T("VolumeDown")) == 0) vk = VK_VOLUME_DOWN;
            else if (_tcsicmp(key, _T("VolumeUp")) == 0) vk = VK_VOLUME_UP;
        }

        input.ki.wVk = vk;

        inputs.push_back(input);
    });

    free(temp);

    // 发送按下
    ::SendInput((UINT)inputs.size(), &inputs[0], sizeof(INPUT));

    // 发送弹起
    for ( auto &input : inputs )
    {
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
    }
    ::SendInput((UINT)inputs.size(), &inputs[0], sizeof(INPUT));
}

// 发送鼠标消息
void SendOneMouse(int mouse)
{
    INPUT input[1];
    memset(input, 0, sizeof(input));

    input[0].type = INPUT_MOUSE;

    input[0].mi.dwFlags = mouse;
    ::SendInput(1, input, sizeof(INPUT));
}

//从资源载入图片
bool ImageFromIDResource(const wchar_t *name, Image *&pImg)
{
    LoadFromResource(L"PNG", name, [&](const char *data, DWORD size)
    {
        HGLOBAL m_hMem = GlobalAlloc(GMEM_FIXED, size);
        BYTE* pmem = (BYTE*)GlobalLock(m_hMem);
        memcpy(pmem, data, size);
        GlobalUnlock(m_hMem);

        IStream* pstm;
        CreateStreamOnHGlobal(m_hMem, FALSE, &pstm);

        pImg = Image::FromStream(pstm);

        pstm->Release();
        GlobalFree(m_hMem);
    });
    return TRUE;
}

bool isEndWith(const wchar_t *path,const wchar_t* ext)
{
    if(!path || !ext) return false;
    size_t len1 = wcslen(path);
    size_t len2 = wcslen(ext);
    if(len2>len1) return false;
    return !_memicmp(path + len1 - len2,ext,len2*sizeof(wchar_t));
}