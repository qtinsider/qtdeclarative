/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativeanimation_p.h"
#include "qdeclarativeanimation_p_p.h"

#include <private/qdeclarativestateoperations_p.h>
#include <private/qdeclarativecontext_p.h>

#include <qdeclarativepropertyvaluesource.h>
#include <qdeclarative.h>
#include <qdeclarativeinfo.h>
#include <qdeclarativeexpression.h>
#include <private/qdeclarativestringconverters_p.h>
#include <private/qdeclarativeglobal_p.h>
#include <private/qdeclarativemetatype_p.h>
#include <private/qdeclarativevaluetype_p.h>
#include <private/qdeclarativeproperty_p.h>
#include <private/qdeclarativeengine_p.h>

#include <qvariant.h>
#include <qcolor.h>
#include <qfile.h>
#include "private/qparallelanimationgroupjob_p.h"
#include "private/qsequentialanimationgroupjob_p.h"
#include <QtCore/qset.h>
#include <QtCore/qrect.h>
#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>
#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE

/*!
    \qmlclass Animation QDeclarativeAbstractAnimation
    \inqmlmodule QtQuick 2
    \ingroup qml-animation-transition
    \brief The Animation element is the base of all QML animations.

    The Animation element cannot be used directly in a QML file.  It exists
    to provide a set of common properties and methods, available across all the
    other animation types that inherit from it.  Attempting to use the Animation
    element directly will result in an error.
*/

QDeclarativeAbstractAnimation::QDeclarativeAbstractAnimation(QObject *parent)
: QObject(*(new QDeclarativeAbstractAnimationPrivate), parent)
{
}

QDeclarativeAbstractAnimation::~QDeclarativeAbstractAnimation()
{
    Q_D(QDeclarativeAbstractAnimation);
    delete d->animationInstance;
}

QDeclarativeAbstractAnimation::QDeclarativeAbstractAnimation(QDeclarativeAbstractAnimationPrivate &dd, QObject *parent)
: QObject(dd, parent)
{
}

QAbstractAnimationJob* QDeclarativeAbstractAnimation::qtAnimation()
{
    Q_D(QDeclarativeAbstractAnimation);
    return d->animationInstance;
}

/*!
    \qmlproperty bool QtQuick2::Animation::running
    This property holds whether the animation is currently running.

    The \c running property can be set to declaratively control whether or not
    an animation is running.  The following example will animate a rectangle
    whenever the \l MouseArea is pressed.

    \code
    Rectangle {
        width: 100; height: 100
        NumberAnimation on x {
            running: myMouse.pressed
            from: 0; to: 100
        }
        MouseArea { id: myMouse }
    }
    \endcode

    Likewise, the \c running property can be read to determine if the animation
    is running.  In the following example the text element will indicate whether
    or not the animation is running.

    \code
    NumberAnimation { id: myAnimation }
    Text { text: myAnimation.running ? "Animation is running" : "Animation is not running" }
    \endcode

    Animations can also be started and stopped imperatively from JavaScript
    using the \c start() and \c stop() methods.

    By default, animations are not running. Though, when the animations are assigned to properties,
    as property value sources using the \e on syntax, they are set to running by default.
*/
bool QDeclarativeAbstractAnimation::isRunning() const
{
    Q_D(const QDeclarativeAbstractAnimation);
    return d->running;
}

// the behavior calls this function
void QDeclarativeAbstractAnimation::notifyRunningChanged(bool running)
{
    Q_D(QDeclarativeAbstractAnimation);
    if (d->disableUserControl && d->running != running) {
        d->running = running;
        emit runningChanged(running);
    }
}

//commence is called to start an animation when it is used as a
//simple animation, and not as part of a transition
void QDeclarativeAbstractAnimationPrivate::commence()
{
    Q_Q(QDeclarativeAbstractAnimation);

    QDeclarativeStateActions actions;
    QDeclarativeProperties properties;

    QAbstractAnimationJob *oldInstance = animationInstance;
    animationInstance = q->transition(actions, properties, QDeclarativeAbstractAnimation::Forward);
    if (oldInstance != animationInstance) {
        animationInstance->addAnimationChangeListener(this, QAbstractAnimationJob::Completion);
        if (oldInstance)
            delete oldInstance;
    }
    animationInstance->start();
    if (animationInstance->isStopped()) {
        running = false;
        emit q->completed();
    }
}

QDeclarativeProperty QDeclarativeAbstractAnimationPrivate::createProperty(QObject *obj, const QString &str, QObject *infoObj)
{
    QDeclarativeProperty prop(obj, str, qmlContext(infoObj));
    if (!prop.isValid()) {
        qmlInfo(infoObj) << QDeclarativeAbstractAnimation::tr("Cannot animate non-existent property \"%1\"").arg(str);
        return QDeclarativeProperty();
    } else if (!prop.isWritable()) {
        qmlInfo(infoObj) << QDeclarativeAbstractAnimation::tr("Cannot animate read-only property \"%1\"").arg(str);
        return QDeclarativeProperty();
    }
    return prop;
}

void QDeclarativeAbstractAnimation::setRunning(bool r)
{
    Q_D(QDeclarativeAbstractAnimation);
    if (!d->componentComplete) {
        d->running = r;
        if (r == false)
            d->avoidPropertyValueSourceStart = true;
        else if (!d->registered) {
            d->registered = true;
            QDeclarativeEnginePrivate *engPriv = QDeclarativeEnginePrivate::get(qmlEngine(this));
            static int finalizedIdx = -1;
            if (finalizedIdx < 0)
                finalizedIdx = metaObject()->indexOfSlot("componentFinalized()");
            engPriv->registerFinalizeCallback(this, finalizedIdx);
        }
        return;
    }

    if (d->running == r)
        return;

    if (d->group || d->disableUserControl) {
        qmlInfo(this) << "setRunning() cannot be used on non-root animation nodes.";
        return;
    }

    d->running = r;
    if (d->running) {
        bool supressStart = false;
        if (d->alwaysRunToEnd && d->loopCount != 1
            && d->animationInstance && d->animationInstance->isRunning()) {
            //we've restarted before the final loop finished; restore proper loop count
            if (d->loopCount == -1)
                d->animationInstance->setLoopCount(d->loopCount);
            else
                d->animationInstance->setLoopCount(d->animationInstance->currentLoop() + d->loopCount);
            supressStart = true;    //we want the animation to continue, rather than restart
        }
        if (!supressStart)
            d->commence();
        emit started();
    } else {
        if (d->animationInstance) {
            if (d->alwaysRunToEnd) {
                if (d->loopCount != 1)
                    d->animationInstance->setLoopCount(d->animationInstance->currentLoop()+1);    //finish the current loop
            } else {
                d->animationInstance->stop();
            }
        }
        emit completed();
    }

    emit runningChanged(d->running);
}

/*!
    \qmlproperty bool QtQuick2::Animation::paused
    This property holds whether the animation is currently paused.

    The \c paused property can be set to declaratively control whether or not
    an animation is paused.

    Animations can also be paused and resumed imperatively from JavaScript
    using the \c pause() and \c resume() methods.

    By default, animations are not paused.
*/
bool QDeclarativeAbstractAnimation::isPaused() const
{
    Q_D(const QDeclarativeAbstractAnimation);
    return d->paused;
}

void QDeclarativeAbstractAnimation::setPaused(bool p)
{
    Q_D(QDeclarativeAbstractAnimation);
    if (d->paused == p)
        return;

    if (d->group || d->disableUserControl) {
        qmlInfo(this) << "setPaused() cannot be used on non-root animation nodes.";
        return;
    }

    d->paused = p;

    if (!d->componentComplete || !d->animationInstance)
        return;

    if (d->paused)
        d->animationInstance->pause();
    else
        d->animationInstance->resume();

    emit pausedChanged(d->paused);
}

void QDeclarativeAbstractAnimation::classBegin()
{
    Q_D(QDeclarativeAbstractAnimation);
    d->componentComplete = false;
}

void QDeclarativeAbstractAnimation::componentComplete()
{
    Q_D(QDeclarativeAbstractAnimation);
    d->componentComplete = true;
}

void QDeclarativeAbstractAnimation::componentFinalized()
{
    Q_D(QDeclarativeAbstractAnimation);
    if (d->running) {
        d->running = false;
        setRunning(true);
    }
    if (d->paused) {
        d->paused = false;
        setPaused(true);
    }
}

/*!
    \qmlproperty bool QtQuick2::Animation::alwaysRunToEnd
    This property holds whether the animation should run to completion when it is stopped.

    If this true the animation will complete its current iteration when it
    is stopped - either by setting the \c running property to false, or by
    calling the \c stop() method.  The \c complete() method is not effected
    by this value.

    This behavior is most useful when the \c repeat property is set, as the
    animation will finish playing normally but not restart.

    By default, the alwaysRunToEnd property is not set.

    \note alwaysRunToEnd has no effect on animations in a Transition.
*/
bool QDeclarativeAbstractAnimation::alwaysRunToEnd() const
{
    Q_D(const QDeclarativeAbstractAnimation);
    return d->alwaysRunToEnd;
}

void QDeclarativeAbstractAnimation::setAlwaysRunToEnd(bool f)
{
    Q_D(QDeclarativeAbstractAnimation);
    if (d->alwaysRunToEnd == f)
        return;

    d->alwaysRunToEnd = f;
    emit alwaysRunToEndChanged(f);
}

