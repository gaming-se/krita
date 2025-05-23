/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoPatternBackground.h"
#include "KoShapeSavingContext.h"
#include <KoXmlNS.h>
#include <KoUnit.h>

#include <FlakeDebug.h>

#include <QBrush>
#include <QPainter>
#include <QPainterPath>
#include <QSharedData>

class KoPatternBackground::Private : public QSharedData
{
public:
    Private()
        : QSharedData()
        , repeat(KoPatternBackground::Tiled)
        , refPoint(KoPatternBackground::TopLeft)
    {
    }

    ~Private()
    {
    }

    QSizeF targetSize() const {
        QSizeF size = pattern.size();
        if (targetImageSizePercent.width() > 0.0)
            size.setWidth(0.01 * targetImageSizePercent.width() * size.width());
        else if (targetImageSize.width() > 0.0)
            size.setWidth(targetImageSize.width());
        if (targetImageSizePercent.height() > 0.0)
            size.setHeight(0.01 * targetImageSizePercent.height() * size.height());
        else if (targetImageSize.height() > 0.0)
            size.setHeight(targetImageSize.height());

        return size;
    }

    QPointF offsetFromRect(const QRectF &fillRect, const QSizeF &imageSize) const {
        QPointF offset;
        switch (refPoint) {
        case KoPatternBackground::TopLeft:
            offset = fillRect.topLeft();
            break;
        case KoPatternBackground::Top:
            offset.setX(fillRect.center().x() - 0.5 * imageSize.width());
            offset.setY(fillRect.top());
            break;
        case KoPatternBackground::TopRight:
            offset.setX(fillRect.right() - imageSize.width());
            offset.setY(fillRect.top());
            break;
        case KoPatternBackground::Left:
            offset.setX(fillRect.left());
            offset.setY(fillRect.center().y() - 0.5 * imageSize.height());
            break;
        case KoPatternBackground::Center:
            offset.setX(fillRect.center().x() - 0.5 * imageSize.width());
            offset.setY(fillRect.center().y() - 0.5 * imageSize.height());
            break;
        case KoPatternBackground::Right:
            offset.setX(fillRect.right() - imageSize.width());
            offset.setY(fillRect.center().y() - 0.5 * imageSize.height());
            break;
        case KoPatternBackground::BottomLeft:
            offset.setX(fillRect.left());
            offset.setY(fillRect.bottom() - imageSize.height());
            break;
        case KoPatternBackground::Bottom:
            offset.setX(fillRect.center().x() - 0.5 * imageSize.width());
            offset.setY(fillRect.bottom() - imageSize.height());
            break;
        case KoPatternBackground::BottomRight:
            offset.setX(fillRect.right() - imageSize.width());
            offset.setY(fillRect.bottom() - imageSize.height());
            break;
        default:
            break;
        }
        if (refPointOffsetPercent.x() > 0.0)
            offset += QPointF(0.01 * refPointOffsetPercent.x() * imageSize.width(), 0);
        if (refPointOffsetPercent.y() > 0.0)
            offset += QPointF(0, 0.01 * refPointOffsetPercent.y() * imageSize.height());

        return offset;
    }

    QTransform matrix;
    KoPatternBackground::PatternRepeat repeat;
    KoPatternBackground::ReferencePoint refPoint;
    QSizeF targetImageSize;
    QSizeF targetImageSizePercent;
    QPointF refPointOffsetPercent;
    QPointF tileRepeatOffsetPercent;
    QImage pattern;
};


// ----------------------------------------------------------------


KoPatternBackground::KoPatternBackground()
    : KoShapeBackground()
    , d(new Private)
{
}

KoPatternBackground::~KoPatternBackground()
{
}

KoPatternBackground::KoPatternBackground(const KoPatternBackground &rhs)
    : d(new Private(*rhs.d))
{
}

KoPatternBackground& KoPatternBackground::operator=(const KoPatternBackground &rhs)
{
    d = rhs.d;
    return *this;
}

bool KoPatternBackground::compareTo(const KoShapeBackground *other) const
{
    Q_UNUSED(other);
    return false;
}

void KoPatternBackground::setTransform(const QTransform &matrix)
{
    d->matrix = matrix;
}

QTransform KoPatternBackground::transform() const
{
    return d->matrix;
}

void KoPatternBackground::setPattern(const QImage &pattern)
{
    d->pattern = pattern;
}

