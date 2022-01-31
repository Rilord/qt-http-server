//
// Created by kodor on 1/25/22.
//

#include "http_content_type.h"

QT_BEGIN_NAMESPACE

QByteArray HttpContentTypes::contentTypeHeader() {
    return QByteArrayLiteral("Content-Type");
}

QByteArray HttpContentTypes::contentTypeXEmpty()  {
    return QByteArrayLiteral("application/x-empty");
}

QByteArray HttpContentTypes::contentTypeTextHTML() {
    return QByteArrayLiteral("text/html");
}

QByteArray HttpContentTypes::contentTypeJson() {
    return QByteArrayLiteral("application/json");
}

QByteArray HttpContentTypes::contentLengthHeader() {
    return QByteArrayLiteral("Content-Length");
}

QT_END_NAMESPACE