/*!
    \qmlproperty int QtQuick2::Animation::loops
    This property holds the number of times the animation should play.

    By default, \c loops is 1: the animation will play through once and then stop.

    If set to Animation.Infinite, the animation will continuously repeat until it is explicitly
    stopped - either by setting the \c running property to false, or by calling
    the \c stop() method.

    In the following example, the rectangle will spin indefinitely.

    \code
    Rectangle {
        width: 100; height: 100; color: "green"
        RotationAnimation on rotation {
            loops: Animation.Infinite
            from: 0
            to: 360
        }
    }
    \endcode
*/
int QDeclarativeAbstractAnimation::loops() const
{
    Q_D(const QDeclarativeAbstractAnimation);
    return d->loopCount;
}

void QDeclarativeAbstractAnimation::setLoops(int loops)
{
    Q_D(QDeclarativeAbstractAnimation);
    if (loops < 0)
        loops = -1;

    if (loops == d->loopCount)
        return;

    d->loopCount = loops;
    emit loopCountChanged(loops);
}

int QDeclarativeAbstractAnimation::duration() const
{
    Q_D(const QDeclarativeAbstractAnimation);
    return d->animationInstance ? d->animationInstance->duration() : 0;
}

int QDeclarativeAbstractAnimation::currentTime()
{
    Q_D(QDeclarativeAbstractAnimation);
    return d->animationInstance ? d->animationInstance->currentLoopTime() : 0;
}

void QDeclarativeAbstractAnimation::setCurrentTime(int time)
{
    Q_D(QDeclarativeAbstractAnimation);
    if (d->animationInstance)
        d->animationInstance->setCurrentTime(time);
    //TODO save value for start?
}

QDeclarativeAnimationGroup *QDeclarativeAbstractAnimation::group() const
{
    Q_D(const QDeclarativeAbstractAnimation);
    return d->group;
}

void QDeclarativeAbstractAnimation::setGroup(QDeclarativeAnimationGroup *g)
{
    Q_D(QDeclarativeAbstractAnimation);
    if (d->group == g)
        return;
    if (d->group)
        static_cast<QDeclarativeAnimationGroupPrivate *>(d->group->d_func())->animations.removeAll(this);

    d->group = g;

    if (d->group && !static_cast<QDeclarativeAnimationGroupPrivate *>(d->group->d_func())->animations.contains(this))
        static_cast<QDeclarativeAnimationGroupPrivate *>(d->group->d_func())->animations.append(this);

    //if (g) //if removed from a group, then the group should no longer be the parent
        setParent(g);
}

/*!
    \qmlmethod QtQuick2::Animation::start()
    \brief Starts the animation.

    If the animation is already running, calling this method has no effect.  The
    \c running property will be true following a call to \c start().
*/
void QDeclarativeAbstractAnimation::start()
{
    setRunning(true);
}

/*!
    \qmlmethod QtQuick2::Animation::pause()
    \brief Pauses the animation.

    If the animation is already paused, calling this method has no effect.  The
    \c paused property will be true following a call to \c pause().
*/
void QDeclarativeAbstractAnimation::pause()
{
    setPaused(true);
}

/*!
    \qmlmethod QtQuick2::Animation::resume()
    \brief Resumes a paused animation.

    If the animation is not paused, calling this method has no effect.  The
    \c paused property will be false following a call to \c resume().
*/
void QDeclarativeAbstractAnimation::resume()
{
    setPaused(false);
}

/*!
    \qmlmethod QtQuick2::Animation::stop()
    \brief Stops the animation.

    If the animation is not running, calling this method has no effect.  The
    \c running property will be false following a call to \c stop().

    Normally \c stop() stops the animation immediately, and the animation has
    no further influence on property values.  In this example animation
    \code
    Rectangle {
        NumberAnimation on x { from: 0; to: 100; duration: 500 }
    }
    \endcode
    was stopped at time 250ms, the \c x property will have a value of 50.

    However, if the \c alwaysRunToEnd property is set, the animation will
    continue running until it completes and then stop.  The \c running property
    will still become false immediately.
*/
void QDeclarativeAbstractAnimation::stop()
{
    setRunning(false);
}

/*!
    \qmlmethod QtQuick2::Animation::restart()
    \brief Restarts the animation.

    This is a convenience method, and is equivalent to calling \c stop() and
    then \c start().
*/
void QDeclarativeAbstractAnimation::restart()
{
    stop();
    start();
}

/*!
    \qmlmethod QtQuick2::Animation::complete()
    \brief Stops the animation, jumping to the final property values.

    If the animation is not running, calling this method has no effect.  The
    \c running property will be false following a call to \c complete().

    Unlike \c stop(), \c complete() immediately fast-forwards the animation to
    its end.  In the following example,
    \code
    Rectangle {
        NumberAnimation on x { from: 0; to: 100; duration: 500 }
    }
    \endcode
    calling \c stop() at time 250ms will result in the \c x property having
    a value of 50, while calling \c complete() will set the \c x property to
    100, exactly as though the animation had played the whole way through.
*/
void QDeclarativeAbstractAnimation::complete()
{
    Q_D(QDeclarativeAbstractAnimation);
    if (isRunning() && d->animationInstance) {
         d->animationInstance->setCurrentTime(d->animationInstance->duration());
    }
}

void QDeclarativeAbstractAnimation::setTarget(const QDeclarativeProperty &p)
{
    Q_D(QDeclarativeAbstractAnimation);
    d->defaultProperty = p;

    if (!d->avoidPropertyValueSourceStart)
        setRunning(true);
}

/*
    we rely on setTarget only being called when used as a value source
    so this function allows us to do the same thing as setTarget without
    that assumption
*/
void QDeclarativeAbstractAnimation::setDefaultTarget(const QDeclarativeProperty &p)
{
    Q_D(QDeclarativeAbstractAnimation);
    d->defaultProperty = p;
}

/*
    don't allow start/stop/pause/resume to be manually invoked,
    because something else (like a Behavior) already has control
    over the animation.
*/
void QDeclarativeAbstractAnimation::setDisableUserControl()
{
    Q_D(QDeclarativeAbstractAnimation);
    d->disableUserControl = true;
}

void QDeclarativeAbstractAnimation::setEnableUserControl()
{
    Q_D(QDeclarativeAbstractAnimation);
    d->disableUserControl = false;

}

bool QDeclarativeAbstractAnimation::userControlDisabled() const
{
    Q_D(const QDeclarativeAbstractAnimation);
    return d->disableUserControl;
}

QAbstractAnimationJob* QDeclarativeAbstractAnimation::initInstance(QAbstractAnimationJob *animation)
{
    Q_D(QDeclarativeAbstractAnimation);
    animation->setLoopCount(d->loopCount);
    return animation;
}

QAbstractAnimationJob* QDeclarativeAbstractAnimation::transition(QDeclarativeStateActions &actions,
                                      QDeclarativeProperties &modified,
                                      TransitionDirection direction)
{
    Q_UNUSED(actions);
    Q_UNUSED(modified);
    Q_UNUSED(direction);
    return 0;
}

void QDeclarativeAbstractAnimationPrivate::animationFinished(QAbstractAnimationJob*)
{
    Q_Q(QDeclarativeAbstractAnimation);
    q->setRunning(false);
    if (alwaysRunToEnd && loopCount != 1) {
        //restore the proper loopCount for the next run
        animationInstance->setLoopCount(loopCount);
    }
}

/*!
    \qmlclass PauseAnimation QDeclarativePauseAnimation
    \inqmlmodule QtQuick 2
    \ingroup qml-animation-transition
    \inherits Animation
    \brief The PauseAnimation element provides a pause for an animation.

    When used in a SequentialAnimation, PauseAnimation is a step when
    nothing happens, for a specified duration.

    A 500ms animation sequence, with a 100ms pause between two animations:
    \code
    SequentialAnimation {
        NumberAnimation { ... duration: 200 }
        PauseAnimation { duration: 100 }
        NumberAnimation { ... duration: 200 }
    }
    \endcode

    \sa {QML Animation and Transitions}, {declarative/animation/basics}{Animation basics example}
*/
QDeclarativePauseAnimation::QDeclarativePauseAnimation(QObject *parent)
: QDeclarativeAbstractAnimation(*(new QDeclarativePauseAnimationPrivate), parent)
{
}

QDeclarativePauseAnimation::~QDeclarativePauseAnimation()
{
}

/*!
    \qmlproperty int QtQuick2::PauseAnimation::duration
    This property holds the duration of the pause in milliseconds

    The default value is 250.
*/
int QDeclarativePauseAnimation::duration() const
{
    Q_D(const QDeclarativePauseAnimation);
    return d->duration;
}

void QDeclarativePauseAnimation::setDuration(int duration)
{
    if (duration < 0) {
        qmlInfo(this) << tr("Cannot set a duration of < 0");
        return;
    }

    Q_D(QDeclarativePauseAnimation);
    if (d->duration == duration)
        return;
    d->duration = duration;
    emit durationChanged(duration);
}

QAbstractAnimationJob* QDeclarativePauseAnimation::transition(QDeclarativeStateActions &actions,
                                    QDeclarativeProperties &modified,
                                    TransitionDirection direction)
{
    Q_D(QDeclarativePauseAnimation);
    Q_UNUSED(actions);
    Q_UNUSED(modified);
    Q_UNUSED(direction);

    return initInstance(new QPauseAnimationJob(d->duration));
}

