#include "tika_worker.h"

#include "app_paths.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QTemporaryFile>

namespace wensousou {

TikaWorker::TikaWorker() {
  process_.setProcessChannelMode(QProcess::SeparateChannels);
}

TikaWorker::~TikaWorker() { stop(); }

bool TikaWorker::start(QString* error) {
  if (process_.state() == QProcess::Running) return true;
  stop();
  const QString java = AppPaths::javaExecutable();
  const QString jar = AppPaths::parserWorkerJar();
  if (!QFileInfo::exists(java)) {
    if (error) *error = QStringLiteral("找不到内置 Java：%1").arg(java);
    return false;
  }
  if (!QFileInfo::exists(jar)) {
    if (error) *error = QStringLiteral("找不到解析 Worker：%1").arg(jar);
    return false;
  }
  process_.setProgram(java);
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
  environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
  process_.setProcessEnvironment(environment);
  process_.setArguments({QStringLiteral("-Dfile.encoding=UTF-8"),
                         QStringLiteral("-Xms64m"), QStringLiteral("-Xmx512m"),
                         QStringLiteral("-jar"), jar});
  process_.start();
  if (!process_.waitForStarted(10000)) {
    if (error) {
      *error = QStringLiteral("无法启动解析 Worker：%1").arg(process_.errorString());
    }
    return false;
  }
  return true;
}

ParseResult TikaWorker::parse(const QString& inputPath, int maxCharacters,
                              int timeoutSeconds) {
  ParseResult result;
  if (!start(&result.error)) return result;

  QTemporaryFile temporaryFile;
  temporaryFile.setAutoRemove(true);
  if (!temporaryFile.open()) {
    result.error = QStringLiteral("无法创建解析临时文件：%1").arg(temporaryFile.errorString());
    return result;
  }
  const QString outputPath = temporaryFile.fileName();
  temporaryFile.close();

  const qint64 requestId = nextRequestId_++;
  QJsonObject request;
  request.insert(QStringLiteral("id"), static_cast<double>(requestId));
  request.insert(QStringLiteral("input"), inputPath);
  request.insert(QStringLiteral("output"), outputPath);
  request.insert(QStringLiteral("maxChars"), maxCharacters);
  const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
  if (process_.write(payload) != payload.size() || !process_.waitForBytesWritten(5000)) {
    result.error = QStringLiteral("无法向解析 Worker 发送任务。");
    stop();
    return result;
  }

  QByteArray responseLine;
  if (!readResponse(requestId, timeoutSeconds, &responseLine, &result.error)) {
    stop();
    return result;
  }
  QJsonParseError parseError;
  const QJsonDocument response = QJsonDocument::fromJson(responseLine, &parseError);
  if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
    result.error = QStringLiteral("解析 Worker 返回了无效响应。");
    stop();
    return result;
  }
  const QJsonObject object = response.object();
  if (object.value(QStringLiteral("status")).toString() != QStringLiteral("ok")) {
    result.error = object.value(QStringLiteral("error")).toString(
        QStringLiteral("文档解析失败。"));
    return result;
  }
  QFile output(outputPath);
  if (!output.open(QIODevice::ReadOnly)) {
    result.error = QStringLiteral("无法读取解析结果：%1").arg(output.errorString());
    return result;
  }
  result.content = QString::fromUtf8(output.readAll());
  result.characters = object.value(QStringLiteral("chars")).toVariant().toLongLong();
  result.truncated = object.value(QStringLiteral("truncated")).toBool();
  result.ok = true;
  return result;
}

bool TikaWorker::readResponse(qint64 requestId, int timeoutSeconds, QByteArray* line,
                              QString* error) {
  QElapsedTimer timer;
  timer.start();
  const qint64 timeoutMs = qMax(1, timeoutSeconds) * 1000LL;
  while (timer.elapsed() < timeoutMs) {
    stdoutBuffer_ += process_.readAllStandardOutput();
    while (stdoutBuffer_.contains('\n')) {
      const int newline = stdoutBuffer_.indexOf('\n');
      const QByteArray candidate = stdoutBuffer_.left(newline).trimmed();
      stdoutBuffer_.remove(0, newline + 1);
      const QJsonDocument document = QJsonDocument::fromJson(candidate);
      if (document.isObject() &&
          document.object().value(QStringLiteral("id")).toVariant().toLongLong() ==
              requestId) {
        *line = candidate;
        return true;
      }
    }
    if (process_.state() != QProcess::Running) {
      if (error) {
        *error = QStringLiteral("解析 Worker 异常退出：%1")
                     .arg(QString::fromUtf8(process_.readAllStandardError()).trimmed());
      }
      return false;
    }
    process_.waitForReadyRead(200);
  }
  if (error) *error = QStringLiteral("文档解析超过 %1 秒。").arg(timeoutSeconds);
  return false;
}

void TikaWorker::stop() {
  stdoutBuffer_.clear();
  if (process_.state() == QProcess::NotRunning) return;
  process_.terminate();
  if (!process_.waitForFinished(2000)) {
    process_.kill();
    process_.waitForFinished(2000);
  }
}

}  // namespace wensousou
