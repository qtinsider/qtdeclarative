/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>
#include <private/qqmljsdiagnosticmessage_p.h>
#include <private/qqmldirparser_p.h>
#include <private/qqmljsresourcefilemapper_p.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QVariant>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QLibraryInfo>

#include <iostream>
#include <algorithm>

QT_USE_NAMESPACE

namespace {

QStringList g_qmlImportPaths;

inline QString typeLiteral()         { return QStringLiteral("type"); }
inline QString versionLiteral()      { return QStringLiteral("version"); }
inline QString nameLiteral()         { return QStringLiteral("name"); }
inline QString relativePathLiteral() { return QStringLiteral("relativePath"); }
inline QString pluginsLiteral()      { return QStringLiteral("plugins"); }
inline QString pluginIsOptionalLiteral() { return QStringLiteral("pluginIsOptional"); }
inline QString pathLiteral()         { return QStringLiteral("path"); }
inline QString classnamesLiteral()   { return QStringLiteral("classnames"); }
inline QString dependenciesLiteral() { return QStringLiteral("dependencies"); }
inline QString moduleLiteral()       { return QStringLiteral("module"); }
inline QString javascriptLiteral()   { return QStringLiteral("javascript"); }
inline QString directoryLiteral()    { return QStringLiteral("directory"); }
inline QString linkTargetLiteral()
{
    return QStringLiteral("linkTarget");
}

void printUsage(const QString &appNameIn)
{
    const std::wstring appName = appNameIn.toStdWString();
    const QString qmlPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    std::wcerr
        << "Usage: " << appName << " -rootPath path/to/app/qml/directory -importPath path/to/qt/qml/directory\n"
           "       " << appName << " -qmlFiles file1 file2 -importPath path/to/qt/qml/directory\n"
           "       " << appName << " -qrcFiles file1.qrc file2.qrc -importPath path/to/qt/qml/directory\n\n"
           "Example: " << appName << " -rootPath . -importPath "
        << QDir::toNativeSeparators(qmlPath).toStdWString()
        << '\n';
}

QVariantList findImportsInAst(QQmlJS::AST::UiHeaderItemList *headerItemList, const QString &path)
{
    QVariantList imports;

    // Extract uri and version from the imports (which look like "import Foo.Bar 1.2.3")
    for (QQmlJS::AST::UiHeaderItemList *headerItemIt = headerItemList; headerItemIt; headerItemIt = headerItemIt->next) {
        QVariantMap import;
        QQmlJS::AST::UiImport *importNode = QQmlJS::AST::cast<QQmlJS::AST::UiImport *>(headerItemIt->headerItem);
        if (!importNode)
            continue;
        // Handle directory imports
        if (!importNode->fileName.isEmpty()) {
            QString name = importNode->fileName.toString();
            import[nameLiteral()] = name;
            if (name.endsWith(QLatin1String(".js"))) {
                import[typeLiteral()] = javascriptLiteral();
            } else {
                import[typeLiteral()] = directoryLiteral();
            }

            import[pathLiteral()] = QDir::cleanPath(path + QLatin1Char('/') + name);
        } else {
            // Walk the id chain ("Foo" -> "Bar" -> etc)
            QString  name;
            QQmlJS::AST::UiQualifiedId *uri = importNode->importUri;
            while (uri) {
                name.append(uri->name);
                name.append(QLatin1Char('.'));
                uri = uri->next;
            }
            name.chop(1); // remove trailing "."
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if (name.startsWith(QLatin1String("QtQuick.Controls")) && name.endsWith(QLatin1String("impl")))
                continue;
#endif
            if (!name.isEmpty())
                import[nameLiteral()] = name;
            import[typeLiteral()] = moduleLiteral();
            auto versionString = importNode->version
                    ? QString::number(importNode->version->version.majorVersion())
                      + QLatin1Char('.')
                      + QString::number(importNode->version->version.minorVersion())
                    : QString();
            if (!versionString.isEmpty())
                import[versionLiteral()] = versionString;
        }

        imports.append(import);
    }

    return imports;
}

QVariantList findQmlImportsInQmlFile(const QString &filePath);
QVariantList findQmlImportsInJavascriptFile(const QString &filePath);

static QString versionSuffix(QTypeRevision version)
{
    return QLatin1Char(' ') + QString::number(version.majorVersion()) + QLatin1Char('.')
            + QString::number(version.minorVersion());
}

// Read the qmldir file, extract a list of plugins by
// parsing the "plugin", "import", and "classname" directives.
QVariantMap pluginsForModulePath(const QString &modulePath, const QString &version) {
    QFile qmldirFile(modulePath + QLatin1String("/qmldir"));
    if (!qmldirFile.exists()) {
        qWarning() << "qmldir file not found at" << modulePath;
        return QVariantMap();
    }

    if (!qmldirFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "qmldir file not found at" << modulePath;
        return QVariantMap();
    }

    QQmlDirParser parser;
    parser.parse(QString::fromUtf8(qmldirFile.readAll()));
    if (parser.hasError()) {
        qWarning() << "qmldir file malformed at" << modulePath;
        for (const auto &error : parser.errors(QLatin1String("qmldir")))
            qWarning() << error.message;
        return QVariantMap();
    }

    QVariantMap pluginInfo;

    QStringList pluginNameList;
    bool isOptional = false;
    const auto plugins = parser.plugins();
    for (const auto &plugin : plugins) {
        pluginNameList.append(plugin.name);
        isOptional = plugin.optional;
    }

    pluginInfo[pluginsLiteral()] = pluginNameList.join(QLatin1Char(' '));

    if (plugins.size() > 1) {
        qWarning() << QStringLiteral("Warning: \"%1\" contains multiple plugin entries. This is discouraged and does not support marking plugins as optional.").arg(modulePath);
        isOptional = false;
    }

    if (isOptional) {
        pluginInfo[pluginIsOptionalLiteral()] = true;
    }

    if (!parser.linkTarget().isEmpty()) {
        pluginInfo[linkTargetLiteral()] = parser.linkTarget();
    }

    pluginInfo[classnamesLiteral()] = parser.classNames().join(QLatin1Char(' '));

    QStringList importsAndDependencies;
    const auto dependencies = parser.dependencies();
    for (const auto &dependency : dependencies)
        importsAndDependencies.append(dependency.module + versionSuffix(dependency.version));

    const auto imports = parser.imports();
    for (const auto &import : imports) {
        if (import.flags & QQmlDirParser::Import::Auto) {
            importsAndDependencies.append(
                        import.module + QLatin1Char(' ')
                        + (version.isEmpty() ? QString::fromLatin1("auto") : version));
        } else if (import.version.isValid()) {
            importsAndDependencies.append(import.module + versionSuffix(import.version));
        } else {
            importsAndDependencies.append(import.module);
        }
    }

    QVariantList importsFromFiles;
    const auto components = parser.components();
    for (const auto &component : components) {
        importsFromFiles
                += findQmlImportsInQmlFile(modulePath + QLatin1Char('/') + component.fileName);
    }
    const auto scripts = parser.scripts();
    for (const auto &script : scripts) {
        importsFromFiles
                += findQmlImportsInJavascriptFile(modulePath + QLatin1Char('/') + script.fileName);
    }

    for (const QVariant &import : importsFromFiles) {
        const QVariantMap details = qvariant_cast<QVariantMap>(import);
        if (details.value(typeLiteral()) != moduleLiteral())
            continue;
        const QString name = details.value(nameLiteral()).toString();
        const QString version = details.value(versionLiteral()).toString();
        importsAndDependencies.append(
                    version.isEmpty() ? name : (name + QLatin1Char(' ') + version));
    }

    if (!importsAndDependencies.isEmpty())
        pluginInfo[dependenciesLiteral()] = importsAndDependencies;
    return pluginInfo;
}

// Search for a given qml import in g_qmlImportPaths and return a pair
// of absolute / relative paths (for deployment).
QPair<QString, QString> resolveImportPath(const QString &uri, const QString &version)
{
    const QLatin1Char dot('.');
    const QLatin1Char slash('/');
    const QStringList parts = uri.split(dot, Qt::SkipEmptyParts);

    QString ver = version;
    QPair<QString, QString> candidate;
    while (true) {
        for (const QString &qmlImportPath : qAsConst(g_qmlImportPaths)) {
            // Search for the most specific version first, and search
            // also for the version in parent modules. For example:
            // - qml/QtQml/Models.2.0
            // - qml/QtQml.2.0/Models
            // - qml/QtQml/Models.2
            // - qml/QtQml.2/Models
            // - qml/QtQml/Models
            if (ver.isEmpty()) {
                QString relativePath = parts.join(slash);
                if (relativePath.endsWith(slash))
                    relativePath.chop(1);
                const QString candidatePath = QDir::cleanPath(qmlImportPath + slash + relativePath);
                const QDir candidateDir(candidatePath);
                if (candidateDir.exists()) {
                    const auto newCandidate = qMakePair(candidatePath, relativePath); // import found
                    if (candidateDir.exists(u"qmldir"_qs)) // if it has a qmldir, we are fine
                        return newCandidate;
                    else if (candidate.first.isEmpty())
                        candidate = newCandidate;
                    // otherwise we keep looking if we can find the module again (with a qmldir this time)
                }
            } else {
                for (int index = parts.count() - 1; index >= 0; --index) {
                    QString relativePath = parts.mid(0, index + 1).join(slash)
                        + dot + ver + slash + parts.mid(index + 1).join(slash);
                    if (relativePath.endsWith(slash))
                        relativePath.chop(1);
                    const QString candidatePath = QDir::cleanPath(qmlImportPath + slash + relativePath);
                    const QDir candidateDir(candidatePath);
                    if (candidateDir.exists()) {
                        const auto newCandidate = qMakePair(candidatePath, relativePath); // import found
                        if (candidateDir.exists(u"qmldir"_qs))
                            return newCandidate;
                        else if (candidate.first.isEmpty())
                            candidate = newCandidate;
                    }
                }
            }
        }

        // Remove the last version digit; stop if there are none left
        if (ver.isEmpty())
            break;

        int lastDot = ver.lastIndexOf(dot);
        if (lastDot == -1)
            ver.clear();
        else
            ver = ver.mid(0, lastDot);
    }

    return candidate;
}

// Find absolute file system paths and plugins for a list of modules.
QVariantList findPathsForModuleImports(const QVariantList &imports)
{
    QVariantList done;
    QVariantList importsCopy(imports);

    for (int i = 0; i < importsCopy.length(); ++i) {
        QVariantMap import = qvariant_cast<QVariantMap>(importsCopy.at(i));
        if (import.value(typeLiteral()) == moduleLiteral()) {
            const QString version = import.value(versionLiteral()).toString();
            const QPair<QString, QString> paths =
                resolveImportPath(import.value(nameLiteral()).toString(), version);
            QVariantMap plugininfo;
            if (!paths.first.isEmpty()) {
                import.insert(pathLiteral(), paths.first);
                import.insert(relativePathLiteral(), paths.second);
                plugininfo = pluginsForModulePath(paths.first, version);
            }
            QString linkTarget = plugininfo.value(linkTargetLiteral()).toString();
            QString plugins = plugininfo.value(pluginsLiteral()).toString();
            bool isOptional = plugininfo.value(pluginIsOptionalLiteral(), QVariant(false)).toBool();
            QString classnames = plugininfo.value(classnamesLiteral()).toString();
            if (!linkTarget.isEmpty())
                import.insert(linkTargetLiteral(), linkTarget);
            if (!plugins.isEmpty())
                import.insert(QStringLiteral("plugin"), plugins);
            if (isOptional)
                import.insert(pluginIsOptionalLiteral(), true);
            if (!classnames.isEmpty())
                import.insert(QStringLiteral("classname"), classnames);
            if (plugininfo.contains(dependenciesLiteral())) {
                const QStringList dependencies = plugininfo.value(dependenciesLiteral()).toStringList();
                for (const QString &line : dependencies) {
                    const auto dep = QStringView{line}.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    const QString name = dep[0].toString();
                    QVariantMap depImport;
                    depImport[typeLiteral()] = moduleLiteral();
                    depImport[nameLiteral()] = name;
                    if (dep.length() > 1)
                        depImport[versionLiteral()] = dep[1].toString();

                    if (!importsCopy.contains(depImport))
                        importsCopy.append(depImport);
                }
            }
        }
        import.remove(versionLiteral());
        done.append(import);
    }
    return done;
}

// Scan a single qml file for import statements
QVariantList findQmlImportsInQmlCode(const QString &filePath, const QString &code)
{
    QQmlJS::Engine engine;
    QQmlJS::Lexer lexer(&engine);
    lexer.setCode(code, /*line = */ 1);
    QQmlJS::Parser parser(&engine);

    if (!parser.parse() || !parser.diagnosticMessages().isEmpty()) {
        // Extract errors from the parser
        const auto diagnosticMessages = parser.diagnosticMessages();
        for (const QQmlJS::DiagnosticMessage &m : diagnosticMessages) {
            std::cerr << QDir::toNativeSeparators(filePath).toStdString() << ':'
                      << m.loc.startLine << ':' << m.message.toStdString() << std::endl;
        }
        return QVariantList();
    }
    return findImportsInAst(parser.ast()->headers, filePath);
}

// Scan a single qml file for import statements
QVariantList findQmlImportsInQmlFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
           std::cerr << "Cannot open input file " << QDir::toNativeSeparators(file.fileName()).toStdString()
                     << ':' << file.errorString().toStdString() << std::endl;
           return QVariantList();
    }
    QString code = QString::fromUtf8(file.readAll());
    return findQmlImportsInQmlCode(filePath, code);
}

