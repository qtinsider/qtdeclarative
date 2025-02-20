/******************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt JavaScript to C++ compiler.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
******************************************************************************/

#include <data/birthdayparty.h>
#include <data/cppbaseclass.h>
#include <data/objectwithmethod.h>

#include <QtTest>
#include <QtQml>
#include <QtGui/qcolor.h>

#if QT_CONFIG(process)
#include <QtCore/qprocess.h>
#endif

Q_IMPORT_QML_PLUGIN(TestTypesPlugin)

class tst_QmlCppCodegen : public QObject
{
    Q_OBJECT
private slots:
    void simpleBinding();
    void anchorsFill();
    void signalHandler();
    void idAccess();
    void globals();
    void multiLookup();
    void enums();
    void funcWithParams();
    void intOverflow();
    void stringLength();
    void scopeVsObject();
    void compositeTypeMethod();
    void excessiveParameters();
    void jsImport();
    void jsmoduleImport();
    void runInterpreted();
    void methods();
    void math();
    void unknownParameter();
    void array();
    void equalsUndefined();
    void conversions();
    void interestingFiles_data();
    void interestingFiles();
    void extendedTypes();
    void construct();
    void contextParam();
    void attachedType();
    void componentReturnType();
    void onAssignment();
    void failures();
    void enumScope();
    void unusedAttached();
    void attachedBaseEnum();
    void nullAccess();
    void interceptor();
    void nonNotifyable();
    void importsFromImportPath();
    void aliasLookup();
    void outOfBoundsArray();
    void compositeSingleton();
    void lotsOfRegisters();
    void inPlaceDecrement();
    void shifts();
    void valueTypeProperty();
    void propertyOfParent();
    void accessModelMethodFromOutSide();
    void functionArguments();
    void bindingExpression();
    void voidFunction();
    void overriddenProperty();
    void listLength();
    void parentProperty();
    void registerElimination();
    void asCast();
    void noQQmlData();
    void scopeObjectDestruction();
    void colorAsVariant();
    void bindToValueType();
    void undefinedResets();
    void innerObjectNonShadowable();
    void ownPropertiesNonShadowable();
    void modulePrefix();
    void colorString();
    void urlString();
    void callContextPropertyLookupResult();
    void deadShoeSize();
    void listIndices();
    void jsMathObject();
    void intEnumCompare();
    void attachedSelf();
    void functionReturningVoid();
};

void tst_QmlCppCodegen::simpleBinding()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/Test.qml"_qs));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(!object.isNull(), component.errorString().toUtf8().constData());
    QCOMPARE(object->property("foo").toInt(), int(3));

    {
        CppBaseClass *base = qobject_cast<CppBaseClass *>(object.data());
        Q_ASSERT(base);
        QVERIFY(!base->cppProp.hasBinding());
        QCOMPARE(base->cppProp.value(), 7);
        QVERIFY(base->cppProp2.hasBinding());
        QCOMPARE(base->cppProp2.value(), 14);
        base->cppProp.setValue(9);
        QCOMPARE(base->cppProp.value(), 9);
        QCOMPARE(base->cppProp2.value(), 18);
    }
}

void tst_QmlCppCodegen::anchorsFill()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/anchorsFill.qml"_qs));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(!object.isNull(), component.errorString().toUtf8().constData());

    QCOMPARE(object->property("width").toInt(), 234);
    QCOMPARE(object->children().length(), 2);

    QObject *child = object->children().front();
    QVERIFY(child);
    QCOMPARE(child->property("width").toInt(), 234);

    QObject *newParent = object->children().back();
    QVERIFY(newParent);
    QCOMPARE(newParent->property("width").toInt(), 47);

    child->setProperty("parent", QVariant::fromValue(newParent));
    QCOMPARE(child->property("width").toInt(), 47);
}

void tst_QmlCppCodegen::signalHandler()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/signal.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->objectName(), QString());
    QCOMPARE(object->property("ff").toInt(), 4);

    object->setObjectName(u"foo"_qs);
    QCOMPARE(object->property("ff").toInt(), 12);
}

void tst_QmlCppCodegen::idAccess()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/idAccess.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QVERIFY(object->property("y").toInt() != 48);
    QCOMPARE(object->property("y").toInt(), 12);
    object->setProperty("z", 13);
    QCOMPARE(object->property("y").toInt(), 13);
    object->setProperty("x", QVariant::fromValue(333));
    QCOMPARE(object->property("y").toInt(), 48);

    // The binding was broken by setting the property
    object->setProperty("z", 14);
    QCOMPARE(object->property("y").toInt(), 48);

    QObject *ttt = qmlContext(object.data())->objectForName(u"ttt"_qs);
    QFont f = qvariant_cast<QFont>(ttt->property("font"));
    QCOMPARE(f.pointSize(), 22);
}

static QByteArray arg1()
{
    const QStringList args = QCoreApplication::instance()->arguments();
    return args.length() > 1 ? args[1].toUtf8() : QByteArray("undefined");
}

void tst_QmlCppCodegen::globals()
{
    QQmlEngine engine;
    int exitCode = -1;
    QObject::connect(&engine, &QQmlEngine::exit, [&](int code) { exitCode = code; });
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/globals.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());

    const QByteArray message = QByteArray("Start 2 ") + arg1();
    QTest::ignoreMessage(QtDebugMsg, message.constData());

    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QTRY_COMPARE(exitCode, 0);

    QObject *application = qvariant_cast<QObject *>(object->property("application"));
    QVERIFY(application);
    QCOMPARE(QString::fromUtf8(application->metaObject()->className()),
             u"QQuickApplication"_qs);

    QTest::ignoreMessage(QtDebugMsg, "End");
    QMetaObject::invokeMethod(application, "aboutToQuit");

    const QVariant somewhere = object->property("somewhere");
    QCOMPARE(somewhere.userType(), QMetaType::QUrl);
    QCOMPARE(qvariant_cast<QUrl>(somewhere).toString(), u"qrc:/somewhere/else.qml"_qs);

    const QVariant somewhereString = object->property("somewhereString");
    QCOMPARE(somewhereString.userType(), QMetaType::QString);
    QCOMPARE(somewhereString.toString(), u"qrc:/somewhere/else.qml"_qs);

    const QVariant plain = object->property("plain");
    QCOMPARE(plain.userType(), QMetaType::QUrl);
    QCOMPARE(qvariant_cast<QUrl>(plain).toString(), u"/not/here.qml"_qs);
}

void tst_QmlCppCodegen::multiLookup()
{
    // Multiple lookups of singletons (Qt in this case) don't clash with one another.
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/immediateQuit.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());

    const QByteArray message = QByteArray("End: ") + arg1();
    QTest::ignoreMessage(QtDebugMsg, message.constData());

    QSignalSpy quitSpy(&engine, &QQmlEngine::quit);
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(quitSpy.count(), 1);
}

