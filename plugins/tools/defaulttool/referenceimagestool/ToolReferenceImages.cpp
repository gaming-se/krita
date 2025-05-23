/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ToolReferenceImages.h"

#include <QDesktopServices>
#include <QFile>
#include <QLayout>
#include <QMenu>
#include <QMessageBox>
#include <QAction>
#include <QApplication>

#include <KoSelection.h>
#include <KoShapeRegistry.h>
#include <KoShapeManager.h>
#include <KoShapeController.h>
#include <KoFileDialog.h>
#include "KisMimeDatabase.h"

#include <kis_action_registry.h>
#include <kis_canvas2.h>
#include <kis_canvas_resource_provider.h>
#include <kis_node_manager.h>
#include <KisViewManager.h>
#include <KisDocument.h>
#include <KisReferenceImagesLayer.h>
#include <kis_image.h>
#include "QClipboard"
#include "kis_action.h"
#include <KisCursorOverrideLock.h>

#include "ToolReferenceImagesWidget.h"
#include "KisReferenceImageCollection.h"

ToolReferenceImages::ToolReferenceImages(KoCanvasBase * canvas)
    : DefaultTool(canvas, false)
{
    setObjectName("ToolReferenceImages");
}

ToolReferenceImages::~ToolReferenceImages()
{
}

void ToolReferenceImages::activate(const QSet<KoShape*> &shapes)
{
    DefaultTool::activate(shapes);

    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT(kisCanvas);
    connect(kisCanvas->image(), SIGNAL(sigNodeAddedAsync(KisNodeSP, KisNodeAdditionFlags)), this, SLOT(slotNodeAdded(KisNodeSP, KisNodeAdditionFlags)));
    connect(kisCanvas->imageView()->document(), &KisDocument::sigReferenceImagesLayerChanged, this, qOverload<KisNodeSP>(&ToolReferenceImages::slotNodeAdded));

    auto referenceImageLayer = document()->referenceImagesLayer();
    if (referenceImageLayer) {
        setReferenceImageLayer(referenceImageLayer);
    }
}

void ToolReferenceImages::deactivate()
{
    DefaultTool::deactivate();
}

void ToolReferenceImages::slotNodeAdded(KisNodeSP node)
{
    slotNodeAdded(node, KisNodeAdditionFlag::None);
}

void ToolReferenceImages::slotNodeAdded(KisNodeSP node, KisNodeAdditionFlags flags)
{
    Q_UNUSED(flags)

    auto *referenceImagesLayer = dynamic_cast<KisReferenceImagesLayer*>(node.data());

    if (referenceImagesLayer) {
        setReferenceImageLayer(referenceImagesLayer);
    }
}

void ToolReferenceImages::setReferenceImageLayer(KisSharedPtr<KisReferenceImagesLayer> layer)
{
    m_layer = layer;
    connect(layer.data(), SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()));
    connect(layer->shapeManager(), SIGNAL(selectionChanged()), this, SLOT(repaintDecorations()));
    connect(layer->shapeManager(), SIGNAL(selectionContentChanged()), this, SLOT(repaintDecorations()));
}

bool ToolReferenceImages::hasSelection()
{
    const KoShapeManager *manager = shapeManager();
    return manager && manager->selection()->count() != 0;
}

void ToolReferenceImages::addReferenceImage()
{
    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);

            KoFileDialog dialog(kisCanvas->viewManager()->mainWindowAsQWidget(), KoFileDialog::OpenFile, "OpenReferenceImage");
    dialog.setCaption(i18n("Select a Reference Image"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDefaultDir(locations.first());
    }

    QString filename = dialog.filename();
    if (filename.isEmpty()) return;
    if (!QFileInfo(filename).exists()) return;

    auto *reference = KisReferenceImage::fromFile(filename, *kisCanvas->coordinatesConverter(), canvas()->canvasWidget());
    if (reference) {
        if (document()->referenceImagesLayer()) {
            reference->setZIndex(document()->referenceImagesLayer()->shapes().size());
        }
        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), {reference}));
    }
}

void ToolReferenceImages::addReferenceImageFromLayer()
{
    KisCanvas2* kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);
    kisCanvas->viewManager()->nodeManager()->createReferenceImageFromLayer();
}

void ToolReferenceImages::addReferenceImageFromVisible()
{
    KisCanvas2* kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);
    kisCanvas->viewManager()->nodeManager()->createReferenceImageFromVisible();
}

