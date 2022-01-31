//
// Created by kodor on 1/25/22.
//

#ifndef QT_TCP_SERVER_HTTP_CONTENT_TYPE_H
#define QT_TCP_SERVER_HTTP_CONTENT_TYPE_H

#include <QtCore/qbytearray.h>

QT_BEGIN_NAMESPACE

class HttpContentTypes {
public:
    static QByteArray contentTypeHeader();
    static QByteArray contentTypeXEmpty();
    static QByteArray contentTypeTextHTML();
    static QByteArray contentTypeJson();
    static QByteArray contentLengthHeader();
};

QT_END_NAMESPACE

#endif //QT_TCP_SERVER_HTTP_CONTENT_TYPE_H

