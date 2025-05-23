/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_image_animation_interface_test.h"

#include <QSignalSpy>
#include <simpletest.h>

#include <testutil.h>
#include <KoColor.h>

#include "kundo2command.h"

#include "kis_debug.h"
#include "kis_paint_device_debug_utils.h"

#include "kis_image_animation_interface.h"
#include "kis_signal_compressor_with_param.h"
#include "kis_raster_keyframe_channel.h"
#include "kis_time_span.h"
#include "KisLockFrameGenerationLock.h"


void checkFrame(KisImageAnimationInterface *i, KisImageSP image, int frameId, bool externalFrameActive, const QRect &rc)
{
    QCOMPARE(i->currentTime(), frameId);
    QCOMPARE(i->externalFrameActive(), externalFrameActive);
    QCOMPARE(image->projection()->exactBounds(), rc);
}

void KisImageAnimationInterfaceTest::testFrameRegeneration()
{
    QRect refRect(QRect(0,0,512,512));
    TestUtil::MaskParent p(refRect);

    KisPaintLayerSP layer2 = new KisPaintLayer(p.image, "paint2", OPACITY_OPAQUE_U8);
    p.image->addNode(layer2);

    const QRect rc1(101,101,100,100);
    const QRect rc2(102,102,100,100);
    const QRect rc3(103,103,100,100);
    const QRect rc4(104,104,100,100);

    KisImageAnimationInterface *i = p.image->animationInterface();
    KisPaintDeviceSP dev1 = p.layer->paintDevice();
    KisPaintDeviceSP dev2 = layer2->paintDevice();

    p.layer->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    layer2->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);

    // check frame 0
    {
        dev1->fill(rc1, KoColor(Qt::red, dev1->colorSpace()));
        QCOMPARE(dev1->exactBounds(), rc1);

        dev2->fill(rc2, KoColor(Qt::green, dev1->colorSpace()));
        QCOMPARE(dev2->exactBounds(), rc2);

        p.image->refreshGraphAsync();
        p.image->waitForDone();

        checkFrame(i, p.image, 0, false, rc1 | rc2);
    }

    // switch/create frame 10
    i->switchCurrentTimeAsync(10);
    p.image->waitForDone();

    KisKeyframeChannel *channel1 = dev1->keyframeChannel();
    channel1->addKeyframe(10);

    KisKeyframeChannel *channel2 = dev2->keyframeChannel();
    channel2->addKeyframe(10);


    // check frame 10
    {
        QVERIFY(dev1->exactBounds().isEmpty());
        QVERIFY(dev2->exactBounds().isEmpty());

        dev1->fill(rc3, KoColor(Qt::red, dev2->colorSpace()));
        QCOMPARE(dev1->exactBounds(), rc3);

        dev2->fill(rc4, KoColor(Qt::green, dev2->colorSpace()));
        QCOMPARE(dev2->exactBounds(), rc4);

        p.image->refreshGraphAsync();
        p.image->waitForDone();
        checkFrame(i, p.image, 10, false, rc3 | rc4);
    }


    // check external frame (frame 0)
    {
        KisLockFrameGenerationLock lock(p.image->animationInterface());
        SignalToFunctionProxy proxy1(std::bind(checkFrame, i, p.image, 0, true, rc1 | rc2));
        connect(i, SIGNAL(sigFrameReady(int)), &proxy1, SLOT(start()), Qt::DirectConnection);
        i->requestFrameRegeneration(0, KisRegion(refRect), false, std::move(lock));
        QTest::qWait(200);
    }

    // current frame (flame 10) is still unchanged
    checkFrame(i, p.image, 10, false, rc3 | rc4);

    // switch back to frame 0
    i->switchCurrentTimeAsync(0);
    p.image->waitForDone();

    // check frame 0
    {
        QCOMPARE(dev1->exactBounds(), rc1);
        QCOMPARE(dev2->exactBounds(), rc2);

        checkFrame(i, p.image, 0, false, rc1 | rc2);
    }

    // check external frame (frame 10)
    {
        KisLockFrameGenerationLock lock(p.image->animationInterface());
        SignalToFunctionProxy proxy2(std::bind(checkFrame, i, p.image, 10, true, rc3 | rc4));
        connect(i, SIGNAL(sigFrameReady(int)), &proxy2, SLOT(start()), Qt::DirectConnection);
        i->requestFrameRegeneration(10, KisRegion(refRect), false, std::move(lock));
        QTest::qWait(200);
    }

    // current frame is still unchanged
    checkFrame(i, p.image, 0, false, rc1 | rc2);
}

