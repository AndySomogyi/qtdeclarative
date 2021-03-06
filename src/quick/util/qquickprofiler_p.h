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

#ifndef QQUICKPROFILER_P_H
#define QQUICKPROFILER_P_H

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

#include <QtCore/private/qabstractanimation_p.h>
#include <QtQml/private/qqmlprofilerdefinitions_p.h>
#include <QtQuick/private/qtquickglobal_p.h>

#include <QtCore/qurl.h>
#include <QtCore/qsize.h>
#include <QtCore/qmutex.h>
#include <QtCore/qthreadstorage.h>

QT_BEGIN_NAMESPACE

#ifdef QT_NO_QML_DEBUGGER

#define Q_QUICK_PROFILE_IF_ENABLED(feature, Code)

struct QQuickProfiler {
    static void registerAnimationCallback() {}
};

#else

#define Q_QUICK_PROFILE_IF_ENABLED(feature, Code)\
    if (QQuickProfiler::featuresEnabled & (1 << feature)) {\
        Code;\
    } else\
        (void)0

// This struct is somewhat dangerous to use:
// You can save values either with 32 or 64 bit precision. toByteArrays will
// guess the precision from messageType. If you state the wrong messageType
// you will get undefined results.
// The messageType is itself a bit field. You can pack multiple messages into
// one object, e.g. RangeStart and RangeLocation. Each one will be read
// independently by toByteArrays. Thus you can only pack messages if their data
// doesn't overlap. Again, it's up to you to figure that out.
struct Q_AUTOTEST_EXPORT QQuickProfilerData
{
    QQuickProfilerData() {}

    QQuickProfilerData(qint64 time, int messageType, int detailType, const QUrl &url, int x = 0,
                       int y = 0, int framerate = 0, int count = 0) :
        time(time), messageType(messageType), detailType(detailType), detailUrl(url), x(x), y(y),
        framerate(framerate), count(count) {}

    QQuickProfilerData(qint64 time, int messageType, int detailType, int framerateOrInputType = 0,
                       int countOrInputA = 0, int threadIdOrInputB = 0) :
        time(time), messageType(messageType), detailType(detailType),
        framerate(framerateOrInputType), count(countOrInputA), threadId(threadIdOrInputB) {}

    // Special ctor for scenegraph frames. Note that it's missing the QString/QUrl params.
    // This is slightly ugly, but makes it easier to disambiguate between int and qint64 params.
    QQuickProfilerData(qint64 time, int messageType, int detailType, qint64 d1, qint64 d2,
                       qint64 d3, qint64 d4, qint64 d5) :
        time(time), messageType(messageType), detailType(detailType), subtime_1(d1), subtime_2(d2),
        subtime_3(d3), subtime_4(d4), subtime_5(d5) {}


    qint64 time;
    int messageType;        //bit field of Message
    int detailType;

    QUrl detailUrl;

    union {
        qint64 subtime_1;
        int x;              //used for pixmaps
    };

    union {
        qint64 subtime_2;
        int y;              //used for pixmaps
    };

    union {
        qint64 subtime_3;
        int framerate;      //used by animation events
        int inputType;
    };

    union {
        qint64 subtime_4;
        int count;          //used by animation events and for pixmaps
        int inputA;         //used by input events
    };

    union {
        qint64 subtime_5;
        int threadId;
        int inputB;         //used by input events
    };
};

Q_DECLARE_TYPEINFO(QQuickProfilerData, Q_MOVABLE_TYPE);

class QQuickProfilerSceneGraphData : public QQmlProfilerDefinitions {
private:
    static const uint s_numSceneGraphTimings = 5;

    template<uint size>
    struct TimingData {
        qint64 values[size][s_numSceneGraphTimings + 1];
        int offsets[size];
    };

    QThreadStorage<TimingData<NumRenderThreadFrameTypes> > renderThreadTimings;
    TimingData<NumGUIThreadFrameTypes> guiThreadTimings;

public:
    template<SceneGraphFrameType type>
    qint64 *timings()
    {
        if (type < NumRenderThreadFrameTypes)
            return renderThreadTimings.localData().values[type];
        else
            return guiThreadTimings.values[type - NumRenderThreadFrameTypes];
    }

