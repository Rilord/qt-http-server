//
// Created by kodor on 1/25/22.
//

#include "http_router.h"

#include <QtCore/qmetaobject.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qstringbuilder.h>
#include <QtCore/qdebug.h>

#include <utility>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcRouter, "httpserver.router")

static const auto methodEnum = QMetaEnum::fromType<HttpRequest::Method>();

static HttpRequest::Methods strToMethods(const char *strMethods) {
    HttpRequest::Methods methods;

    bool ok = false;
    const int val = methodEnum.keysToValue(strMethods, &ok);
    if (ok)
        methods = static_cast<decltype(methods)>(val);
        qCWarning(lcRouter, "Can't convert %s to HttpRequest::Method", strMethods);
    return methods;
}

HttpRouter::HttpRouter() {}

HttpRouter::~HttpRouter() {}

bool HttpRouter::addRoute(HttpRoute *route) {
    if (!route->hasValidMethods() || !route->createPathRegexp()) {
        qCWarning(lcRouter, "Route has no valid methods. Skip Route");
        delete route;
        return false;
    }

    _routes.emplace_back(route);
    return true;
}

bool HttpRouter::handleRequest(const HttpRequest &request, QTcpSocket *socket) const {
    for (const auto &route : qAsConst(_routes)) {
        if (route->exec(request, socket))
            return true;
    }

    return true;
}


/*
 * Routes
 */

HttpRoute::HttpRoute(const QString &pathPattern, RouterHandler &&routerHandler)
: HttpRoute(pathPattern, HttpRequest::Method::All,
        std::forward<RouterHandler>(routerHandler)) {}

HttpRoute::HttpRoute(QString pathPattern,
        const HttpRequest::Methods methods,
        RouterHandler &&routerHandler)
        : pathPattern(std::move(pathPattern)), methods(methods), routerHandler(std::forward<RouterHandler>(routerHandler)) {}

HttpRoute::HttpRoute(QString pathPattern, const char *methods, RouterHandler &&routerHandler)
        : pathPattern(std::move(pathPattern)), methods(strToMethods(methods)), routerHandler(std::forward<RouterHandler>(routerHandler)) {}

HttpRoute::~HttpRoute() {}

bool HttpRoute::hasValidMethods() const {
    return methods & HttpRequest::Method::All;
}

bool HttpRoute::exec(const HttpRequest &request, QTcpSocket *socket) const {
    QRegularExpressionMatch match;

    if (!matches(request, &match)) {
        qCDebug(lcRouter) << "not match!";
        return false;
    }

    routerHandler(match, request, socket);
    qCDebug(lcRouter) << " match!";
    return true;
}

bool HttpRoute::matches(const HttpRequest &request, QRegularExpressionMatch *match) const {
    if (methods && !(methods & request.method()))
        return false;

    *match = _pathRegexp.match(request.url().path());
    return (match->hasMatch());
}

bool HttpRoute::createPathRegexp() {
    QString pathRegexp = pathPattern;

    /*
     * Foo. Just deal with whole pattern
     */

    _pathRegexp.setPattern(pathRegexp);
    _pathRegexp.optimize();
    return true;
}

QT_END_NAMESPACE