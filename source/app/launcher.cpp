#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
std::wstring quote(const std::wstring &value) {
    std::wstring output = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'\"')
            output.append(slashes * 2 + 1, L'\\');
        else
            output.append(slashes, L'\\');
        slashes = 0;
        output += c;
    }
    output.append(slashes * 2, L'\\');
    return output + L"\"";
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command, int) {
    wchar_t buffer[32768];
    const auto length = GetModuleFileNameW(nullptr, buffer, 32768);
    if (!length || length == 32768)
        return 2;
    const std::wstring path(buffer, length);
    const auto root = path.substr(0, path.find_last_of(L"\\/"));
    const auto executable = root + L"\\app\\ModynToolbox.App.exe";
    auto arguments = quote(executable) + L" --data-root " + quote(root + L"\\data") +
                     L" --packages-root " + quote(root + L"\\tools");
    if (command && *command)
        arguments += L" " + std::wstring(command);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), arguments.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        (root + L"\\app").c_str(), &startup, &process)) {
        MessageBoxW(nullptr, L"无法启动程序，请保留 app 文件夹并使用完整发布包。", L"ModynToolbox",
                    MB_OK | MB_ICONERROR);
        return 3;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