struct ImportCollector : public QQmlJS::Directives
{
    QVariantList imports;

    void importFile(const QString &jsfile, const QString &module, int line, int column) override
    {
        QVariantMap entry;
        entry[typeLiteral()] = javascriptLiteral();
        entry[pathLiteral()] = jsfile;
        imports << entry;

        Q_UNUSED(module);
        Q_UNUSED(line);
        Q_UNUSED(column);
    }

    void importModule(const QString &uri, const QString &version, const QString &module, int line, int column) override
    {
        QVariantMap entry;
        if (uri.contains(QLatin1Char('/'))) {
            entry[typeLiteral()] = directoryLiteral();
            entry[nameLiteral()] = uri;
        } else {
            entry[typeLiteral()] = moduleLiteral();
            entry[nameLiteral()] = uri;
            if (!version.isEmpty())
                entry[versionLiteral()] = version;
        }
        imports << entry;

        Q_UNUSED(module);
        Q_UNUSED(line);
        Q_UNUSED(column);
    }
};

// Scan a single javascrupt file for import statements
QVariantList findQmlImportsInJavascriptFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
           std::cerr << "Cannot open input file " << QDir::toNativeSeparators(file.fileName()).toStdString()
                     << ':' << file.errorString().toStdString() << std::endl;
           return QVariantList();
    }

    QString sourceCode = QString::fromUtf8(file.readAll());
    file.close();

    QQmlJS::Engine ee;
    ImportCollector collector;
    ee.setDirectives(&collector);
    QQmlJS::Lexer lexer(&ee);
    lexer.setCode(sourceCode, /*line*/1, /*qml mode*/false);
    QQmlJS::Parser parser(&ee);
    parser.parseProgram();

    const auto diagnosticMessages = parser.diagnosticMessages();
    for (const QQmlJS::DiagnosticMessage &m : diagnosticMessages)
        if (m.isError())
            return QVariantList();

    return collector.imports;
}

