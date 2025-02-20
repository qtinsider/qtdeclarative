/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#include "qqmljsimporter_p.h"
#include "qqmljstypedescriptionreader_p.h"
#include "qqmljstypereader_p.h"

#include <QtQml/private/qqmlimportresolver_p.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qdiriterator.h>

QT_BEGIN_NAMESPACE

static const QLatin1String SlashQmldir             = QLatin1String("/qmldir");
static const QLatin1String SlashPluginsDotQmltypes = QLatin1String("/plugins.qmltypes");

static const QString prefixedName(const QString &prefix, const QString &name)
{
    Q_ASSERT(!prefix.endsWith(u'.'));
    return prefix.isEmpty() ? name : (prefix  + QLatin1Char('.') + name);
}

static QQmlDirParser createQmldirParserForFile(const QString &filename)
{
    QFile f(filename);
    f.open(QFile::ReadOnly);
    QQmlDirParser parser;
    parser.parse(QString::fromUtf8(f.readAll()));
    return parser;
}

void QQmlJSImporter::readQmltypes(
        const QString &filename, QHash<QString, QQmlJSScope::Ptr> *objects,
        QList<QQmlDirParser::Import> *dependencies)
{
    const QFileInfo fileInfo(filename);
    if (!fileInfo.exists()) {
        m_warnings.append({
                              QStringLiteral("QML types file does not exist: ") + filename,
                              QtWarningMsg,
                              QQmlJS::SourceLocation()
                          });
        return;
    }

    if (fileInfo.isDir()) {
        m_warnings.append({
                              QStringLiteral("QML types file cannot be a directory: ") + filename,
                              QtWarningMsg,
                              QQmlJS::SourceLocation()
                          });
        return;
    }

    QFile file(filename);
    file.open(QFile::ReadOnly);
    QQmlJSTypeDescriptionReader reader { filename, QString::fromUtf8(file.readAll()) };
    QStringList dependencyStrings;
    auto succ = reader(objects, &dependencyStrings);
    if (!succ)
        m_warnings.append({ reader.errorMessage(), QtCriticalMsg, QQmlJS::SourceLocation() });

    const QString warningMessage = reader.warningMessage();
    if (!warningMessage.isEmpty())
        m_warnings.append({ warningMessage, QtWarningMsg, QQmlJS::SourceLocation() });

    if (dependencyStrings.isEmpty())
        return;

    m_warnings.append({
                          QStringLiteral("Found deprecated dependency specifications in %1."
                                         "Specify dependencies in qmldir and use qmltyperegistrar "
                                         "to generate qmltypes files without dependencies.")
                                .arg(filename),
                          QtWarningMsg,
                          QQmlJS::SourceLocation()
                      });

    for (const QString &dependency : qAsConst(dependencyStrings)) {
        const auto blank = dependency.indexOf(u' ');
        if (blank < 0) {
            dependencies->append(QQmlDirParser::Import(dependency, {},
                                                       QQmlDirParser::Import::Default));
            continue;
        }

        const QString module = dependency.left(blank);
        const QString versionString = dependency.mid(blank + 1).trimmed();
        if (versionString == QStringLiteral("auto")) {
            dependencies->append(QQmlDirParser::Import(module, {}, QQmlDirParser::Import::Auto));
            continue;
        }

        const auto dot = versionString.indexOf(u'.');

        const QTypeRevision version = dot < 0
                ? QTypeRevision::fromMajorVersion(versionString.toUShort())
                : QTypeRevision::fromVersion(versionString.left(dot).toUShort(),
                                             versionString.mid(dot + 1).toUShort());

        dependencies->append(QQmlDirParser::Import(module, version,
                                                   QQmlDirParser::Import::Default));
    }
}

