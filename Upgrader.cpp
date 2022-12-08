#include "Upgrader.h"

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <string>
#include <thread>
#include <Windows.h>
#include <libloaderapi.h>
#include <Wincrypt.h>
#include <WinHttp.h>
#include <Psapi.h>
#include <tlhelp32.h >

#include "aux-cvt.h"

#pragma comment(lib, "Winhttp.lib")

#define MD5LEN 16

static DWORD CalcMD5(LPCWSTR filename, LPBYTE rgbHash);
static char* base64_encode(const unsigned char* binData, int binLength, char* base64);
static std::wstring GetRemoteFileMD5(const wchar_t* host, const wchar_t* uri);
static void GetProcessPathByPId(TCHAR* cstrPath);

bool HasNewVersion(const wchar_t* remote_host, const wchar_t* remote_uri) {
    wchar_t sfxPath[MAX_PATH] = {};
    //GetProcessPathByPId(sfxPath);
    ::GetModuleFileName(NULL, sfxPath, MAX_PATH);
    if (sfxPath[0] == '\0') return false;
    BYTE hash[MD5LEN];
    if (CalcMD5(sfxPath, hash) != 0) return false;
    char md5B64[MD5LEN * 2];
    base64_encode(hash, MD5LEN, md5B64);

    auto remoteMD5 = GetRemoteFileMD5(remote_host, remote_uri);
    if (remoteMD5.empty()) return false;
    return std::wstring(aux::a2w(md5B64)) != remoteMD5;
}

#define BUFSIZE 1024