// Scan a single qml or js file for import statements
QVariantList findQmlImportsInFile(const QString &filePath)
{
    QVariantList imports;
    if (filePath == QLatin1String("-")) {
        QFile f;
        if (f.open(stdin, QIODevice::ReadOnly))
            imports = findQmlImportsInQmlCode(QLatin1String("<stdin>"), QString::fromUtf8(f.readAll()));
    } else if (filePath.endsWith(QLatin1String(".qml"))) {
        imports = findQmlImportsInQmlFile(filePath);
    } else if (filePath.endsWith(QLatin1String(".js"))) {
        imports = findQmlImportsInJavascriptFile(filePath);
    }

    return findPathsForModuleImports(imports);
}

// Merge two lists of imports, discard duplicates.
QVariantList mergeImports(const QVariantList &a, const QVariantList &b)
{
    QVariantList merged = a;
    for (const QVariant &variant : b) {
        if (!merged.contains(variant))
            merged.append(variant);
    }
    return merged;
}

// Predicates needed by findQmlImportsInDirectory.

struct isMetainfo {
    bool operator() (const QFileInfo &x) const {
        return x.suffix() == QLatin1String("metainfo");
    }
};

struct pathStartsWith {
    pathStartsWith(const QString &path) : _path(path) {}
    bool operator() (const QString &x) const {
        return _path.startsWith(x);
    }
    const QString _path;
};