QQmlJSImporter::Import QQmlJSImporter::readQmldir(const QString &path)
{
    Import result;
    auto reader = createQmldirParserForFile(path + SlashQmldir);
    result.imports.append(reader.imports());
    result.dependencies.append(reader.dependencies());

    QHash<QString, QQmlJSScope::Ptr> qmlComponents;
    const auto components = reader.components();
    for (auto it = components.begin(), end = components.end(); it != end; ++it) {
        const QString filePath = path + QLatin1Char('/') + it->fileName;
        if (!QFile::exists(filePath)) {
            m_warnings.append({
                                  it->fileName + QStringLiteral(" is listed as component in ")
                                        + path + SlashQmldir
                                        + QStringLiteral(" but does not exist.\n"),
                                  QtWarningMsg,
                                  QQmlJS::SourceLocation()
                              });
            continue;
        }

        auto mo = qmlComponents.find(it.key());
        if (mo == qmlComponents.end()) {
            QQmlJSScope::Ptr imported = localFile2ScopeTree(filePath);
            if (it->singleton)
                imported->setIsSingleton(true);
            mo = qmlComponents.insert(it.key(), imported);
        }

        (*mo)->addExport(it.key(), reader.typeNamespace(), it->version);
    }
    for (auto it = qmlComponents.begin(), end = qmlComponents.end(); it != end; ++it)
        result.objects.insert(it.key(), it.value());

    const auto typeInfos = reader.typeInfos();
    for (const auto &typeInfo : typeInfos) {
        const QString typeInfoPath = QFileInfo(typeInfo).isRelative()
                ? path + u'/' + typeInfo : typeInfo;
        readQmltypes(typeInfoPath, &result.objects, &result.dependencies);
    }

    if (typeInfos.isEmpty() && !reader.plugins().isEmpty()) {
        const QString defaultTypeInfoPath = path + SlashPluginsDotQmltypes;
        if (QFile::exists(defaultTypeInfoPath)) {
            m_warnings.append({
                                  QStringLiteral("typeinfo not declared in qmldir file: ")
                                    + defaultTypeInfoPath,
                                  QtWarningMsg,
                                  QQmlJS::SourceLocation()
                              });
            readQmltypes(defaultTypeInfoPath, &result.objects, &result.dependencies);
        }
    }

    const auto scripts = reader.scripts();
    for (const auto &script : scripts) {
        const QString filePath = path + QLatin1Char('/') + script.fileName;
        result.scripts.insert(script.nameSpace, localFile2ScopeTree(filePath));
    }
    return result;
}

void QQmlJSImporter::importDependencies(const QQmlJSImporter::Import &import,
                                        QQmlJSImporter::AvailableTypes *types,
                                        const QString &prefix, QTypeRevision version,
                                        bool isDependency)
{
    // Import the dependencies with an invalid prefix. The prefix will never be matched by actual
    // QML code but the C++ types will be visible.
    for (auto const &dependency : qAsConst(import.dependencies))
        importHelper(dependency.module, types, QString(), dependency.version, true);

    for (auto const &import : qAsConst(import.imports)) {
        importHelper(import.module, types, isDependency ? QString() : prefix,
                     (import.flags & QQmlDirParser::Import::Auto) ? version : import.version,
                     isDependency);
    }
}