void tst_QmlCppCodegen::enums()
{
    QQmlEngine engine;
    {
        QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/Enums.qml"_qs));
        QVERIFY2(!component.isError(), component.errorString().toUtf8());

        QTest::ignoreMessage(QtWarningMsg, "qrc:/TestTypes/Enums.qml:4:1: "
                                           "QML Enums: Layout must be attached to Item elements");
        QScopedPointer<QObject> object(component.create());

        QVERIFY(!object.isNull());
        bool ok = false;
        QCOMPARE(object->property("appState").toInt(&ok), 2);
        QVERIFY(ok);
        QCOMPARE(object->property("color").toString(), u"blue"_qs);

        QTRY_COMPARE(object->property("appState").toInt(&ok), 1);
        QVERIFY(ok);
        QCOMPARE(object->property("color").toString(), u"green"_qs);

        const auto func = qmlAttachedPropertiesFunction(
                    object.data(), QMetaType::fromName("QQuickLayout*").metaObject());

        QTest::ignoreMessage(QtWarningMsg, "qrc:/TestTypes/enumsInOtherObject.qml:4:25: "
                                           "QML Enums: Layout must be attached to Item elements");
        QObject *attached = qmlAttachedPropertiesObject(object.data(), func);

        const QVariant prop = attached->property("alignment");
        QVERIFY(prop.isValid());
        QCOMPARE(qvariant_cast<Qt::Alignment>(prop), Qt::AlignCenter);
    }
    {
        QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/enumsInOtherObject.qml"_qs));
        QVERIFY2(!component.isError(), component.errorString().toUtf8());
        QScopedPointer<QObject> object(component.create());
        QVERIFY(!object.isNull());
        QCOMPARE(object->property("color").toString(), u"blue"_qs);
        QTRY_COMPARE(object->property("color").toString(), u"green"_qs);
    }
}

void tst_QmlCppCodegen::funcWithParams()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/funcWithParams.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("bar").toInt(), 30);
}

void tst_QmlCppCodegen::intOverflow()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/intOverflow.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("a").toDouble(), 1.09951162778e+12);
    QCOMPARE(object->property("b").toInt(), 5);
}

void tst_QmlCppCodegen::stringLength()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/stringLength.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("stringLength").toInt(), 8);
}

void tst_QmlCppCodegen::scopeVsObject()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/scopeVsObject.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("objectName").toString(), u"foobar"_qs);
}

void tst_QmlCppCodegen::compositeTypeMethod()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/compositeTypeMethod.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QSignalSpy spy(object.data(), SIGNAL(foo()));
    QTRY_VERIFY(spy.count() > 0);
}

void tst_QmlCppCodegen::excessiveParameters()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/excessiveParameters.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QSignalSpy spy(object.data(), SIGNAL(foo()));
    QTRY_VERIFY(spy.count() > 0);
}

void tst_QmlCppCodegen::jsImport()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/jsimport.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("value").toInt(), 42);
}

void tst_QmlCppCodegen::jsmoduleImport()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/jsmoduleimport.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("ok").toBool(), true);
    QVariant okFunc = object->property("okFunc");
    QCOMPARE(okFunc.metaType(), QMetaType::fromType<QJSValue>());
    QJSValue val = engine.toScriptValue(okFunc);
    QJSValue result = val.call();
    QVERIFY(result.isBool());
    QVERIFY(result.toBool());
}

void tst_QmlCppCodegen::methods()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/methods.qml"_qs));
    QVERIFY(component.isReady());

    QTest::ignoreMessage(QtDebugMsg, "The Bar");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"TypeError: .* is not a function"_qs));
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(obj);
    BirthdayParty *party(qobject_cast<BirthdayParty *>(obj.data()));

    QVERIFY(party && party->host());
    QCOMPARE(party->guestCount(), 5);

    bool foundGreen = false;
    bool foundFoo = false;
    for (int ii = 0; ii < party->guestCount(); ++ii) {
        if (party->guest(ii)->name() == u"William Green"_qs)
            foundGreen = true;
        if (party->guest(ii)->name() == u"The Foo"_qs)
            foundFoo = true;
    }

    QVERIFY(foundGreen);
    QVERIFY(foundFoo);

    QCOMPARE(obj->property("n1").toString(), u"onGurk"_qs);
    QCOMPARE(obj->property("n2").toString(), u"onSemmeln"_qs);
    QCOMPARE(obj->property("n3"), QVariant());

    {
        QVariant ret;
        obj->metaObject()->invokeMethod(obj.data(), "retrieveVar", Q_RETURN_ARG(QVariant, ret));
        QCOMPARE(ret.typeId(), QMetaType::QString);
        QCOMPARE(ret.toString(), u"Jack Smith"_qs);
    }

    {
        QString ret;
        obj->metaObject()->invokeMethod(obj.data(), "retrieveString", Q_RETURN_ARG(QString, ret));
        QCOMPARE(ret, u"Jack Smith"_qs);
    }

    QCOMPARE(party->host()->shoeSize(), 12);
    obj->metaObject()->invokeMethod(obj.data(), "storeElement");
    QCOMPARE(party->host()->shoeSize(), 13);
    QJSManagedValue v = engine.toManagedValue(obj->property("dresses"));
    QVERIFY(v.isArray());

    QJSManagedValue inner(v.property(2), &engine);
    QVERIFY(inner.isArray());
    QCOMPARE(inner.property(0).toInt(), 1);
    QCOMPARE(inner.property(1).toInt(), 2);
    QCOMPARE(inner.property(2).toInt(), 3);

    QCOMPARE(obj->property("enumValue").toInt(), 2);
}

void tst_QmlCppCodegen::math()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/math.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("a").toInt(), 9);
    QCOMPARE(object->property("b").toDouble(), 50.0 / 22.0);
}

void tst_QmlCppCodegen::unknownParameter()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/unknownParameter.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("cppProp").toInt(), 18);
}

void tst_QmlCppCodegen::array()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/array.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    const QJSValue value1 = object->property("values1").value<QJSValue>();
    QVERIFY(value1.isArray());
    QCOMPARE(value1.property(u"length"_qs).toInt(), 3);
    QCOMPARE(value1.property(0).toInt(), 1);
    QCOMPARE(value1.property(1).toInt(), 2);
    QCOMPARE(value1.property(2).toInt(), 3);

    const QJSValue value2 = object->property("values2").value<QJSValue>();
    QVERIFY(value2.isArray());
    QCOMPARE(value2.property(u"length"_qs).toInt(), 0);
}

void tst_QmlCppCodegen::equalsUndefined()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/equalsUndefined.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QCOMPARE(object->property("a").toInt(), 50);
    QCOMPARE(object->property("b").toInt(), 5000);
}