/*!
    \qmlclass ColorAnimation QDeclarativeColorAnimation
    \inqmlmodule QtQuick 2
  \ingroup qml-animation-transition
    \inherits PropertyAnimation
    \brief The ColorAnimation element animates changes in color values.

    ColorAnimation is a specialized PropertyAnimation that defines an
    animation to be applied when a color value changes.

    Here is a ColorAnimation applied to the \c color property of a \l Rectangle
    as a property value source. It animates the \c color property's value from
    its current value to a value of "red", over 1000 milliseconds:

    \snippet doc/src/snippets/declarative/coloranimation.qml 0

    Like any other animation element, a ColorAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {QML Animation and Transitions} documentation shows a
    variety of methods for creating animations.

    For convenience, when a ColorAnimation is used in a \l Transition, it will
    animate any \c color properties that have been modified during the state
    change. If a \l{PropertyAnimation::}{property} or
    \l{PropertyAnimation::}{properties} are explicitly set for the animation,
    then those are used instead.

    \sa {QML Animation and Transitions}, {declarative/animation/basics}{Animation basics example}
*/
QDeclarativeColorAnimation::QDeclarativeColorAnimation(QObject *parent)
: QDeclarativePropertyAnimation(parent)
{
    Q_D(QDeclarativePropertyAnimation);
    d->interpolatorType = QMetaType::QColor;
    d->defaultToInterpolatorType = true;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
}

QDeclarativeColorAnimation::~QDeclarativeColorAnimation()
{
}

/*!
    \qmlproperty color QtQuick2::ColorAnimation::from
    This property holds the color value at which the animation should begin.

    For example, the following animation is not applied until a color value
    has reached "#c0c0c0":

    \qml
    Item {
        states: [
            // States are defined here...
        ]

        transition: Transition {
            NumberAnimation { from: "#c0c0c0"; duration: 2000 }
        }
    }
    \endqml

    If the ColorAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {QML Animation and Transitions}
*/
QColor QDeclarativeColorAnimation::from() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->from.value<QColor>();
}

void QDeclarativeColorAnimation::setFrom(const QColor &f)
{
    QDeclarativePropertyAnimation::setFrom(f);
}

/*!
    \qmlproperty color QtQuick2::ColorAnimation::to

    This property holds the color value at which the animation should end.

    If the ColorAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {QML Animation and Transitions}
*/
QColor QDeclarativeColorAnimation::to() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->to.value<QColor>();
}

void QDeclarativeColorAnimation::setTo(const QColor &t)
{
    QDeclarativePropertyAnimation::setTo(t);
}

QActionAnimation::QActionAnimation()
    : QAbstractAnimationJob(), animAction(0)
{
}

QActionAnimation::QActionAnimation(QAbstractAnimationAction *action)
    : QAbstractAnimationJob(), animAction(action)
{
}

QActionAnimation::~QActionAnimation()
{
    delete animAction;
}

int QActionAnimation::duration() const
{
    return 0;
}

void QActionAnimation::setAnimAction(QAbstractAnimationAction *action)
{
    if (isRunning())
        stop();
    animAction = action;
}

void QActionAnimation::updateCurrentTime(int)
{
}

void QActionAnimation::updateState(State newState, State oldState)
{
    Q_UNUSED(oldState);

    if (newState == Running) {
        if (animAction) {
            animAction->doAction();
        }
    }
}

/*!
    \qmlclass ScriptAction QDeclarativeScriptAction
    \inqmlmodule QtQuick 2
    \ingroup qml-animation-transition
    \inherits Animation
    \brief The ScriptAction element allows scripts to be run during an animation.

    ScriptAction can be used to run a script at a specific point in an animation.

    \qml
    SequentialAnimation {
        NumberAnimation {
            // ...
        }
        ScriptAction { script: doSomething(); }
        NumberAnimation {
            // ...
        }
    }
    \endqml

    When used as part of a Transition, you can also target a specific
    StateChangeScript to run using the \c scriptName property.

    \snippet doc/src/snippets/declarative/states/statechangescript.qml state and transition

    \sa StateChangeScript
*/
QDeclarativeScriptAction::QDeclarativeScriptAction(QObject *parent)
    :QDeclarativeAbstractAnimation(*(new QDeclarativeScriptActionPrivate), parent)
{
}

QDeclarativeScriptAction::~QDeclarativeScriptAction()
{
}

QDeclarativeScriptActionPrivate::QDeclarativeScriptActionPrivate()
    : QDeclarativeAbstractAnimationPrivate(), hasRunScriptScript(false), reversing(false){}

/*!
    \qmlproperty script QtQuick2::ScriptAction::script
    This property holds the script to run.
*/
QDeclarativeScriptString QDeclarativeScriptAction::script() const
{
    Q_D(const QDeclarativeScriptAction);
    return d->script;
}

void QDeclarativeScriptAction::setScript(const QDeclarativeScriptString &script)
{
    Q_D(QDeclarativeScriptAction);
    d->script = script;
}

/*!
    \qmlproperty string QtQuick2::ScriptAction::scriptName
    This property holds the the name of the StateChangeScript to run.

    This property is only valid when ScriptAction is used as part of a transition.
    If both script and scriptName are set, scriptName will be used.

    \note When using scriptName in a reversible transition, the script will only
    be run when the transition is being run forwards.
*/
QString QDeclarativeScriptAction::stateChangeScriptName() const
{
    Q_D(const QDeclarativeScriptAction);
    return d->name;
}

void QDeclarativeScriptAction::setStateChangeScriptName(const QString &name)
{
    Q_D(QDeclarativeScriptAction);
    d->name = name;
}

QAbstractAnimationAction* QDeclarativeScriptActionPrivate::createAction()
{
    return new Proxy(this);
}

void QDeclarativeScriptActionPrivate::execute()
{
    Q_Q(QDeclarativeScriptAction);
    if (hasRunScriptScript && reversing)
        return;

    QDeclarativeScriptString scriptStr = hasRunScriptScript ? runScriptScript : script;

    if (!scriptStr.script().isEmpty()) {
        QDeclarativeExpression expr(scriptStr);
        expr.evaluate();
        if (expr.hasError())
            qmlInfo(q) << expr.error();
    }
}

QAbstractAnimationJob* QDeclarativeScriptAction::transition(QDeclarativeStateActions &actions,
                                    QDeclarativeProperties &modified,
                                    TransitionDirection direction)
{
    Q_D(QDeclarativeScriptAction);
    Q_UNUSED(modified);

    d->hasRunScriptScript = false;
    d->reversing = (direction == Backward);
    for (int ii = 0; ii < actions.count(); ++ii) {
        QDeclarativeAction &action = actions[ii];

        if (action.event && action.event->typeName() == QLatin1String("StateChangeScript")
            && static_cast<QDeclarativeStateChangeScript*>(action.event)->name() == d->name) {
            d->runScriptScript = static_cast<QDeclarativeStateChangeScript*>(action.event)->script();
            d->hasRunScriptScript = true;
            action.actionDone = true;
            break;  //only match one (names should be unique)
        }
    }
    return initInstance(new QActionAnimation(d->createAction()));
}

/*!
    \qmlclass PropertyAction QDeclarativePropertyAction
    \inqmlmodule QtQuick 2
    \ingroup qml-animation-transition
    \inherits Animation
    \brief The PropertyAction element allows immediate property changes during animation.

    PropertyAction is used to specify an immediate property change during an
    animation. The property change is not animated.

    It is useful for setting non-animated property values during an animation.

    For example, here is a SequentialAnimation that sets the image's
    \l {Image::}{smooth} property to \c true, animates the width of the image,
    then sets \l {Image::}{smooth} back to \c false:

    \snippet doc/src/snippets/declarative/propertyaction.qml standalone

    PropertyAction is also useful for setting the exact point at which a property
    change should occur during a \l Transition. For example, if PropertyChanges
    was used in a \l State to rotate an item around a particular
    \l {Item::}{transformOrigin}, it might be implemented like this:

    \snippet doc/src/snippets/declarative/propertyaction.qml transition

    However, with this code, the \c transformOrigin is not set until \e after
    the animation, as a \l State is taken to define the values at the \e end of
    a transition. The animation would rotate at the default \c transformOrigin,
    then jump to \c Item.BottomRight. To fix this, insert a PropertyAction
    before the RotationAnimation begins:

    \snippet doc/src/snippets/declarative/propertyaction-sequential.qml sequential

    This immediately sets the \c transformOrigin property to the value defined
    in the end state of the \l Transition (i.e. the value defined in the
    PropertyAction object) so that the rotation animation begins with the
    correct transform origin.

    \sa {QML Animation and Transitions}, QtDeclarative
*/
QDeclarativePropertyAction::QDeclarativePropertyAction(QObject *parent)
: QDeclarativeAbstractAnimation(*(new QDeclarativePropertyActionPrivate), parent)
{
}

QDeclarativePropertyAction::~QDeclarativePropertyAction()
{
}

QObject *QDeclarativePropertyAction::target() const
{
    Q_D(const QDeclarativePropertyAction);
    return d->target;
}

void QDeclarativePropertyAction::setTarget(QObject *o)
{
    Q_D(QDeclarativePropertyAction);
    if (d->target == o)
        return;
    d->target = o;
    emit targetChanged();
}

QString QDeclarativePropertyAction::property() const
{
    Q_D(const QDeclarativePropertyAction);
    return d->propertyName;
}

void QDeclarativePropertyAction::setProperty(const QString &n)
{
    Q_D(QDeclarativePropertyAction);
    if (d->propertyName == n)
        return;
    d->propertyName = n;
    emit propertyChanged();
}

/*!
    \qmlproperty Object QtQuick2::PropertyAction::target
    \qmlproperty list<Object> QtQuick2::PropertyAction::targets
    \qmlproperty string QtQuick2::PropertyAction::property
    \qmlproperty string QtQuick2::PropertyAction::properties

    These properties determine the items and their properties that are
    affected by this action.

    The details of how these properties are interpreted in different situations
    is covered in the \l{PropertyAnimation::properties}{corresponding} PropertyAnimation
    documentation.

    \sa exclude
*/
QString QDeclarativePropertyAction::properties() const
{
    Q_D(const QDeclarativePropertyAction);
    return d->properties;
}

