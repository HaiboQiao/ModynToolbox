#include "../core/video.h"
#include "modyn_sdk.h"
#include <QCoreApplication>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        auto input = Modyn::readInput();
        Video::Hooks hooks;
        hooks.cancelled = [] { return Modyn::stopped.load(); };
        hooks.progress = [](int n, const QString &s) { Modyn::progress(n, 100, s); };
        hooks.reserve = [](const QStringList &p) { Modyn::reserve(p); };
        hooks.commit = [] { Modyn::critical(); };
        return Modyn::result(Video::execute(Modyn::request.value("action").toString(), input,
                                            QCoreApplication::applicationDirPath(),
                                            Modyn::request.value("data_root").toString(), hooks));
    } catch (const std::exception &e) {
        return Modyn::result({{"status", Modyn::stopped ? "cancelled" : "failed"},
                              {"message", QString::fromUtf8(e.what())}});
    }
}
