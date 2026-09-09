#include "cursorhand.h"

#include <QEvent>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPixmap>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>

namespace {

const char kFilterProp[] = "_cursorhand_watch";

bool isPassThroughOverlay(const QQuickItem *item)
{
    if (!item || !item->inherits("QQuickMouseArea"))
        return false;
    const auto buttons = Qt::MouseButtons(item->property("acceptedButtons").toInt());
    if (buttons & Qt::LeftButton)
        return false;
    if (item->property("hoverEnabled").toBool())
        return false;
    return true;
}

QQuickItem *pickThrough(QQuickItem *item, const QPointF &localPos)
{
    if (!item || !item->isVisible() || item->opacity() < 0.01)
        return nullptr;

    const QRectF bounds(0, 0, item->width(), item->height());
    if (item->width() > 0 && item->height() > 0 && !bounds.contains(localPos))
        return nullptr;

    const QList<QQuickItem *> kids = item->childItems();
    for (int i = kids.size() - 1; i >= 0; --i) {
        QQuickItem *child = kids.at(i);
        if (!child->isEnabled() && !isPassThroughOverlay(child))
            continue;
        if (QQuickItem *hit = pickThrough(child, child->mapFromItem(item, localPos)))
            return hit;
    }

    if (isPassThroughOverlay(item))
        return nullptr;
    if (item->width() <= 0 || item->height() <= 0)
        return nullptr;
    return item;
}

bool resolveCursor(QQuickItem *hit, QCursor *out)
{
    for (QQuickItem *p = hit; p; p = p->parentItem()) {
        if (isPassThroughOverlay(p))
            continue;
        if (p->inherits("QQuickTextInput") || p->inherits("QQuickTextEdit"))
            return (*out = QCursor(Qt::IBeamCursor), true);
        if (p->inherits("QQuickMouseArea")) {
            const auto shape = Qt::CursorShape(p->property("cursorShape").toInt());
            return (*out = QCursor(shape), true);
        }
        if (p->inherits("QQuickAbstractButton"))
            return (*out = QCursor(Qt::PointingHandCursor), true);
        if (p->inherits("QQuickHoverHandler")) {
            const QVariant shape = p->property("cursorShape");
            if (shape.isValid())
                return (*out = QCursor(Qt::CursorShape(shape.toInt())), true);
        }
    }
    return false;
}

class CursorHandWindowFilter : public QObject
{
public:
    explicit CursorHandWindowFilter(QQuickWindow *window)
        : QObject(window)
        , m_window(window)
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched != m_window)
            return false;
        if (QGuiApplication::overrideCursor())
            return false;

        if (event->type() == QEvent::Leave) {
            m_window->unsetCursor();
            return false;
        }
        if (event->type() != QEvent::MouseMove && event->type() != QEvent::HoverMove)
            return false;

        QPointF pos;
        if (event->type() == QEvent::MouseMove)
            pos = static_cast<QMouseEvent *>(event)->position();
        else
            pos = static_cast<QHoverEvent *>(event)->position();

        QQuickItem *content = m_window->contentItem();
        if (!content)
            return false;

        QQuickItem *hit = pickThrough(content, content->mapFromScene(pos));
        QCursor cursor;
        if (resolveCursor(hit, &cursor))
            m_window->setCursor(cursor);
        else
            m_window->unsetCursor();
        return false;
    }

private:
    QQuickWindow *m_window = nullptr;
};

void installWatch(QQuickWindow *window)
{
    if (!window || window->property(kFilterProp).toBool())
        return;
    auto *filter = new CursorHandWindowFilter(window);
    window->installEventFilter(filter);
    window->setProperty(kFilterProp, true);
}

} // namespace

static QPixmap pixmapFromUrl(const QUrl &url)
{
    auto loadPath = [](const QString &path) {
        QPixmap px;
        if (!path.isEmpty())
            px.load(path);
        return px;
    };

    QPixmap px;
    if (url.scheme() == QLatin1String("qrc"))
        px = loadPath(QLatin1Char(':') + url.path());
    if (px.isNull() && url.isLocalFile())
        px = loadPath(url.toLocalFile());
    if (px.isNull() && (url.scheme().isEmpty() || url.scheme() == QLatin1String("file")))
        px = loadPath(url.path());
#if defined(Q_OS_WASM)
    // Emscripten FS / qrc fallbacks: no real local files in the browser.
    if (px.isNull() && !url.path().isEmpty()) {
        px = loadPath(url.path());
        if (px.isNull() && !url.path().startsWith(QLatin1Char(':')))
            px = loadPath(QLatin1Char(':') + url.path());
    }
#endif
    return px;
}

CursorHandAttached::CursorHandAttached(QObject *parent)
    : QObject(parent)
{
    if (auto *item = qobject_cast<QQuickItem *>(parent)) {
        connect(item, &QQuickItem::windowChanged, this, [this](QQuickWindow *) { apply(); });
        connect(item, &QQuickItem::parentChanged, this, [this](QQuickItem *) { apply(); });
    }
    apply();
    // Engine/window are often not ready during construction (especially on WASM).
    QMetaObject::invokeMethod(this, [this] { apply(); }, Qt::QueuedConnection);
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
                      QUrl(QStringLiteral("qrc:/qt/qml/Cursor/Hand/internal/Hover.qml")));

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
        CursorHand::ensureWatch(item);
        return;
    }

    if (auto *window = qobject_cast<QWindow *>(parent())) {
        if (m_enabled)
            window->setCursor(QCursor(m_shape));
        else
            window->unsetCursor();
        if (auto *quick = qobject_cast<QQuickWindow *>(window))
            installWatch(quick);
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
    const QPixmap pixmap = pixmapFromUrl(url);
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

void CursorHand::ensureWatch(QObject *target)
{
    auto *item = qobject_cast<QQuickItem *>(target);
    if (!item)
        return;
    if (auto *window = item->window())
        installWatch(window);
    static const char kItemWatched[] = "_cursorhand_item_watched";
    if (item->property(kItemWatched).toBool())
        return;
    item->setProperty(kItemWatched, true);
    QObject::connect(item, &QQuickItem::windowChanged, item, [](QQuickWindow *window) {
        if (window)
            installWatch(window);
    });
}
