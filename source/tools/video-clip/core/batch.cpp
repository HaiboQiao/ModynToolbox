#include "video.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <QDateTime>
#include <QRegularExpression>
#include <stdexcept>
namespace Video {
namespace {
void store(const QString &path,const QJsonObject &record) {
    if(path.isEmpty())return;
    QSaveFile file(path);auto bytes=QJsonDocument(record).toJson(QJsonDocument::Compact);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())throw std::runtime_error("Cannot save video queue record");
}
QJsonObject failed(const QString &text) {return {{"status","failed"},{"message",text}};}
}
QJsonObject executeBatch(const QString &action,const QJsonObject &input,const QString &bin,const QString &dataRoot,const Hooks &h) {
    QJsonArray rows,artifacts;QJsonObject context,record;QString path;
    try {
        const auto folder=dataRoot+"/batches";
        if(action=="batch-status") {
            if(dataRoot.isEmpty())return failed(QStringLiteral("此入口没有持久队列目录"));
            const auto names=QDir(folder).entryInfoList({"*.json"},QDir::Files|QDir::NoSymLinks,QDir::Time);
            if(names.isEmpty())return {{"status","succeeded"},{"message",QStringLiteral("还没有队列记录")},{"queue",QJsonArray{}}};
            QFile file(names.first().filePath());if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024)return failed(QStringLiteral("队列记录无法读取，已保留原文件"));
            auto saved=QJsonDocument::fromJson(file.readAll()).object();if(saved.value("version").toInt()!=1)return failed(QStringLiteral("未知队列记录版本，已保留原文件"));
            auto result=saved.value("result").toObject();result.remove("context");result["queue_source_identity"]=saved.value("source_identity");result["queue_input"]=saved.value("queue_input");result["queue"]=saved.value("rows");result["batch_id"]=saved.value("id");
            result["status"]="succeeded";result["message"]=saved.value("state")==QJsonValue("running")?QStringLiteral("上次队列未正常结束；请核对已完成文件，未完成项不会自动执行"):QStringLiteral("已读取上次队列记录");
            QJsonArray files;for(const auto &v:saved.value("rows").toArray()){auto row=v.toObject();auto f=row.value("file").toString();if(row.value("state")==QJsonValue("succeeded")&&QFileInfo(f).isFile())files.append(f);}result["artifacts"]=files;return result;
        }
        if(action!="batch-export")return failed(QStringLiteral("未知队列操作"));
        const auto clips=input.value("clips").toArray();if(clips.isEmpty()||clips.size()>100)return failed(QStringLiteral("一次队列应为1–100个片段"));
        if(input.value("params").toObject().value("mode").toString()==QStringLiteral("快速截取"))return failed(QStringLiteral("多片段队列请使用精确裁剪；快速模式逐片段准备并导出"));
        auto checked=execute("probe",input,bin,dataRoot,h);if(checked.value("status")!="succeeded")return checked;
        context=checked.value("context").toObject();auto old=input.value("context").toObject().value("media").toObject();
        if(!old.isEmpty()&&old.value("identity")!=context.value("media").toObject().value("identity"))return failed(QStringLiteral("源视频已变化，请重新分析"));
        QStringList reserved;QVector<QJsonObject> requests;
        int index=0;
        for(const auto &v:clips) {
            if(h.cancelled())return {{"status","cancelled"},{"message",QStringLiteral("队列已取消")}};
            if(!v.isObject())return failed(QStringLiteral("片段记录无效"));
            auto clip=v.toObject();if(clip.value("name").toString().size()>80||clip.value("id").toString().size()>80)return failed(QStringLiteral("片段名称或标识过长"));auto req=input;req.remove("clips");auto params=input.value("params").toObject();params["start_ms"]=clip.value("start_ms");params["end_ms"]=clip.value("end_ms");
            auto base=params.value("filename").toString().trimmed();if(base.isEmpty())base=QFileInfo(input.value("files").toArray().first().toString()).completeBaseName()+"_clip";
            for(auto ext:QStringList{".mp4",".mov",".gif",".png"})if(base.endsWith(ext,Qt::CaseInsensitive)){base.chop(4);break;}
            ++index;if(clip.value("filename").isString()&&!clip.value("filename").toString().isEmpty())base=clip.value("filename").toString();else base+="_"+QString::number(index).rightJustified(3,'0');params["filename"]=base;req["params"]=params;req["context"]=context;
            auto pre=execute("prepare",req,bin,dataRoot,h);if(pre.value("status")!="succeeded")return pre;
            auto plan=pre.value("context").toObject().value("plan").toObject();const auto dir=plan.value("output_dir").toString(),ext=plan.value("format").toString("mp4");QString name=base;int suffix=0;
            while(QFileInfo::exists(QDir(dir).filePath(name+"."+ext))||reserved.contains(QDir(dir).filePath(name+"."+ext))){if(++suffix>9999)return failed(QStringLiteral("同名输出过多"));name=base+QString(" (%1)").arg(suffix);}
            params["filename"]=name;req["params"]=params;requests.append(req);reserved.append(QDir(dir).filePath(name+"."+ext));
            rows.append(QJsonObject{{"id",clip.value("id").toString(QString::number(index))},{"name",clip.value("name").toString(QStringLiteral("片段 %1").arg(index))},{"state","queued"},{"start_ms",clip.value("start_ms")},{"end_ms",clip.value("end_ms")},{"file",reserved.last()}});
        }
        h.reserve(reserved);
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        if(!dataRoot.isEmpty()){if(!QDir().mkpath(folder))throw std::runtime_error("Cannot create video queue directory");path=folder+"/"+id+".json";}
        QJsonObject frozenParams;const auto sourceParams=input.value("params").toObject();for(const auto &key:QStringList{"mode","quality","size","audio_index","remove_audio","format","fps","bitrate_kbps","filename","output_dir"})if(sourceParams.contains(key))frozenParams[key]=sourceParams.value(key);
        const QJsonObject frozenInput{{"files",QJsonArray{context.value("media").toObject().value("source")}},{"params",frozenParams}};
        record={{"queue_input",frozenInput},{"version",1},{"id",id},{"created",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"state","running"},{"rows",rows},{"source_identity",context.value("media").toObject().value("identity")}};store(path,record);
        int completed=0,errors=0;
        for(int i=0;i<requests.size();++i) {
            if(h.cancelled())break;
            auto row=rows[i].toObject();row["state"]="running";rows[i]=row;record["rows"]=rows;store(path,record);
            auto sub=h;sub.reserve=[&](const QStringList &paths){for(const auto &p:paths)if(!reserved.contains(p))throw std::runtime_error("Output changed after queue reservation; retry to choose a new name");};
            sub.progress=[&,i](int n,const QString &message){h.progress((i*100+qBound(0,n,100))/requests.size(),QStringLiteral("片段 %1/%2 · %3").arg(i+1).arg(requests.size()).arg(message));};
            auto result=execute("export",requests[i],bin,dataRoot,sub);h.resume();
            row["state"]=result.value("status");row["message"]=result.value("message");row["file"]=result.value("artifacts").toArray().isEmpty()?row.value("file"):result.value("artifacts").toArray().first();
            if(result.value("status")==QJsonValue("succeeded")){++completed;for(const auto &f:result.value("artifacts").toArray())artifacts.append(f);}else if(result.value("status")!=QJsonValue("cancelled"))++errors;
            rows[i]=row;record["rows"]=rows;store(path,record);
            if(result.value("status")==QJsonValue("cancelled"))break;
        }
        for(int i=0;i<rows.size();++i){auto row=rows[i].toObject();if(row.value("state")==QJsonValue("queued")||row.value("state")==QJsonValue("running")){row["state"]="cancelled";row["message"]=QStringLiteral("未执行，可明确重试");rows[i]=row;}}
        const auto outcome=h.cancelled()?"cancelled":errors?"failed":"succeeded";
        QJsonObject result{{"status",outcome},{"message",QStringLiteral("队列完成 %1/%2，失败 %3；已完成输出保留").arg(completed).arg(rows.size()).arg(errors)},{"context",context},{"queue",rows},{"queue_source_identity",record.value("source_identity")},{"queue_input",frozenInput},{"batch_id",id},{"completed",completed},{"total",rows.size()},{"failed",errors},{"artifacts",artifacts},{"rows",QJsonArray{}},{"columns",QJsonArray{}}};
        record["rows"]=rows;record["state"]=outcome;record["result"]=result;store(path,record);return result;
    }catch(const std::exception &e){h.resume();record["rows"]=rows;record["state"]="interrupted";if(!path.isEmpty())try{store(path,record);}catch(...){}return {{"status",h.cancelled()?"cancelled":"failed"},{"message",QString::fromUtf8(e.what())},{"queue",rows},{"artifacts",artifacts},{"context",context}};}
}
} // namespace Video
