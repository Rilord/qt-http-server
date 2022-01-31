//
// Created by kodor on 1/25/22.
//

#ifndef QT_TCP_SERVER_HTTP_RESPONSE_H
#define QT_TCP_SERVER_HTTP_RESPONSE_H

#include <QtCore/qdebug.h>
#include <QtCore/qpair.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qmimetype.h>

#include <utility>
#include <initializer_list>
#include <functional>
#include <unordered_map>

QT_BEGIN_NAMESPACE

class QTcpSocket;
class HttpRequest;

class HttpResponderPrivate;

class HttpResponder final {

    friend class HttpServer;

public:
    enum class StatusCode {
        Continue = 100,
        Ok = 200,
        BadRequest = 400,
        InternalServerError = 500,
        Created = 201,
        Accepted,
        NoContent,

        NotFound,
        Forbidden,
        BadGateway,


    };

    using HeaderList = std::initializer_list<std::pair<QByteArray, QByteArray>>;

    HttpResponder(HttpResponder &&other);
    ~HttpResponder();

    void write(QIODevice *data,
            HeaderList headers,
            StatusCode status = StatusCode::Ok);


    void write(QIODevice *data,
               const QByteArray &mimeType,
               StatusCode status = StatusCode::Ok);

    void write(const QJsonDocument &document,
               HeaderList headers,
               StatusCode status = StatusCode::Ok);

    void write(const QJsonDocument &document,
               StatusCode status = StatusCode::Ok);

    void write(const QByteArray &data,
               HeaderList headers,
               StatusCode status = StatusCode::Ok);

    void write(const QByteArray &data,
               const QByteArray &mimeType,
               StatusCode status = StatusCode::Ok);

    void write(HeaderList headers, StatusCode status = StatusCode::Ok);
    void write(StatusCode status = StatusCode::Ok);

    void writeStatusLine(StatusCode status = StatusCode::Ok,
            const QPair<quint8, quint8> &version = qMakePair(1u, 1u));


    void writeHeader(const QByteArray &key, const QByteArray &value);
    void writeHeaders(HeaderList headers);

    void writeBody(const char *body, qint64 size);
    void writeBody(const char *body);
    void writeBody(const QByteArray &body);

    QTcpSocket *socket() const;

private:
    HttpResponder(const HttpRequest &request, QTcpSocket *socket);
    const HttpRequest &_request;
    QTcpSocket *const _socket;

    bool _bodyStarted { false };

};

class QJsonObject;

class HttpResponse {
public:
   using StatusCode = HttpResponder::StatusCode;

    HttpResponse() = delete;
    HttpResponse(const HttpResponse &other) = delete;

    HttpResponse(const StatusCode statusCode);

    HttpResponse(const char *data);

    HttpResponse(const QString &data);

    explicit HttpResponse(const QByteArray &data);

    explicit HttpResponse(QByteArray &&data);

    HttpResponse(const QJsonObject &data);
    HttpResponse(const QJsonArray &data);

    HttpResponse(const QByteArray &mimeType,
                        const QByteArray &data,
                        const StatusCode status = StatusCode::Ok);
    HttpResponse(QByteArray &&mimeType,
    const QByteArray &data,
    const StatusCode status = StatusCode::Ok);
    HttpResponse(const QByteArray &mimeType,
                        QByteArray &&data,
                        const StatusCode status = StatusCode::Ok);
    HttpResponse(QByteArray &&mimeType,
    QByteArray &&data,
    const StatusCode status = StatusCode::Ok);

    virtual ~HttpResponse();

    QByteArray data() const;

    StatusCode statusCode() const;

    void addHeader(QByteArray &&name, QByteArray &&value);
    void addHeader(QByteArray &&name, const QByteArray &value);
    void addHeader(const QByteArray &name, QByteArray &&value);
    void addHeader(const QByteArray &name, const QByteArray &value);

    void addHeaders(HttpResponder::HeaderList headerList);

    template <typename Container>
    void addHeaders(const Container &headerList) {
        for (const auto &header : headerList)
            addHeader(header.first, header.second);
    }

    void clearHeader(const QByteArray &name);
    void clearHeaders();

    void setHeader(QByteArray &&name, QByteArray &&value);
    void setHeader(QByteArray &&name, const QByteArray &value);
    void setHeader(const QByteArray &name, QByteArray &&value);
    void setHeader(const QByteArray &name, const QByteArray &value);

    void setHeaders(HttpResponder::HeaderList headers);

    bool hasHeader(const QByteArray &name) const;
    bool hasHeader(const QByteArray &name, const QByteArray &value) const;

    QVector<QByteArray> headers(const QByteArray &name) const;

    virtual void write(HttpResponder &&responder) const;

private:

    struct Hash {
        std::size_t operator()(const QByteArray &key) const {
            return qHash(key.toLower());
        }
    };

    QByteArray _data;
    HttpResponse::StatusCode _statusCode;
    std::unordered_map<QByteArray, QByteArray, Hash> _headers;
    bool derived { false };

};

QT_END_NAMESPACE

#endif //QT_TCP_SERVER_HTTP_RESPONSE_H