void tst_QmlCppCodegen::conversions()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/conversions.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());

    QTest::ignoreMessage(QtWarningMsg, "qrc:/TestTypes/conversions.qml:42: TypeError: Type error");

    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QVERIFY(object->property("nullIsNull").toBool());
    QVERIFY(!object->property("intIsNull").toBool());
    QVERIFY(!object->property("zeroIsNull").toBool());
    QVERIFY(!object->property("arrayIsNull").toBool());
    QVERIFY(!object->property("objectIsNull").toBool());
    QVERIFY(!object->property("nullIsNotNull").toBool());
    QVERIFY(object->property("intIsNotNull").toBool());
    QVERIFY(object->property("zeroIsNotNull").toBool());
    QVERIFY(object->property("arrayIsNotNull").toBool());
    QVERIFY(object->property("objectIsNotNull").toBool());
    QVERIFY(!object->property("neNull").toBool());
    QVERIFY(object->property("eqNull").toBool());

    QVERIFY(!object->property("boolEqualsBool").toBool());
    QVERIFY(object->property("boolNotEqualsBool").toBool());

    QCOMPARE(object->property("cmpEqInt").toInt(), 17);

    QCOMPARE(object->property("undefinedType").toString(), u"undefined"_qs);
    QCOMPARE(object->property("booleanType").toString(), u"boolean"_qs);
    QCOMPARE(object->property("numberType").toString(), u"number"_qs);
    QCOMPARE(object->property("stringType").toString(), u"string"_qs);
    QCOMPARE(object->property("objectType").toString(), u"object"_qs);
    QCOMPARE(object->property("symbolType").toString(), u"symbol"_qs);

    QJSManagedValue obj = engine.toManagedValue(object->property("anObject"));
    QCOMPARE(obj.property(u"a"_qs).toInt(), 12);
    QCOMPARE(obj.property(u"b"_qs).toInt(), 14);
    QCOMPARE(obj.property(u"c"_qs).toString(), u"somestring"_qs);

    QVERIFY(object->property("aInObject").toBool());
    QVERIFY(object->property("lengthInArray").toBool());
    QVERIFY(!object->property("fooInNumber").isValid());
    QVERIFY(!object->property("numberInObject").toBool());
    QVERIFY(object->property("numberInArray").toBool());

    QCOMPARE(object->property("varPlusVar").toDouble(), 2.0);
    QVERIFY(qIsNaN(object->property("varMinusVar").toDouble()));
    QVERIFY(qIsNaN(object->property("varTimesVar").toDouble()));
    QCOMPARE(object->property("varDivVar").toDouble(), 0.0);

    const QVariant stringPlusString = object->property("stringPlusString");
    QCOMPARE(stringPlusString.typeId(), QMetaType::QString);
    QCOMPARE(stringPlusString.toString(), u"1220"_qs);

    const QVariant stringMinusString = object->property("stringMinusString");
    QCOMPARE(stringMinusString.typeId(), QMetaType::Double);
    QCOMPARE(stringMinusString.toDouble(), -8.0);

    const QVariant stringTimesString = object->property("stringTimesString");
    QCOMPARE(stringTimesString.typeId(), QMetaType::Double);
    QCOMPARE(stringTimesString.toDouble(), 240.0);

    const QVariant stringDivString = object->property("stringDivString");
    QCOMPARE(stringDivString.typeId(), QMetaType::Double);
    QCOMPARE(stringDivString.toDouble(), 0.6);

    QCOMPARE(object->property("uglyString").toString(),
             u"with\nnewlinewith\"quotwith\\slashes"_qs);

    QCOMPARE(qvariant_cast<QObject *>(object->property("nullObject1")), nullptr);
    QCOMPARE(qvariant_cast<QObject *>(object->property("nullObject2")), object.data());

    QCOMPARE(object->property("passObjectLiteral").toInt(), 11);
    QCOMPARE(object->property("passArrayLiteral").toInt(), 4);

    QCOMPARE(object->property("doneStuff").toInt(), 19);

    QVariantList modulos = object->property("modulos").toList();
    QCOMPARE(modulos.length(), 7);

    QCOMPARE(modulos[0].userType(), QMetaType::Double);
    QCOMPARE(modulos[0].toDouble(), 0.0);

    QCOMPARE(modulos[1].userType(), QMetaType::Int);
    QCOMPARE(modulos[1].toInt(), 0);

    QCOMPARE(modulos[2].userType(), QMetaType::Double);
    QCOMPARE(modulos[2].toDouble(), 2.4);

    QCOMPARE(modulos[3].userType(), QMetaType::Double);
    QCOMPARE(modulos[3].toDouble(), -0.4);

    QCOMPARE(modulos[4].userType(), QMetaType::Double);
    QCOMPARE(modulos[4].toDouble(), 0.0);

    QCOMPARE(modulos[5].userType(), QMetaType::Double);
    QVERIFY(qIsNaN(modulos[5].toDouble()));

    QCOMPARE(modulos[6].userType(), QMetaType::Double);
    QVERIFY(qIsNaN(modulos[6].toDouble()));

    QVariantList unaryOps = object->property("unaryOps").toList();
    QCOMPARE(unaryOps.length(), 6);

    QCOMPARE(unaryOps[0].userType(), QMetaType::Double);
    QCOMPARE(unaryOps[0].toDouble(), 1221);

    QCOMPARE(unaryOps[1].userType(), QMetaType::Double);
    QCOMPARE(unaryOps[1].toDouble(), 1219);

    QCOMPARE(unaryOps[2].userType(), QMetaType::Double);
    QCOMPARE(unaryOps[2].toDouble(), 1220);

    QCOMPARE(unaryOps[3].userType(), QMetaType::Double);
    QCOMPARE(unaryOps[3].toDouble(), -1220);

    QCOMPARE(unaryOps[4].userType(), QMetaType::Double);
    QCOMPARE(unaryOps[4].toDouble(), 1220);

    QCOMPARE(unaryOps[5].userType(), QMetaType::Double);
    QCOMPARE(unaryOps[5].toDouble(), 1220);

    QVariant undef;
    QVERIFY(object->metaObject()->invokeMethod(object.data(), "retUndefined",
                                               Q_RETURN_ARG(QVariant, undef)));
    QVERIFY(!undef.isValid());
}

void tst_QmlCppCodegen::interestingFiles_data()
{
    QTest::addColumn<QString>("file");
    QTest::addRow("conversions2") << u"conversions2.qml"_qs;
    QTest::addRow("TestCase") << u"TestCase.qml"_qs;
    QTest::addRow("layouts") << u"layouts.qml"_qs;
    QTest::addRow("interactive") << u"interactive.qml"_qs;
    QTest::addRow("Panel") << u"Panel.qml"_qs;
    QTest::addRow("ProgressBar") << u"ProgressBar/ProgressBar.ui.qml"_qs;
    QTest::addRow("Root") << u"ProgressBar/Root.qml"_qs;
    QTest::addRow("noscope") << u"noscope.qml"_qs;
    QTest::addRow("dynamicscene") << u"dynamicscene.qml"_qs;
    QTest::addRow("curlygrouped") << u"curlygrouped.qml"_qs;
}

