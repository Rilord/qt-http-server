//
// Created by kodor on 1/25/22.
//

#include "http_request.h"

#include <array>
#include <string>



QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lc, "httpserver.request")

HttpRequest::HttpRequest(const QHostAddress &remoteAddress)
: _remoteAddress(remoteAddress) {}

QByteArray HttpRequest::header(const QByteArray &key) const {
    return _headers.value(headerHash(key)).second;
}

QByteArray HttpRequest::value(const QByteArray &key) const {
    return _headers.value(headerHash(key)).second;
}

HttpRequest::~HttpRequest() {}

bool HttpRequest::parse(QIODevice *socket) {
    const auto fragment = socket->readAll();

    if (fragment.size()) {
        _url.setScheme(QStringLiteral("http"));

        const auto parsed = parse(fragment);

        qCDebug(lc) << url();
    }

    return true;
}

// TODO: USE QT type for string, char and other

size_t HttpRequest::parse(const QByteArray &fragment) {

    for (auto &&it : fragment) {
        char input = it;

        switch (state) {
            case State::RequestMethodStart:
                if (!isChar(input) || isControl(input) || isSpecial(input) ) {
                    return -1;
                } else {
                    state = State::RequestMethod;
                    parserState.method.push_back(input);
                }
                break;
            case State::RequestMethod:
                if (input == ' ') {
                    state = State::RequestUrlStart;
                } else if (!isChar(input) || isControl(input) || isSpecial(input)) {
                    return -1;
                } else {
                    parserState.method.push_back(input);
                }
                break;
            case State::RequestUrlStart:
                if (isControl(input)) {
                    return -1;
                } else {
                    state = State::RequestUrl;
                    parserState.url.push_back(input);
                }
                break;
            case State::RequestUrl:
                if (input == ' ') {
                    state = State::RequestHttpVersion_h;
                } else if (input == '\r') {
                    parserState.http_major = 0;
                    parserState.http_minor = 9;

                    return 0;
                } else if (isControl(input)) {
                    return -1;
                } else {
                    parserState.url.push_back(input);
                }
                break;
            case State::RequestHttpVersion_h:
                if (input == 'H') {
                    state = State::RequestHttpVersion_ht;
                } else {
                    return -1;
                }
                break;
            case State::RequestHttpVersion_ht:
                if (input == 'T') {
                    state = State::RequestHttpVersion_htt;
                } else {
                    return -1;
                }
                break;
            case State::RequestHttpVersion_htt:
                if (input == 'T') {
                    state = State::RequestHttpVersion_http;
                } else {
                    return -1;
                }
                break;
            case State::RequestHttpVersion_http:
                if (input == 'P') {
                    state = State::RequestHttpVersion_slash;
                } else {
                    return -1;
                }
                break;
            case State::RequestHttpVersion_slash:
                if (input == '/') {
                    parserState.http_minor = 0;
                    parserState.http_major = 0;
                    state = State::RequestHttpVersion_majorStart;
                } else {
                    return -1;
                }
                break;
            case State::RequestHttpVersion_majorStart:
                if (isDigit(input)) {
                    parserState.http_major = input - '0';
                    state = State::RequestHttpVersion_major;
                } else {
                    return -1;
                }
                break;
            case State::RequestHttpVersion_major:
                if (input == '.') {
                    state = State::RequestHttpVersion_minorStart;
                } else if (isDigit(input)) {
                    parserState.http_major = parserState.http_major * 10 + input - '0';
                } else {
                    return -1;
                }
                break;
            case State::RequestHttpVersion_minorStart:
                if (isDigit(input)) {
                    parserState.http_minor = input - '0';
                    state = State::RequestHttpVersion_minor;
                } else {
                    return -1;
                }
                break;
            case State::RequestHttpVersion_minor:
                if (input == '\r') {
                    state = State::ResponseHttpVersion_newline;
                } else if (isDigit(input)) {
                    parserState.http_minor = parserState.http_minor * 10 + input - '0';
                } else {
                    return -1;
                }
                break;
            case State::ResponseHttpVersion_newline:
                if (input == '\n') {
                    state = State::HeaderLineStart;
                } else {
                    return -1;
                }
                break;
            case State::HeaderLineStart:
                if (input == '\r') {
                    state = State::ExpectingNewline_3;
                } else if ( !_headers.empty() && (input == ' ' || input == '\t')) {
                    state = State::HeaderLws;
                } else if (!isChar(input) || isControl(input) || isSpecial(input)) {
                    return -1;
                } else {
                    if (!parserState.currentHeaderName.isEmpty()) {
                        const auto key = QByteArray(parserState.currentHeaderName.toStdString().c_str(),
                                                    parserState.currentHeaderName.size());
                        const auto value = QByteArray(parserState.currentHeaderValue.toStdString().c_str(),
                                                      parserState.currentHeaderValue.size());
                        _headers[headerHash(key)] =
                                qMakePair(key, value);
                        parserState.currentHeaderName.clear();
                        parserState.currentHeaderValue.clear();
                    }
                    parserState.currentHeaderName.push_back(input);
                    state = State::HeaderName;
                }
                break;
            case State::HeaderLws:
                if (input == '\r') {
                    state = State::ExpectingNewline_2;
                } else if (input == ' ' || input == '\t') {

                } else if (isControl(input)) {
                    return -1;
                } else {
                    state = State::HeaderValue;
                    parserState.currentHeaderValue.push_back(input);
                }
                break;
            case State::HeaderName:
                if (input == ':') {
                    state = State::SpaceBeforeHeaderValue;
                } else if (!isChar(input) || isControl(input) || isSpecial(input)) {
                    return -1;
                } else {
                    parserState.currentHeaderName.push_back(input);
                }
                break;
            case State::SpaceBeforeHeaderValue:
                if (input == ' ') {
                    state = State::HeaderValue;
                } else {
                    return -1;
                }
                break;
            case State::HeaderValue:
                if (input == '\r') {
                    if (parserState.method == "POST" ||
                        parserState.method == "PUT" ||
                        parserState.method == "DELETE") {
                        if (strcasecmp(parserState.currentHeaderName.toStdString().c_str(), "Content-Length") == 0) {
                            parserState.contentSize = std::atoi(parserState.currentHeaderValue.toStdString().c_str());
                        } else if (strcasecmp(parserState.currentHeaderName.toStdString().c_str(), "Transfer-Encoding") == 0) {
                            if (strcasecmp(parserState.currentHeaderValue.toStdString().c_str(), "chunked") == 0)
                                parserState.chunked = true;
                        }
                    }
                    state = State::ExpectingNewline_2;
                } else if (isControl(input)) {
                    return -1;
                } else {
                    parserState.currentHeaderValue.push_back(input);
                }
                break;
            case State::ExpectingNewline_2:
                if (input == '\n') {
                    state = State::HeaderLineStart;
                } else {
                    return -1;
                }
                break;
            case State::ExpectingNewline_3: {
                // iterate over headers and find connection

                auto it = _headers.find(headerHash("Connection"));

                if (it != _headers.end()) {
                    parserState.keepAlive =
                            strcasecmp(
                                    it.value().second,
                                    "Keep-Alive") == 0;
                } else {
                    if (parserState.http_major > 1 ||
                        (parserState.http_major == 1 && parserState.http_minor == 1))
                        parserState.keepAlive = true;
                }

                if (parserState.chunked) {
                    state = State::ChunkSize;
                } else if (parserState.contentSize == 0) {
                    if (input == '\n')
                        return 0;
                    else
                        return -1;
                } else {
                    state = State::Post;
                }
                break;
            }
            case State::Post:
                --parserState.contentSize;
                parserState.content.push_back(input);

                if (parserState.contentSize == 0)
                    return 0;
                break;
            case State::ChunkSize:
                if (isalnum(input))
                    parserState.chunkSizeStr.push_back(input);
                else if (input == ';')
                    state = State::ChunkExtensionName;
                else if (input == '\r')
                    state = State::ChunkSizeNewLine;
                else
                    return -1;
                break;
            case State::ChunkExtensionName:
                if (input == '=')
                    state = State::ChunkExtensionValue;
                else if (input == '\r')
                    state = State::ChunkSizeNewLine;
                else
                    return -1;
                break;
            case State::ChunkExtensionValue:
                if (input == '\r')
                    state = State::ChunkSizeNewLine;
                else
                    return -1;
                break;
            case State::ChunkSizeNewLine:
                if (input == '\n') {
                    parserState.chunkSize = std::strtol(parserState.chunkSizeStr.toStdString().data(), NULL, 16);
                    parserState.chunkSizeStr.clear();
                    parserState.content.reserve(parserState.content.size() + parserState.chunkSize);

                    if (parserState.chunkSize == 0)
                        state = State::ChunkSizeNewLine_2;
                    else
                        state = State::ChunkData;
                } else {
                    return -1;
                }
                break;
            case State::ChunkSizeNewLine_2:
                if (input == '\r')
                    state = State::ChunkSizeNewLine_3;
                else if (isalpha(input))
                    state = State::ChunkTrailerName;
                else
                    return -1;
                break;
            case State::ChunkSizeNewLine_3:
                if (input == '\n')
                    return 0;
                else
                    return -1;
                break;
            case State::ChunkTrailerName:
                if (input == ':')
                    state = State::ChunkTrailerValue;
                else
                    return -1;
                break;
            case State::ChunkTrailerValue:
                if (input == '\r')
                    state = State::ChunkSizeNewLine;
                else
                    return -1;
                break;
            case State::ChunkData:
                parserState.content.push_back(input);

                if (--parserState.chunkSize == 0)
                    state = State::ChunkDataNewLine_1;
                break;
            case State::ChunkDataNewLine_1:
                if (input == '\r')
                    state = State::ChunkDataNewLine_2;
                else
                    return -1;
                break;
            case State::ChunkDataNewLine_2:
                if (input == '\n')
                    state = State::ChunkSize;
                else
                    return -1;
                break;
            default:
                return -1;
        }

    }

    return -2;
}

uint HttpRequest::headerHash(const QByteArray &key) const {
    return qHash(key.toLower(), headersSeed);
}

void HttpRequest::clear() {
    _url.clear();
    lastHeader.clear();
    _headers.clear();
    _body.clear();
}

bool HttpRequest::parseUrl(const char *at, size_t length, bool connect, QUrl *url) {
    return true;
}

HttpRequest::Method HttpRequest::method() const {
    if (parserState.method == "POST") {
        return Method::POST;
    } else if (parserState.method == "PUT") {
        return Method::PUT;
    } else if (parserState.method == "DELETE") {
        return Method::DELETE;
    } else if (parserState.method == "GET") {
        return Method::GET;
    } else {
        return Method::All;
    }
}

QVariantMap HttpRequest::headers() const {
    QVariantMap ret;

    for (auto it : _headers)
        ret.insert(QString::fromUtf8(it.first), it.second);
    return ret;
}

QByteArray HttpRequest::body() const {
    return parserState.content;
}

QUrl HttpRequest::url() const {
    return QUrl(parserState.url);
}

QT_END_NAMESPACE