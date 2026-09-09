#include "cursorhand.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>

CursorHandAttached::CursorHandAttached(QObject *parent)
    : QObject(parent)
{
    if (auto *item = qobject_cast<QQuickItem *>(parent)) {
        QObject::connect(item, &QQuickItem::windowChanged, this, [this](QQuickWindow *window) {
            if (window)
                apply();
        });
    }
    apply();
}

CursorHandAttached::~CursorHandAttached()
{
    clearItemCursor();
}

void CursorHandAttached::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    apply();
}

void CursorHandAttached::setShape(Qt::CursorShape shape)
{
    if (m_shape == shape)
        return;
    m_shape = shape;
    emit shapeChanged();
    apply();
}

void CursorHandAttached::clearItemCursor()
{
    if (auto *item = qobject_cast<QQuickItem *>(parent()))
        item->unsetCursor();
    else if (auto *window = qobject_cast<QWindow *>(parent()))
        window->unsetCursor();
}

void CursorHandAttached::ensureHandler()
{
    if (m_handler)
        return;

    auto *item = qobject_cast<QQuickItem *>(parent());
    if (!item)
        return;

    QQmlEngine *engine = qmlEngine(item);
    if (!engine)
        engine = qmlEngine(this);
    if (!engine)
        return;

    QQmlComponent component(engine);
    component.setData(QByteArrayLiteral(
                          "import QtQuick\n"
                          "HoverHandler {\n"
                          "    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad\n"
                          "}\n"),
                      QUrl(QStringLiteral("qrc:/Cursor/Hand/internal/Hover.qml")));

    m_handler = component.create(qmlContext(item));
    if (!m_handler) {
        qWarning("CursorHand: failed to create HoverHandler: %s",
                 qPrintable(component.errorString()));
        return;
    }

    m_handler->setParent(this);
    m_handler->setProperty("parent", QVariant::fromValue(item));
    connect(m_handler, SIGNAL(hoveredChanged()), this, SLOT(syncHovered()));
}

void CursorHandAttached::syncHovered()
{
    if (!m_handler)
        return;
    const bool hovered = m_handler->property("hovered").toBool();
    if (m_hovered == hovered)
        return;
    m_hovered = hovered;
    emit hoveredChanged();
}

void CursorHandAttached::apply()
{
    if (auto *item = qobject_cast<QQuickItem *>(parent())) {
        item->setAcceptHoverEvents(true);
        if (m_enabled)
            item->setCursor(QCursor(m_shape));
        else
            item->unsetCursor();

        ensureHandler();
        if (m_handler) {
            m_handler->setProperty("enabled", m_enabled);
            m_handler->setProperty("cursorShape", int(m_shape));
        }
        return;
    }

    if (auto *window = qobject_cast<QWindow *>(parent())) {
        if (m_enabled)
            window->setCursor(QCursor(m_shape));
        else
            window->unsetCursor();
    }
}

CursorHand::CursorHand(QObject *parent)
    : QObject(parent)
{
}

CursorHand *CursorHand::create(QQmlEngine *, QJSEngine *)
{
    return new CursorHand;
}

CursorHandAttached *CursorHand::qmlAttachedProperties(QObject *object)
{
    return new CursorHandAttached(object);
}

void CursorHand::setOverride(Qt::CursorShape shape)
{
    QGuiApplication::setOverrideCursor(QCursor(shape));
}

void CursorHand::setPixmapOverride(const QUrl &url, int hotX, int hotY)
{
    QString path;
    if (url.scheme() == QLatin1String("qrc"))
        path = QLatin1Char(':') + url.path();
    else
        path = url.toLocalFile();

    const QPixmap pixmap(path);
    if (pixmap.isNull()) {
        qWarning("CursorHand: cannot load cursor pixmap from %s", qPrintable(url.toString()));
        return;
    }
    QGuiApplication::setOverrideCursor(QCursor(pixmap, hotX, hotY));
}

void CursorHand::restoreOverride()
{
    QGuiApplication::restoreOverrideCursor();
}