void QDeclarativePropertyAction::setProperties(const QString &p)
{
    Q_D(QDeclarativePropertyAction);
    if (d->properties == p)
        return;
    d->properties = p;
    emit propertiesChanged(p);
}

QDeclarativeListProperty<QObject> QDeclarativePropertyAction::targets()
{
    Q_D(QDeclarativePropertyAction);
    return QDeclarativeListProperty<QObject>(this, d->targets);
}

/*!
    \qmlproperty list<Object> QtQuick2::PropertyAction::exclude
    This property holds the objects that should not be affected by this action.

    \sa targets
*/
QDeclarativeListProperty<QObject> QDeclarativePropertyAction::exclude()
{
    Q_D(QDeclarativePropertyAction);
    return QDeclarativeListProperty<QObject>(this, d->exclude);
}

/*!
    \qmlproperty any QtQuick2::PropertyAction::value
    This property holds the value to be set on the property.

    If the PropertyAction is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.
*/
QVariant QDeclarativePropertyAction::value() const
{
    Q_D(const QDeclarativePropertyAction);
    return d->value;
}

void QDeclarativePropertyAction::setValue(const QVariant &v)
{
    Q_D(QDeclarativePropertyAction);
    if (d->value.isNull || d->value != v) {
        d->value = v;
        emit valueChanged(v);
    }
}

QAbstractAnimationJob* QDeclarativePropertyAction::transition(QDeclarativeStateActions &actions,
                                      QDeclarativeProperties &modified,
                                      TransitionDirection direction)
{
    Q_D(QDeclarativePropertyAction);
    Q_UNUSED(direction);

    struct QDeclarativeSetPropertyAnimationAction : public QAbstractAnimationAction
    {
        QDeclarativeStateActions actions;
        virtual void doAction()
        {
            for (int ii = 0; ii < actions.count(); ++ii) {
                const QDeclarativeAction &action = actions.at(ii);
                QDeclarativePropertyPrivate::write(action.property, action.toValue, QDeclarativePropertyPrivate::BypassInterceptor | QDeclarativePropertyPrivate::DontRemoveBinding);
            }
        }
    };

    QStringList props = d->properties.isEmpty() ? QStringList() : d->properties.split(QLatin1Char(','));
    for (int ii = 0; ii < props.count(); ++ii)
        props[ii] = props.at(ii).trimmed();
    if (!d->propertyName.isEmpty())
        props << d->propertyName;

    QList<QObject*> targets = d->targets;
    if (d->target)
        targets.append(d->target);

    bool hasSelectors = !props.isEmpty() || !targets.isEmpty() || !d->exclude.isEmpty();

    if (d->defaultProperty.isValid() && !hasSelectors) {
        props << d->defaultProperty.name();
        targets << d->defaultProperty.object();
    }

    QDeclarativeSetPropertyAnimationAction *data = new QDeclarativeSetPropertyAnimationAction;

    bool hasExplicit = false;
    //an explicit animation has been specified
    if (d->value.isValid()) {
        for (int i = 0; i < props.count(); ++i) {
            for (int j = 0; j < targets.count(); ++j) {
                QDeclarativeAction myAction;
                myAction.property = d->createProperty(targets.at(j), props.at(i), this);
                if (myAction.property.isValid()) {
                    myAction.toValue = d->value;
                    QDeclarativePropertyAnimationPrivate::convertVariant(myAction.toValue, myAction.property.propertyType());
                    data->actions << myAction;
                    hasExplicit = true;
                    for (int ii = 0; ii < actions.count(); ++ii) {
                        QDeclarativeAction &action = actions[ii];
                        if (action.property.object() == myAction.property.object() &&
                            myAction.property.name() == action.property.name()) {
                            modified << action.property;
                            break;  //### any chance there could be multiples?
                        }
                    }
                }
            }
        }
    }

    if (!hasExplicit)
    for (int ii = 0; ii < actions.count(); ++ii) {
        QDeclarativeAction &action = actions[ii];

        QObject *obj = action.property.object();
        QString propertyName = action.property.name();
        QObject *sObj = action.specifiedObject;
        QString sPropertyName = action.specifiedProperty;
        bool same = (obj == sObj);

        if ((targets.isEmpty() || targets.contains(obj) || (!same && targets.contains(sObj))) &&
           (!d->exclude.contains(obj)) && (same || (!d->exclude.contains(sObj))) &&
           (props.contains(propertyName) || (!same && props.contains(sPropertyName)))) {
            QDeclarativeAction myAction = action;

            if (d->value.isValid())
                myAction.toValue = d->value;
            QDeclarativePropertyAnimationPrivate::convertVariant(myAction.toValue, myAction.property.propertyType());

            modified << action.property;
            data->actions << myAction;
            action.fromValue = myAction.toValue;
        }
    }

    QActionAnimation *action = new QActionAnimation;
    if (data->actions.count()) {
        action->setAnimAction(data);
    } else {
        delete data;
    }
    return initInstance(action);
}

/*!
    \qmlclass NumberAnimation QDeclarativeNumberAnimation
    \inqmlmodule QtQuick 2
  \ingroup qml-animation-transition
    \inherits PropertyAnimation
    \brief The NumberAnimation element animates changes in qreal-type values.

    NumberAnimation is a specialized PropertyAnimation that defines an
    animation to be applied when a numerical value changes.

    Here is a NumberAnimation applied to the \c x property of a \l Rectangle
    as a property value source. It animates the \c x value from its current
    value to a value of 50, over 1000 milliseconds:

    \snippet doc/src/snippets/declarative/numberanimation.qml 0

    Like any other animation element, a NumberAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {QML Animation and Transitions} documentation shows a
    variety of methods for creating animations.

    Note that NumberAnimation may not animate smoothly if there are irregular
    changes in the number value that it is tracking. If this is the case, use
    SmoothedAnimation instead.

    \sa {QML Animation and Transitions}, {declarative/animation/basics}{Animation basics example}
*/
QDeclarativeNumberAnimation::QDeclarativeNumberAnimation(QObject *parent)
: QDeclarativePropertyAnimation(parent)
{
    init();
}

QDeclarativeNumberAnimation::QDeclarativeNumberAnimation(QDeclarativePropertyAnimationPrivate &dd, QObject *parent)
: QDeclarativePropertyAnimation(dd, parent)
{
    init();
}

QDeclarativeNumberAnimation::~QDeclarativeNumberAnimation()
{
}

void QDeclarativeNumberAnimation::init()
{
    Q_D(QDeclarativePropertyAnimation);
    d->interpolatorType = QMetaType::QReal;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
}

/*!
    \qmlproperty real QtQuick2::NumberAnimation::from
    This property holds the starting value for the animation.

    For example, the following animation is not applied until the \c x value
    has reached 100:

    \qml
    Item {
        states: [
            // ...
        ]

        transition: Transition {
            NumberAnimation { properties: "x"; from: 100; duration: 200 }
        }
    }
    \endqml

    If the NumberAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {QML Animation and Transitions}
*/

qreal QDeclarativeNumberAnimation::from() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->from.toReal();
}

void QDeclarativeNumberAnimation::setFrom(qreal f)
{
    QDeclarativePropertyAnimation::setFrom(f);
}

/*!
    \qmlproperty real QtQuick2::NumberAnimation::to
    This property holds the end value for the animation.

    If the NumberAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {QML Animation and Transitions}
*/
qreal QDeclarativeNumberAnimation::to() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->to.toReal();
}

void QDeclarativeNumberAnimation::setTo(qreal t)
{
    QDeclarativePropertyAnimation::setTo(t);
}



/*!
    \qmlclass Vector3dAnimation QDeclarativeVector3dAnimation
    \inqmlmodule QtQuick 2
    \ingroup qml-animation-transition
    \inherits PropertyAnimation
    \brief The Vector3dAnimation element animates changes in QVector3d values.

    Vector3dAnimation is a specialized PropertyAnimation that defines an
    animation to be applied when a Vector3d value changes.

    Like any other animation element, a Vector3dAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {QML Animation and Transitions} documentation shows a
    variety of methods for creating animations.

    \sa {QML Animation and Transitions}, {declarative/animation/basics}{Animation basics example}
*/
QDeclarativeVector3dAnimation::QDeclarativeVector3dAnimation(QObject *parent)
: QDeclarativePropertyAnimation(parent)
{
    Q_D(QDeclarativePropertyAnimation);
    d->interpolatorType = QMetaType::QVector3D;
    d->defaultToInterpolatorType = true;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
}

QDeclarativeVector3dAnimation::~QDeclarativeVector3dAnimation()
{
}

/*!
    \qmlproperty real QtQuick2::Vector3dAnimation::from
    This property holds the starting value for the animation.

    If the Vector3dAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {QML Animation and Transitions}
*/
QVector3D QDeclarativeVector3dAnimation::from() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->from.value<QVector3D>();
}

void QDeclarativeVector3dAnimation::setFrom(QVector3D f)
{
    QDeclarativePropertyAnimation::setFrom(f);
}

/*!
    \qmlproperty real QtQuick2::Vector3dAnimation::to
    This property holds the end value for the animation.

    If the Vector3dAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {QML Animation and Transitions}
*/
QVector3D QDeclarativeVector3dAnimation::to() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->to.value<QVector3D>();
}

void QDeclarativeVector3dAnimation::setTo(QVector3D t)
{
    QDeclarativePropertyAnimation::setTo(t);
}



