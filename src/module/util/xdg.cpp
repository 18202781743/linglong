/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "xdg.h"

#include <QDir>
#include <QMap>
#include <QStandardPaths>
#include <QDebug>

namespace linglong {
namespace util {

QDir userRuntimeDir()
{
    return QDir(QStandardPaths::standardLocations(QStandardPaths::RuntimeLocation).value(0));
}

QDir userCacheDir()
{
    return QDir(QStandardPaths::standardLocations(QStandardPaths::GenericCacheLocation).value(0));
}

//! https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
//! \param exec
//! \return
//! TODO: should compatibility with：
//!     env a=b c
QStringList parseExec(const QString &exec)
{
    // const auto Backtick = '`';
    const auto Quote = '"';
    const auto Space = ' ';
    const auto BackSlash = '\\';

    QStringList args;
    bool argBegin = false;
    bool inQuote = false;
    QString arg;
    for (int i = 0; i < exec.length(); ++i) {
        switch (exec.at(i).toLatin1()) {
        case BackSlash:
            if (inQuote) {
                inQuote = !inQuote;
                arg.push_back(exec.at(i));
                break;
            }

            argBegin = true;
            inQuote = true;
            break;
        case Quote:
            if (inQuote) {
                inQuote = !inQuote;
                arg.push_back(exec.at(i));
                break;
            }
            break;
        case Space:
            if (inQuote) {
                inQuote = !inQuote;
                arg.push_back(exec.at(i));
                break;
            }

            // terminal arg with space
            if (!argBegin) {
            } else {
                args.push_back(arg);
                arg = "";
            }
            argBegin = false;
            break;
        default:
            argBegin = true;
            arg.push_back(exec.at(i));
            break;
        }
    }
    if (!arg.isEmpty()) {
        args.push_back(arg);
    }
    return args;
}

QPair<QString, QString> parseEnvKeyValue(QString env, const QString &sep)
{
    QRegExp exp("(\\$\\{.*\\})");
    exp.setMinimal(true);
    exp.indexIn(env);

    for (auto const &envReplace : exp.capturedTexts()) {
        auto envKey = QString(envReplace).replace("$", "").replace("{", "").replace("}", "");
        auto envValue = qEnvironmentVariable(envKey.toStdString().c_str());
        env.replace(envReplace, envValue);
    }

    auto sepPos = env.indexOf(sep);

    if (sepPos < 0) {
        return {env, ""};
    }

    return {env.left(sepPos), env.right(env.length() - sepPos - 1)};
}

QStringList convertSpecialCharacters(const QStringList &args)
{
    QStringList retArgs;
    retArgs.clear();
    for (auto arg : args) {
        arg.replace(' ', QString("\\%1").arg(' '));
        arg.replace('"', QString("\\%1").arg('"'));
        retArgs.push_back(arg);
    }
    return retArgs;
}


QMap<QString, QString> getUserDirMap()
{
    static QMap<QString, QString> const USER_DIR_MAP = {
        {"desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)},
        {"documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)},
        {"downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)},
        {"music", QStandardPaths::writableLocation(QStandardPaths::MusicLocation)},
        {"pictures", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)},
        {"picture", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)},
        {"videos", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)},
        {"templates", ""},
        {"home", QStandardPaths::writableLocation(QStandardPaths::HomeLocation)},
        {"cache", QStandardPaths::writableLocation(QStandardPaths::CacheLocation)},
        {"config", QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)},
        {"data", QStandardPaths::writableLocation(QStandardPaths::DataLocation)},
        {"runtime", QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)},
        {"temp", QStandardPaths::writableLocation(QStandardPaths::TempLocation)},
        {"public_share", ""}};

    return USER_DIR_MAP;
}

inline bool checkPathIsExists(const QString &name)
{
    if (!name.isEmpty()) {
        QFileInfo fileInfo(name);
        if (fileInfo.exists()) {
            return true;
        }
        return false;
    } else {
        return false;
    }
}

QPair<bool, QString> getXdgDir(QString name)
{
    // mostly check cache map first
    // after process the special key find in user-dirs.dirs config file
    auto foundResult = getUserDirMap().value(name.toLower(), "");

    if (!foundResult.isEmpty()) {
        return {checkPathIsExists(foundResult), foundResult};
    } else if (name.toLower() == "public_share") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        auto publicSharePath = QStandardPaths::writableLocation(QStandardPaths::PublicShareLocation);
        return {checkPathIsExists(publicSharePath), publicSharePath};
#else
        // read user-dirs.dirs XDG_PUBLICSHARE_DIR
        auto publicSharePath = getPathInXdgUserConfig("XDG_PUBLICSHARE_DIR");
        return {checkPathIsExists(publicSharePath), publicSharePath};
#endif
    } else if (name.toLower() == "templates") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        auto templatesPath = QStandardPaths::writableLocation(QStandardPaths::TemplatesLocation);
        return {checkPathIsExists(templatesPath), templatesPath};
#else
        // read user-dirs.dirs XDG_TEMPLATES_DIR
        auto templatesPath = getPathInXdgUserConfig("XDG_TEMPLATES_DIR");
        return {checkPathIsExists(templatesPath), templatesPath};
#endif
    }
    return {false, ""};
}

QList<QString> getXdgUserDir()
{
    if (getUserDirMap().isEmpty()) {
        return QList<QString>();
    }
    return getUserDirMap().keys();
}

QString getPathInXdgUserConfig(const QString &key)
{
    auto userDirPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/user-dirs.dirs";

    // check user-dirs.dirs file
    QFile userDirFile(userDirPath);
    if (!userDirFile.exists()) {
        qDebug() << "user-dirs.dirs not found";
        return "";
    }

    // read user-dirs.dirs file
    if (!userDirFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "user-dirs.dirs open failed";
        return "";
    }

    // load data for user-dirs.dirs file
    auto originData = userDirFile.readAll();
    for (auto const &line : originData.split('\n')) {
        // search spec key
        if (line.startsWith(key.toStdString().c_str())) {
            auto readLine = line.split('=');
            if (readLine.size() != 2) {
                continue;
            }
            auto specLine = readLine.at(1);
            if (specLine.isEmpty()) {
                return "";
            }

            // convert  xdg path to local path
            auto specData = QString(specLine);
            specData.replace("$HOME", QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
            specData.replace("\"", "");
            return specData;
        }
    }
    return "";
}

} // namespace util
} // namespace linglong
