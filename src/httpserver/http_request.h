//
// Created by kodor on 1/25/22.
//

#ifndef QT_TCP_SERVER_HTTP_REQUEST_H
#define QT_TCP_SERVER_HTTP_REQUEST_H

#include <QtCore/qbytearray.h>
#include <QtCore/qpair.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>
#include <QtCore/qdebug.h>
#include <QtCore/qglobal.h>
#include <QtCore/qurlquery.h>
#include <QtNetwork/qhostaddress.h>
#include <QtCore/qloggingcategory.h>

QT_BEGIN_NAMESPACE

class QRegularExpression;
class QString;
class QTcpSocket;

class HttpRequest : public QSharedData {

    Q_GADGET

public:
    virtual ~HttpRequest();

    enum class Method {
        Get    = 0x0001,
        Put    = 0x0002,
        Delete = 0x0004,
        Post   = 0x0008,
        GET    = Get   ,
        PUT    = Put   ,
        DELETE = Delete,
        POST   = Post  ,
        All = Get | Put | Delete | Post,
    };

    Q_ENUM(Method)
    Q_DECLARE_FLAGS(Methods, Method)
    Q_FLAG(Methods)

    QByteArray value(const QByteArray &key) const;
    QUrl url() const;
    Method method() const;
    QVariantMap headers() const;
    QByteArray body() const;


protected:
    struct HttpParserState {
        QString method, url;
        bool upgrade = true;
        unsigned short http_major = 0, http_minor = 0;
        QString currentHeaderName;
        QString currentHeaderValue;
        bool keepAlive = false;
        bool chunked = false;
        size_t contentSize = 0;
        QByteArray content;
        QString chunkSizeStr;
        size_t chunkSize = 0;
    } parserState;

    bool handling { false };

private:

    friend class HttpServer;
    friend class HttpResponse;


    Q_DISABLE_COPY(HttpRequest)

    quint16  port = 0;

    enum class State {
        RequestMethodStart,
        RequestMethod,
        RequestUrlStart,
        RequestUrl,
        RequestHttpVersion_h,
        RequestHttpVersion_ht,
        RequestHttpVersion_htt,
        RequestHttpVersion_http,
        RequestHttpVersion_slash,
        RequestHttpVersion_majorStart,
        RequestHttpVersion_major,
        RequestHttpVersion_minorStart,
        RequestHttpVersion_minor,
        ResponseHttpVersion_newline,

        HeaderLineStart,
        HeaderLws,
        HeaderName,
        SpaceBeforeHeaderValue,
        HeaderValue,
        ExpectingNewline_2,
        ExpectingNewline_3,
        Post,
        ChunkSize,
        ChunkExtensionName,
        ChunkExtensionValue,
        ChunkSizeNewLine,
        ChunkSizeNewLine_2,
        ChunkSizeNewLine_3,
        ChunkTrailerName,
        ChunkTrailerValue,

        ChunkDataNewLine_1,
        ChunkDataNewLine_2,
        ChunkData,
        MessageComplete
        } state = State::RequestMethodStart;


    QByteArray _body;

    QUrl _url;

    QByteArray header(const QByteArray &key) const;

    bool parse(QIODevice *socket);
    size_t parse(const QByteArray &fragment);

    QByteArray lastHeader;
    QMap<uint, QPair<QByteArray, QByteArray>> _headers;
    const uint headersSeed = uint(qGlobalQHashSeed());
    uint headerHash(const QByteArray &key) const;

    void clear();

    QHostAddress _remoteAddress;

    bool parseUrl(const char *at, size_t length, bool connect, QUrl *url);

    explicit HttpRequest(const QHostAddress &remoteAddress);

    static bool ifConnection(const QByteArray &item) {
        return item == "Connection";
    }


};


inline bool isChar(int c) {
    return c >= 0 && c <= 127;
}

inline bool isControl(int c) {
    return (c >= 0 && c <= 31) || (c == 127);
}

inline bool isSpecial(int c) {
    switch(c) {
        case '(' : case ')' : case '<' : case '>' : case '@':
        case ',' : case ';' : case ':' : case '\\' : case '"':
        case '/' : case '[' : case ']' : case '?' : case '=':
        case '{' : case '}' : case ' ' : case '\t' :
            return true;
        default:
            return false;
    }
}

inline bool isDigit(int c) {
    return c >= '0' && c <= '9';
}

QT_END_NAMESPACE

#endif //QT_TCP_SERVER_HTTP_REQUEST_H