void QQmlJSImporter::processImport(const QQmlJSImporter::Import &import,
                                   QQmlJSImporter::AvailableTypes *types, const QString &prefix,
                                   QTypeRevision version)
{
    const QString anonPrefix = QStringLiteral("$anonymous$");

    if (!prefix.isEmpty())
        types->qmlNames.insert(prefix, {}); // Empty type means "this is the prefix"

    for (auto it = import.scripts.begin(); it != import.scripts.end(); ++it)
        types->qmlNames.insert(prefixedName(prefix, it.key()), it.value());

    // add objects
    for (auto it = import.objects.begin(); it != import.objects.end(); ++it) {
        const auto &val = it.value();
        const QString name = val->isComposite()
                ? prefixedName(anonPrefix, val->internalName())
                : val->internalName();
        types->cppNames.insert(name, val);

        const auto exports = val->exports();
        if (exports.isEmpty()) {
            types->qmlNames.insert(
                        prefixedName(prefix, prefixedName(anonPrefix, val->internalName())), val);
        }

        for (const auto &valExport : exports) {
            const QString &name = prefixedName(prefix, valExport.type());
            // Resolve conflicting qmlNames within an import
            if (types->qmlNames.contains(name)) {
                const auto &existing = types->qmlNames[name];

                if (existing == val)
                    continue;

                if (valExport.version() > version)
                    continue;

                const auto existingExports = existing->exports();

                auto betterExport =
                        std::find_if(existingExports.constBegin(), existingExports.constEnd(),
                                     [&](const QQmlJSScope::Export &exportEntry) {
                                         return exportEntry.type() == name
                                                 && exportEntry.version()
                                                 <= version // Ensure that the entry isn't newer
                                                            // than the module version
                                                 && valExport.version() < exportEntry.version();
                                     });

                if (betterExport != existingExports.constEnd())
                    continue;
            }

            types->qmlNames.insert(name, val);
        }
    }

    /* We need to create a temporary AvailableTypes instance here to make builtins available as
       QQmlJSScope::resolveTypes relies on them being available. They cannot be part of the regular
       types as they would keep overwriting existing types when loaded from cache.
       This is only a problem with builtin types as only builtin types can be overridden by any
       sibling import. Consider the following qmldir:

       module Things
       import QtQml 2.0
       import QtQuick.LocalStorage auto

       The module "Things" sees QtQml's definition of Qt, not the builtins', even though
       QtQuick.LocalStorage does not depend on QtQml and is imported afterwards. Conversely:

       module Stuff
       import ModuleOverridingQObject
       import QtQuick

       The module "Stuff" sees QtQml's definition of QObject (via QtQuick), even if
       ModuleOverridingQObject has overridden it.
    */

    QQmlJSImporter::AvailableTypes tempTypes(builtinImportHelper().cppNames);
    tempTypes.cppNames.insert(types->cppNames);

    // At present, there are corner cases that couldn't be resolved in a single
    // pass of resolveTypes() (e.g. QQmlEasingEnums::Type). However, such cases
    // only happen when enumerations are involved, thus the strategy is to
    // resolve enumerations (which can potentially create new child scopes)
    // before resolving the type fully
    for (auto it = import.objects.begin(); it != import.objects.end(); ++it)
        QQmlJSScope::resolveEnums(it.value(), tempTypes.cppNames, nullptr);

    for (auto it = import.objects.begin(); it != import.objects.end(); ++it) {
        const auto &val = it.value();
        if (val->baseType().isNull()) // Otherwise we have already done it in localFile2ScopeTree()
            QQmlJSScope::resolveNonEnumTypes(val, tempTypes.cppNames);
    }
}

/*!
 * Imports builtins.qmltypes and jsroot.qmltypes found in any of the import paths.
 */
QQmlJSImporter::ImportedTypes QQmlJSImporter::importBuiltins()
{
    return builtinImportHelper().qmlNames;
}


QQmlJSImporter::AvailableTypes QQmlJSImporter::builtinImportHelper()
{
    if (!m_builtins.qmlNames.isEmpty() || !m_builtins.cppNames.isEmpty())
        return m_builtins;

    Import result;

    QStringList qmltypesFiles = { QStringLiteral("builtins.qmltypes"),
                                  QStringLiteral("jsroot.qmltypes") };
    const auto importBuiltins = [&](const QStringList &imports) {
        for (auto const &dir : imports) {
            QDirIterator it { dir, qmltypesFiles, QDir::NoFilter, QDirIterator::Subdirectories };
            while (it.hasNext() && !qmltypesFiles.isEmpty()) {
                readQmltypes(it.next(), &result.objects, &result.dependencies);
                qmltypesFiles.removeOne(it.fileName());
            }

            importDependencies(result, &m_builtins);

            if (qmltypesFiles.isEmpty())
                return;
        }
    };

    importBuiltins(m_importPaths);
    if (!qmltypesFiles.isEmpty()) {
        const QString pathsString =
                m_importPaths.isEmpty() ? u"<empty>"_qs : m_importPaths.join(u"\n\t");
        m_warnings.append({ QStringLiteral("Failed to find the following builtins: %1 (so will use "
                                           "qrc). Import paths used:\n\t%2")
                                    .arg(qmltypesFiles.join(u", "), pathsString),
                            QtWarningMsg, QQmlJS::SourceLocation() });
        importBuiltins({ u":/qt-project.org/qml/builtins"_qs }); // use qrc as a "last resort"
    }
    Q_ASSERT(qmltypesFiles.isEmpty()); // since qrc must cover it in all the bad cases

    // Process them together since there they have interdependencies that wouldn't get resolved
    // otherwise
    processImport(result, &m_builtins);

    return m_builtins;
}