DWORD CalcMD5(LPCWSTR filename, LPBYTE rgbHash)
{
    DWORD dwStatus = 0;
    BOOL bResult = FALSE;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    HANDLE hFile = NULL;
    BYTE rgbFile[BUFSIZE];
    DWORD cbRead = 0;
    DWORD cbHash = 0;
    // Logic to check usage goes here.

    hFile = CreateFile(filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);

    if (INVALID_HANDLE_VALUE == hFile)
    {
        dwStatus = GetLastError();
        printf("Error opening file Error: %d\n", dwStatus);
        return dwStatus;
    }

    // Get handle to the crypto provider
    if (!CryptAcquireContext(&hProv,
        NULL,
        NULL,
        PROV_RSA_FULL,
        CRYPT_VERIFYCONTEXT))
    {
        dwStatus = GetLastError();
        printf("CryptAcquireContext failed: %d\n", dwStatus);
        CloseHandle(hFile);
        return dwStatus;
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
    {
        dwStatus = GetLastError();
        printf("CryptAcquireContext failed: %d\n", dwStatus);
        CloseHandle(hFile);
        CryptReleaseContext(hProv, 0);
        return dwStatus;
    }

    while (bResult = ReadFile(hFile, rgbFile, BUFSIZE,
        &cbRead, NULL))
    {
        if (0 == cbRead)
        {
            break;
        }

        if (!CryptHashData(hHash, rgbFile, cbRead, 0))
        {
            dwStatus = GetLastError();
            printf("CryptHashData failed: %d\n", dwStatus);
            CryptReleaseContext(hProv, 0);
            CryptDestroyHash(hHash);
            CloseHandle(hFile);
            return dwStatus;
        }
    }

    if (!bResult)
    {
        dwStatus = GetLastError();
        printf("ReadFile failed: %d\n", dwStatus);
        CryptReleaseContext(hProv, 0);
        CryptDestroyHash(hHash);
        CloseHandle(hFile);
        return dwStatus;
    }

    cbHash = MD5LEN;
    if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
    {
        dwStatus = GetLastError();
        printf("CryptGetHashParam failed: %d\n", dwStatus);
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);

    return dwStatus;
}

char base64char[] = {
        'A','B','C','D','E','F','G','H','I','J',
        'K','L','M','N','O','P','Q','R','S','T',
        'U','V','W','X','Y','Z','a','b','c','d',
        'e','f','g','h','i','j','k','l','m','n',
        'o','p','q','r','s','t','u','v','w','x',
        'y','z','0','1','2','3','4','5','6','7',
        '8','9','+', '/', '\0'
};

char* base64_encode(const unsigned char* binData, int binLength, char* base64)
{
    int i = 0;
    int j = 0;
    int current = 0;
    for (i = 0; i < binLength; i += 3) {
        //获取第一个6位
        current = (*(binData + i) >> 2) & 0x3F;
        *(base64 + j++) = base64char[current];
        //获取第二个6位的前两位
        current = (*(binData + i) << 4) & 0x30;
        //如果只有一个字符，那么需要做特殊处理
        if (binLength <= (i + 1)) {
            *(base64 + j++) = base64char[current];
            *(base64 + j++) = '=';
            *(base64 + j++) = '=';
            break;
        }
        //获取第二个6位的后四位
        current |= (*(binData + i + 1) >> 4) & 0xf;
        *(base64 + j++) = base64char[current];
        //获取第三个6位的前四位
        current = (*(binData + i + 1) << 2) & 0x3c;
        if (binLength <= (i + 2)) {
            *(base64 + j++) = base64char[current];
            *(base64 + j++) = '=';
            break;
        }
        //获取第三个6位的后两位
        current |= (*(binData + i + 2) >> 6) & 0x03;
        *(base64 + j++) = base64char[current];
        //获取第四个6位
        current = *(binData + i + 2) & 0x3F;
        *(base64 + j++) = base64char[current];
    }
    *(base64 + j) = '\0';
    return base64;
}

std::wstring GetRemoteFileMD5(const wchar_t* host, const wchar_t* uri) {
    HRESULT hr = S_OK;
    HINTERNET sessionHandle = NULL;
    HINTERNET connectionHandle = NULL;
    HINTERNET requestHandle = NULL;
    DWORD dwResolveTimeOut = 3000;
    DWORD dwConnectTimeOut = 3000;
    DWORD dwHeadersReceiveTimeOut = 3000;
    WCHAR headerBuffer[256] = {};
    ULONG headerLength = sizeof(headerBuffer);
    HANDLE hTempFile = INVALID_HANDLE_VALUE;
    DWORD dwBytesWritten = 0;
    DWORD dwRetVal = 0;
    UINT uRetVal = 0;
    BOOL fSuccess = FALSE;

    std::wstring ret;

    // Get a session handle
    sessionHandle = WinHttpOpen(
        L"",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (sessionHandle == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    // Get a connection handle
    connectionHandle = WinHttpConnect(
        sessionHandle,
        host,
        INTERNET_DEFAULT_PORT,
        0);
    if (connectionHandle == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    // Open a request
    requestHandle = WinHttpOpenRequest(
        connectionHandle,
        L"GET",
        uri,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);
    if (requestHandle == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    //WINHTTP_OPTION_RECEIVE_TIMEOUT
    WinHttpSetOption(requestHandle, WINHTTP_OPTION_RESOLVE_TIMEOUT, &dwResolveTimeOut, sizeof(DWORD));
    WinHttpSetOption(requestHandle, WINHTTP_OPTION_CONNECT_TIMEOUT, &dwConnectTimeOut, sizeof(DWORD));
    WinHttpSetOption(requestHandle, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &dwHeadersReceiveTimeOut, sizeof(DWORD));

    // Send the request
    if (!WinHttpSendRequest(
        requestHandle,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        NULL))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    // Receive the response
    if (!WinHttpReceiveResponse(requestHandle, NULL))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    if (!WinHttpQueryHeaders(
        requestHandle,
        WINHTTP_QUERY_CONTENT_MD5,
        NULL,
        headerBuffer,
        &headerLength,
        WINHTTP_NO_HEADER_INDEX))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

Exit:
    if (requestHandle != NULL)
    {
        WinHttpCloseHandle(requestHandle);
    }
    if (connectionHandle != NULL)
    {
        WinHttpCloseHandle(connectionHandle);
    }
    if (sessionHandle != NULL)
    {
        WinHttpCloseHandle(sessionHandle);
    }

    return headerBuffer;
}

void GetProcessPathByPId(TCHAR* cstrPath)
{
    //低权限进程获取父进程路径，使用dostontpath 转换
    HANDLE hProcess = NULL;
    HANDLE hProcessSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
        return;

    //find parent process id
    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(pe32);
    BOOL bContinue = ::Process32First(hProcessSnap, &pe32);
    DWORD dwParentProcessId = 0;
    DWORD dwProcessId = GetCurrentProcessId();
    while (bContinue)
    {
        if (pe32.th32ProcessID == dwProcessId)
        {
            dwParentProcessId = pe32.th32ParentProcessID;
            break;
        }

        bContinue = ::Process32Next(hProcessSnap, &pe32);
    }
    ::CloseHandle(hProcessSnap);

    hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwParentProcessId);
    if (NULL == hProcess)
    {
        return;
    }

    TCHAR szPath[MAX_PATH + 1] = { 0 };
    DWORD cbNeeded = MAX_PATH;

    DWORD dwCount = GetProcessImageFileName(hProcess, szPath, cbNeeded);
    if (0 == dwCount)
    {
        return;
    }

    // 获取Logic Drive String长度
    UINT uiLen = GetLogicalDriveStrings(0, NULL);
    if (0 == uiLen)
    {
        return;
    }

    PTSTR pLogicDriveString = new TCHAR[uiLen + 1];
    ZeroMemory(pLogicDriveString, uiLen + 1);
    uiLen = GetLogicalDriveStrings(uiLen, pLogicDriveString);
    if (0 == uiLen)
    {
        delete[]pLogicDriveString;
        return;
    }

    TCHAR szDrive[3] = TEXT(" :");
    PTSTR pDosDriveName = new TCHAR[MAX_PATH];
    PTSTR pLogicIndex = pLogicDriveString;

    do
    {
        szDrive[0] = *pLogicIndex;
        uiLen = QueryDosDevice(szDrive, pDosDriveName, MAX_PATH);
        if (0 == uiLen)
        {
            if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
            {
                break;
            }

            delete[]pDosDriveName;
            pDosDriveName = new TCHAR[uiLen + 1];
            uiLen = QueryDosDevice(szDrive, pDosDriveName, uiLen + 1);
            if (0 == uiLen)
            {
                break;
            }
        }

        uiLen = wcslen(pDosDriveName);
        if (0 == _wcsnicmp(szPath, pDosDriveName, uiLen))
        {
            //sFilePath.Format(TEXT("%s%s"), szDrive, szPath + uiLen);
            swprintf(cstrPath, TEXT("%s%s"), szDrive, szPath + uiLen);
            break;
        }

        while (*pLogicIndex++);
    } while (*pLogicIndex);

    delete[]pLogicDriveString;
    delete[]pDosDriveName;

    return;
}