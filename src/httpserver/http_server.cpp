//
// Created by kodor on 1/25/22.
//

#include <QTcpServer>
#include "http_server.h"
#include "http_request.h"
#include "http_response.h"
#include "http_router.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qmetaobject.h>
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcHttpServer, "httpserver");

void HttpServer::handleNewConnections() {
    auto tcpServer = qobject_cast<QTcpServer *>(sender());

    Q_ASSERT(tcpServer);

    while (auto socket = tcpServer->nextPendingConnection()) {
        auto request = new HttpRequest(socket->peerAddress());
        QObject::connect(socket, &QTcpSocket::readyRead, this,
                [this, request, socket] {
            handleReadyRead(socket, request);
        });

        QObject::connect(socket, &QTcpSocket::disconnected, socket, [request, socket] () {
            if (!request->handling)
                socket->deleteLater();
        });

        QObject::connect(socket, &QObject::destroyed, socket, [request] () {
            delete request;
        });
    }
}

void HttpServer::handleReadyRead(QTcpSocket *socket, HttpRequest *request) {
    Q_ASSERT(socket);
    Q_ASSERT(request);


    if (!socket->isTransactionStarted())
        socket->startTransaction();

    if (request->state == HttpRequest::State::MessageComplete)
        request->clear();

    if (!request->parse(socket)) {
        socket->disconnect();
        return;
    }


    if (!request->parserState.upgrade &&
    request->state != HttpRequest::State::MessageComplete)
        return;

    socket->commitTransaction();
    request->handling = true;

    if (!handleRequest(*request, socket))
        Q_EMIT missingHandler(*request, socket);

    request->handling = false;

    if (socket->state() == QAbstractSocket::UnconnectedState)
        socket->deleteLater();

}

quint16 HttpServer::listen(const QHostAddress &address, quint16 port) {
    auto tcpServer = new QTcpServer(this);

    const auto listening = tcpServer->listen(address, port);

    if (listening) {
        bind(tcpServer);
        return tcpServer->serverPort();
    } else {
        qCCritical(lcHttpServer, "failed to listen %s",
                   tcpServer->errorString().toStdString().c_str());
    }

    delete tcpServer;
    return 0;
}

void HttpServer::bind(QTcpServer *server) {
    if (!server) {
        server = new QTcpServer(this);
        if (!server->listen()) {
            qCCritical(lcHttpServer, "QtTcpServer listen failed (%s)",
                       qPrintable(server->errorString()));
        }
    } else {
        if (!server->isListening())
            qCWarning(lcHttpServer) << "The TCP server" << server << "is not listening.";
        server->setParent(this);
    }
    QObject::connect(server, &QTcpServer::newConnection, this, &HttpServer::handleNewConnections,
                     Qt::UniqueConnection);
}

QVector<quint16> HttpServer::serverPorts() {
    QVector<quint16> ports;
    auto children = findChildren<QTcpServer *>();
    ports.reserve(children.count());
    std::transform(children.cbegin(), children.cend(), std::back_inserter(ports),
            [](const QTcpServer *server) { return server->serverPort(); });
    return ports;
}

QVector<QTcpServer *> HttpServer::servers() const {
    return findChildren<QTcpServer *>().toVector();
}

HttpResponder HttpServer::makeResponder(const HttpRequest &request, QTcpSocket *socket) {
    return HttpResponder(request, socket);
}


HttpServer::HttpServer(QObject *parent) {
    connect(this, &HttpServer::missingHandler, this,
            [=] (const HttpRequest &request, QTcpSocket *socket) {
        qCDebug(lcHttpServer) << "Missing handler: " << request.url().path();
        sendResponse(HttpResponder::StatusCode::NotFound, request, socket);
    });
}

HttpServer::~HttpServer() {}

HttpRouter * HttpServer::router() {
    return &_router;
}

void HttpServer::sendResponse(HttpResponse &&response,
        const HttpRequest &request,
        QTcpSocket *socket) {
    response.write(makeResponder(request, socket));
}

bool HttpServer::handleRequest(const HttpRequest &request, QTcpSocket *socket) {
    return _router.handleRequest(request, socket);
}

QT_END_NAMESPACE