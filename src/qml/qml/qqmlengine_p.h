/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQMLENGINE_P_H
#define QQMLENGINE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qqmlengine.h"

#include <private/qfieldlist_p.h>
#include <private/qintrusivelist_p.h>
#include <private/qjsengine_p.h>
#include <private/qjsvalue_p.h>
#include <private/qpodvector_p.h>
#include <private/qqmldirparser_p.h>
#include <private/qqmlimport_p.h>
#include <private/qqmlmetatype_p.h>
#include <private/qqmlnotifier_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmltypeloader_p.h>
#include <private/qqmlvaluetype_p.h>
#include <private/qrecyclepool_p.h>
#include <private/qv4engine_p.h>

#include <QtQml/qqml.h>
#include <QtQml/qqmlcontext.h>

#include <QtCore/qlist.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qmutex.h>
#include <QtCore/qpair.h>
#include <QtCore/qproperty.h>
#include <QtCore/qstack.h>
#include <QtCore/qstring.h>
#include <QtCore/qthread.h>

QT_BEGIN_NAMESPACE

class QNetworkAccessManager;
class QQmlDelayedError;
class QQmlIncubator;
class QQmlMetaObject;
class QQmlNetworkAccessManagerFactory;
class QQmlObjectCreator;
class QQmlProfiler;
class QQmlPropertyCapture;

struct QObjectForeign {
    Q_GADGET
    QML_FOREIGN(QObject)
    QML_NAMED_ELEMENT(QtObject)
    QML_ADDED_IN_VERSION(2, 0)
    Q_CLASSINFO("QML.Root", "QML")
};

// This needs to be declared here so that the pool for it can live in QQmlEnginePrivate.
// The inline method definitions are in qqmljavascriptexpression_p.h
class QQmlJavaScriptExpressionGuard : public QQmlNotifierEndpoint
{
public:
    inline QQmlJavaScriptExpressionGuard(QQmlJavaScriptExpression *);

    static inline QQmlJavaScriptExpressionGuard *New(QQmlJavaScriptExpression *e,
                                                             QQmlEngine *engine);
    inline void Delete();

    QQmlJavaScriptExpression *expression;
    QQmlJavaScriptExpressionGuard *next;
};

struct QPropertyChangeTrigger : QPropertyObserver {
    QPropertyChangeTrigger(QQmlJavaScriptExpression *expression) : QPropertyObserver(&QPropertyChangeTrigger::trigger), m_expression(expression) {}
    QQmlJavaScriptExpression * m_expression;
    QObject *target = nullptr;
    int propertyIndex = 0;
    static void trigger(QPropertyObserver *, QUntypedPropertyData *);

    QMetaProperty property() const;
};

struct TriggerList : QPropertyChangeTrigger {
    TriggerList(QQmlJavaScriptExpression *expression)
        : QPropertyChangeTrigger(expression)
    {}
    TriggerList *next = nullptr;
};

class Q_QML_PRIVATE_EXPORT QQmlEnginePrivate : public QJSEnginePrivate
{
    Q_DECLARE_PUBLIC(QQmlEngine)
public:
    explicit QQmlEnginePrivate(QQmlEngine *q) : importDatabase(q), typeLoader(q) {}
    ~QQmlEnginePrivate() override;

    void init();
    // No mutex protecting baseModulesUninitialized, because use outside QQmlEngine
    // is just qmlClearTypeRegistrations (which can't be called while an engine exists)
    static bool baseModulesUninitialized;

    QQmlPropertyCapture *propertyCapture = nullptr;

    QRecyclePool<QQmlJavaScriptExpressionGuard> jsExpressionGuardPool;
    QRecyclePool<TriggerList> qPropertyTriggerPool;

    QQmlContext *rootContext = nullptr;
    Q_OBJECT_BINDABLE_PROPERTY(QQmlEnginePrivate, QString, translationLanguage);

#if !QT_CONFIG(qml_debug)
    static const quintptr profiler = 0;
#else
    QQmlProfiler *profiler = nullptr;
#endif

    bool outputWarningsToMsgLog = true;

    // Bindings that have had errors during startup
    QQmlDelayedError *erroredBindings = nullptr;
    int inProgressCreations = 0;

    QV4::ExecutionEngine *v4engine() const { return q_func()->handle(); }

#if QT_CONFIG(qml_worker_script)
    QThread *workerScriptEngine = nullptr;
#endif

    QUrl baseUrl;

    QQmlObjectCreator *activeObjectCreator = nullptr;
#if QT_CONFIG(qml_network)
    QNetworkAccessManager *createNetworkAccessManager(QObject *parent) const;
    QNetworkAccessManager *getNetworkAccessManager() const;
    mutable QNetworkAccessManager *networkAccessManager = nullptr;
    mutable QQmlNetworkAccessManagerFactory *networkAccessManagerFactory = nullptr;
#endif
    QHash<QString,QSharedPointer<QQmlImageProviderBase> > imageProviders;
    QSharedPointer<QQmlImageProviderBase> imageProvider(const QString &providerId) const;