/*!
 * Imports types from the specified \a qmltypesFiles.
 */
void QQmlJSImporter::importQmltypes(const QStringList &qmltypesFiles)
{
    AvailableTypes types(builtinImportHelper().cppNames);

    for (const auto &qmltypeFile : qmltypesFiles) {
        Import result;
        readQmltypes(qmltypeFile, &result.objects, &result.dependencies);

        // Append _FAKE_QMLDIR to our made up qmldir name so that if it ever gets used somewhere else except for cache lookups,
        // it will blow up due to a missing file instead of producing weird results.
        const QString qmldirName = qmltypeFile + QStringLiteral("_FAKE_QMLDIR");
        m_seenQmldirFiles.insert(qmldirName, result);

        for (const auto &object : result.objects.values()) {
            for (const auto &ex : object->exports()) {
                m_seenImports.insert({ex.package(), ex.version()}, qmldirName);
                // We also have to handle the case that no version is provided
                m_seenImports.insert({ex.package(), QTypeRevision()}, qmldirName);
            }
        }
    }
}

QQmlJSImporter::ImportedTypes QQmlJSImporter::importModule(
        const QString &module, const QString &prefix, QTypeRevision version)
{
    AvailableTypes result(builtinImportHelper().cppNames);
    if (!importHelper(module, &result, prefix, version)) {
        m_warnings.append({
                              QStringLiteral("Failed to import %1. Are your include paths set up properly?").arg(module),
                              QtWarningMsg,
                              QQmlJS::SourceLocation()
                          });
    }
    return result.qmlNames;
}

QQmlJSImporter::ImportedTypes QQmlJSImporter::builtinInternalNames()
{
    return builtinImportHelper().cppNames;
}

bool QQmlJSImporter::importHelper(const QString &module, AvailableTypes *types,
                                  const QString &prefix, QTypeRevision version, bool isDependency,
                                  bool isFile)
{
    // QtQuick/Controls and QtQuick.Controls are the same module
    const QString moduleCacheName = QString(module).replace(u'/', u'.');

    if (isDependency)
        Q_ASSERT(prefix.isEmpty());

    const CacheKey cacheKey = { prefix, moduleCacheName, version, isFile, isDependency  };

    auto getTypesFromCache = [&]() -> bool {
        if (!m_cachedImportTypes.contains(cacheKey))
            return false;

        const auto &cacheEntry = m_cachedImportTypes[cacheKey];

        types->cppNames.insert(cacheEntry->cppNames);

        // No need to import qml names for dependencies
        if (!isDependency)
            types->qmlNames.insert(cacheEntry->qmlNames);

        return true;
    };

    // The QML module only contains builtins and is not registered declaratively, so ignore requests
    // for importing it
    if (module == u"QML"_qs)
        return true;

    if (getTypesFromCache())
        return true;

    auto cacheTypes =
            QSharedPointer<QQmlJSImporter::AvailableTypes>(new QQmlJSImporter::AvailableTypes({}));
    m_cachedImportTypes[cacheKey] = cacheTypes;

    const QPair<QString, QTypeRevision> importId { module, version };
    const auto it = m_seenImports.constFind(importId);

    if (it != m_seenImports.constEnd()) {
        if (it->isEmpty())
            return false;

        Q_ASSERT(m_seenQmldirFiles.contains(*it));
        const QQmlJSImporter::Import import = m_seenQmldirFiles.value(*it);

        importDependencies(import, cacheTypes.get(), prefix, version, isDependency);
        processImport(import, cacheTypes.get(), prefix, version);

        const bool typesFromCache = getTypesFromCache();
        Q_ASSERT(typesFromCache);
        return typesFromCache;
    }

    const auto modulePaths = isFile ? QStringList { module }
                                    : qQmlResolveImportPaths(module, m_importPaths, version);

    for (auto const &modulePath : modulePaths) {
        QString qmldirPath;
        if (modulePath.startsWith(u':')) {
            if (m_mapper) {
                const QString resourcePath = modulePath.mid(
                            1, modulePath.endsWith(u'/') ? modulePath.length() - 2 : -1)
                        + SlashQmldir;
                const auto entry = m_mapper->entry(
                            QQmlJSResourceFileMapper::resourceFileFilter(resourcePath));
                qmldirPath = entry.filePath;
            } else {
                qWarning() << "Cannot read files from resource directory" << modulePath
                           << "because no resource file mapper was provided";
            }
        } else {
            qmldirPath = modulePath + SlashQmldir;
        }

        const auto it = m_seenQmldirFiles.constFind(qmldirPath);
        if (it != m_seenQmldirFiles.constEnd()) {
            const QQmlJSImporter::Import import = *it;
            m_seenImports.insert(importId, qmldirPath);
            importDependencies(import, cacheTypes.get(), prefix, version, isDependency);
            processImport(import, cacheTypes.get(), prefix, version);

            const bool typesFromCache = getTypesFromCache();
            Q_ASSERT(typesFromCache);
            return typesFromCache;
        }

        const QFileInfo file(qmldirPath);
        if (file.exists()) {
            const auto import = readQmldir(file.canonicalPath());
            m_seenQmldirFiles.insert(qmldirPath, import);
            m_seenImports.insert(importId, qmldirPath);
            importDependencies(import, cacheTypes.get(), prefix, version, isDependency);
            processImport(import, cacheTypes.get(), prefix, version);

            const bool typesFromCache = getTypesFromCache();
            Q_ASSERT(typesFromCache);
            return typesFromCache;
        }
    }

    m_seenImports.insert(importId, QString());

    return false;
}

