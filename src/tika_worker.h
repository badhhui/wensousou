#pragma once

#include <QByteArray>
#include <QProcess>
#include <QString>

namespace wensousou {

struct ParseResult {
  bool ok = false;
  bool truncated = false;
  qint64 characters = 0;
  QString content;
  QString error;
};

class TikaWorker {
 public:
  TikaWorker();
  ~TikaWorker();

  ParseResult parse(const QString& inputPath, int maxCharacters,
                    int timeoutSeconds);
  void stop();

 private:
  bool start(QString* error);
  bool readResponse(qint64 requestId, int timeoutSeconds, QByteArray* line,
                    QString* error);

  QProcess process_;
  QByteArray stdoutBuffer_;
  qint64 nextRequestId_ = 1;
};

}  // namespace wensousou