void KisImageAnimationInterfaceTest::testFramesChangedSignal()
{
    QRect refRect(QRect(0,0,512,512));
    TestUtil::MaskParent p(refRect);

    KisPaintLayerSP layer1 = p.layer;
    KisPaintLayerSP layer2 = new KisPaintLayer(p.image, "paint2", OPACITY_OPAQUE_U8);
    p.image->addNode(layer2);

    layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    layer2->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);

    KisImageAnimationInterface *i = p.image->animationInterface();
    KisPaintDeviceSP dev1 = p.layer->paintDevice();
    KisPaintDeviceSP dev2 = layer2->paintDevice();

    KisKeyframeChannel *channel = dev2->keyframeChannel();
    channel->addKeyframe(10);
    channel->addKeyframe(20);

    // check switching a frame doesn't invalidate cache
    QSignalSpy spy(i, SIGNAL(sigFramesChanged(KisTimeSpan,QRect)));

    p.image->animationInterface()->switchCurrentTimeAsync(15);
    p.image->waitForDone();

    QCOMPARE(spy.count(), 0);

    i->notifyNodeChanged(layer1.data(), QRect(), false);

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).value<KisTimeSpan>(), KisTimeSpan::infinite(0));

    i->notifyNodeChanged(layer2.data(), QRect(), false);

    QCOMPARE(spy.count(), 1);
    arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).value<KisTimeSpan>(), KisTimeSpan::fromTimeWithDuration(10, 10));

    // Recursive

    channel = dev1->keyframeChannel();
    channel->addKeyframe(13);

    spy.clear();
    i->notifyNodeChanged(p.image->root().data(), QRect(), true);

    QCOMPARE(spy.count(), 1);
    arguments = spy.takeFirst();
    QEXPECT_FAIL("", "Infinite time range is expected to be (0, -2147483648), but is (1, -2147483648)", Continue);
    QCOMPARE(arguments.at(0).value<KisTimeSpan>(), KisTimeSpan::infinite(10));

}

void KisImageAnimationInterfaceTest::testAnimationCompositionBug()
{
    QRect rect(QRect(0,0,512,512));
    TestUtil::MaskParent p(rect);

    KUndo2Command parentCommand;

    KisPaintLayerSP layer1 = p.layer;
    KisPaintLayerSP layer2 = new KisPaintLayer(p.image, "paint2", OPACITY_OPAQUE_U8);
    p.image->addNode(layer2);

    layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    layer2->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);

    layer1->paintDevice()->fill(rect, KoColor(Qt::red, layer1->paintDevice()->colorSpace()));
    layer2->paintDevice()->fill(QRect(128,128,128,128), KoColor(Qt::black, layer2->paintDevice()->colorSpace()));

    KisKeyframeChannel *rasterChannel = layer2->getKeyframeChannel(KisKeyframeChannel::Raster.id());
    rasterChannel->addKeyframe(10, &parentCommand);
    p.image->initialRefreshGraph();

    m_image = p.image;
    connect(p.image->animationInterface(), SIGNAL(sigFrameReady(int)), this, SLOT(slotFrameDone()), Qt::DirectConnection);

    {
        KisLockFrameGenerationLock lock(p.image->animationInterface());
        p.image->animationInterface()->requestFrameRegeneration(5, rect, false, std::move(lock));
    }

    QTest::qWait(200);

    KisPaintDeviceSP tmpDevice = new KisPaintDevice(p.image->colorSpace());
    tmpDevice->fill(rect, KoColor(Qt::red, tmpDevice->colorSpace()));
    tmpDevice->fill(QRect(128,128,128,128), KoColor(Qt::black, tmpDevice->colorSpace()));
    QImage expected = tmpDevice->createThumbnail(512, 512);

    QVERIFY(m_compositedFrame == expected);
}