// Scan all qml files in directory for import statements
QVariantList findQmlImportsInDirectory(const QString &qmlDir)
{
    QVariantList ret;
    if (qmlDir.isEmpty())
        return ret;

    QDirIterator iterator(qmlDir, QDir::AllDirs | QDir::NoDotDot, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    QStringList blacklist;

    while (iterator.hasNext()) {
        iterator.next();
        const QString path = iterator.filePath();
        const QFileInfoList entries = QDir(path).entryInfoList();

        // Skip designer related stuff
        if (std::find_if(entries.cbegin(), entries.cend(), isMetainfo()) != entries.cend()) {
            blacklist << path;
            continue;
        }

        if (std::find_if(blacklist.cbegin(), blacklist.cend(), pathStartsWith(path)) != blacklist.cend())
            continue;

        // Skip obvious build output directories
        if (path.contains(QLatin1String("Debug-iphoneos")) || path.contains(QLatin1String("Release-iphoneos")) ||
            path.contains(QLatin1String("Debug-iphonesimulator")) || path.contains(QLatin1String("Release-iphonesimulator"))
#ifdef Q_OS_WIN
            || path.endsWith(QLatin1String("/release")) || path.endsWith(QLatin1String("/debug"))
#endif
        ){
            continue;
        }

        for (const QFileInfo &x : entries)
            if (x.isFile())
                ret = mergeImports(ret, findQmlImportsInFile(x.absoluteFilePath()));
     }
     return ret;
}

// Find qml imports recursively from a root set of qml files.
// The directories in qmlDirs are searched recursively.
// The files in qmlFiles parsed directly.
QVariantList findQmlImportsRecursively(const QStringList &qmlDirs, const QStringList &scanFiles)
{
    QVariantList ret;

    // Scan all app root qml directories for imports
    for (const QString &qmlDir : qmlDirs) {
        QVariantList imports = findQmlImportsInDirectory(qmlDir);
        ret = mergeImports(ret, imports);
    }

    // Scan app qml files for imports
    for (const QString &file : scanFiles) {
        QVariantList imports = findQmlImportsInFile(file);
        ret = mergeImports(ret, imports);
    }

    return ret;
}


QString generateCmakeIncludeFileContent(const QVariantList &importList) {
    // The function assumes that "list" is a QVariantList with 0 or more QVariantMaps, where
    // each map contains QString -> QVariant<QString> mappings. This matches with the structure
    // that qmake parses for static qml plugin auto imporitng.
    // So: [ {"a": "a","b": "b"}, {"c": "c"} ]
    QString content;
    QTextStream s(&content);
    int importsCount = 0;
    for (const QVariant &importVariant: importList) {
        if (static_cast<QMetaType::Type>(importVariant.userType()) == QMetaType::QVariantMap) {
            s << QStringLiteral("set(qml_import_scanner_import_") << importsCount
              << QStringLiteral(" \"");

            const QMap<QString, QVariant> &importDict = importVariant.toMap();
            for (auto it = importDict.cbegin(); it != importDict.cend(); ++it) {
                s << it.key().toUpper() << QLatin1Char(';')
                  << it.value().toString() << QLatin1Char(';');
            }
            s << QStringLiteral("\")\n");
            ++importsCount;
        }
    }
    if (importsCount >= 0) {
        content.prepend(QString(QStringLiteral("set(qml_import_scanner_imports_count %1)\n"))
               .arg(importsCount));
    }
    return content;
}

bool argumentsFromCommandLineAndFile(QStringList &allArguments, const QStringList &arguments)
{
    allArguments.reserve(arguments.size());
    for (const QString &argument : arguments) {
        // "@file" doesn't start with a '-' so we can't use QCommandLineParser for it
        if (argument.startsWith(QLatin1Char('@'))) {
            QString optionsFile = argument;
            optionsFile.remove(0, 1);
            if (optionsFile.isEmpty()) {
                fprintf(stderr, "The @ option requires an input file");
                return false;
            }
            QFile f(optionsFile);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                fprintf(stderr, "Cannot open options file specified with @");
                return false;
            }
            while (!f.atEnd()) {
                QString line = QString::fromLocal8Bit(f.readLine().trimmed());
                if (!line.isEmpty())
                    allArguments << line;
            }
        } else {
            allArguments << argument;
        }
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));
    QStringList args;
    if (!argumentsFromCommandLineAndFile(args, app.arguments()))
        return EXIT_FAILURE;
    const QString appName = QFileInfo(app.applicationFilePath()).baseName();
    if (args.size() < 2) {
        printUsage(appName);
        return 1;
    }

    QStringList qmlRootPaths;
    QStringList scanFiles;
    QStringList qmlImportPaths;
    QStringList qrcFiles;
    bool generateCmakeContent = false;

    int i = 1;
    while (i < args.count()) {
        const QString &arg = args.at(i);
        ++i;
        QStringList *argReceiver = nullptr;
        if (!arg.startsWith(QLatin1Char('-')) || arg == QLatin1String("-")) {
            qmlRootPaths += arg;
        } else if (arg == QLatin1String("-rootPath")) {
            if (i >= args.count())
                std::cerr << "-rootPath requires an argument\n";
            argReceiver = &qmlRootPaths;
        } else if (arg == QLatin1String("-qmlFiles")) {
            if (i >= args.count())
                std::cerr << "-qmlFiles requires an argument\n";
            argReceiver = &scanFiles;
        } else if (arg == QLatin1String("-jsFiles")) {
            if (i >= args.count())
                std::cerr << "-jsFiles requires an argument\n";
            argReceiver = &scanFiles;
        } else if (arg == QLatin1String("-importPath")) {
            if (i >= args.count())
                std::cerr << "-importPath requires an argument\n";
            argReceiver = &qmlImportPaths;
        } else if (arg == QLatin1String("-cmake-output")) {
             generateCmakeContent = true;
        } else if (arg == QLatin1String("-qrcFiles")) {
            argReceiver = &qrcFiles;
        } else {
            std::cerr << qPrintable(appName) << ": Invalid argument: \""
                << qPrintable(arg) << "\"\n";
            return 1;
        }

        while (i < args.count()) {
            const QString arg = args.at(i);
            if (arg.startsWith(QLatin1Char('-')) && arg != QLatin1String("-"))
                break;
            ++i;
            if (arg != QLatin1String("-") && !QFile::exists(arg)) {
                std::cerr << qPrintable(appName) << ": No such file or directory: \""
                    << qPrintable(arg) << "\"\n";
                return 1;
            } else {
                *argReceiver += arg;
            }
        }
    }

    if (!qrcFiles.isEmpty()) {
        scanFiles << QQmlJSResourceFileMapper(qrcFiles).filePaths(
                         QQmlJSResourceFileMapper::allQmlJSFilter());
    }

    g_qmlImportPaths = qmlImportPaths;

    // Find the imports!
    QVariantList imports = findQmlImportsRecursively(qmlRootPaths, scanFiles);

    QByteArray content;
    if (generateCmakeContent) {
        // Convert to CMake code
        content = generateCmakeIncludeFileContent(imports).toUtf8();
    } else {
        // Convert to JSON
        content = QJsonDocument(QJsonArray::fromVariantList(imports)).toJson();
    }

    std::cout << content.constData() << std::endl;
    return 0;
}