    template<SceneGraphFrameType type>
    int &offset()
    {
        if (type < NumRenderThreadFrameTypes)
            return renderThreadTimings.localData().offsets[type];
        else
            return guiThreadTimings.offsets[type - NumRenderThreadFrameTypes];
    }
};

class Q_QUICK_PRIVATE_EXPORT QQuickProfiler : public QObject, public QQmlProfilerDefinitions {
    Q_OBJECT
public:

    enum AnimationThread {
        GuiThread,
        RenderThread
    };

    template<EventType DetailType, InputEventType InputType>
    static void inputEvent(int x, int y = 0)
    {
        s_instance->processMessage(QQuickProfilerData(s_instance->timestamp(), 1 << Event,
                                                      1 << DetailType, InputType, x, y));
    }

    static void animationFrame(qint64 delta, AnimationThread threadId)
    {
        int animCount = QUnifiedTimer::instance()->runningAnimationCount();

        if (animCount > 0 && delta > 0) {
            s_instance->processMessage(QQuickProfilerData(s_instance->timestamp(), 1 << Event,
                    1 << AnimationFrame, 1000 / (int)delta /* trim fps to integer */, animCount,
                    threadId));
        }
    }

    template<SceneGraphFrameType FrameType1, SceneGraphFrameType FrameType2>
    static void startSceneGraphFrame()
    {
        startSceneGraphFrame<FrameType1>();
        s_instance->m_sceneGraphData.offset<FrameType2>() = 0;
        s_instance->m_sceneGraphData.timings<FrameType2>()[0] =
                s_instance->m_sceneGraphData.timings<FrameType1>()[0];
    }

    template<SceneGraphFrameType FrameType>
    static void startSceneGraphFrame()
    {
        s_instance->m_sceneGraphData.offset<FrameType>() = 0;
        s_instance->m_sceneGraphData.timings<FrameType>()[0] = s_instance->timestamp();
    }

    template<SceneGraphFrameType FrameType>
    static void recordSceneGraphTimestamp()
    {
        s_instance->m_sceneGraphData.timings<FrameType>()
            [++s_instance->m_sceneGraphData.offset<FrameType>()] = s_instance->timestamp();
    }

    template<SceneGraphFrameType FrameType, uint Skip>
    static void skipSceneGraphTimestamps()
    {
        qint64 *timings = s_instance->m_sceneGraphData.timings<FrameType>();
        const qint64 last = timings[s_instance->m_sceneGraphData.offset<FrameType>()];
        for (uint i = 0; i < Skip; ++i)
            timings[++s_instance->m_sceneGraphData.offset<FrameType>()] = last;
    }

    template<SceneGraphFrameType FrameType, bool Record>
    static void reportSceneGraphFrame(quint64 payload = ~0)
    {
        qint64 *timings = s_instance->m_sceneGraphData.timings<FrameType>();
        int &offset = s_instance->m_sceneGraphData.offset<FrameType>();
        if (Record)
            timings[++offset] = s_instance->timestamp();
        s_instance->processMessage(QQuickProfilerData(
                timings[offset], 1 << SceneGraphFrame, 1 << FrameType,
                offset > 0 ? timings[1] - timings[0] : payload,
                offset > 1 ? timings[2] - timings[1] : payload,
                offset > 2 ? timings[3] - timings[2] : payload,
                offset > 3 ? timings[4] - timings[3] : payload,
                offset > 4 ? timings[5] - timings[4] : payload));
    }

    template<SceneGraphFrameType FrameType, bool Record, SceneGraphFrameType SwitchTo>
    static void reportSceneGraphFrame(quint64 payload = ~0)
    {
        reportSceneGraphFrame<FrameType, Record>(payload);
        s_instance->m_sceneGraphData.offset<SwitchTo>() = 0;
        s_instance->m_sceneGraphData.timings<SwitchTo>()[0] =
                s_instance->m_sceneGraphData.timings<FrameType>()
                [s_instance->m_sceneGraphData.offset<FrameType>()];
    }

