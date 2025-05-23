/*
 *  kis_tool_ellipse.cc - part of Krayon
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@compuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2004 Clarence Dang <dang@kde.org>
 *  SPDX-FileCopyrightText: 2009 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_ellipse.h"
#include <KoCanvasBase.h>
#include <KoShapeStroke.h>

#include <KisViewManager.h>
#include <canvas/kis_canvas2.h>
#include <kis_canvas_resource_provider.h>
#include <kis_shape_tool_helper.h>
#include "kis_figure_painting_tool_helper.h"
#include <brushengine/kis_paintop_preset.h>

KisToolEllipse::KisToolEllipse(KoCanvasBase * canvas)
        : KisToolEllipseBase(canvas, KisToolEllipseBase::PAINT, KisCursor::load("tool_ellipse_cursor.png", 6, 6))
{
    setObjectName("tool_ellipse");
    setSupportOutline(true);
    setIsOpacityPresetMode(true);

    KisCanvas2 *kritaCanvas = dynamic_cast<KisCanvas2*>(canvas);

    connect(kritaCanvas->viewManager()->canvasResourceProvider(), SIGNAL(sigEffectiveCompositeOpChanged()), SLOT(resetCursorStyle()));
}

KisToolEllipse::~KisToolEllipse()
{
}

void KisToolEllipse::resetCursorStyle()
{
    if (isEraser() && (nodePaintAbility() == NodePaintAbility::PAINT)) {
        useCursor(KisCursor::load("tool_ellipse_eraser_cursor.png", 6, 6));
    } else {
        KisToolEllipseBase::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolEllipse::finishRect(const QRectF& rect, qreal roundCornersX, qreal roundCornersY)
{
    Q_UNUSED(roundCornersX);
    Q_UNUSED(roundCornersY);

    if (rect.isEmpty())
        return;

    const KisToolShape::ShapeAddInfo info =
        shouldAddShape(currentNode());

    if (!info.shouldAddShape) {
        KisFigurePaintingToolHelper helper(kundo2_i18n("Draw Ellipse"),
                                           image(),
                                           currentNode(),
                                           canvas()->resourceManager(),
                                           strokeStyle(),
                                           fillStyle(),
                                           fillTransform());
        QPainterPath path;
        path.addEllipse(rect);
        getRotatedPath(path, rect.center(), getRotationAngle());
        helper.paintPainterPath(path);
    } else {
        KisResourcesSnapshot resources(image(),
                                       currentNode(),
                                       canvas()->resourceManager());
        QRectF r = convertToPt(rect);
        KoShape* shape = KisShapeToolHelper::createEllipseShape(r);
        shape->rotate(qRadiansToDegrees(getRotationAngle()));
        KoShapeStrokeSP border(new KoShapeStroke(currentStrokeWidth(), resources.currentFgColor().toQColor()));
        shape->setStroke(border);

        info.markAsSelectionShapeIfNeeded(shape);

        addShape(shape);
    }
}

bool KisToolEllipse::supportsPaintingAssistants() const
{
    return true;
}
