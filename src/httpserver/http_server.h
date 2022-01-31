//
// Created by kodor on 1/25/22.
//

#pragma once

#include "http_request.h"
#include "http_response.h"
#include "http_router.h"
#include "http_content_type.h"

#include <QtCore/qobject.h>
#include <QtNetwork/qhostaddress.h>
#include <tuple>

QT_BEGIN_NAMESPACE

class QTcpServer;
class QTcpSocket;


class HttpServer : public QObject {
    Q_OBJECT


public:


    explicit HttpServer(QObject *parent = nullptr);
    ~HttpServer();

    HttpRouter *router();

    using ViewHandler = std::function<void(
            QMap<quint8, QByteArray> &table,
            QList<QString> &transactionLog,
            const HttpRequest &request,
            HttpResponder &&responder)>;
    using BoundHandler = std::function<void(
            QMap<quint8, QByteArray> &table,
            QList<QString> &transactionLog,
            const HttpRequest &request,
            HttpResponder &&responder)>;

    bool route(QString &&pathPattern, ViewHandler &&handler) {

        auto routerHandler = [this, handler] (
                const QRegularExpressionMatch &match,
                const HttpRequest &request,
                QTcpSocket *socket) mutable {
            auto boundHandler = router()->bindCaptured<ViewHandler>(std::move(handler), match);
            response(boundHandler, request, socket);
        };
        return router()->addRoute(new HttpRoute(std::forward<QString>(pathPattern),
                                                std::move(routerHandler)));
    }

    void response(BoundHandler &boundHandler, const HttpRequest &request, QTcpSocket *socket) {
        //HttpResponse response(boundHandler(request));
        //sendResponse(std::move(response), request, socket);
        boundHandler(table, transactionLog, request, makeResponder(request, socket));
    }

    quint16 listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0);
    QVector<quint16> serverPorts();

    void bind(QTcpServer *server = nullptr);
    QVector<QTcpServer *> servers() const;

    void handleNewConnections();
    void handleReadyRead(QTcpSocket *socket, HttpRequest *request);

    bool handleRequest(const HttpRequest &request, QTcpSocket *socket);

    void sendResponse(HttpResponse &&response,
                      const HttpRequest &request,
                      QTcpSocket *socket);

Q_SIGNALS:
    void missingHandler(const HttpRequest &request, QTcpSocket *socket);

protected:
    static HttpResponder makeResponder(const HttpRequest &request, QTcpSocket *socket);

private:

    HttpRouter _router;
    QTcpServer *tcpServer;
    QMap<quint8, QByteArray> table;
    QList<QString> transactionLog;


};

QT_END_NAMESPACE