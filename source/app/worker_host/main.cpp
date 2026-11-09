#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#define NOMINMAX
#include <windows.h>
namespace {
QString quote(const QString &argument) {
    QString result = "\"";
    int slashes = 0;
    for (const auto character : argument) {
        if (character == '\\') {
            ++slashes;
            continue;
        }
        if (character == '"')
            result += QString(slashes * 2 + 1, '\\') + character;
        else
            result += QString(slashes, '\\') + character;
        slashes = 0;
    }
    return result + QString(slashes * 2, '\\') + "\"";
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    QByteArray control;
    char byte = 0;
    DWORD count = 0;
    const auto input = GetStdHandle(STD_INPUT_HANDLE);
    while (control.size() < 65536 && ReadFile(input, &byte, 1, &count, nullptr) && count) {
        if (byte == '\n')
            break;
        control.append(byte);
    }
    const auto document = QJsonDocument::fromJson(control);
    if (!document.isObject())
        return 2;
    const auto object = document.object();
    const auto program = object.value("program").toString();
    const auto directory = object.value("cwd").toString();
    if (program.isEmpty() || directory.isEmpty())
        return 2;
    QStringList command{quote(program)};
    for (const auto &value : object.value("args").toArray())
        command.append(quote(value.toString()));
    auto line = command.join(' ').toStdWString();
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(reinterpret_cast<const wchar_t *>(program.utf16()), line.data(), nullptr,
                        nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                        reinterpret_cast<const wchar_t *>(directory.utf16()), &startup, &process))
        return 3;
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