    QList<QQmlAbstractUrlInterceptor *> urlInterceptors;

    int scarceResourcesRefCount = 0;
    void referenceScarceResources();
    void dereferenceScarceResources();

    QQmlImportDatabase importDatabase;
    QQmlTypeLoader typeLoader;

    QString offlineStoragePath;

    // Unfortunate workaround to avoid a circular dependency between
    // qqmlengine_p.h and qqmlincubator_p.h
    struct Incubator : public QSharedData {
        QIntrusiveListNode next;
        // Unfortunate workaround for MSVC
        QIntrusiveListNode nextWaitingFor;
    };
    QIntrusiveList<Incubator, &Incubator::next> incubatorList;
    unsigned int incubatorCount = 0;
    QQmlIncubationController *incubationController = nullptr;
    void incubate(QQmlIncubator &, const QQmlRefPointer<QQmlContextData> &);

    // These methods may be called from any thread
    QString offlineStorageDatabaseDirectory() const;

    // These methods may be called from the loader thread
    inline QQmlRefPointer<QQmlPropertyCache> cache(const QQmlType &, QTypeRevision version);
    using QJSEnginePrivate::cache;

    // These methods may be called from the loader thread
    QQmlMetaObject rawMetaObjectForType(QMetaType metaType) const;
    QQmlMetaObject metaObjectForType(QMetaType metaType) const;
    QQmlRefPointer<QQmlPropertyCache> propertyCacheForType(QMetaType metaType);
    QQmlRefPointer<QQmlPropertyCache> rawPropertyCacheForType(QMetaType metaType);
    QQmlRefPointer<QQmlPropertyCache> rawPropertyCacheForType(
            QMetaType metaType, QTypeRevision version);
    void registerInternalCompositeType(QV4::ExecutableCompilationUnit *compilationUnit);
    void unregisterInternalCompositeType(QV4::ExecutableCompilationUnit *compilationUnit);
    QV4::ExecutableCompilationUnit *obtainExecutableCompilationUnit(int typeId);

    bool isTypeLoaded(const QUrl &url) const;
    bool isScriptLoaded(const QUrl &url) const;

    template <typename T>
    T singletonInstance(const QQmlType &type);

    void sendQuit();
    void sendExit(int retCode = 0);
    void warning(const QQmlError &);
    void warning(const QList<QQmlError> &);
    static void warning(QQmlEngine *, const QQmlError &);
    static void warning(QQmlEngine *, const QList<QQmlError> &);
    static void warning(QQmlEnginePrivate *, const QQmlError &);
    static void warning(QQmlEnginePrivate *, const QList<QQmlError> &);

    inline static QV4::ExecutionEngine *getV4Engine(QQmlEngine *e);
    inline static QQmlEnginePrivate *get(QQmlEngine *e);
    inline static const QQmlEnginePrivate *get(const QQmlEngine *e);
    inline static QQmlEnginePrivate *get(QQmlContext *c);
    inline static QQmlEnginePrivate *get(const QQmlRefPointer<QQmlContextData> &c);
    inline static QQmlEngine *get(QQmlEnginePrivate *p);
    inline static QQmlEnginePrivate *get(QV4::ExecutionEngine *e);

    static QList<QQmlError> qmlErrorFromDiagnostics(const QString &fileName, const QList<QQmlJS::DiagnosticMessage> &diagnosticMessages);

    static bool designerMode();
    static void activateDesignerMode();

    static bool qml_debugging_enabled;

    mutable QMutex networkAccessManagerMutex;

    QQmlGadgetPtrWrapper *valueTypeInstance(QMetaType type)
    {
        int typeIndex = type.id();
        auto it = cachedValueTypeInstances.find(typeIndex);
        if (it != cachedValueTypeInstances.end())
            return *it;

        if (QQmlValueType *valueType = QQmlMetaType::valueType(type)) {
            QQmlGadgetPtrWrapper *instance = new QQmlGadgetPtrWrapper(valueType, q_func());
            cachedValueTypeInstances.insert(typeIndex, instance);
            return instance;
        }

        return nullptr;
    }

    void executeRuntimeFunction(const QUrl &url, qsizetype functionIndex, QObject *thisObject,
                                int argc = 0, void **args = nullptr, QMetaType *types = nullptr);
    QV4::ExecutableCompilationUnit *compilationUnitFromUrl(const QUrl &url);
    QQmlRefPointer<QQmlContextData>
    createInternalContext(const QQmlRefPointer<QV4::ExecutableCompilationUnit> &unit,
                          const QQmlRefPointer<QQmlContextData> &parentContext,
                          int subComponentIndex, bool isComponentRoot);
    static void setInternalContext(QObject *This, const QQmlRefPointer<QQmlContextData> &context,
                                   QQmlContextData::QmlObjectKind kind)
    {
        Q_ASSERT(This);
        QQmlData *ddata = QQmlData::get(This, /*create*/ true);
        // NB: copied from QQmlObjectCreator::createInstance()
        //
        // the if-statement logic to determine the kind is:
        // if (static_cast<quint32>(index) == 0 || ddata->rootObjectInCreation || isInlineComponent)
        // then QQmlContextData::DocumentRoot. here, we pass this through qmltc
        context->installContext(ddata, kind);
        Q_ASSERT(qmlEngine(This));
    }

private:
    class SingletonInstances : private QHash<QQmlType, QJSValue>
    {
    public:
        void convertAndInsert(QV4::ExecutionEngine *engine, const QQmlType &type, QJSValue *value)
        {
            QJSValuePrivate::manageStringOnV4Heap(engine, value);
            insert(type, *value);
        }

