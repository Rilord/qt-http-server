//
// Created by kodor on 1/25/22.
//

#include "http_content_type.h"
#include "http_response.h"
#include "status_map.h"

#include <QtCore/qjsondocument.h>
#include <QtCore/qmimedatabase.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qtcpsocket.h>
#include <QtCore/QPointer>

#include <map>
#include <memory>



QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcHttpResponse, "httpserver.response")

static const std::map<HttpResponder::StatusCode, QByteArray> statusString {
#define XX(num, name, string) { static_cast<HttpResponder::StatusCode>(num), QByteArrayLiteral(#string) },
        HTTP_STATUS_MAP(XX)
#undef XX
};

template <quint64 BUFFERSIZE = 512>
struct IOChunkedTransfer {
    const quint64 bufferSize = BUFFERSIZE;
    char buffer[BUFFERSIZE];
    quint64  beginIndex = -1;
    quint64  endIndex = -1;
    QPointer<QIODevice> source;
    const QPointer<QIODevice> sink;
    const QMetaObject::Connection bytesWrittenConnection;
    const QMetaObject::Connection readyReadConnection;

    IOChunkedTransfer(QIODevice *input, QIODevice *output) :
            source(input),
            sink(output),
            bytesWrittenConnection(QObject::connect(sink.data(), &QIODevice::bytesWritten, [this] () {
                writeToOutput();
            })),
            readyReadConnection(QObject::connect(source.data(), &QIODevice::readyRead, [this] () {
                readFromInput();
            })) {
        Q_ASSERT(!source->atEnd());
        QObject::connect(sink.data(), &QObject::destroyed, source.data(), &QObject::deleteLater);
        QObject::connect(source.data(), &QObject::destroyed, [this] () {
            delete this;
        });
        readFromInput();

    }

    ~IOChunkedTransfer() {
        QObject::disconnect(bytesWrittenConnection);
        QObject::disconnect(readyReadConnection);
    }

    inline bool isBufferEmpty() {
        Q_ASSERT(beginIndex <= endIndex);
        return beginIndex == endIndex;
    }

    void readFromInput() {
        if (!isBufferEmpty())
            return;
        beginIndex = 0;
        endIndex = source->read(buffer, bufferSize);
        if (endIndex < 0) {
            endIndex = beginIndex;
            qCWarning(lcHttpResponse, "Error reading chunk: %s", qPrintable(source->errorString()));
        } else if (endIndex) {
            memset(buffer + endIndex, 0, sizeof(buffer) - std::size_t(endIndex));
            writeToOutput();
        }
    }

    void writeToOutput() {
        if (isBufferEmpty())
            return;

        const auto writtenBytes = sink->write(buffer + beginIndex, endIndex);
        if (writtenBytes < 0) {
            qCWarning(lcHttpResponse, "Error writing chunk: %s", qPrintable(sink->errorString()));
            return;
        }
        beginIndex += writtenBytes;
        if (isBufferEmpty()) {
            if (source->bytesAvailable())
                QTimer::singleShot(0, source.data(), [this] () { readFromInput(); });
            else if (source->atEnd())
                source->deleteLater();
        }

    }
};

HttpResponder::HttpResponder(const HttpRequest &request, QTcpSocket *socket) :
 _request(request), _socket(socket) {
    Q_ASSERT(socket);
}

HttpResponder::~HttpResponder() {}

void HttpResponder::write(QIODevice *data, HeaderList headers, StatusCode status) {
    Q_ASSERT(_socket);

    QScopedPointer<QIODevice, QScopedPointerDeleteLater> input(data);

    input->setParent(nullptr);

    if (!input->isOpen()) {
        if (!input->open(QIODevice::ReadOnly)) {
            qCDebug(lcHttpResponse, "500: Could not open device %s", qPrintable(input->errorString()));
            write(StatusCode::InternalServerError);
            return;
        }
    } else if (!(input->openMode() & QIODevice::ReadOnly)) {
        qCDebug(lcHttpResponse) << "500: Device is opened in a wrong mode " << input->openMode();
        write(StatusCode::InternalServerError);
        return;
    }

    if (!_socket->isOpen()) {
        qCWarning(lcHttpResponse, "Cannot write to soscket. It has been disconnected");
        return;
    }

    writeStatusLine(status);

    if (!input->isSequential()) {
        writeHeader(HttpContentTypes::contentLengthHeader(),
                    QByteArray::number(input->size()));
    }

    for (auto &&header : headers)
        writeHeader(header.first, header.second);

    _socket->write("\r\n");

    if (input->atEnd()) {
        qCDebug(lcHttpResponse, "No more data available.");
        return;
    }

    new IOChunkedTransfer<>(input.take(), _socket);
}