/*!
    \qmlclass RotationAnimation QDeclarativeRotationAnimation
    \inqmlmodule QtQuick 2
    \ingroup qml-animation-transition
    \inherits PropertyAnimation
    \brief The RotationAnimation element animates changes in rotation values.

    RotationAnimation is a specialized PropertyAnimation that gives control
    over the direction of rotation during an animation.

    By default, it rotates in the direction
    of the numerical change; a rotation from 0 to 240 will rotate 240 degrees
    clockwise, while a rotation from 240 to 0 will rotate 240 degrees
    counterclockwise. The \l direction property can be set to specify the
    direction in which the rotation should occur.

    In the following example we use RotationAnimation to animate the rotation
    between states via the shortest path:

    \snippet doc/src/snippets/declarative/rotationanimation.qml 0

    Notice the RotationAnimation did not need to set a \l target
    value. As a convenience, when used in a transition, RotationAnimation will rotate all
    properties named "rotation" or "angle". You can override this by providing
    your own properties via \l {PropertyAnimation::properties}{properties} or
    \l {PropertyAnimation::property}{property}.

    Also, note the \l Rectangle will be rotated around its default
    \l {Item::}{transformOrigin} (which is \c Item.Center). To use a different
    transform origin, set the origin in the PropertyChanges object and apply
    the change at the start of the animation using PropertyAction. See the
    PropertyAction documentation for more details.

    Like any other animation element, a RotationAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {QML Animation and Transitions} documentation shows a
    variety of methods for creating animations.

    \sa {QML Animation and Transitions}, {declarative/animation/basics}{Animation basics example}
*/
QVariant _q_interpolateShortestRotation(qreal &f, qreal &t, qreal progress)
{
    qreal newt = t;
    qreal diff = t-f;
    while(diff > 180.0){
        newt -= 360.0;
        diff -= 360.0;
    }
    while(diff < -180.0){
        newt += 360.0;
        diff += 360.0;
    }
    return QVariant(f + (newt - f) * progress);
}

QVariant _q_interpolateClockwiseRotation(qreal &f, qreal &t, qreal progress)
{
    qreal newt = t;
    qreal diff = t-f;
    while(diff < 0.0){
        newt += 360.0;
        diff += 360.0;
    }
    return QVariant(f + (newt - f) * progress);
}

QVariant _q_interpolateCounterclockwiseRotation(qreal &f, qreal &t, qreal progress)
{
    qreal newt = t;
    qreal diff = t-f;
    while(diff > 0.0){
        newt -= 360.0;
        diff -= 360.0;
    }
    return QVariant(f + (newt - f) * progress);
}

QDeclarativeRotationAnimation::QDeclarativeRotationAnimation(QObject *parent)
: QDeclarativePropertyAnimation(*(new QDeclarativeRotationAnimationPrivate), parent)
{
    Q_D(QDeclarativeRotationAnimation);
    d->interpolatorType = QMetaType::QReal;
    d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
    d->defaultProperties = QLatin1String("rotation,angle");
}

QDeclarativeRotationAnimation::~QDeclarativeRotationAnimation()
{
}

/*!
    \qmlproperty real QtQuick2::RotationAnimation::from
    This property holds the starting value for the animation.

    For example, the following animation is not applied until the \c angle value
    has reached 100:

    \qml
    Item {
        states: [
            // ...
        ]

        transition: Transition {
            RotationAnimation { properties: "angle"; from: 100; duration: 2000 }
        }
    }
    \endqml

    If the RotationAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {QML Animation and Transitions}
*/
qreal QDeclarativeRotationAnimation::from() const
{
    Q_D(const QDeclarativeRotationAnimation);
    return d->from.toReal();
}

void QDeclarativeRotationAnimation::setFrom(qreal f)
{
    QDeclarativePropertyAnimation::setFrom(f);
}

/*!
    \qmlproperty real QtQuick2::RotationAnimation::to
    This property holds the end value for the animation..

    If the RotationAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {QML Animation and Transitions}
*/
qreal QDeclarativeRotationAnimation::to() const
{
    Q_D(const QDeclarativeRotationAnimation);
    return d->to.toReal();
}

void QDeclarativeRotationAnimation::setTo(qreal t)
{
    QDeclarativePropertyAnimation::setTo(t);
}

/*!
    \qmlproperty enumeration QtQuick2::RotationAnimation::direction
    This property holds the direction of the rotation.

    Possible values are:

    \list
    \o RotationAnimation.Numerical (default) - Rotate by linearly interpolating between the two numbers.
           A rotation from 10 to 350 will rotate 340 degrees clockwise.
    \o RotationAnimation.Clockwise - Rotate clockwise between the two values
    \o RotationAnimation.Counterclockwise - Rotate counterclockwise between the two values
    \o RotationAnimation.Shortest - Rotate in the direction that produces the shortest animation path.
           A rotation from 10 to 350 will rotate 20 degrees counterclockwise.
    \endlist
*/
QDeclarativeRotationAnimation::RotationDirection QDeclarativeRotationAnimation::direction() const
{
    Q_D(const QDeclarativeRotationAnimation);
    return d->direction;
}

void QDeclarativeRotationAnimation::setDirection(QDeclarativeRotationAnimation::RotationDirection direction)
{
    Q_D(QDeclarativeRotationAnimation);
    if (d->direction == direction)
        return;

    d->direction = direction;
    switch(d->direction) {
    case Clockwise:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&_q_interpolateClockwiseRotation);
        break;
    case Counterclockwise:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&_q_interpolateCounterclockwiseRotation);
        break;
    case Shortest:
        d->interpolator = reinterpret_cast<QVariantAnimation::Interpolator>(&_q_interpolateShortestRotation);
        break;
    default:
        d->interpolator = QVariantAnimationPrivate::getInterpolator(d->interpolatorType);
        break;
    }
    emit directionChanged();
}



QDeclarativeAnimationGroup::QDeclarativeAnimationGroup(QObject *parent)
: QDeclarativeAbstractAnimation(*(new QDeclarativeAnimationGroupPrivate), parent)
{
}

QDeclarativeAnimationGroup::QDeclarativeAnimationGroup(QDeclarativeAnimationGroupPrivate &dd, QObject *parent)
    : QDeclarativeAbstractAnimation(dd, parent)
{
}

void QDeclarativeAnimationGroupPrivate::append_animation(QDeclarativeListProperty<QDeclarativeAbstractAnimation> *list, QDeclarativeAbstractAnimation *a)
{
    QDeclarativeAnimationGroup *q = qobject_cast<QDeclarativeAnimationGroup *>(list->object);
    if (q) {
        a->setGroup(q);
    }
}

void QDeclarativeAnimationGroupPrivate::clear_animation(QDeclarativeListProperty<QDeclarativeAbstractAnimation> *list)
{
    QDeclarativeAnimationGroup *q = qobject_cast<QDeclarativeAnimationGroup *>(list->object);
    if (q) {
        while (q->d_func()->animations.count()) {
            QDeclarativeAbstractAnimation *firstAnim = q->d_func()->animations.at(0);
            firstAnim->setGroup(0);
        }
    }
}

QDeclarativeAnimationGroup::~QDeclarativeAnimationGroup()
{
}

QDeclarativeListProperty<QDeclarativeAbstractAnimation> QDeclarativeAnimationGroup::animations()
{
    Q_D(QDeclarativeAnimationGroup);
    QDeclarativeListProperty<QDeclarativeAbstractAnimation> list(this, d->animations);
    list.append = &QDeclarativeAnimationGroupPrivate::append_animation;
    list.clear = &QDeclarativeAnimationGroupPrivate::clear_animation;
    return list;
}

/*!
    \qmlclass SequentialAnimation QDeclarativeSequentialAnimation
    \inqmlmodule QtQuick 2
  \ingroup qml-animation-transition
    \inherits Animation
    \brief The SequentialAnimation element allows animations to be run sequentially.

    The SequentialAnimation and ParallelAnimation elements allow multiple
    animations to be run together. Animations defined in a SequentialAnimation
    are run one after the other, while animations defined in a ParallelAnimation
    are run at the same time.

    The following example runs two number animations in a sequence.  The \l Rectangle
    animates to a \c x position of 50, then to a \c y position of 50.

    \snippet doc/src/snippets/declarative/sequentialanimation.qml 0

    Animations defined within a \l Transition are automatically run in parallel,
    so SequentialAnimation can be used to enclose the animations in a \l Transition
    if this is the preferred behavior.

    Like any other animation element, a SequentialAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {QML Animation and Transitions} documentation shows a
    variety of methods for creating animations.

    \note Once an animation has been grouped into a SequentialAnimation or
    ParallelAnimation, it cannot be individually started and stopped; the
    SequentialAnimation or ParallelAnimation must be started and stopped as a group.

    \sa ParallelAnimation, {QML Animation and Transitions}, {declarative/animation/basics}{Animation basics example}
*/

QDeclarativeSequentialAnimation::QDeclarativeSequentialAnimation(QObject *parent) :
    QDeclarativeAnimationGroup(parent)
{
}

QDeclarativeSequentialAnimation::~QDeclarativeSequentialAnimation()
{
}

QAbstractAnimationJob* QDeclarativeSequentialAnimation::transition(QDeclarativeStateActions &actions,
                                    QDeclarativeProperties &modified,
                                    TransitionDirection direction)
{
    Q_D(QDeclarativeAnimationGroup);

    QSequentialAnimationGroupJob *ag = new QSequentialAnimationGroupJob;

    int inc = 1;
    int from = 0;
    if (direction == Backward) {
        inc = -1;
        from = d->animations.count() - 1;
    }

    bool valid = d->defaultProperty.isValid();
    QAbstractAnimationJob* anim;
    for (int ii = from; ii < d->animations.count() && ii >= 0; ii += inc) {
        if (valid)
            d->animations.at(ii)->setDefaultTarget(d->defaultProperty);
        anim = d->animations.at(ii)->transition(actions, modified, direction);
        inc == -1 ? ag->prependAnimation(anim) : ag->appendAnimation(anim);
    }

    return initInstance(ag);
}



