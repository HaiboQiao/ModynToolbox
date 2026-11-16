#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include "modyn_sdk.h"
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{auto input=Modyn::readInput();auto bytes=input.value("content").toString().toUtf8();int histogram[256]{};for(auto b:bytes)++histogram[(unsigned char)b];QJsonArray bins;for(int n=0;n<256;++n)if(histogram[n])bins.append(QJsonObject{{"byte",n},{"count",histogram[n]}});return Modyn::result({{"byte_count",bytes.size()},{"histogram",bins},{"text",QString::number(bytes.size())+" bytes"}});}catch(const std::exception&e){return Modyn::result({{"status","failed"},{"message",e.what()}});}}