QQmlJSScope::Ptr QQmlJSImporter::localFile2ScopeTree(const QString &filePath)
{
    const auto seen = m_importedFiles.find(filePath);
    if (seen != m_importedFiles.end())
        return *seen;

    return *m_importedFiles.insert(filePath, {
                                       QQmlJSScope::create(),
                                       QSharedPointer<QDeferredFactory<QQmlJSScope>>(
                                            new QDeferredFactory<QQmlJSScope>(this, filePath))
                                   });
}

QQmlJSScope::Ptr QQmlJSImporter::importFile(const QString &file)
{
    return localFile2ScopeTree(file);
}

QQmlJSImporter::ImportedTypes QQmlJSImporter::importDirectory(
        const QString &directory, const QString &prefix)
{
    QQmlJSImporter::AvailableTypes types({});

    if (directory.startsWith(u':')) {
        if (m_mapper) {
            const auto resources = m_mapper->filter(
                        QQmlJSResourceFileMapper::resourceQmlDirectoryFilter(directory.mid(1)));
            for (const auto &entry : resources) {
                const QString name = QFileInfo(entry.resourcePath).baseName();
                if (name.front().isUpper()) {
                    types.qmlNames.insert(prefixedName(prefix, name),
                                          localFile2ScopeTree(entry.filePath));
                }
            }
        } else {
            qWarning() << "Cannot read files from resource directory" << directory
                       << "because no resource file mapper was provided";
        }

        importHelper(directory, &types, QString(), QTypeRevision(), false, true);

        return types.qmlNames;
    }

    QDirIterator it {
        directory,
        QStringList() << QLatin1String("*.qml"),
        QDir::NoFilter
    };
    while (it.hasNext()) {
        it.next();
        if (!it.fileName().front().isUpper())
            continue; // Non-uppercase names cannot be imported anyway.

        types.qmlNames.insert(prefixedName(prefix, QFileInfo(it.filePath()).baseName()),
                              localFile2ScopeTree(it.filePath()));
    }

    importHelper(directory, &types, QString(), QTypeRevision(), false, true);

    return types.qmlNames;
}

void QQmlJSImporter::setImportPaths(const QStringList &importPaths)
{
    m_importPaths = importPaths;

    // We have to get rid off all cache elements directly referencing modules, since changing
    // importPaths might change which module is found first
    m_seenImports.clear();
    m_cachedImportTypes.clear();
    // Luckily this doesn't apply to m_seenQmldirFiles
}

QQmlJSScope::ConstPtr QQmlJSImporter::jsGlobalObject() const
{
    return m_builtins.cppNames[u"GlobalObject"_qs];
}

QT_END_NAMESPACE