/*!
    \qmlclass ParallelAnimation QDeclarativeParallelAnimation
    \inqmlmodule QtQuick 2
  \ingroup qml-animation-transition
    \inherits Animation
    \brief The ParallelAnimation element allows animations to be run in parallel.

    The SequentialAnimation and ParallelAnimation elements allow multiple
    animations to be run together. Animations defined in a SequentialAnimation
    are run one after the other, while animations defined in a ParallelAnimation
    are run at the same time.

    The following animation runs two number animations in parallel. The \l Rectangle
    moves to (50,50) by animating its \c x and \c y properties at the same time.

    \snippet doc/src/snippets/declarative/parallelanimation.qml 0

    Like any other animation element, a ParallelAnimation can be applied in a
    number of ways, including transitions, behaviors and property value
    sources. The \l {QML Animation and Transitions} documentation shows a
    variety of methods for creating animations.

    \note Once an animation has been grouped into a SequentialAnimation or
    ParallelAnimation, it cannot be individually started and stopped; the
    SequentialAnimation or ParallelAnimation must be started and stopped as a group.

    \sa SequentialAnimation, {QML Animation and Transitions}, {declarative/animation/basics}{Animation basics example}
*/
QDeclarativeParallelAnimation::QDeclarativeParallelAnimation(QObject *parent) :
    QDeclarativeAnimationGroup(parent)
{
}

QDeclarativeParallelAnimation::~QDeclarativeParallelAnimation()
{
}

QAbstractAnimationJob* QDeclarativeParallelAnimation::transition(QDeclarativeStateActions &actions,
                                      QDeclarativeProperties &modified,
                                      TransitionDirection direction)
{
    Q_D(QDeclarativeAnimationGroup);
    QParallelAnimationGroupJob *ag = new QParallelAnimationGroupJob;

    bool valid = d->defaultProperty.isValid();
    QAbstractAnimationJob* anim;
    for (int ii = 0; ii < d->animations.count(); ++ii) {
        if (valid)
            d->animations.at(ii)->setDefaultTarget(d->defaultProperty);
        anim = d->animations.at(ii)->transition(actions, modified, direction);
        ag->appendAnimation(anim);
    }
    return initInstance(ag);
}

//convert a variant from string type to another animatable type
void QDeclarativePropertyAnimationPrivate::convertVariant(QVariant &variant, int type)
{
    if (variant.userType() != QVariant::String) {
        variant.convert((QVariant::Type)type);
        return;
    }

    switch (type) {
    case QVariant::Rect: {
        variant.setValue(QDeclarativeStringConverters::rectFFromString(variant.toString()).toRect());
        break;
    }
    case QVariant::RectF: {
        variant.setValue(QDeclarativeStringConverters::rectFFromString(variant.toString()));
        break;
    }
    case QVariant::Point: {
        variant.setValue(QDeclarativeStringConverters::pointFFromString(variant.toString()).toPoint());
        break;
    }
    case QVariant::PointF: {
        variant.setValue(QDeclarativeStringConverters::pointFFromString(variant.toString()));
        break;
    }
    case QVariant::Size: {
        variant.setValue(QDeclarativeStringConverters::sizeFFromString(variant.toString()).toSize());
        break;
    }
    case QVariant::SizeF: {
        variant.setValue(QDeclarativeStringConverters::sizeFFromString(variant.toString()));
        break;
    }
    case QVariant::Color: {
        variant.setValue(QDeclarativeStringConverters::colorFromString(variant.toString()));
        break;
    }
    case QVariant::Vector3D: {
        variant.setValue(QDeclarativeStringConverters::vector3DFromString(variant.toString()));
        break;
    }
    default:
        if (QDeclarativeValueTypeFactory::isValueType((uint)type)) {
            variant.convert((QVariant::Type)type);
        } else {
            QDeclarativeMetaType::StringConverter converter = QDeclarativeMetaType::customStringConverter(type);
            if (converter)
                variant = converter(variant.toString());
        }
        break;
    }
}

QDeclarativeBulkValueAnimator::QDeclarativeBulkValueAnimator()
    : QAbstractAnimationJob(), animValue(0), fromSourced(0), m_duration(250)
{
}

QDeclarativeBulkValueAnimator::~QDeclarativeBulkValueAnimator()
{
    delete animValue;
}

void QDeclarativeBulkValueAnimator::setAnimValue(QDeclarativeBulkValueUpdater *value)
{
    if (isRunning())
        stop();
    animValue = value;
}

void QDeclarativeBulkValueAnimator::updateCurrentTime(int currentTime)
{
    if (isStopped())
        return;

    const qreal progress = easing.valueForProgress(((m_duration == 0) ? qreal(1) : qreal(currentTime) / qreal(m_duration)));

    if (animValue)
        animValue->setValue(progress);
}

void QDeclarativeBulkValueAnimator::topLevelAnimationLoopChanged()
{
    //check for new from every top-level loop (when the top level animation is started and all subsequent loops)
    if (fromSourced)
        *fromSourced = false;
}

/*!
    \qmlclass PropertyAnimation QDeclarativePropertyAnimation
    \inqmlmodule QtQuick 2
  \ingroup qml-animation-transition
    \inherits Animation
    \brief The PropertyAnimation element animates changes in property values.

    PropertyAnimation provides a way to animate changes to a property's value.

    It can be used to define animations in a number of ways:

    \list
    \o In a \l Transition

    For example, to animate any objects that have changed their \c x or \c y properties
    as a result of a state change, using an \c InOutQuad easing curve:

    \snippet doc/src/snippets/declarative/propertyanimation.qml transition


    \o In a \l Behavior

    For example, to animate all changes to a rectangle's \c x property:

    \snippet doc/src/snippets/declarative/propertyanimation.qml behavior


    \o As a property value source

    For example, to repeatedly animate the rectangle's \c x property:

    \snippet doc/src/snippets/declarative/propertyanimation.qml propertyvaluesource


    \o In a signal handler

    For example, to fade out \c theObject when clicked:
    \qml
    MouseArea {
        anchors.fill: theObject
        onClicked: PropertyAnimation { target: theObject; property: "opacity"; to: 0 }
    }
    \endqml

    \o Standalone

    For example, to animate \c rect's \c width property over 500ms, from its current width to 30:

    \snippet doc/src/snippets/declarative/propertyanimation.qml standalone

    \endlist

    Depending on how the animation is used, the set of properties normally used will be
    different. For more information see the individual property documentation, as well
    as the \l{QML Animation and Transitions} introduction.

    Note that PropertyAnimation inherits the abstract \l Animation element.
    This includes additional properties and methods for controlling the animation.

    \sa {QML Animation and Transitions}, {declarative/animation/basics}{Animation basics example}
*/

QDeclarativePropertyAnimation::QDeclarativePropertyAnimation(QObject *parent)
: QDeclarativeAbstractAnimation(*(new QDeclarativePropertyAnimationPrivate), parent)
{
}

QDeclarativePropertyAnimation::QDeclarativePropertyAnimation(QDeclarativePropertyAnimationPrivate &dd, QObject *parent)
: QDeclarativeAbstractAnimation(dd, parent)
{
}

QDeclarativePropertyAnimation::~QDeclarativePropertyAnimation()
{
}

/*!
    \qmlproperty int QtQuick2::PropertyAnimation::duration
    This property holds the duration of the animation, in milliseconds.

    The default value is 250.
*/
int QDeclarativePropertyAnimation::duration() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->duration;
}

void QDeclarativePropertyAnimation::setDuration(int duration)
{
    if (duration < 0) {
        qmlInfo(this) << tr("Cannot set a duration of < 0");
        return;
    }

    Q_D(QDeclarativePropertyAnimation);
    if (d->duration == duration)
        return;
    d->duration = duration;
    emit durationChanged(duration);
}

/*!
    \qmlproperty real QtQuick2::PropertyAnimation::from
    This property holds the starting value for the animation.

    If the PropertyAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the starting state of the
    \l Transition, or the current value of the property at the moment the
    \l Behavior is triggered.

    \sa {QML Animation and Transitions}
*/
QVariant QDeclarativePropertyAnimation::from() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->from;
}

void QDeclarativePropertyAnimation::setFrom(const QVariant &f)
{
    Q_D(QDeclarativePropertyAnimation);
    if (d->fromIsDefined && f == d->from)
        return;
    d->from = f;
    d->fromIsDefined = f.isValid();
    emit fromChanged(f);
}

/*!
    \qmlproperty real QtQuick2::PropertyAnimation::to
    This property holds the end value for the animation.

    If the PropertyAnimation is defined within a \l Transition or \l Behavior,
    this value defaults to the value defined in the end state of the
    \l Transition, or the value of the property change that triggered the
    \l Behavior.

    \sa {QML Animation and Transitions}
*/
QVariant QDeclarativePropertyAnimation::to() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->to;
}

void QDeclarativePropertyAnimation::setTo(const QVariant &t)
{
    Q_D(QDeclarativePropertyAnimation);
    if (d->toIsDefined && t == d->to)
        return;
    d->to = t;
    d->toIsDefined = t.isValid();
    emit toChanged(t);
}