    template<PixmapEventType PixmapState>
    static void pixmapStateChanged(const QUrl &url)
    {
        s_instance->processMessage(QQuickProfilerData(s_instance->timestamp(),
                1 << PixmapCacheEvent, 1 << PixmapState, url));
    }

    static void pixmapLoadingFinished(const QUrl &url, const QSize &size)
    {
        s_instance->processMessage(QQuickProfilerData(s_instance->timestamp(),
                1 << PixmapCacheEvent,
                (1 << PixmapLoadingFinished) | ((size.width() > 0 && size.height() > 0) ? (1 << PixmapSizeKnown) : 0),
                url, size.width(), size.height()));
    }

    template<PixmapEventType CountType>
    static void pixmapCountChanged(const QUrl &url, int count)
    {
        s_instance->processMessage(QQuickProfilerData(s_instance->timestamp(),
                1 << PixmapCacheEvent, 1 << CountType, url, 0, 0, 0, count));
    }

    static void registerAnimationCallback();

    qint64 timestamp() { return m_timer.nsecsElapsed(); }

    static quint64 featuresEnabled;

    static void initialize(QObject *parent);

    virtual ~QQuickProfiler();

signals:
    void dataReady(const QVector<QQuickProfilerData> &data);

protected:
    friend class QQuickProfilerAdapter;

    static QQuickProfiler *s_instance;
    QMutex m_dataMutex;
    QElapsedTimer m_timer;
    QVector<QQuickProfilerData> m_data;
    QQuickProfilerSceneGraphData m_sceneGraphData;

    QQuickProfiler(QObject *parent);

    void processMessage(const QQuickProfilerData &message)
    {
        QMutexLocker lock(&m_dataMutex);
        m_data.append(message);
    }

    void startProfilingImpl(quint64 features);
    void stopProfilingImpl();
    void reportDataImpl(bool trackLocations);
    void setTimer(const QElapsedTimer &t);
};

#endif // QT_NO_QML_DEBUGGER

#define Q_QUICK_PROFILE(feature, Method)\
    Q_QUICK_PROFILE_IF_ENABLED(feature, QQuickProfiler::Method)

#define Q_QUICK_SG_PROFILE_START(Type)\
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileSceneGraph,\
                               (QQuickProfiler::startSceneGraphFrame<Type>()))

#define Q_QUICK_SG_PROFILE_RECORD(Type)\
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileSceneGraph,\
                               (QQuickProfiler::recordSceneGraphTimestamp<Type>()))

#define Q_QUICK_SG_PROFILE_SKIP(Type, Skip)\
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileSceneGraph,\
                               (QQuickProfiler::skipSceneGraphTimestamps<Type, Skip>()))

#define Q_QUICK_SG_PROFILE_START_SYNCHRONIZED(Type1, Type2)\
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileSceneGraph,\
                               (QQuickProfiler::startSceneGraphFrame<Type1, Type2>()))

#define Q_QUICK_SG_PROFILE_SWITCH(Type1, Type2) \
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileSceneGraph,\
                               (QQuickProfiler::reportSceneGraphFrame<Type1, true, Type2>()))

#define Q_QUICK_SG_PROFILE_REPORT(Type)\
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileSceneGraph,\
                               (QQuickProfiler::reportSceneGraphFrame<Type, false>()))

#define Q_QUICK_SG_PROFILE_END(Type)\
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileSceneGraph,\
                               (QQuickProfiler::reportSceneGraphFrame<Type, true>()))

#define Q_QUICK_SG_PROFILE_END_WITH_PAYLOAD(Type, Payload)\
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileSceneGraph,\
                               (QQuickProfiler::reportSceneGraphFrame<Type, true>(Payload)))


#define Q_QUICK_INPUT_PROFILE(Type, DetailType, A, B)\
    Q_QUICK_PROFILE_IF_ENABLED(QQuickProfiler::ProfileInputEvents,\
                               (QQuickProfiler::inputEvent<Type, DetailType>(A, B)))

QT_END_NAMESPACE

#endif
