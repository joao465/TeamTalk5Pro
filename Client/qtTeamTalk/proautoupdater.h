/*
 * TeamTalk 5 Pro automatic updater for the joao465/TeamTalk5Pro fork.
 *
 * This file is part of the Qt TeamTalk client and follows the same GPL
 * licensing terms as Client/qtTeamTalk.
 */

#ifndef PROAUTOUPDATER_H
#define PROAUTOUPDATER_H

#include "appinfo.h"

#include <QAbstractButton>
#include <QAction>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>
#include <QWidget>

class ProAutoUpdater : public QObject
{
public:
    explicit ProAutoUpdater(QWidget* parentWindow)
        : QObject(parentWindow)
        , m_parentWindow(parentWindow)
    {
    }

    void installMenuHook()
    {
        if (!m_parentWindow)
            return;

        // Replace only the original MainWindow handler for this action. Do not
        // disturb any Qt/menu internals that may also observe the QAction.
        if (QAction* action = m_parentWindow->findChild<QAction*>("actionCheckUpdate"))
        {
            action->disconnect(m_parentWindow);
            QObject::connect(action, &QAction::triggered, this, [this]() {
                checkForUpdates(true);
            });
        }
    }

    void checkForUpdates(bool manualCheck = false)
    {
        QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/joao465/TeamTalk5Pro/releases/latest")));
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
        request.setRawHeader("User-Agent", "TeamTalk-5-Pro-Updater");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

        auto* manager = new QNetworkAccessManager(this);
        QNetworkReply* reply = manager->get(request);
        QObject::connect(reply, &QNetworkReply::finished, this,
                         [this, manager, reply, manualCheck]() {
            handleReleaseReply(reply, manualCheck);
            reply->deleteLater();
            manager->deleteLater();
        });
    }

private:
    static QString normalizeVersion(const QString& text)
    {
        // Accept tags such as "pro-v5.26.3", "v5.26.3" or "5.26.3".
        QRegularExpression re(QStringLiteral("(\\d+(?:\\.\\d+)+)"));
        QRegularExpressionMatch match = re.match(text);
        return match.hasMatch() ? match.captured(1) : QString();
    }

    static bool isNewerVersion(const QString& candidate)
    {
        const QVersionNumber current = QVersionNumber::fromString(QStringLiteral(APPVERSION_SHORT));
        const QVersionNumber available = QVersionNumber::fromString(candidate);
        if (current.isNull() || available.isNull())
            return false;
        return QVersionNumber::compare(available, current) > 0;
    }

