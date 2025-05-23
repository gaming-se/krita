/* This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1997 Matthias Kalle Dalheimer (kalle@kde.org)
    SPDX-FileCopyrightText: 1998, 1999, 2000 KDE Team
    SPDX-FileCopyrightText: 2008 Nick Shaforostoff <shaforostoff@kde.ru>

    SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kcheckaccelerators.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QShortcutEvent>
#include <QMouseEvent>
#include <QLayout>
#include <QMenuBar>
#include <QTabBar>
#include <QTextBrowser>
#include <QChar>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QClipboard>
#include <QProcess>
#include <QDialogButtonBox>
#include <QFile>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>
#include <klocalizedstring.h>
#include <kacceleratormanager.h>

class KisKCheckAcceleratorsInitializer : public QObject
{
    Q_OBJECT
public:
    explicit KisKCheckAcceleratorsInitializer(QObject *parent = 0)
        : QObject(parent)
    {
    }

public Q_SLOTS:
    void initiateIfNeeded()
    {
        KConfigGroup cg(KSharedConfig::openConfig(), "Development");
        QString sKey = cg.readEntry("CheckAccelerators").trimmed();
        int key = 0;
        if (!sKey.isEmpty()) {
            QList<QKeySequence> cuts = QKeySequence::listFromString(sKey);
            if (!cuts.isEmpty()) {
                key = cuts.first()[0];
            }
        }
        const bool autoCheck = cg.readEntry("AutoCheckAccelerators", true);
        const bool copyWidgetText = cg.readEntry("CopyWidgetText", false);
        if (!copyWidgetText && key == 0 && !autoCheck) {
            deleteLater();
            return;
        }

        new KisKCheckAccelerators(qApp, key, autoCheck, copyWidgetText);
        deleteLater();
    }
};

static void startupFunc()
{
    // Call initiateIfNeeded once we're in the event loop
    // This is to prevent using KSharedConfig before main() can set the app name
    QCoreApplication *app = QCoreApplication::instance();
    KisKCheckAcceleratorsInitializer *initializer = new KisKCheckAcceleratorsInitializer(app);
    QMetaObject::invokeMethod(initializer, "initiateIfNeeded", Qt::QueuedConnection);
}

Q_COREAPP_STARTUP_FUNCTION(startupFunc)

KisKCheckAccelerators::KisKCheckAccelerators(QObject *parent, int key_, bool autoCheck_, bool copyWidgetText_)
    : QObject(parent)
    , key(key_)
    , autoCheck(autoCheck_)
    , copyWidgetText(copyWidgetText_)
    , drklash(nullptr)
{
    setObjectName(QStringLiteral("kapp_accel_filter"));

    KConfigGroup cg(KSharedConfig::openConfig(), "Development");
    alwaysShow = cg.readEntry("AlwaysShowCheckAccelerators", false);
    copyWidgetTextCommand = cg.readEntry("CopyWidgetTextCommand", QString());

    parent->installEventFilter(this);
    connect(&autoCheckTimer, SIGNAL(timeout()), SLOT(autoCheckSlot()));
}

bool KisKCheckAccelerators::eventFilter(QObject *obj, QEvent *e)
{
    if (block) {
        return false;
    }

    switch (e->type()) {   // just simplify debugging
    case QEvent::ShortcutOverride:
        if (key && (static_cast<QKeyEvent *>(e)->key() == key)) {
            block = true;
            checkAccelerators(false);
            block = false;
            e->accept();
            return true;
        }
        break;
    case QEvent::ChildAdded:
    case QEvent::ChildRemoved:
        // Only care about widgets; this also avoids starting the timer in other threads
        if (!static_cast<QChildEvent *>(e)->child()->isWidgetType()) {
            break;
        }
        Q_FALLTHROUGH();
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
        if (autoCheck) {
            autoCheckTimer.setSingleShot(true);
            autoCheckTimer.start(20);   // 20 ms
        }
        break;
    //case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
        if (copyWidgetText && static_cast<QMouseEvent *>(e)->button() == Qt::MiddleButton) {
            //kWarning()<<"obj"<<obj;
            QWidget *w = static_cast<QWidget *>(obj)->childAt(static_cast<QMouseEvent *>(e)->pos());
            if (!w) {
                w = static_cast<QWidget *>(obj);
            }
            if (!w) {
                return false;
            }
            //kWarning()<<"MouseButtonDblClick"<<w;
            QString text;
            if (qobject_cast<QLabel *>(w)) {
                text = static_cast<QLabel *>(w)->text();
            } else if (qobject_cast<QAbstractButton *>(w)) {
                text = static_cast<QAbstractButton *>(w)->text();
            } else if (qobject_cast<QComboBox *>(w)) {
                text = static_cast<QComboBox *>(w)->currentText();
            } else if (qobject_cast<QTabBar *>(w)) {
                text = static_cast<QTabBar *>(w)->tabText(static_cast<QTabBar *>(w)->tabAt(static_cast<QMouseEvent *>(e)->pos()));
            } else if (qobject_cast<QGroupBox *>(w)) {
                text = static_cast<QGroupBox *>(w)->title();
            } else if (qobject_cast<QMenu *>(obj)) {
                QAction *a = static_cast<QMenu *>(obj)->actionAt(static_cast<QMouseEvent *>(e)->pos());
                if (!a) {
                    return false;
                }
                text = a->text();
                if (text.isEmpty()) {
                    text = a->iconText();
                }
            }
            if (text.isEmpty()) {
                return false;
            }

            if (static_cast<QMouseEvent *>(e)->modifiers() == Qt::ControlModifier) {
                text.remove(QChar::fromLatin1('&'));
            }

            //kWarning()<<KGlobal::activeComponent().catalogName()<<text;
            if (copyWidgetTextCommand.isEmpty()) {
                QClipboard *clipboard = QApplication::clipboard();
                clipboard->setText(text);
            } else {
                QProcess *script = new QProcess(this);
                script->start(copyWidgetTextCommand.arg(text, QFile::decodeName(KLocalizedString::applicationDomain())), QStringList());
                connect(script, SIGNAL(finished(int,QProcess::ExitStatus)), script, SLOT(deleteLater()));
            }
            e->accept();
            return true;

            //kWarning()<<"MouseButtonDblClick"<<static_cast<QWidget*>(obj)->childAt(static_cast<QMouseEvent*>(e)->globalPos());
        }
        return false;
    case QEvent::Timer:
    case QEvent::MouseMove:
    case QEvent::Paint:
        return false;
    default:
        // qDebug() << "KisKCheckAccelerators::eventFilter " << e->type() << " " << autoCheck;
        break;
    }
    return false;
}

void KisKCheckAccelerators::autoCheckSlot()
{
    if (block) {
        autoCheckTimer.setSingleShot(true);
        autoCheckTimer.start(20);
        return;
    }
    block = true;
    checkAccelerators(!alwaysShow);
    block = false;
}

void KisKCheckAccelerators::createDialog(QWidget *actWin, bool automatic)
{
    if (drklash) {
        return;
    }

    drklash = new QDialog(actWin);
    drklash->setAttribute(Qt::WA_DeleteOnClose);
    drklash->setObjectName(QStringLiteral("kapp_accel_check_dlg"));
    drklash->setWindowTitle(i18nc("@title:window", "Dr. Klash' Accelerator Diagnosis"));
    drklash->resize(500, 460);
    QVBoxLayout *layout = new QVBoxLayout(drklash);
    drklash_view = new QTextBrowser(drklash);
    layout->addWidget(drklash_view);
    QCheckBox *disableAutoCheck = 0;
    if (automatic)  {
        disableAutoCheck = new QCheckBox(i18nc("@option:check", "Disable automatic checking"), drklash);
        connect(disableAutoCheck, SIGNAL(toggled(bool)), SLOT(slotDisableCheck(bool)));
        layout->addWidget(disableAutoCheck);
    }
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, drklash);
    layout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), drklash, SLOT(close()));
    if (disableAutoCheck) {
        disableAutoCheck->setFocus();
    } else {
        drklash_view->setFocus();
    }
}

void KisKCheckAccelerators::slotDisableCheck(bool on)
{
    autoCheck = !on;
    if (!on) {
        autoCheckSlot();
    }
}

void KisKCheckAccelerators::checkAccelerators(bool automatic)
{
    QWidget *actWin = qApp->activeWindow();
    if (!actWin) {
        return;
    }

    KAcceleratorManager::manage(actWin);
    QString a, c, r;
    KAcceleratorManager::last_manage(a, c,  r);

    if (automatic) { // for now we only show dialogs on F12 checks
        return;
    }

    if (c.isEmpty() && r.isEmpty() && (automatic || a.isEmpty())) {
        return;
    }

    QString s;

    if (! c.isEmpty())  {
        s += i18n("<h2>Accelerators changed</h2>");
        s += QStringLiteral("<table border><tr><th><b>");
        s += i18n("Old Text");
        s += QStringLiteral("</b></th><th><b>");
        s += i18n("New Text");
        s += QStringLiteral("</b></th></tr>");
        s += c;
        s += QStringLiteral("</table>");
    }

    if (! r.isEmpty())  {
        s += i18n("<h2>Accelerators removed</h2>");
        s += QStringLiteral("<table border><tr><th><b>");
        s += i18n("Old Text");
        s += QStringLiteral("</b></th></tr>");
        s += r;
        s += QStringLiteral("</table>");
    }

    if (! a.isEmpty())  {
        s += i18n("<h2>Accelerators added (just for your info)</h2>");
        s += QStringLiteral("<table border><tr><th><b>");
        s += i18n("New Text");
        s += QStringLiteral("</b></th></tr>");
        s += a;
        s += QStringLiteral("</table>");
    }

    createDialog(actWin, automatic);
    drklash_view->setHtml(s);
    drklash->show();
    drklash->raise();

    // dlg will be destroyed before returning
}

#include "kcheckaccelerators.moc"