void ToolReferenceImages::pasteReferenceImage()
{
    KisCanvas2* kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);

    KisReferenceImage* reference = KisReferenceImage::fromClipboard(*kisCanvas->coordinatesConverter());
    if (reference) {
        if (document()->referenceImagesLayer()) {
            reference->setZIndex(document()->referenceImagesLayer()->shapes().size());
        }
        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), {reference}));
    } else {
        if (canvas()->canvasWidget()) {
            QMessageBox::critical(canvas()->canvasWidget(), i18nc("@title:window", "Krita"), i18n("Could not load reference image from clipboard"));
        }
    }
}

void ToolReferenceImages::removeSelectedReferenceImages()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;
    if (!koSelection()) return;
    if (koSelection()->selectedEditableShapes().isEmpty()) return;

    canvas()->addCommand(layer->removeReferenceImages(document(), koSelection()->selectedEditableShapes()));
}

void ToolReferenceImages::removeAllReferenceImages()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    canvas()->addCommand(layer->removeReferenceImages(document(), layer->shapes()));
}

void ToolReferenceImages::loadReferenceImages()
{
    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);

            KoFileDialog dialog(kisCanvas->viewManager()->mainWindowAsQWidget(), KoFileDialog::OpenFile, "OpenReferenceImageCollection");
    dialog.setMimeTypeFilters(QStringList() << "application/x-krita-reference-images");
    dialog.setCaption(i18n("Load Reference Images"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDefaultDir(locations.first());
    }

    QString filename = dialog.filename();
    if (filename.isEmpty()) return;
    if (!QFileInfo(filename).exists()) return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not open '%1'.", filename));
        return;
    }

    KisReferenceImageCollection collection;

    int currentZIndex = 0;
    if (document()->referenceImagesLayer()) {
        currentZIndex = document()->referenceImagesLayer()->shapes().size();
    }

    if (collection.load(&file)) {
        QList<KoShape*> shapes;
        Q_FOREACH(auto *reference, collection.referenceImages()) {
            reference->setZIndex(currentZIndex);
            shapes.append(reference);
            currentZIndex += 1;
        }

        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), shapes));
    } else {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not load reference images from '%1'.", filename));
    }
    file.close();
}

void ToolReferenceImages::saveReferenceImages()
{
    KisCursorOverrideLock cursorLock(Qt::BusyCursor);

    auto layer = m_layer.toStrongRef();
    if (!layer || layer->shapeCount() == 0) return;

    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);

            KoFileDialog dialog(kisCanvas->viewManager()->mainWindowAsQWidget(), KoFileDialog::SaveFile, "SaveReferenceImageCollection");
    QString mimetype = "application/x-krita-reference-images";
    dialog.setMimeTypeFilters(QStringList() << mimetype, mimetype);
    dialog.setCaption(i18n("Save Reference Images"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDefaultDir(locations.first());
    }

    QString filename = dialog.filename();
    if (filename.isEmpty()) return;

    QString fileMime = KisMimeDatabase::mimeTypeForFile(filename, false);
    if (fileMime != "application/x-krita-reference-images") {
        filename.append(filename.endsWith(".") ? "krf" : ".krf");
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not open '%1' for saving.", filename));
        return;
    }

    KisReferenceImageCollection collection(layer->referenceImages());
    bool ok = collection.save(&file);
    file.close();

    if (!ok) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Failed to save reference images."));
    }
}

void ToolReferenceImages::slotSelectionChanged()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    m_optionsWidget->selectionChanged(layer->shapeManager()->selection());
    updateActions();
}

QList<QPointer<QWidget>> ToolReferenceImages::createOptionWidgets()
{
    // Instead of inheriting DefaultTool's multi-tab implementation, inherit straight from KoToolBase
    return KoToolBase::createOptionWidgets();
}

QWidget *ToolReferenceImages::createOptionWidget()
{
    if (!m_optionsWidget) {
        m_optionsWidget = new ToolReferenceImagesWidget(this);
        // See https://bugs.kde.org/show_bug.cgi?id=316896
        QWidget *specialSpacer = new QWidget(m_optionsWidget);
        specialSpacer->setObjectName("SpecialSpacer");
        specialSpacer->setFixedSize(0, 0);
        m_optionsWidget->layout()->addWidget(specialSpacer);
    }
    return m_optionsWidget;
}