    void handleReleaseReply(QNetworkReply* reply, bool manualCheck)
    {
        if (reply->error() != QNetworkReply::NoError)
        {
            if (manualCheck)
            {
                QMessageBox::warning(m_parentWindow,
                                     QString::fromUtf8("Atualização do TeamTalk"),
                                     QString::fromUtf8("Não foi possível verificar atualizações agora.\n\n%1")
                                         .arg(reply->errorString()));
            }
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            if (manualCheck)
            {
                QMessageBox::warning(m_parentWindow,
                                     QString::fromUtf8("Atualização do TeamTalk"),
                                     QString::fromUtf8("A resposta do servidor de atualizações é inválida."));
            }
            return;
        }

        const QJsonObject release = document.object();
        if (release.value(QStringLiteral("draft")).toBool() ||
            release.value(QStringLiteral("prerelease")).toBool())
        {
            if (manualCheck)
            {
                QMessageBox::information(m_parentWindow,
                                         QString::fromUtf8("Atualização do TeamTalk"),
                                         QString::fromUtf8("Você já está usando a versão mais recente.\n\nVersão atual: %1")
                                             .arg(QStringLiteral(APPVERSION_SHORT)));
            }
            return;
        }

        const QString availableVersion = normalizeVersion(release.value(QStringLiteral("tag_name")).toString());
        if (availableVersion.isEmpty() || !isNewerVersion(availableVersion))
        {
            if (manualCheck)
            {
                QMessageBox::information(m_parentWindow,
                                         QString::fromUtf8("Atualização do TeamTalk"),
                                         QString::fromUtf8("Você já está usando a versão mais recente.\n\nVersão atual: %1")
                                             .arg(QStringLiteral(APPVERSION_SHORT)));
            }
            return;
        }

        QString assetName;
        QUrl assetUrl;
        QByteArray expectedSha256;

        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& value : assets)
        {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (name.contains(QStringLiteral("TeamTalk_5_Pro_"), Qt::CaseInsensitive) &&
                name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
            {
                assetName = name;
                assetUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
                QString digest = asset.value(QStringLiteral("digest")).toString();
                if (digest.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
                    expectedSha256 = digest.mid(7).toLatin1().toLower();
                break;
            }
        }

        if (!assetUrl.isValid() || assetName.isEmpty())
        {
            if (manualCheck)
            {
                QMessageBox::warning(m_parentWindow,
                                     QString::fromUtf8("Atualização do TeamTalk"),
                                     QString::fromUtf8("A versão %1 foi publicada, mas o instalador do Windows não foi encontrado na Release.")
                                         .arg(availableVersion));
            }
            return;
        }

        QMessageBox answer(m_parentWindow);
        answer.setWindowTitle(QString::fromUtf8("Atualização do TeamTalk"));
        answer.setText(QString::fromUtf8(
            "Uma nova versão de TeamTalk está disponível!\n\n"
            "Versão atual: %1\n"
            "Nova versão: %2\n\n"
            "Deseja atualizar agora?")
            .arg(QStringLiteral(APPVERSION_SHORT), availableVersion));
        QAbstractButton* yesButton = answer.addButton(QString::fromUtf8("Sim"), QMessageBox::YesRole);
        answer.addButton(QString::fromUtf8("Não"), QMessageBox::NoRole);
        answer.setIcon(QMessageBox::Question);
        answer.exec();

        if (answer.clickedButton() == yesButton)
            downloadInstaller(assetUrl, assetName, expectedSha256, availableVersion);
    }

    void downloadInstaller(const QUrl& url,
                           const QString& assetName,
                           const QByteArray& expectedSha256,
                           const QString& availableVersion)
    {
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", "TeamTalk-5-Pro-Updater");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

        auto* manager = new QNetworkAccessManager(this);
        QNetworkReply* reply = manager->get(request);

        auto* progress = new QProgressDialog(QString::fromUtf8("Baixando TeamTalk %1...").arg(availableVersion),
                                             QString::fromUtf8("Cancelar"),
                                             0, 100,
                                             m_parentWindow);
        progress->setWindowTitle(QString::fromUtf8("Atualização do TeamTalk"));
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->setAutoClose(false);
        progress->setValue(0);

        QObject::connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
        QObject::connect(reply, &QNetworkReply::downloadProgress, progress,
                         [progress](qint64 received, qint64 total) {
            if (total > 0)
                progress->setValue(int((received * 100) / total));
        });

        QObject::connect(reply, &QNetworkReply::finished, this,
                         [this, manager, reply, progress, assetName, expectedSha256]() {
            progress->hide();
            progress->deleteLater();

            if (reply->error() != QNetworkReply::NoError)
            {
                if (reply->error() != QNetworkReply::OperationCanceledError)
                {
                    QMessageBox::critical(m_parentWindow,
                                          QString::fromUtf8("Atualização do TeamTalk"),
                                          QString::fromUtf8("Falha ao baixar a atualização.\n\n%1")
                                              .arg(reply->errorString()));
                }
                reply->deleteLater();
                manager->deleteLater();
                return;
            }

            const QByteArray installerData = reply->readAll();
            if (!expectedSha256.isEmpty())
            {
                const QByteArray actualSha256 = QCryptographicHash::hash(installerData, QCryptographicHash::Sha256).toHex().toLower();
                if (actualSha256 != expectedSha256)
                {
                    QMessageBox::critical(m_parentWindow,
                                          QString::fromUtf8("Atualização do TeamTalk"),
                                          QString::fromUtf8("A verificação de integridade da atualização falhou. O instalador não será executado."));
                    reply->deleteLater();
                    manager->deleteLater();
                    return;
                }
            }

            QString tempFolder = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            if (tempFolder.isEmpty())
                tempFolder = QDir::tempPath();

            const QString safeName = QFileInfo(assetName).fileName();
            const QString installerPath = QDir(tempFolder).filePath(safeName);
            QFile installer(installerPath);
            if (!installer.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
                installer.write(installerData) != installerData.size())
            {
                QMessageBox::critical(m_parentWindow,
                                      QString::fromUtf8("Atualização do TeamTalk"),
                                      QString::fromUtf8("Não foi possível salvar o instalador em:\n%1")
                                          .arg(QDir::toNativeSeparators(installerPath)));
                reply->deleteLater();
                manager->deleteLater();
                return;
            }
            installer.close();

            reply->deleteLater();
            manager->deleteLater();

            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(installerPath)))
            {
                QMessageBox::critical(m_parentWindow,
                                      QString::fromUtf8("Atualização do TeamTalk"),
                                      QString::fromUtf8("O instalador foi baixado, mas não pôde ser iniciado.\n\nArquivo: %1")
                                          .arg(QDir::toNativeSeparators(installerPath)));
                return;
            }

            // Leave the installer responsible for closing/replacing files.
            // A short delay lets Windows start the setup process first.
            QTimer::singleShot(500, qApp, &QCoreApplication::quit);
        });
    }

    QWidget* m_parentWindow = nullptr;
};

#endif // PROAUTOUPDATER_H