QImage KoPatternBackground::pattern() const
{
    return d->pattern;
}

void KoPatternBackground::setRepeat(PatternRepeat repeat)
{
    d->repeat = repeat;
}

KoPatternBackground::PatternRepeat KoPatternBackground::repeat() const
{
    return d->repeat;
}

KoPatternBackground::ReferencePoint KoPatternBackground::referencePoint() const
{
    return d->refPoint;
}

void KoPatternBackground::setReferencePoint(ReferencePoint referencePoint)
{
    d->refPoint = referencePoint;
}

QPointF KoPatternBackground::referencePointOffset() const
{
    return d->refPointOffsetPercent;
}

void KoPatternBackground::setReferencePointOffset(const QPointF &offset)
{
    qreal ox = qMax(qreal(0.0), qMin(qreal(100.0), offset.x()));
    qreal oy = qMax(qreal(0.0), qMin(qreal(100.0), offset.y()));

    d->refPointOffsetPercent = QPointF(ox, oy);
}

QPointF KoPatternBackground::tileRepeatOffset() const
{
    return d->tileRepeatOffsetPercent;
}

void KoPatternBackground::setTileRepeatOffset(const QPointF &offset)
{
    d->tileRepeatOffsetPercent = offset;
}

QSizeF KoPatternBackground::patternDisplaySize() const
{
    return d->targetSize();
}

void KoPatternBackground::setPatternDisplaySize(const QSizeF &size)
{
    d->targetImageSizePercent = QSizeF();
    d->targetImageSize = size;
}

QSizeF KoPatternBackground::patternOriginalSize() const
{
    return d->pattern.size();
}

void KoPatternBackground::paint(QPainter &painter, const QPainterPath &fillPath) const
{
    if (d->pattern.isNull()) {
        return;
    }

    painter.save();
    if (d->repeat == Tiled) {
        // calculate scaling of pixmap
        QSizeF targetSize = d->targetSize();
        QSizeF imageSize = d->pattern.size();
        qreal scaleX = targetSize.width() / imageSize.width();
        qreal scaleY = targetSize.height() / imageSize.height();

        QRectF targetRect = fillPath.boundingRect();
        // undo scaling on target rectangle
        targetRect.setWidth(targetRect.width() / scaleX);
        targetRect.setHeight(targetRect.height() / scaleY);

        // determine pattern offset
        QPointF offset = d->offsetFromRect(targetRect, imageSize);

        // create matrix for pixmap scaling
        QTransform matrix;
        matrix.scale(scaleX, scaleY);

        painter.setClipPath(fillPath);
        painter.setWorldTransform(matrix, true);
        painter.drawTiledPixmap(targetRect, QPixmap::fromImage(d->pattern), -offset);
    } else if (d->repeat == Original) {
        QRectF sourceRect(QPointF(0, 0), d->pattern.size());
        QRectF targetRect(QPoint(0, 0), d->targetSize());
        targetRect.moveCenter(fillPath.boundingRect().center());
        painter.setClipPath(fillPath);
        painter.drawPixmap(targetRect, QPixmap::fromImage(d->pattern).scaled(sourceRect.size().toSize()), sourceRect);
    } else if (d->repeat == Stretched) {
        painter.setClipPath(fillPath);
        // undo conversion of the scaling so that we can use a nicely scaled image of the correct size
        qWarning() << "WARNING: stretched KoPatternBackground painting code is abandoned. The result might be not correct";
        const QRectF targetRect = fillPath.boundingRect();
        painter.drawPixmap(targetRect.topLeft(), QPixmap::fromImage(d->pattern).scaled(targetRect.size().toSize()));
    }

    painter.restore();
}


QRectF KoPatternBackground::patternRectFromFillSize(const QSizeF &size)
{
    QRectF rect;

    switch (d->repeat) {
    case Tiled:
        rect.setTopLeft(d->offsetFromRect(QRectF(QPointF(), size), d->targetSize()));
        rect.setSize(d->targetSize());
        break;
    case Original:
        rect.setLeft(0.5 * (size.width() - d->targetSize().width()));
        rect.setTop(0.5 * (size.height() - d->targetSize().height()));
        rect.setSize(d->targetSize());
        break;
    case Stretched:
        rect.setTopLeft(QPointF(0.0, 0.0));
        rect.setSize(size);
        break;
    }

    return rect;
}