/*!
    \qmlproperty enumeration QtQuick2::PropertyAnimation::easing.type
    \qmlproperty real QtQuick2::PropertyAnimation::easing.amplitude
    \qmlproperty real QtQuick2::PropertyAnimation::easing.overshoot
    \qmlproperty real QtQuick2::PropertyAnimation::easing.period
    \qmlproperty list<real> QtQuick2::PropertyAnimation::easing.bezierCurve
    \brief the easing curve used for the animation.

    To specify an easing curve you need to specify at least the type. For some curves you can also specify
    amplitude, period and/or overshoot (more details provided after the table). The default easing curve is
    \c Easing.Linear.

    \qml
    PropertyAnimation { properties: "y"; easing.type: Easing.InOutElastic; easing.amplitude: 2.0; easing.period: 1.5 }
    \endqml

    Available types are:

    \table
    \row
        \o \c Easing.Linear
        \o Easing curve for a linear (t) function: velocity is constant.
        \o \inlineimage qeasingcurve-linear.png
    \row
        \o \c Easing.InQuad
        \o Easing curve for a quadratic (t^2) function: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-inquad.png
    \row
        \o \c Easing.OutQuad
        \o Easing curve for a quadratic (t^2) function: decelerating to zero velocity.
        \o \inlineimage qeasingcurve-outquad.png
    \row
        \o \c Easing.InOutQuad
        \o Easing curve for a quadratic (t^2) function: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutquad.png
    \row
        \o \c Easing.OutInQuad
        \o Easing curve for a quadratic (t^2) function: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outinquad.png
    \row
        \o \c Easing.InCubic
        \o Easing curve for a cubic (t^3) function: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-incubic.png
    \row
        \o \c Easing.OutCubic
        \o Easing curve for a cubic (t^3) function: decelerating from zero velocity.
        \o \inlineimage qeasingcurve-outcubic.png
    \row
        \o \c Easing.InOutCubic
        \o Easing curve for a cubic (t^3) function: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutcubic.png
    \row
        \o \c Easing.OutInCubic
        \o Easing curve for a cubic (t^3) function: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outincubic.png
    \row
        \o \c Easing.InQuart
        \o Easing curve for a quartic (t^4) function: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-inquart.png
    \row
        \o \c Easing.OutQuart
        \o Easing curve for a quartic (t^4) function: decelerating from zero velocity.
        \o \inlineimage qeasingcurve-outquart.png
    \row
        \o \c Easing.InOutQuart
        \o Easing curve for a quartic (t^4) function: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutquart.png
    \row
        \o \c Easing.OutInQuart
        \o Easing curve for a quartic (t^4) function: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outinquart.png
    \row
        \o \c Easing.InQuint
        \o Easing curve for a quintic (t^5) function: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-inquint.png
    \row
        \o \c Easing.OutQuint
        \o Easing curve for a quintic (t^5) function: decelerating from zero velocity.
        \o \inlineimage qeasingcurve-outquint.png
    \row
        \o \c Easing.InOutQuint
        \o Easing curve for a quintic (t^5) function: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutquint.png
    \row
        \o \c Easing.OutInQuint
        \o Easing curve for a quintic (t^5) function: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outinquint.png
    \row
        \o \c Easing.InSine
        \o Easing curve for a sinusoidal (sin(t)) function: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-insine.png
    \row
        \o \c Easing.OutSine
        \o Easing curve for a sinusoidal (sin(t)) function: decelerating from zero velocity.
        \o \inlineimage qeasingcurve-outsine.png
    \row
        \o \c Easing.InOutSine
        \o Easing curve for a sinusoidal (sin(t)) function: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutsine.png
    \row
        \o \c Easing.OutInSine
        \o Easing curve for a sinusoidal (sin(t)) function: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outinsine.png
    \row
        \o \c Easing.InExpo
        \o Easing curve for an exponential (2^t) function: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-inexpo.png
    \row
        \o \c Easing.OutExpo
        \o Easing curve for an exponential (2^t) function: decelerating from zero velocity.
        \o \inlineimage qeasingcurve-outexpo.png
    \row
        \o \c Easing.InOutExpo
        \o Easing curve for an exponential (2^t) function: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutexpo.png
    \row
        \o \c Easing.OutInExpo
        \o Easing curve for an exponential (2^t) function: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outinexpo.png
    \row
        \o \c Easing.InCirc
        \o Easing curve for a circular (sqrt(1-t^2)) function: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-incirc.png
    \row
        \o \c Easing.OutCirc
        \o Easing curve for a circular (sqrt(1-t^2)) function: decelerating from zero velocity.
        \o \inlineimage qeasingcurve-outcirc.png
    \row
        \o \c Easing.InOutCirc
        \o Easing curve for a circular (sqrt(1-t^2)) function: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutcirc.png
    \row
        \o \c Easing.OutInCirc
        \o Easing curve for a circular (sqrt(1-t^2)) function: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outincirc.png
    \row
        \o \c Easing.InElastic
        \o Easing curve for an elastic (exponentially decaying sine wave) function: accelerating from zero velocity.
        \br The peak amplitude can be set with the \e amplitude parameter, and the period of decay by the \e period parameter.
        \o \inlineimage qeasingcurve-inelastic.png
    \row
        \o \c Easing.OutElastic
        \o Easing curve for an elastic (exponentially decaying sine wave) function: decelerating from zero velocity.
        \br The peak amplitude can be set with the \e amplitude parameter, and the period of decay by the \e period parameter.
        \o \inlineimage qeasingcurve-outelastic.png
    \row
        \o \c Easing.InOutElastic
        \o Easing curve for an elastic (exponentially decaying sine wave) function: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutelastic.png
    \row
        \o \c Easing.OutInElastic
        \o Easing curve for an elastic (exponentially decaying sine wave) function: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outinelastic.png
    \row
        \o \c Easing.InBack
        \o Easing curve for a back (overshooting cubic function: (s+1)*t^3 - s*t^2) easing in: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-inback.png
    \row
        \o \c Easing.OutBack
        \o Easing curve for a back (overshooting cubic function: (s+1)*t^3 - s*t^2) easing out: decelerating to zero velocity.
        \o \inlineimage qeasingcurve-outback.png
    \row
        \o \c Easing.InOutBack
        \o Easing curve for a back (overshooting cubic function: (s+1)*t^3 - s*t^2) easing in/out: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutback.png
    \row
        \o \c Easing.OutInBack
        \o Easing curve for a back (overshooting cubic easing: (s+1)*t^3 - s*t^2) easing out/in: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outinback.png
    \row
        \o \c Easing.InBounce
        \o Easing curve for a bounce (exponentially decaying parabolic bounce) function: accelerating from zero velocity.
        \o \inlineimage qeasingcurve-inbounce.png
    \row
        \o \c Easing.OutBounce
        \o Easing curve for a bounce (exponentially decaying parabolic bounce) function: decelerating from zero velocity.
        \o \inlineimage qeasingcurve-outbounce.png
    \row
        \o \c Easing.InOutBounce
        \o Easing curve for a bounce (exponentially decaying parabolic bounce) function easing in/out: acceleration until halfway, then deceleration.
        \o \inlineimage qeasingcurve-inoutbounce.png
    \row
        \o \c Easing.OutInBounce
        \o Easing curve for a bounce (exponentially decaying parabolic bounce) function easing out/in: deceleration until halfway, then acceleration.
        \o \inlineimage qeasingcurve-outinbounce.png
    \row
        \o \c Easing.Bezier
        \o Custom easing curve defined by the easing.bezierCurve property.
        \o
    \endtable

    \c easing.amplitude is only applicable for bounce and elastic curves (curves of type
    \c Easing.InBounce, \c Easing.OutBounce, \c Easing.InOutBounce, \c Easing.OutInBounce, \c Easing.InElastic,
    \c Easing.OutElastic, \c Easing.InOutElastic or \c Easing.OutInElastic).

    \c easing.overshoot is only applicable if \c easing.type is: \c Easing.InBack, \c Easing.OutBack,
    \c Easing.InOutBack or \c Easing.OutInBack.

    \c easing.period is only applicable if easing.type is: \c Easing.InElastic, \c Easing.OutElastic,
    \c Easing.InOutElastic or \c Easing.OutInElastic.

    \c easing.bezierCurve is only applicable if easing.type is: \c Easing.Bezier.  This property is a list<real> containing
    groups of three points defining a curve from 0,0 to 1,1 - control1, control2,
    end point: [cx1, cy1, cx2, cy2, endx, endy, ...].  The last point must be 1,1.

    See the \l {declarative/animation/easing}{easing} example for a demonstration of
    the different easing settings.
*/
QEasingCurve QDeclarativePropertyAnimation::easing() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->easing;
}

void QDeclarativePropertyAnimation::setEasing(const QEasingCurve &e)
{
    Q_D(QDeclarativePropertyAnimation);
    if (d->easing == e)
        return;

    d->easing = e;
    emit easingChanged(e);
}

QObject *QDeclarativePropertyAnimation::target() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->target;
}

void QDeclarativePropertyAnimation::setTarget(QObject *o)
{
    Q_D(QDeclarativePropertyAnimation);
    if (d->target == o)
        return;
    d->target = o;
    emit targetChanged();
}

QString QDeclarativePropertyAnimation::property() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->propertyName;
}

void QDeclarativePropertyAnimation::setProperty(const QString &n)
{
    Q_D(QDeclarativePropertyAnimation);
    if (d->propertyName == n)
        return;
    d->propertyName = n;
    emit propertyChanged();
}

QString QDeclarativePropertyAnimation::properties() const
{
    Q_D(const QDeclarativePropertyAnimation);
    return d->properties;
}

void QDeclarativePropertyAnimation::setProperties(const QString &prop)
{
    Q_D(QDeclarativePropertyAnimation);
    if (d->properties == prop)
        return;

    d->properties = prop;
    emit propertiesChanged(prop);
}