void tst_QmlCppCodegen::interestingFiles()
{
    QFETCH(QString, file);

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/%1"_qs.arg(file)));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
}

void tst_QmlCppCodegen::extendedTypes()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/extendedTypes.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());

    QTest::ignoreMessage(QtDebugMsg, "6 QSizeF(10, 20) 30");
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QCOMPARE(object->property("a").toInt(), 6);
    QCOMPARE(qvariant_cast<QSizeF>(object->property("b")), QSizeF(10, 20));
    QCOMPARE(object->property("c").toInt(), 30);
    QCOMPARE(object->property("d").toString(), u"QSizeF(10, 20)"_qs);

    QCOMPARE(object->property("e").toInt(), 2);
}

void tst_QmlCppCodegen::construct()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/construct.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    const QJSManagedValue v = engine.toManagedValue(object->property("foo"));
    QVERIFY(v.isError());
    QCOMPARE(v.toString(), u"Error: bar"_qs);

    QCOMPARE(object->property("aaa").toInt(), 12);
    QTest::ignoreMessage(QtWarningMsg, "qrc:/TestTypes/construct.qml:9: Error: ouch");
    object->metaObject()->invokeMethod(object.data(), "ouch");
    QCOMPARE(object->property("aaa").toInt(), 13);
}

void tst_QmlCppCodegen::contextParam()
{
    // The compiler cannot resolve context parameters.
    // Make sure the binding is interpreted.

    QQmlEngine engine;

    QVariantMap m;
    m.insert(u"foo"_qs, 10);
    engine.rootContext()->setContextProperty(u"contextParam"_qs, m);

    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/contextParam.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QCOMPARE(object->property("foo").toInt(), 10);
}

void tst_QmlCppCodegen::attachedType()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/text.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("dayz").toDateTime(), QDateTime(QDate(1911, 3, 4), QTime()));
    QCOMPARE(object->property("oParty").toDateTime(), QDateTime(QDate(1911, 3, 4), QTime()));

    QObject *party = qvariant_cast<QObject *>(object->property("party"));
    QVERIFY(party);
    QCOMPARE(party->property("eee").toInt(), 21);
    QCOMPARE(party->property("fff").toInt(), 33);
    QCOMPARE(object->property("ggg").toInt(), 37);
}

void tst_QmlCppCodegen::componentReturnType()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/componentReturnType.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());

    QCOMPARE(object->property("count").toInt(), 10);
    QCOMPARE(QQmlListReference(object.data(), "children", &engine).count(), 11);
}

void tst_QmlCppCodegen::onAssignment()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/pressAndHoldButton.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());

    QCOMPARE(object->property("pressed").toBool(), false);
    QCOMPARE(object->property("scale").toDouble(), 1.0);

    object->metaObject()->invokeMethod(object.data(), "press");
    QTRY_COMPARE(object->property("pressed").toBool(), true);
    QCOMPARE(object->property("scale").toDouble(), 0.9);

    object->metaObject()->invokeMethod(object.data(), "release");
    QCOMPARE(object->property("pressed").toBool(), false);
    QCOMPARE(object->property("scale").toDouble(), 1.0);
}

namespace QmlCacheGeneratedCode {
namespace _0x5f_TestTypes_failures_qml {
extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
}
}

void tst_QmlCppCodegen::failures()
{
    const auto &aotFailure
            = QmlCacheGeneratedCode::_0x5f_TestTypes_failures_qml::aotBuiltFunctions[0];
    QVERIFY(aotFailure.argumentTypes.isEmpty());
    QVERIFY(!aotFailure.functionPtr);
    QCOMPARE(aotFailure.index, 0);
}

void tst_QmlCppCodegen::enumScope()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/enumScope.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QCOMPARE(object->property("flow").toInt(), 1);
}

void tst_QmlCppCodegen::unusedAttached()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/unusedAttached.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());

    const auto func = qmlAttachedPropertiesFunction(
                object.data(), QMetaType::fromName("QQuickKeyNavigationAttached*").metaObject());
    QObject *attached = qmlAttachedPropertiesObject(object.data(), func);
    const QVariant prop = attached->property("priority");
    QVERIFY(prop.isValid());
    QCOMPARE(QByteArray(prop.metaType().name()), "QQuickKeyNavigationAttached::Priority");
    bool ok = false;
    QCOMPARE(prop.toInt(&ok), 0);
    QVERIFY(ok);
}

void tst_QmlCppCodegen::attachedBaseEnum()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/attachedBaseEnum.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());

    QObject *drag = qvariant_cast<QObject *>(object->property("drag"));
    QVERIFY(drag);

    // Drag.YAxis is 2, but we cannot #include it here.
    bool ok = false;
    QCOMPARE(drag->property("axis").toInt(&ok), 2);
    QVERIFY(ok);
}

void tst_QmlCppCodegen::nullAccess()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/nullAccess.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());

    QTest::ignoreMessage(QtWarningMsg,
                         "qrc:/TestTypes/nullAccess.qml:4:5: TypeError: "
                         "Cannot read property 'width' of null");
    QTest::ignoreMessage(QtWarningMsg,
                         "qrc:/TestTypes/nullAccess.qml:5:5: TypeError: "
                         "Cannot read property 'height' of null");
    QTest::ignoreMessage(QtWarningMsg,
                         "qrc:/TestTypes/nullAccess.qml:6: TypeError: Value is null and "
                         "could not be converted to an object");
    QScopedPointer<QObject> object(component.create());

    QCOMPARE(object->property("width").toDouble(), 0.0);
    QCOMPARE(object->property("height").toDouble(), 0.0);
}

void tst_QmlCppCodegen::interceptor()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/interceptor.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QCOMPARE(object->property("x").toInt(), 100);
    QCOMPARE(object->property("y").toInt(), 100);

    QVERIFY(object->property("width").toInt() != 200);
    QVERIFY(object->property("height").toInt() != 200);
    QVERIFY(object->property("qProperty1").toInt() != 300);
    QVERIFY(object->property("qProperty2").toInt() != 300);
    QTRY_COMPARE(object->property("width").toInt(), 200);
    QTRY_COMPARE(object->property("height").toInt(), 200);
    QTRY_COMPARE(object->property("qProperty1").toInt(), 300);
    QTRY_COMPARE(object->property("qProperty2").toInt(), 300);
}

void tst_QmlCppCodegen::nonNotifyable()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/nonNotifyable.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QCOMPARE(qvariant_cast<QDateTime>(object->property("dayz")),
             QDateTime(QDate(2121, 1, 12), QTime()));
    QCOMPARE(qvariant_cast<QDateTime>(object->property("oParty")),
             QDateTime(QDate(2111, 12, 11), QTime()));
}