bool ToolReferenceImages::isValidForCurrentLayer() const
{
    return true;
}

KoShapeManager *ToolReferenceImages::shapeManager() const
{
    auto layer = m_layer.toStrongRef();
    return layer ? layer->shapeManager() : nullptr;
}

KoSelection *ToolReferenceImages::koSelection() const
{
    auto manager = shapeManager();
    return manager ? manager->selection() : nullptr;
}

void ToolReferenceImages::updateDistinctiveActions(const QList<KoShape*> &)
{
    action("object_group")->setEnabled(false);
    action("object_unite")->setEnabled(false);
    action("object_intersect")->setEnabled(false);
    action("object_subtract")->setEnabled(false);
    action("object_split")->setEnabled(false);
    action("object_ungroup")->setEnabled(false);
}

void ToolReferenceImages::deleteSelection()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    QList<KoShape *> shapes = koSelection()->selectedShapes();

    if (!shapes.empty()) {
        canvas()->addCommand(layer->removeReferenceImages(document(), shapes));
    }
}

QMenu* ToolReferenceImages::popupActionsMenu()
{
    if (m_contextMenu) {
        m_contextMenu->clear();
        m_contextMenu->addSection(i18n("Reference Image Actions"));
        m_contextMenu->addSeparator();

        QMenu *transform = m_contextMenu->addMenu(i18n("Transform"));

        transform->addAction(action("object_transform_rotate_90_cw"));
        transform->addAction(action("object_transform_rotate_90_ccw"));
        transform->addAction(action("object_transform_rotate_180"));
        transform->addSeparator();
        transform->addAction(action("object_transform_mirror_horizontally"));
        transform->addAction(action("object_transform_mirror_vertically"));
        transform->addSeparator();
        transform->addAction(action("object_transform_reset"));

        m_contextMenu->addSeparator();

        m_contextMenu->addAction(action("edit_cut"));
        m_contextMenu->addAction(action("edit_copy"));
        m_contextMenu->addAction(action("edit_paste"));

        m_contextMenu->addSeparator();

        m_contextMenu->addAction(action("object_order_front"));
        m_contextMenu->addAction(action("object_order_raise"));
        m_contextMenu->addAction(action("object_order_lower"));
        m_contextMenu->addAction(action("object_order_back"));
    }

    return m_contextMenu.data();
}

void ToolReferenceImages::cut()
{
    copy();
    deleteSelection();
}

void ToolReferenceImages::copy() const
{
    QList<KoShape *> shapes = koSelection()->selectedShapes();
    if (!shapes.isEmpty()) {
        KoShape* shape = shapes.at(0);
        KisReferenceImage *reference = dynamic_cast<KisReferenceImage*>(shape);
        KIS_SAFE_ASSERT_RECOVER_RETURN(reference);
        QClipboard *cb = QApplication::clipboard();
        cb->setImage(reference->getImage());
    }
}

bool ToolReferenceImages::paste()
{
    pasteReferenceImage();
    return true;
}

bool ToolReferenceImages::selectAll()
{
    Q_FOREACH(KoShape *shape, shapeManager()->shapes()) {
        if (!shape->isSelectable()) continue;
        koSelection()->select(shape);
    }
    repaintDecorations();

    return true;
}

void ToolReferenceImages::deselect()
{
    koSelection()->deselectAll();
    repaintDecorations();
}

KisDocument *ToolReferenceImages::document() const
{
    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT(kisCanvas);
    return kisCanvas->imageView()->document();
}

QList<QAction *> ToolReferenceImagesFactory::createActionsImpl()
{
    QList<QAction *> defaultActions = DefaultToolFactory::createActionsImpl();
    QList<QAction *> actions;

    QStringList actionNames;
    actionNames << "object_order_front"
                << "object_order_raise"
                << "object_order_lower"
                << "object_order_back"
                << "object_transform_rotate_90_cw"
                << "object_transform_rotate_90_ccw"
                << "object_transform_rotate_180"
                << "object_transform_mirror_horizontally"
                << "object_transform_mirror_vertically"
                << "object_transform_reset";

    Q_FOREACH(QAction *action, defaultActions) {
        if (actionNames.contains(action->objectName())) {
            actions << action;
        } else {
            delete action;
        }
    }
    return actions;
}