void KisImageAnimationInterfaceTest::slotFrameDone()
{
    m_compositedFrame = m_image->projection()->createThumbnail(512, 512);
}

void KisImageAnimationInterfaceTest::testSwitchFrameWithUndo()
{
        QRect refRect(QRect(0,0,512,512));
    TestUtil::MaskParent p(refRect);

    KisPaintLayerSP layer1 = p.layer;

    layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);

    KisImageAnimationInterface *i = p.image->animationInterface();
    KisPaintDeviceSP dev1 = p.layer->paintDevice();

    KisKeyframeChannel *channel = dev1->keyframeChannel();
    channel->addKeyframe(10);
    channel->addKeyframe(20);


    QCOMPARE(i->currentTime(), 0);

    i->requestTimeSwitchWithUndo(15);
    QTest::qWait(100);
    p.image->waitForDone();
    QCOMPARE(i->currentTime(), 15);

    i->requestTimeSwitchWithUndo(16);
    QTest::qWait(100);
    p.image->waitForDone();
    QCOMPARE(i->currentTime(), 16);

    // the two commands have been merged!
    p.undoStore->undo();
    QTest::qWait(100);
    p.image->waitForDone();
    QCOMPARE(i->currentTime(), 0);

    p.undoStore->redo();
    QTest::qWait(100);
    p.image->waitForDone();
    QCOMPARE(i->currentTime(), 16);
}
#include "kis_processing_applicator.h"
void KisImageAnimationInterfaceTest::testSwitchFrameHangup()
{
    QRect refRect(QRect(0,0,512,512));
    TestUtil::MaskParent p(refRect);

    KisPaintLayerSP layer1 = p.layer;

    layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);

    KisImageAnimationInterface *i = p.image->animationInterface();
    KisPaintDeviceSP dev1 = p.layer->paintDevice();

    KisKeyframeChannel *channel = dev1->keyframeChannel();
    channel->addKeyframe(10);
    channel->addKeyframe(20);


    QCOMPARE(i->currentTime(), 0);

    i->requestTimeSwitchWithUndo(15);
    QTest::qWait(100);
    p.image->waitForDone();
    QCOMPARE(i->currentTime(), 15);

    KisProcessingApplicator applicator(p.image, 0);

    i->requestTimeSwitchWithUndo(16);

    applicator.end();

    QTest::qWait(100);
    p.image->waitForDone();
    QCOMPARE(i->currentTime(), 16);
}

namespace {

inline QRect rectForTime(int time) {
    return QRect(time * 64 + 1, 65, 62, 62);
}

} // namespace