/*!
    \qmlproperty string QtQuick2::PropertyAnimation::properties
    \qmlproperty list<Object> QtQuick2::PropertyAnimation::targets
    \qmlproperty string QtQuick2::PropertyAnimation::property
    \qmlproperty Object QtQuick2::PropertyAnimation::target

    These properties are used as a set to determine which properties should be animated.
    The singular and plural forms are functionally identical, e.g.
    \qml
    NumberAnimation { target: theItem; property: "x"; to: 500 }
    \endqml
    has the same meaning as
    \qml
    NumberAnimation { targets: theItem; properties: "x"; to: 500 }
    \endqml
    The singular forms are slightly optimized, so if you do have only a single target/property
    to animate you should try to use them.

    The \c targets property allows multiple targets to be set. For example, this animates the
    \c x property of both \c itemA and \c itemB:

    \qml
    NumberAnimation { targets: [itemA, itemB]; properties: "x"; to: 500 }
    \endqml

    In many cases these properties do not need to be explicitly specified, as they can be
    inferred from the animation framework:

    \table 80%
    \row
    \o Value Source / Behavior
    \o When an animation is used as a value source or in a Behavior, the default target and property
       name to be animated can both be inferred.
       \qml
       Rectangle {
           id: theRect
           width: 100; height: 100
           color: Qt.rgba(0,0,1)
           NumberAnimation on x { to: 500; loops: Animation.Infinite } //animate theRect's x property
           Behavior on y { NumberAnimation {} } //animate theRect's y property
       }
       \endqml
    \row
    \o Transition
    \o When used in a transition, a property animation is assumed to match \e all targets
       but \e no properties. In practice, that means you need to specify at least the properties
       in order for the animation to do anything.
       \qml
       Rectangle {
           id: theRect
           width: 100; height: 100
           color: Qt.rgba(0,0,1)
           Item { id: uselessItem }
           states: State {
               name: "state1"
               PropertyChanges { target: theRect; x: 200; y: 200; z: 4 }
               PropertyChanges { target: uselessItem; x: 10; y: 10; z: 2 }
           }
           transitions: Transition {
               //animate both theRect's and uselessItem's x and y to their final values
               NumberAnimation { properties: "x,y" }

               //animate theRect's z to its final value
               NumberAnimation { target: theRect; property: "z" }
           }
       }
       \endqml
    \row
    \o Standalone
    \o When an animation is used standalone, both the target and property need to be
       explicitly specified.
       \qml
       Rectangle {
           id: theRect
           width: 100; height: 100
           color: Qt.rgba(0,0,1)
           //need to explicitly specify target and property
           NumberAnimation { id: theAnim; target: theRect; property: "x"; to: 500 }
           MouseArea {
               anchors.fill: parent
               onClicked: theAnim.start()
           }
       }
       \endqml
    \endtable

    As seen in the above example, properties is specified as a comma-separated string of property names to animate.

    \sa exclude, {QML Animation and Transitions}
*/
QDeclarativeListProperty<QObject> QDeclarativePropertyAnimation::targets()
{
    Q_D(QDeclarativePropertyAnimation);
    return QDeclarativeListProperty<QObject>(this, d->targets);
}

/*!
    \qmlproperty list<Object> QtQuick2::PropertyAnimation::exclude
    This property holds the items not to be affected by this animation.
    \sa PropertyAnimation::targets
*/
QDeclarativeListProperty<QObject> QDeclarativePropertyAnimation::exclude()
{
    Q_D(QDeclarativePropertyAnimation);
    return QDeclarativeListProperty<QObject>(this, d->exclude);
}

void QDeclarativeAnimationPropertyUpdater::setValue(qreal v)
{
    bool deleted = false;
    wasDeleted = &deleted;
    if (reverse)
        v = 1 - v;
    for (int ii = 0; ii < actions.count(); ++ii) {
        QDeclarativeAction &action = actions[ii];

        if (v == 1.) {
            QDeclarativePropertyPrivate::write(action.property, action.toValue, QDeclarativePropertyPrivate::BypassInterceptor | QDeclarativePropertyPrivate::DontRemoveBinding);
        } else {
            if (!fromSourced && !fromDefined) {
                action.fromValue = action.property.read();
                if (interpolatorType) {
                    QDeclarativePropertyAnimationPrivate::convertVariant(action.fromValue, interpolatorType);
                }
            }
            if (!interpolatorType) {
                int propType = action.property.propertyType();
                if (!prevInterpolatorType || prevInterpolatorType != propType) {
                    prevInterpolatorType = propType;
                    interpolator = QVariantAnimationPrivate::getInterpolator(prevInterpolatorType);
                }
            }
            if (interpolator)
                QDeclarativePropertyPrivate::write(action.property, interpolator(action.fromValue.constData(), action.toValue.constData(), v), QDeclarativePropertyPrivate::BypassInterceptor | QDeclarativePropertyPrivate::DontRemoveBinding);
        }
        if (deleted)
            return;
    }
    wasDeleted = 0;
    fromSourced = true;
}

QDeclarativeStateActions QDeclarativePropertyAnimation::createTransitionActions(QDeclarativeStateActions &actions,
                                                                                QDeclarativeProperties &modified)
{
    Q_D(QDeclarativePropertyAnimation);
    QDeclarativeStateActions newActions;

    QStringList props = d->properties.isEmpty() ? QStringList() : d->properties.split(QLatin1Char(','));
    for (int ii = 0; ii < props.count(); ++ii)
        props[ii] = props.at(ii).trimmed();
    if (!d->propertyName.isEmpty())
        props << d->propertyName;

    QList<QObject*> targets = d->targets;
    if (d->target)
        targets.append(d->target);

    bool hasSelectors = !props.isEmpty() || !targets.isEmpty() || !d->exclude.isEmpty();
    bool useType = (props.isEmpty() && d->defaultToInterpolatorType) ? true : false;

    if (d->defaultProperty.isValid() && !hasSelectors) {
        props << d->defaultProperty.name();
        targets << d->defaultProperty.object();
    }

    if (props.isEmpty() && !d->defaultProperties.isEmpty()) {
        props << d->defaultProperties.split(QLatin1Char(','));
    }

    bool hasExplicit = false;
    //an explicit animation has been specified
    if (d->toIsDefined) {
        for (int i = 0; i < props.count(); ++i) {
            for (int j = 0; j < targets.count(); ++j) {
                QDeclarativeAction myAction;
                myAction.property = d->createProperty(targets.at(j), props.at(i), this);
                if (myAction.property.isValid()) {
                    if (d->fromIsDefined) {
                        myAction.fromValue = d->from;
                        d->convertVariant(myAction.fromValue, d->interpolatorType ? d->interpolatorType : myAction.property.propertyType());
                    }
                    myAction.toValue = d->to;
                    d->convertVariant(myAction.toValue, d->interpolatorType ? d->interpolatorType : myAction.property.propertyType());
                    newActions << myAction;
                    hasExplicit = true;
                    for (int ii = 0; ii < actions.count(); ++ii) {
                        QDeclarativeAction &action = actions[ii];
                        if (action.property.object() == myAction.property.object() &&
                            myAction.property.name() == action.property.name()) {
                            modified << action.property;
                            break;  //### any chance there could be multiples?
                        }
                    }
                }
            }
        }
    }

    if (!hasExplicit)
    for (int ii = 0; ii < actions.count(); ++ii) {
        QDeclarativeAction &action = actions[ii];

        QObject *obj = action.property.object();
        QString propertyName = action.property.name();
        QObject *sObj = action.specifiedObject;
        QString sPropertyName = action.specifiedProperty;
        bool same = (obj == sObj);

        if ((targets.isEmpty() || targets.contains(obj) || (!same && targets.contains(sObj))) &&
           (!d->exclude.contains(obj)) && (same || (!d->exclude.contains(sObj))) &&
           (props.contains(propertyName) || (!same && props.contains(sPropertyName))
               || (useType && action.property.propertyType() == d->interpolatorType))) {
            QDeclarativeAction myAction = action;

            if (d->fromIsDefined)
                myAction.fromValue = d->from;
            else
                myAction.fromValue = QVariant();
            if (d->toIsDefined)
                myAction.toValue = d->to;

            d->convertVariant(myAction.fromValue, d->interpolatorType ? d->interpolatorType : myAction.property.propertyType());
            d->convertVariant(myAction.toValue, d->interpolatorType ? d->interpolatorType : myAction.property.propertyType());

            modified << action.property;

            newActions << myAction;
            action.fromValue = myAction.toValue;
        }
    }
    return newActions;
}

QAbstractAnimationJob* QDeclarativePropertyAnimation::transition(QDeclarativeStateActions &actions,
                                                                     QDeclarativeProperties &modified,
                                                                     TransitionDirection direction)
{
    Q_D(QDeclarativePropertyAnimation);

    QDeclarativeStateActions dataActions = createTransitionActions(actions, modified);

    QDeclarativeBulkValueAnimator *animator = new QDeclarativeBulkValueAnimator;
    animator->setDuration(d->duration);
    animator->setEasingCurve(d->easing);

    if (!dataActions.isEmpty()) {
        QDeclarativeAnimationPropertyUpdater *data = new QDeclarativeAnimationPropertyUpdater;
        data->interpolatorType = d->interpolatorType;
        data->interpolator = d->interpolator;
        data->reverse = direction == Backward ? true : false;
        data->fromSourced = false;
        data->fromDefined = d->fromIsDefined;
        data->actions = dataActions;
        animator->setAnimValue(data);
        animator->setFromSourcedValue(&data->fromSourced);
        d->actions = &data->actions; //remove this?
    }

    return initInstance(animator);
}

QT_END_NAMESPACE