void tst_QmlCppCodegen::importsFromImportPath()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/importsFromImportPath.qml"_qs));

    // We might propagate the import path, eventually, but for now instantiating is not important.
    // If the compiler accepts the file, it's probably fine.
    QVERIFY(component.isError());
    QCOMPARE(component.errorString(),
             u"qrc:/TestTypes/importsFromImportPath.qml:1 module \"Module\" is not installed\n"_qs);
}

void tst_QmlCppCodegen::aliasLookup()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/aliasLookup.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    const QVariant t = object->property("t");
    QCOMPARE(t.metaType(), QMetaType::fromType<QString>());
    QCOMPARE(t.toString(), u"12"_qs);
}

void tst_QmlCppCodegen::outOfBoundsArray()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/outOfBounds.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());

    QTest::ignoreMessage(QtDebugMsg, "oob undefined");
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QVERIFY(object->metaObject()->indexOfProperty("oob") > 0);
    QVERIFY(!object->property("oob").isValid());
}

void tst_QmlCppCodegen::compositeSingleton()
{
    QQmlEngine engine;
    engine.addImportPath(u":/TestTypes/imports/"_qs);
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/compositesingleton.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> o(component.create());
    QCOMPARE(o->property("x").toDouble(), 4.5);
    QCOMPARE(o->property("y").toDouble(), 10.0);
    QCOMPARE(o->property("smooth").toBool(), true);
}

void tst_QmlCppCodegen::lotsOfRegisters()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/page.qml"_qs));
    QVERIFY2(!component.isError(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    const auto compare = [&]() {
        const qreal implicitBackgroundWidth = object->property("implicitBackgroundWidth").toDouble();
        const qreal leftInset = object->property("leftInset").toDouble();
        const qreal rightInset = object->property("rightInset").toDouble();
        const qreal contentWidth = object->property("contentWidth").toDouble();
        const qreal leftPadding = object->property("leftPadding").toDouble();
        const qreal rightPadding = object->property("rightPadding").toDouble();
        const qreal implicitFooterWidth = object->property("implicitFooterWidth").toDouble();
        const qreal implicitHeaderWidth = object->property("implicitHeaderWidth").toDouble();

        const qreal implicitWidth = object->property("implicitWidth").toDouble();
        QCOMPARE(implicitWidth, qMax(qMax(implicitBackgroundWidth + leftInset + rightInset,
                                     contentWidth + leftPadding + rightPadding),
                                     qMax(implicitHeaderWidth, implicitFooterWidth)));
    };

    compare();

    const QList<const char *> props = {
        "leftInset", "rightInset", "contentWidth", "leftPadding", "rightPadding"
    };

    for (int i = 0; i < 100; ++i) {
        QVERIFY(object->setProperty(props[i % props.length()], (i * 17) % 512));
        compare();
    }
}

void tst_QmlCppCodegen::inPlaceDecrement()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/dialog.qml"_qs));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QObject *header = qvariant_cast<QObject *>(object->property("header"));
    QVERIFY(header);
    QObject *background = qvariant_cast<QObject *>(header->property("background"));
    QObject *parent = qvariant_cast<QObject *>(background->property("parent"));

    QCOMPARE(background->property("width").toInt(), parent->property("width").toInt() + 1);
    QCOMPARE(background->property("height").toInt(), parent->property("height").toInt() - 1);

    QVERIFY(object->setProperty("width", QVariant::fromValue(17)));
    QVERIFY(parent->property("width").toInt() > 0);
    QVERIFY(object->setProperty("height", QVariant::fromValue(53)));
    QVERIFY(parent->property("height").toInt() > 0);

    QCOMPARE(background->property("width").toInt(), parent->property("width").toInt() + 1);
    QCOMPARE(background->property("height").toInt(), parent->property("height").toInt() - 1);

    QCOMPARE(object->property("a").toInt(), 1024);
}

void tst_QmlCppCodegen::shifts()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/shifts.qml"_qs));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QCOMPARE(object->property("a").toInt(), 9728);
    QCOMPARE(object->property("b").toInt(), 4864);
    QCOMPARE(object->property("c").toInt(), 19448);
    QCOMPARE(object->property("d").toInt(), 9731);
    QCOMPARE(object->property("e").toInt(), 0);
}

void tst_QmlCppCodegen::valueTypeProperty()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/valueTypeProperty.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());

    QFont font = qvariant_cast<QFont>(object->property("font"));
    QCOMPARE(object->property("foo").toString(), font.family());
    font.setFamily(u"Bar"_qs);
    object->setProperty("font", QVariant::fromValue(font));
    QCOMPARE(object->property("foo").toString(), u"Bar"_qs);
}

void tst_QmlCppCodegen::propertyOfParent()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/RootWithoutId.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());

    QObject *child = qmlContext(object.data())->objectForName(u"item"_qs);

    bool expected = false;

    for (int i = 0; i < 3; ++i) {
        const QVariant foo = object->property("foo");
        QCOMPARE(foo.metaType(), QMetaType::fromType<bool>());
        QCOMPARE(foo.toBool(), expected);

        const QVariant bar = object->property("bar");
        QCOMPARE(bar.metaType(), QMetaType::fromType<bool>());
        QCOMPARE(bar.toBool(), expected);

        const QVariant visible = child->property("visible");
        QCOMPARE(visible.metaType(), QMetaType::fromType<bool>());
        QCOMPARE(visible.toBool(), expected);

        expected = !expected;
        object->setProperty("foo", expected);
    }
}

void tst_QmlCppCodegen::accessModelMethodFromOutSide()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/AccessModelMethodsFromOutside.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());

    QTest::ignoreMessage(QtDebugMsg, "3");
    QTest::ignoreMessage(QtDebugMsg, "Apple");
    QScopedPointer<QObject> object(component.create());

    QCOMPARE(object->property("cost1").toDouble(), 3);
    QCOMPARE(object->property("name1").toString(), u"Orange"_qs);
    QCOMPARE(object->property("cost2").toDouble(), 1.95);
    QCOMPARE(object->property("name2").toString(), u"Banana"_qs);
}

void tst_QmlCppCodegen::functionArguments()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/Dummy.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());

    const QMetaObject *metaObject = object->metaObject();

    int result;
    int a = 1;
    bool b = false;
    QObject *c = nullptr;
    double d = -1.2;
    int e = 3;

    const QByteArray className = QByteArray(metaObject->className()) + '*';
    metaObject->invokeMethod(
                object.data(), "someFunction", Q_RETURN_ARG(int, result),
                Q_ARG(int, a), Q_ARG(bool, b), QGenericArgument(className, &c),
                Q_ARG(double, d), Q_ARG(int, e));
    QCOMPARE(result, 42);

    QString astr = u"foo"_qs;
    QString bstr = u"bar"_qs;
    QString concatenated;
    metaObject->invokeMethod(
                object.data(), "concat", Q_RETURN_ARG(QString, concatenated),
                Q_ARG(QString, astr), Q_ARG(QString, bstr));
    QCOMPARE(concatenated, u"foobar"_qs);
}