        void clear() {
            for (auto it = constBegin(), end = constEnd(); it != end; ++it) {
                QObject *instance = it.value().toQObject();
                if (!instance)
                    continue;

                if (it.key().singletonInstanceInfo()->url.isEmpty()) {
                    const QQmlData *ddata = QQmlData::get(instance, false);
                    if (ddata && ddata->indestructible && ddata->explicitIndestructibleSet)
                        continue;
                }

                delete instance;
            }

            QHash<QQmlType, QJSValue>::clear();
        }

        using QHash<QQmlType, QJSValue>::value;
        using QHash<QQmlType, QJSValue>::take;
    };

    SingletonInstances singletonInstances;
    QHash<int, QQmlGadgetPtrWrapper *> cachedValueTypeInstances;

    // These members must be protected by the engine's mutex as they are required by
    // the threaded loader.  Only access them through their respective accessor methods.
    QHash<int, QV4::ExecutableCompilationUnit *> m_compositeTypes;
    static bool s_designerMode;

    void cleanupScarceResources();
    QQmlRefPointer<QQmlPropertyCache> findPropertyCacheInCompositeTypes(int t) const;
};

/*
   This function should be called prior to evaluation of any js expression,
   so that scarce resources are not freed prematurely (eg, if there is a
   nested javascript expression).
 */
inline void QQmlEnginePrivate::referenceScarceResources()
{
    scarceResourcesRefCount += 1;
}

/*
   This function should be called after evaluation of the js expression is
   complete, and so the scarce resources may be freed safely.
 */
inline void QQmlEnginePrivate::dereferenceScarceResources()
{
    Q_ASSERT(scarceResourcesRefCount > 0);
    scarceResourcesRefCount -= 1;

    // if the refcount is zero, then evaluation of the "top level"
    // expression must have completed.  We can safely release the
    // scarce resources.
    if (Q_LIKELY(scarceResourcesRefCount == 0)) {
        QV4::ExecutionEngine *engine = v4engine();
        if (Q_UNLIKELY(!engine->scarceResources.isEmpty())) {
            cleanupScarceResources();
        }
    }
}

/*!
Returns a QQmlPropertyCache for \a type with \a minorVersion.

The returned cache is not referenced, so if it is to be stored, call addref().
*/
QQmlRefPointer<QQmlPropertyCache> QQmlEnginePrivate::cache(
        const QQmlType &type, QTypeRevision version)
{
    Q_ASSERT(type.isValid());
    Q_ASSERT(type.containsRevisionedAttributes());

    QMutexLocker locker(&this->mutex);
    return QQmlMetaType::propertyCache(type, version);
}

QV4::ExecutionEngine *QQmlEnginePrivate::getV4Engine(QQmlEngine *e)
{
    Q_ASSERT(e);

    return e->handle();
}

QQmlEnginePrivate *QQmlEnginePrivate::get(QQmlEngine *e)
{
    Q_ASSERT(e);

    return e->d_func();
}

const QQmlEnginePrivate *QQmlEnginePrivate::get(const QQmlEngine *e)
{
    Q_ASSERT(e);

    return e ? e->d_func() : nullptr;
}

template<typename Context>
QQmlEnginePrivate *contextEngine(const Context &context)
{
    if (!context)
        return nullptr;
    if (QQmlEngine *engine = context->engine())
        return QQmlEnginePrivate::get(engine);
    return nullptr;
}

QQmlEnginePrivate *QQmlEnginePrivate::get(QQmlContext *c)
{
    return contextEngine(c);
}

QQmlEnginePrivate *QQmlEnginePrivate::get(const QQmlRefPointer<QQmlContextData> &c)
{
    return contextEngine(c);
}

QQmlEngine *QQmlEnginePrivate::get(QQmlEnginePrivate *p)
{
    Q_ASSERT(p);

    return p->q_func();
}

QQmlEnginePrivate *QQmlEnginePrivate::get(QV4::ExecutionEngine *e)
{
    QQmlEngine *qmlEngine = e->qmlEngine();
    if (!qmlEngine)
        return nullptr;
    return get(qmlEngine);
}

template<>
Q_QML_PRIVATE_EXPORT QJSValue QQmlEnginePrivate::singletonInstance<QJSValue>(const QQmlType &type);

template<typename T>
T QQmlEnginePrivate::singletonInstance(const QQmlType &type) {
    return qobject_cast<T>(singletonInstance<QJSValue>(type).toQObject());
}


QT_END_NAMESPACE

#endif // QQMLENGINE_P_H
