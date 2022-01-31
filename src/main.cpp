//
// Created by kodor on 1/25/22.
//

#include <QtCore>
#include <httpserver/http_server.h>



int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    HttpServer server;

    server.route("/api", [] (
            QMap<quint8, QByteArray> &table,
            QList<QString> &transactionLog,
            const HttpRequest &request,
            HttpResponder &&responder) {

        switch (request.method()) {
            case HttpRequest::Method::GET: {

                QJsonDocument ret;
                QJsonArray data;

                QMap<quint8, QByteArray>::const_iterator i = table.constBegin();

                while (i != table.constEnd()) {
                    QJsonObject obj;
                    obj["id"] = i.key();
                    obj["value"] = QString(i.value());
                    data.append(obj);
                    ++i;
                }

                ret.setArray(data);

                responder.write(ret,
                                {{
                                         HttpContentTypes::contentTypeHeader(),
                                         HttpContentTypes::contentTypeJson()
                                 }},
                                HttpResponder::StatusCode::Ok);

                break;
            }
            case HttpRequest::Method::POST: {
                auto content = QJsonDocument::fromJson(request.body());

                if (!content.object().contains("value") || !content.object().contains("id")) {
                    responder.write("No value or id provided!", {{  }},
                                    HttpResponder::StatusCode::NoContent);
                    return;
                }

                auto key = content["id"].toInt();
                auto value = content["value"].toString().toLocal8Bit();


                if (!table.contains(key)) {
                    table[key] = value;
                }
                else {
                    responder.write("An element with such already exists",
                                    {{}},
                                    HttpResponder::StatusCode::Forbidden);
                }

                transactionLog.push_back(
                        QString("An element with id %1 has been added").
                                arg(QString::number(key))
                );

                auto location = QString::number(key);

                responder.write(location.toLocal8Bit(),
                                {{ "Location", location.toLocal8Bit() }},
                                HttpResponder::StatusCode::Created);
                break;
            }
            case HttpRequest::Method::DELETE: {
                auto content = QJsonDocument::fromJson(request.body());

                if (!content.object().contains("id")) {
                    responder.write("No id provided!", {{  }},
                                    HttpResponder::StatusCode::NoContent);
                    return;
                }

                auto key = content["id"].toInt();

                if (table.contains(key))
                    table.remove(key);
                else
                    responder.write("No such element in table",
                                    {{  }},
                                    HttpResponder::StatusCode::Created);


                auto msg = QString("An item with id(%1) deleted").arg(key);

                transactionLog.push_back(msg);

                responder.write(msg.toLocal8Bit(),
                                {{  }},
                                HttpResponder::StatusCode::Created);


                break;
            }
            case HttpRequest::Method::PUT: {
                auto content = QJsonDocument::fromJson(request.body());

                if (!content.object().contains("value") || !content.object().contains("id")) {
                    responder.write("No value provided!", {{  }},
                                    HttpResponder::StatusCode::NoContent);
                    return;
                }

                auto key = content["id"].toInt();
                auto value = content["value"].toString().toLocal8Bit();

                QString msg;

                if (!table.contains(key)) {
                    table[key] = value;
                    msg = QString("An element with id %1 has been added").arg(key);
                    transactionLog.push_back(msg);
                }
                else {
                    table[key] = value;
                    msg = QString("An element with id %1 has been modified").arg(key);
                    transactionLog.push_back(msg);
                }

                auto location = QString::number(key);

                responder.write(msg.toLocal8Bit(),
                                {{ "Location", location.toLocal8Bit() }},
                                HttpResponder::StatusCode::Created);
                break;
            }
            default:
                break;
        }
    });

    server.route("/test", [] (
            QMap<quint8, QByteArray> &table,
            QList<QString> &transactionLog,
            const HttpRequest &request,
            HttpResponder &&responder) {

        auto htmlMessage= QString("<html>\n<body>\n%1</body>\n</html>");
        auto body = QString();

        auto htmlTableRow = QString("<tr>\n%1</tr><br/>");
        auto htmlTableData = QString("<td>%1</td>");

        QMap<quint8, QByteArray>::const_iterator i = table.constBegin();

        while (i != table.constEnd()) {
            auto td1 = htmlTableData.arg(i.key());
            auto td2 = htmlTableData.arg(QString(i.value()));
            td1.append(" ");
            td1.append(td2);
            auto row = htmlTableRow.arg(td1);
            body.append(row);
            ++i;
        }

        body.append("<br/>");

        for (auto &it : transactionLog) {
            auto td = htmlTableData.arg(it);
            auto row = htmlTableRow.arg(td);
            body.append(row);
        }

        std::initializer_list<std::pair<QByteArray, QByteArray>> headers =
                {{ HttpContentTypes::contentTypeHeader(), HttpContentTypes::contentTypeTextHTML()}};

        responder.write(htmlMessage.arg(body).toLocal8Bit(), headers,HttpResponder::StatusCode::Ok);

    });

    const auto port = server.listen(QHostAddress::LocalHost, 1234);

    if (!port) {
        qDebug() << "HTTP server failed to listen on port.";
        return 0;
    }

    qDebug() << "HTTP server running on http://127.0.0.1:1234";

    return app.exec();
}