void tst_QmlCppCodegen::bindingExpression()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/BindingExpression.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());

    QObject *child = qmlContext(object.data())->objectForName(u"child"_qs);

    double width = 200;
    double y = 10;
    for (int i = 0; i < 10; ++i) {
        QCOMPARE(object->property("width").toDouble(), width);
        QCOMPARE(object->property("height").toDouble(), width);
        QCOMPARE(object->property("y").toDouble(), y);

        const double childY = y + (width - 100) / 2;
        QCOMPARE(child->property("y").toDouble(), childY);
        QCOMPARE(object->property("mass"), childY > 100 ? u"heavy"_qs : u"light"_qs);
        QCOMPARE(object->property("test_division").toDouble(), width / 1000 + 50);
        QCOMPARE(object->property("test_ternary").toDouble(), 2.2);

        const int test_switch = object->property("test_switch").toInt();
        switch (int(width) % 3) {
        case 0:
            QCOMPARE(test_switch, 130);
            break;
        case 1:
            QCOMPARE(test_switch, 380);
            break;
        case 2:
            QCOMPARE(test_switch, 630);
            break;
        }

        width = 200 * i;
        y = 10 + i;
        object->setProperty("width", width);
        object->setProperty("y", y);
    }
}

void tst_QmlCppCodegen::voidFunction()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/voidfunction.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QVERIFY(object->objectName().isEmpty());
    object->metaObject()->invokeMethod(object.data(), "doesNotReturnValue");
    QCOMPARE(object->objectName(), u"barbar"_qs);
}

void tst_QmlCppCodegen::overriddenProperty()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/childobject.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->objectName(), u"kraut"_qs);
    QCOMPARE(object->property("doneThing").toInt(), 5);
    QCOMPARE(object->property("usingFinal").toInt(), 5);

    auto checkAssignment = [&]() {
        const QString newName = u"worscht"_qs;
        QMetaObject::invokeMethod(object.data(), "setChildObjectName", Q_ARG(QString, newName));
        QCOMPARE(object->objectName(), newName);
    };
    checkAssignment();

    ObjectWithMethod *benign = new ObjectWithMethod(object.data());
    benign->theThing = 10;
    benign->setObjectName(u"cabbage"_qs);
    object->setProperty("child", QVariant::fromValue(benign));
    QCOMPARE(object->objectName(), u"cabbage"_qs);
    checkAssignment();
    QCOMPARE(object->property("doneThing").toInt(), 10);
    QCOMPARE(object->property("usingFinal").toInt(), 10);

    OverriddenObjectName *evil = new OverriddenObjectName(object.data());
    QTest::ignoreMessage(QtWarningMsg,
                         "Final member fff is overridden in class OverriddenObjectName. "
                         "The override won't be used.");
    object->setProperty("child", QVariant::fromValue(evil));

    QCOMPARE(object->objectName(), u"borschtsch"_qs);

    checkAssignment();
    QCOMPARE(object->property("doneThing").toInt(), 7);
    QCOMPARE(object->property("usingFinal").toInt(), 5);
}

void tst_QmlCppCodegen::listLength()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/listlength.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("l").toInt(), 2);
}

void tst_QmlCppCodegen::parentProperty()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/parentProp.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("c").toInt(), 11);
    QCOMPARE(object->property("i").toInt(), 22);
    object->setProperty("a", QVariant::fromValue(22));
    QCOMPARE(object->property("c").toInt(), 28);
    object->setProperty("implicitWidth", QVariant::fromValue(14));
    QCOMPARE(object->property("i").toInt(), 26);

    QObject *child = qmlContext(object.data())->objectForName(u"child"_qs);
    QObject *sibling = qmlContext(object.data())->objectForName(u"sibling"_qs);
    QObject *evil = qmlContext(object.data())->objectForName(u"evil"_qs);

    child->setProperty("parent", QVariant::fromValue(sibling));

    QCOMPARE(child->property("b").toInt(), 0);
    QCOMPARE(child->property("i").toInt(), 28);
    QCOMPARE(object->property("i").toInt(), 56);

    child->setProperty("parent", QVariant::fromValue(evil));

    QCOMPARE(child->property("b").toInt(), 5994);
    QCOMPARE(object->property("c").toInt(), 5996);

    QCOMPARE(child->property("i").toInt(), 443);
    QCOMPARE(object->property("i").toInt(), 886);

    {
        QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/specificParent.qml"_qs));

        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> rootObject(component.create());
        QVERIFY(rootObject);

        QCOMPARE(rootObject->property("a").toReal(), 77.0);
    }
}

void tst_QmlCppCodegen::registerElimination()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/registerelimination.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    // Increment of 23 hits both 0 and 460
    for (int input = -23; input < 700; input += 23) {
        object->setProperty("input", input);
        if (input <= 0 || input >= 460)
            QCOMPARE(object->property("output").toInt(), 459);
        else
            QCOMPARE(object->property("output").toInt(), input);
    }
}

void tst_QmlCppCodegen::asCast()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/asCast.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());
    QScopedPointer<QObject> root(component.create());
    QVERIFY(!root.isNull());

    QQmlContext *context = qmlContext(root.data());
    const QObject *object = context->objectForName(u"object"_qs);
    const QObject *item = context->objectForName(u"item"_qs);
    const QObject *rectangle = context->objectForName(u"rectangle"_qs);
    const QObject *dummy = context->objectForName(u"dummy"_qs);

    QCOMPARE(qvariant_cast<QObject *>(root->property("objectAsObject")), object);
    QCOMPARE(qvariant_cast<QObject *>(root->property("objectAsItem")), nullptr);
    QCOMPARE(qvariant_cast<QObject *>(root->property("objectAsRectangle")), nullptr);
    QCOMPARE(qvariant_cast<QObject *>(root->property("objectAsDummy")), nullptr);

    QCOMPARE(qvariant_cast<QObject *>(root->property("itemAsObject")), item);
    QCOMPARE(qvariant_cast<QObject *>(root->property("itemAsItem")), item);
    QCOMPARE(qvariant_cast<QObject *>(root->property("itemAsRectangle")), nullptr);
    QCOMPARE(qvariant_cast<QObject *>(root->property("itemAsDummy")), nullptr);

    QCOMPARE(qvariant_cast<QObject *>(root->property("rectangleAsObject")), rectangle);
    QCOMPARE(qvariant_cast<QObject *>(root->property("rectangleAsItem")), rectangle);
    QCOMPARE(qvariant_cast<QObject *>(root->property("rectangleAsRectangle")), rectangle);
    QCOMPARE(qvariant_cast<QObject *>(root->property("rectangleAsDummy")), nullptr);

    QCOMPARE(qvariant_cast<QObject *>(root->property("dummyAsObject")), dummy);
    QCOMPARE(qvariant_cast<QObject *>(root->property("dummyAsItem")), dummy);
    QCOMPARE(qvariant_cast<QObject *>(root->property("dummyAsRectangle")), nullptr);
    QCOMPARE(qvariant_cast<QObject *>(root->property("dummyAsDummy")), dummy);
}