void HttpResponder::write(QIODevice *data, const QByteArray &mimeType, StatusCode status) {
    write(data,
          {{ HttpContentTypes::contentTypeHeader(), mimeType }},
          status);
}

void HttpResponder::write(const QJsonDocument &document, HeaderList headers, StatusCode status) {
    const QByteArray &json = document.toJson();

    writeStatusLine(status);
    writeHeader(HttpContentTypes::contentTypeHeader(), HttpContentTypes::contentTypeJson());
    writeHeader(HttpContentTypes::contentLengthHeader(), QByteArray::number(json.size()));
    writeHeaders(std::move(headers));
    writeBody(document.toJson());
}

void HttpResponder::write(const QJsonDocument &document, StatusCode status) {
    write(document, {}, status);
}

void HttpResponder::write(const QByteArray &data, const QByteArray &mimeType, StatusCode status) {
    write(data,
          {{ HttpContentTypes::contentTypeHeader(), mimeType }},
          status);
}

void HttpResponder::write(StatusCode status) {
    write(QByteArray(), HttpContentTypes::contentTypeXEmpty(), status);
}

void HttpResponder::write(const QByteArray &data, HeaderList headers, StatusCode status) {
    writeStatusLine(status);

    //for (auto &&header : headers)
    //    writeHeader(header.first, header.second);

    writeHeader(HttpContentTypes::contentLengthHeader(), QByteArray::number(data.size()));
    writeBody(data);
}

void HttpResponder::write(HeaderList headers, StatusCode status) {
    write(QByteArray(), std::move(headers), status);
}

void HttpResponder::writeStatusLine(StatusCode status, const QPair<quint8, quint8> &version) {
    Q_ASSERT(_socket->isOpen());
    _socket->write("HTTP/");
    _socket->write(QByteArray::number(version.first));
    _socket->write(".");
    _socket->write(QByteArray::number(version.second));
    _socket->write(" ");
    _socket->write(QByteArray::number(quint32(status)));
    _socket->write(" ");
    _socket->write(statusString.at(status));
    _socket->write("\r\n");
}

void HttpResponder::writeHeader(const QByteArray &header, const QByteArray &value) {
    Q_ASSERT(_socket->isOpen());
    _socket->write(header);
    _socket->write(": ");
    _socket->write(value);
    _socket->write("\r\n");
}

void HttpResponder::writeHeaders(HeaderList headers) {
    for (auto &&header : headers)
        writeHeader(header.first, header.second);
}

void HttpResponder::writeBody(const char *body, qint64 size) {
    Q_ASSERT(_socket->isOpen());

    if (!_bodyStarted) {
        _socket->write("\r\n");
        _bodyStarted = true;
    }

    _socket->write(body, size);
}

void HttpResponder::writeBody(const char *body) {
    writeBody(body, qstrlen(body));
}

void HttpResponder::writeBody(const QByteArray &body) {
    writeBody(body.constData(), body.size());
}

QTcpSocket * HttpResponder::socket() const {
    return _socket;
}

// Responses

HttpResponse::HttpResponse(const char *data)
: HttpResponse(QByteArray::fromRawData(data, qstrlen(data))) {}

HttpResponse::HttpResponse(const QString &data)
: HttpResponse(data.toUtf8()){}

HttpResponse::HttpResponse(const QByteArray &data)
: HttpResponse(QMimeDatabase().mimeTypeForData(data).name().toLocal8Bit(), data) {}

HttpResponse::HttpResponse(QByteArray &&data)
        : HttpResponse(QMimeDatabase().mimeTypeForData(data).name().toLocal8Bit(), std::move(data)) {}

HttpResponse::HttpResponse(const QByteArray &mimeType, const QByteArray &data, const StatusCode status)
: _data(QByteArray(data)), _statusCode(status) {}