void KisImageAnimationInterfaceTest::testAutoKeyframeWithOnionSkins()
{
    KisSurrogateUndoStore *undoStore = new KisSurrogateUndoStore();
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(undoStore, 512, 512, cs, "");
    KisPaintLayerSP layer = new KisPaintLayer(image, "", OPACITY_OPAQUE_U8);
    image->addNode(layer);

    KisPaintDeviceSP dev = layer->paintDevice();

    layer->enableAnimation();
    layer->setOnionSkinEnabled(true);

    KisKeyframeChannel *contentChannel = layer->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    QVERIFY(contentChannel);

    for (int time = 1; time < 6; ++time) {
        qInfo() << "Switch to time" << time;
        image->animationInterface()->requestTimeSwitchWithUndo(time);
        QTest::qWait(200);
        image->waitForDone();

        QVERIFY(layer->needProjection());
        QVERIFY(layer->projection() != layer->paintDevice());
        QCOMPARE(image->animationInterface()->currentTime(), time);
        QCOMPARE(image->animationInterface()->currentUITime(), time);

        qInfo() << "Create frame at position" << time;
        KUndo2Command *parentCommand = new KUndo2Command;
        contentChannel->addKeyframe(time, parentCommand);
        image->undoAdapter()->addCommand(parentCommand);
        QCOMPARE(contentChannel->keyframeCount(), 1 + time);

        image->waitForDone();

        const QRect fillRect = rectForTime(time);

        dev->fill(fillRect, KoColor(Qt::black, cs));

        layer->setDirty(fillRect);
        image->waitForDone();

//        KIS_DUMP_DEVICE_2(dev, QRect(0,0,1000,200), "pd", "dd");
//        KIS_DUMP_DEVICE_2(layer->projection(), QRect(0,0,1000,200), "proj", "dd");

        const QRect farthestOnionSkin(rectForTime(qMax(1, time - 2)));

        QCOMPARE(layer->paintDevice()->exactBounds(), fillRect);
        QCOMPARE(layer->projection()->exactBounds(), fillRect | farthestOnionSkin);
    }

    QCOMPARE(image->animationInterface()->currentTime(), 5);
    QCOMPARE(image->animationInterface()->currentUITime(), 5);
    QCOMPARE(layer->paintDevice()->exactBounds(), rectForTime(5));
    QCOMPARE(layer->projection()->exactBounds(), rectForTime(3) | rectForTime(5));

    {
        qInfo() << "Remove frame at position" << 5;
        KUndo2Command *parentCommand = new KUndo2Command;
        contentChannel->removeKeyframe(5, parentCommand);
        image->undoAdapter()->addCommand(parentCommand);
        image->waitForDone();
    }

//    KIS_DUMP_DEVICE_2(dev, QRect(0,0,1000,200), "pd", "dd_xx");
//    KIS_DUMP_DEVICE_2(layer->projection(), QRect(0,0,1000,200), "proj", "dd_xx");

    // verify that the outline of onion skins has changed
    QCOMPARE(image->animationInterface()->currentTime(), 5);
    QCOMPARE(image->animationInterface()->currentUITime(), 5);
    QCOMPARE(layer->paintDevice()->exactBounds(), rectForTime(4));
    QCOMPARE(layer->projection()->exactBounds(), rectForTime(2) | rectForTime(4));

    qInfo() << "Undo the removal of the frame at position" << 5;
    // undo the removal, not the state should be the same as before
    undoStore->undo();
    QTest::qWait(200);
    image->waitForDone();

    QCOMPARE(image->animationInterface()->currentTime(), 5);
    QCOMPARE(image->animationInterface()->currentUITime(), 5);
    QCOMPARE(layer->paintDevice()->exactBounds(), rectForTime(5));
    QCOMPARE(layer->projection()->exactBounds(), rectForTime(3) | rectForTime(5));

    for (int time = 5; time >= 2; time--) {
        qInfo() << "Cycle: undo adding new frame at position" << time;
        undoStore->undo();
        QTest::qWait(200);
        image->waitForDone();

        QCOMPARE(image->animationInterface()->currentTime(), time);
        QCOMPARE(image->animationInterface()->currentUITime(), time);
        QCOMPARE(layer->paintDevice()->exactBounds(), rectForTime(time - 1));
        QCOMPARE(layer->projection()->exactBounds(), rectForTime(qMax(1, time - 3)) | rectForTime(time - 1));

        qInfo() << "Cycle: undo switching to time" << time;
        undoStore->undo();
        QTest::qWait(200);
        image->waitForDone();

        QCOMPARE(image->animationInterface()->currentTime(), time - 1);
        QCOMPARE(image->animationInterface()->currentUITime(), time - 1);
        QCOMPARE(layer->paintDevice()->exactBounds(), rectForTime(time - 1));
        QCOMPARE(layer->projection()->exactBounds(), rectForTime(qMax(1, time - 3)) | rectForTime(time - 1));
    }
}


SIMPLE_TEST_MAIN(KisImageAnimationInterfaceTest)