void tst_QmlCppCodegen::noQQmlData()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/noQQmlData.qml"_qs));
    QVERIFY2(component.isReady(), component.errorString().toUtf8());

    QTest::ignoreMessage(QtWarningMsg, "qrc:/TestTypes/noQQmlData.qml:7: TypeError: "
                                       "Cannot read property 'name' of null");
    QScopedPointer<QObject> root(component.create());
    QVERIFY(!root.isNull());

    BirthdayParty *party = qobject_cast<BirthdayParty *>(root.data());
    QVERIFY(party != nullptr);

    QCOMPARE(party->host(), nullptr);
    QCOMPARE(party->property("n").toString(), QString());

    Person *host1 = new Person(party);
    party->setHost(host1);
    QCOMPARE(party->property("n").toString(), u"Bart in da house!"_qs);
    host1->setName(u"Marge"_qs);
    QCOMPARE(party->property("n").toString(), u"Marge in da house!"_qs);

    QTest::ignoreMessage(QtWarningMsg, "qrc:/TestTypes/noQQmlData.qml:7: TypeError: "
                                       "Cannot read property 'name' of null");

    // Doesn't crash
    party->setHost(nullptr);

    // Lookups are initialized now, and we introduce an object without QQmlData
    Person *host2 = new Person(party);
    party->setHost(host2);
    QCOMPARE(party->property("n").toString(), u"Bart in da house!"_qs);
    host2->setName(u"Homer"_qs);
    QCOMPARE(party->property("n").toString(), u"Homer in da house!"_qs);

    QMetaObject::invokeMethod(party, "burn");
    engine.collectGarbage();

    // Does not crash
    party->setProperty("inDaHouse", u" burns!"_qs);

    // Mr Burns may or may not burn, depending on whether we use lookups.
    // If using lookups, the binding is aborted when we find the isQueuedForDeletion flag.
    // If reading the property directly, we don't have to care about it.
    QVERIFY(party->property("n").toString().startsWith(u"Mr Burns"_qs));
}

void tst_QmlCppCodegen::scopeObjectDestruction()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/fileDialog.qml"_qs));

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> rootObject(component.create());
    QVERIFY(rootObject);

    QObject *dialog = rootObject->property("dialog").value<QObject *>();
    QVERIFY(dialog);

    // We cannot check the warning messages. The AOT compiled code complains about reading the
    // "parent" property of an object scheduled for deletion. The runtime silently returns undefined
    // at that point and then complains about not being able to read a property on undefined.

    // Doesn't crash, even though it triggers bindings on scope objects scheduled for deletion.
    QMetaObject::invokeMethod(dialog, "open");
}

static void checkColorProperties(QQmlComponent *component)
{
    QVERIFY2(component->isReady(), qPrintable(component->errorString()));
    QScopedPointer<QObject> rootObject(component->create());
    QVERIFY(rootObject);

    const QMetaObject *mo = QMetaType::fromName("QQuickIcon").metaObject();
    QVERIFY(mo != nullptr);

    const QMetaProperty prop = mo->property(mo->indexOfProperty("color"));
    QVERIFY(prop.isValid());

    const QVariant a = rootObject->property("a");
    QVERIFY(a.isValid());

    const QVariant iconColor = prop.readOnGadget(rootObject->property("icon").data());
    QVERIFY(iconColor.isValid());

    const QMetaType colorType = QMetaType::fromName("QColor");
    QVERIFY(colorType.isValid());

    QCOMPARE(a.metaType(), colorType);
    QCOMPARE(iconColor.metaType(), colorType);

    QCOMPARE(iconColor, a);
}

void tst_QmlCppCodegen::colorAsVariant()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/colorAsVariant.qml"_qs));
    checkColorProperties(&component);
}

void tst_QmlCppCodegen::bindToValueType()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/bindToValueType.qml"_qs));
    checkColorProperties(&component);
}

void tst_QmlCppCodegen::undefinedResets()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/undefinedResets.qml"_qs));

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> rootObject(component.create());
    QVERIFY(rootObject);

    Person *person = qobject_cast<Person *>(rootObject.data());
    QVERIFY(person);
    QCOMPARE(person->shoeSize(), 0);
    QCOMPARE(person->name(), u"Marge"_qs);

    person->setShoeSize(11);

    QCOMPARE(person->shoeSize(), 11);
    QCOMPARE(person->name(), u"Bart"_qs);

    person->setShoeSize(10);
    QCOMPARE(person->shoeSize(), 10);
    QCOMPARE(person->name(), u"Marge"_qs);
}

void tst_QmlCppCodegen::innerObjectNonShadowable()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/ownProperty.qml"_qs));

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> rootObject(component.create());
    QVERIFY(rootObject);

    QCOMPARE(rootObject->objectName(), u"foo"_qs);
}

void tst_QmlCppCodegen::ownPropertiesNonShadowable()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/overriddenMember.qml"_qs));

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> rootObject(component.create());
    QVERIFY(rootObject);

    QCOMPARE(rootObject->property("ppp").toInt(), 16);
    QCOMPARE(rootObject->property("ppp2").toInt(), 9);
    QCOMPARE(rootObject->property("ppp3").toInt(), 12);
}

void tst_QmlCppCodegen::modulePrefix()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/modulePrefix.qml"_qs));

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> rootObject(component.create());
    QVERIFY(rootObject);

    QCOMPARE(rootObject->property("foo").toDateTime(), QDateTime(QDate(1911, 3, 4), QTime()));
    QCOMPARE(rootObject->property("bar").toDateTime(), QDateTime(QDate(1911, 3, 4), QTime()));
    QCOMPARE(rootObject->property("baz").toString(), QStringLiteral("ItIsTheSingleton"));
}

void tst_QmlCppCodegen::colorString()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/colorString.qml"_qs));

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> rootObject(component.create());
    QVERIFY(rootObject);

    QCOMPARE(qvariant_cast<QColor>(rootObject->property("c")), QColor::fromRgb(0xdd, 0xdd, 0xdd));
    QCOMPARE(qvariant_cast<QColor>(rootObject->property("d")), QColor::fromRgb(0xaa, 0xaa, 0xaa));
    QCOMPARE(qvariant_cast<QColor>(rootObject->property("e")), QColor::fromRgb(0x11, 0x22, 0x33));
}