HttpResponse::HttpResponse(const StatusCode statusCode)
        : HttpResponse(HttpContentTypes::contentTypeXEmpty(),
                       QByteArray(), statusCode) {}

HttpResponse::HttpResponse(const QJsonObject &data)
        : HttpResponse(HttpContentTypes::contentTypeJson(),
                       QJsonDocument(data).toJson(QJsonDocument::Compact)) {}

HttpResponse::HttpResponse(const QJsonArray &data)
: HttpResponse(HttpContentTypes::contentTypeJson(), QJsonDocument(data).toJson(QJsonDocument::Compact)) {}

HttpResponse::HttpResponse(const QByteArray &mimeType, QByteArray &&data, const StatusCode status)
    : _data(QByteArray(data)), _statusCode(status) {}

HttpResponse::HttpResponse(QByteArray &&mimeType, const QByteArray &data, const StatusCode status)
: _data(QByteArray(data)), _statusCode(status) {}

HttpResponse::HttpResponse(QByteArray &&mimeType, QByteArray &&data, const StatusCode status)
: _data(std::move(data)), _statusCode(status) {
    setHeader(HttpContentTypes::contentTypeHeader(), std::move(mimeType));
}

HttpResponse::~HttpResponse() {}


QByteArray HttpResponse::data() const {
    return _data;
}

HttpResponse::StatusCode HttpResponse::statusCode() const {
    return _statusCode;
}

void HttpResponse::addHeader(QByteArray &&name, QByteArray &&value) {
    _headers.emplace(std::move(name), std::move(value));
}

void HttpResponse::addHeader(QByteArray &&name, const QByteArray &value) {
    _headers.emplace(std::move(name), value);
}

void HttpResponse::addHeader(const QByteArray &name, QByteArray &&value) {
    _headers.emplace(name, std::move(value));
}

void HttpResponse::addHeader(const QByteArray &name, const QByteArray &value) {
    _headers.emplace(name, value);
}

void HttpResponse::addHeaders(const HttpResponder::HeaderList headers) {
    for (auto &&header : headers)
        addHeader(header.first, header.second);
}

void HttpResponse::clearHeader(const QByteArray &name) {
    _headers.erase(name);
}

void HttpResponse::clearHeaders() {
    _headers.clear();
}

void HttpResponse::setHeader(QByteArray &&name, const QByteArray &value) {
    clearHeader(name);
    addHeader(std::move(name), value);
}

void HttpResponse::setHeader(const QByteArray &name, const QByteArray &value) {
    clearHeader(name);
    addHeader(name, value);
}

void HttpResponse::setHeader(QByteArray &&name, QByteArray &&value) {
    clearHeader(name);
    addHeader(std::move(name), std::move(value));
}

void HttpResponse::setHeader(const QByteArray &name, QByteArray &&value) {
    clearHeader(name);
    addHeader(name, std::move(value));
}

void HttpResponse::setHeaders(HttpResponder::HeaderList headers) {
    for (auto &&header : headers)
        setHeader(header.first, header.second);
}

bool HttpResponse::hasHeader(const QByteArray &name) const {
    return _headers.find(name) != _headers.end();
}

QVector<QByteArray> HttpResponse::headers(const QByteArray &name) const {
    QVector<QByteArray> results;

    auto range = _headers.equal_range(name);

    for (auto it = range.first; it != range.second; ++it)
        results.append(it->second);

    return results;
}

bool HttpResponse::hasHeader(const QByteArray &name, const QByteArray &value) const {
    auto range = _headers.equal_range(name);

    auto condition = [&value] (const std::pair<QByteArray, QByteArray> &pair) {
        return pair.second == value;
    };

    return std::find_if(range.first, range.second, condition) != range.second;
}

void HttpResponse::write(HttpResponder &&responder) const {
    if (responder.socket()->state() != QAbstractSocket::ConnectedState)
        return;

    responder.writeStatusLine(_statusCode);

    for (auto &&header : _headers)
        responder.writeHeader(header.first, header.second);

    responder.writeHeader(HttpContentTypes::contentLengthHeader(),
                          QByteArray::number(_data.size()));

    responder.writeBody(_data);
}


QT_END_NAMESPACE