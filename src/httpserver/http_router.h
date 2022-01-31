//
// Created by kodor on 1/25/22.
//

#ifndef QT_TCP_SERVER_HTTP_ROUTER_H
#define QT_TCP_SERVER_HTTP_ROUTER_H

#include <QtCore/qmap.h>

#include <memory>
#include "http_request.h"
#include "http_response.h"

QT_BEGIN_NAMESPACE

class QString;
class HttpRequest;
class QTcpSocket;
class QRegularExpressionMatch;
class HttpRouter;

class QTcpSocket;
class HttpRequest;
class HttpRoute;

class HttpRouter {
public:
    HttpRouter();
    ~HttpRouter();

    template <typename ViewHandler>
    std::function<void(QMap<quint8, QByteArray> &, QList<QString> &, const HttpRequest &, HttpResponder &&)>
            bindCaptured(ViewHandler &&handler, const QRegularExpressionMatch &match) const {
        return handler;
    }

    bool handleRequest(const HttpRequest &request, QTcpSocket *socket) const;


    bool addRoute(HttpRoute *route);

private:
    std::list<std::unique_ptr<HttpRoute>> _routes;

};

class HttpRoute {

public:
    using RouterHandler = std::function<void(const QRegularExpressionMatch &, const HttpRequest &,
            QTcpSocket *)>;

    explicit HttpRoute(const QString &pathPattern, RouterHandler &&routerHandler);
    explicit HttpRoute(QString pathPattern,
            const HttpRequest::Methods methods, RouterHandler &&routerHandler);

    explicit HttpRoute(QString pathPattern, const char *methods, RouterHandler &&routerHandler);

    HttpRoute(HttpRoute &&other) = delete;
    HttpRoute &operator=(HttpRoute &&other) = delete;

    virtual ~HttpRoute();

protected:
    bool exec(const HttpRequest &request, QTcpSocket *socket) const;

    bool hasValidMethods() const;

    bool createPathRegexp();

    virtual bool matches(const HttpRequest &request,
                         QRegularExpressionMatch *match) const;


private:
    QString pathPattern;
    HttpRequest::Methods methods;
    HttpRoute::RouterHandler routerHandler;

    QRegularExpression _pathRegexp;

    friend class HttpRouter;

};

#endif //QT_TCP_SERVER_HTTP_ROUTER_H