void tst_QmlCppCodegen::urlString()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/TestTypes/urlString.qml"_qs));

    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> rootObject(component.create());
    QVERIFY(rootObject);

    QCOMPARE(qvariant_cast<QUrl>(rootObject->property("c")), QUrl(u"http://dddddd.com"_qs));
    QCOMPARE(qvariant_cast<QUrl>(rootObject->property("d")), QUrl(u"http://aaaaaa.com"_qs));
    QCOMPARE(qvariant_cast<QUrl>(rootObject->property("e")), QUrl(u"http://a112233.de"_qs));
}


void tst_QmlCppCodegen::callContextPropertyLookupResult()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, QUrl(u"qrc:/TestTypes/callContextPropertyLookupResult.qml"_qs));
    QVERIFY2(c.isReady(), qPrintable(c.errorString()));
    QScopedPointer<QObject> o(c.create());
    QVERIFY(o);

    QVERIFY(qvariant_cast<QQmlComponent *>(o->property("c")) != nullptr);
}

void tst_QmlCppCodegen::deadShoeSize()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, QUrl(u"qrc:/TestTypes/deadShoeSize.qml"_qs));
    QVERIFY2(c.isReady(), qPrintable(c.errorString()));
    QTest::ignoreMessage(QtWarningMsg, "qrc:/TestTypes/deadShoeSize.qml:5: Error: ouch");
    QScopedPointer<QObject> o(c.create());
    QVERIFY(o);
    QCOMPARE(o->property("shoeSize").toInt(), 0);
}

void tst_QmlCppCodegen::listIndices()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, QUrl(u"qrc:/TestTypes/listIndices.qml"_qs));
    QVERIFY2(c.isReady(), qPrintable(c.errorString()));
    QScopedPointer<QObject> o(c.create());
    QVERIFY(o);

    QQmlListReference list(o.data(), "items");
    QCOMPARE(list.count(), 3);
    for (int i = 0; i < 3; ++i)
        QCOMPARE(list.at(i), o.data());
    QCOMPARE(o->property("numItems").toInt(), 3);
}

void tst_QmlCppCodegen::jsMathObject()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, QUrl(u"qrc:/TestTypes/jsMathObject.qml"_qs));
    QVERIFY2(c.isReady(), qPrintable(c.errorString()));
    QScopedPointer<QObject> o(c.create());
    QVERIFY(o);

    const double inputs[] = {
        qQNaN(), -qInf(),
        std::numeric_limits<double>::min(),
        std::numeric_limits<float>::min(),
        std::numeric_limits<qint32>::min(),
        -1000.2, -100, -2, -1.333, -1, -0.84, -0.5,
        std::copysign(0.0, -1), 0.0,
        0.5, 0.77, 1, 1.4545, 2, 199, 2002.13,
        std::numeric_limits<qint32>::max(),
        std::numeric_limits<quint32>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<double>::max(),
    };

    QJSManagedValue math(engine.globalObject().property(QStringLiteral("Math")), &engine);

    const QMetaObject *metaObject = o->metaObject();

    QString name;
    for (double a : inputs) {
        for (double b : inputs) {
            o->setProperty("a", a);
            o->setProperty("b", b);
            for (int i = 0, end = metaObject->propertyCount(); i != end; ++i) {
                const QMetaProperty prop = metaObject->property(i);
                name = QString::fromUtf8(prop.name());

                if (!math.hasProperty(name))
                    continue;

                const double result = prop.read(o.data()).toDouble();
                if (name == QStringLiteral("random")) {
                    QVERIFY(result >= 0.0 && result < 1.0);
                    continue;
                }

                QJSManagedValue jsMethod(math.property(name), &engine);
                const double expected = jsMethod.call(QJSValueList {a, b}).toNumber();
                QCOMPARE(result, expected);
            }
        }
    }

    if (QTest::currentTestFailed())
        qDebug() << name << "failed.";
}

void tst_QmlCppCodegen::intEnumCompare()
{
    QQmlEngine engine;
    {
        QQmlComponent c(&engine, QUrl(u"qrc:/TestTypes/intEnumCompare.qml"_qs));
        QVERIFY2(c.isReady(), qPrintable(c.errorString()));
        QScopedPointer<QObject> o(c.create());
        QVERIFY(o);
        QCOMPARE(o->property("a").toBool(), true);
        QCOMPARE(o->property("b").toBool(), false);
        QCOMPARE(o->property("c").toBool(), true);
        QCOMPARE(o->property("d").toBool(), false);
    }

    {
        // We cannot use Qt.red in QML because it's lower case.
        QQmlComponent c(&engine, QUrl(u"qrc:/TestTypes/enumInvalid.qml"_qs));
        QVERIFY2(c.isReady(), qPrintable(c.errorString()));
        QScopedPointer<QObject> o(c.create());
        QVERIFY(o);
        QCOMPARE(o->property("c").toBool(), false);
        QCOMPARE(o->property("d").toBool(), false);
    }
}

void tst_QmlCppCodegen::attachedSelf()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, QUrl(u"qrc:/TestTypes/SelectionRectangle.qml"_qs));
    QVERIFY2(c.isReady(), qPrintable(c.errorString()));
    QScopedPointer<QObject> o(c.create());
    QVERIFY(o);

    QObject *handle = qvariant_cast<QObject *>(o->property("aa"));
    QVERIFY(handle);
    QVERIFY(qvariant_cast<QObject *>(handle->property("rect")) != nullptr);
}

void tst_QmlCppCodegen::functionReturningVoid()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, QUrl(u"qrc:/TestTypes/functionReturningVoid.qml"_qs));
    QVERIFY2(c.isReady(), qPrintable(c.errorString()));
    QScopedPointer<QObject> o(c.create());
    QVERIFY(o);

    // It should be able to call the methods and wrap the void values into invalid QVariants,
    // without crashing.
    QVERIFY(o->metaObject()->indexOfProperty("aa") >= 0);
    QVERIFY(o->metaObject()->indexOfProperty("bb") >= 0);
    QVERIFY(!o->property("aa").isValid());
    QVERIFY(!o->property("bb").isValid());
}

void tst_QmlCppCodegen::runInterpreted()
{
    if (qEnvironmentVariableIsSet("QV4_FORCE_INTERPRETER"))
        QSKIP("Already running in interpreted mode");

#if QT_CONFIG(process)
    QProcess process;
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setEnvironment(QProcess::systemEnvironment()
                           + QStringList(u"QV4_FORCE_INTERPRETER=1"_qs));
    process.start();
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
#else
    QSKIP("Test needs QProcess");
#endif
}

QTEST_MAIN(tst_QmlCppCodegen)

#include "tst_qmlcppcodegen.moc"
