#include "cursorhand.h"

#include <QChildEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>

#include <algorithm>

namespace {

const char kFilterProp[] = "_cursorhand_watch";

bool isPassThroughOverlay(const QQuickItem *item)
{
    if (!item || !item->inherits("QQuickMouseArea"))
        return false;
    // QQuickMouseArea.enabled is not QQuickItem::isEnabled(). A disabled
    // MouseArea still reports Item::isEnabled() == true, stays in childItems(),
    // and would otherwise lock the window cursor to Arrow (full-window click
    // guards with enabled: false).
    const QVariant enabled = item->property("enabled");
    if (enabled.isValid() && !enabled.toBool())
        return true;
    if (item->property("hoverEnabled").toBool())
        return false;
    // Explicit OpenHand / CrossHair / IBeam still win. Default Arrow plus
    // hoverEnabled: false is a click-eater or KDDW fill shell, not a cursor.
    const auto shape = Qt::CursorShape(item->property("cursorShape").toInt());
    if (shape != Qt::ArrowCursor)
        return false;
    return true;
}

bool hoverHandlerCursor(const QQuickItem *item, QCursor *out)
{
    const QObjectList kids = item->children();
    for (QObject *child : kids) {
        if (!child->inherits("QQuickHoverHandler"))
            continue;
        const QVariant enabled = child->property("enabled");
        if (enabled.isValid() && !enabled.toBool())
            continue;
        const QVariant shape = child->property("cursorShape");
        if (!shape.isValid())
            continue;
        if (out)
            *out = QCursor(Qt::CursorShape(shape.toInt()));
        return true;
    }
    return false;
}

bool hasCursorSemantics(const QQuickItem *item)
{
    if (item->inherits("QQuickTextInput") || item->inherits("QQuickTextEdit"))
        return true;
    if (item->inherits("QQuickAbstractButton"))
        return true;
    if (item->inherits("QQuickMouseArea") && !isPassThroughOverlay(item))
        return true;
    return hoverHandlerCursor(item, nullptr);
}

QQuickItem *pickThrough(QQuickItem *item, const QPointF &localPos)
{
    if (!item || !item->isVisible() || item->opacity() < 0.01)
        return nullptr;

    const QRectF bounds(0, 0, item->width(), item->height());
    if (item->width() > 0 && item->height() > 0 && !bounds.contains(localPos))
        return nullptr;

    QList<QQuickItem *> kids = item->childItems();
    std::stable_sort(kids.begin(), kids.end(), [](const QQuickItem *a, const QQuickItem *b) {
        return a->z() < b->z();
    });
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
    // Layout / toast fillers have size but no cursor of their own. Returning
    // them would stop the sibling walk and unsetCursor(), after which Qt's
    // item-cursor pick can still land on a higher-z MouseArea's Arrow.
    if (!hasCursorSemantics(item))
        return nullptr;
    return item;
}

// Default MouseArea calls setCursor(Arrow) in its ctor, so hasCursor is
// true even when hoverEnabled is false. findCursorItemAndHandler then
// stops on a later-declared fill overlay and never sees HoverHandler.
// Clearing that flag lets Qt's own picker look through. Walk the whole
// tree: HandCursor.onCompleted (and thus ensureWatch) often runs before
// a sibling fill MouseArea is created, so a point-limited strip at
// install time would miss it.
void stripPassThroughCursors(QQuickItem *item)
{
    if (!item)
        return;
    const QList<QQuickItem *> kids = item->childItems();
    for (QQuickItem *child : kids)
        stripPassThroughCursors(child);
    if (isPassThroughOverlay(item))
        item->unsetCursor();
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
        // HoverHandler is a QObject child, not a QQuickItem, so walking
        // parentItem() never sees it.
        if (hoverHandlerCursor(p, out))
            return true;
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
        window->installEventFilter(this);
        if (QQuickItem *content = window->contentItem())
            watchItem(content);
        // QQuickMouseArea::setCursor(Arrow) runs in the ctor *after*
        // ChildAdded. postEvent does not need Q_OBJECT (unlike
        // invokeMethod / QTimer::singleShot on this).
        scheduleApply();
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::User) {
            m_pending = false;
            applyCursor();
            return true;
        }
        return QObject::event(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::ChildAdded) {
            if (auto *item = qobject_cast<QQuickItem *>(
                    static_cast<QChildEvent *>(event)->child())) {
                watchItem(item);
                scheduleApply();
            }
            return false;
        }

        if (watched != m_window)
            return false;
        if (QGuiApplication::overrideCursor())
            return false;

        if (event->type() == QEvent::Leave) {
            m_left = true;
            scheduleApply();
            return false;
        }
        if (event->type() != QEvent::MouseMove && event->type() != QEvent::HoverMove)
            return false;

        m_left = false;
        m_havePos = true;
        if (event->type() == QEvent::MouseMove)
            m_pos = static_cast<QMouseEvent *>(event)->position();
        else
            m_pos = static_cast<QHoverEvent *>(event)->position();
        if (QQuickItem *content = m_window->contentItem())
            stripPassThroughCursors(content);
        scheduleApply();
        return false;
    }

private:
    void watchItem(QQuickItem *item)
    {
        if (!item || item->property("_cursorhand_childwatch").toBool())
            return;
        item->setProperty("_cursorhand_childwatch", true);
        item->installEventFilter(this);
        const QList<QQuickItem *> kids = item->childItems();
        for (QQuickItem *child : kids)
            watchItem(child);
    }

    void scheduleApply()
    {
        if (m_pending || !m_window)
            return;
        m_pending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::User));
    }

    void applyCursor()
    {
        if (!m_window || QGuiApplication::overrideCursor())
            return;
        QQuickItem *content = m_window->contentItem();
        if (content)
            stripPassThroughCursors(content);
        // Construction / ChildAdded only strip overlay Arrow. Do not
        // unsetCursor() with a dummy (0,0) pick — that clobbers a correct
        // HoverHandler hand Qt already applied.
        if (!m_havePos)
            return;
        if (m_left) {
            m_window->unsetCursor();
            return;
        }
        if (!content)
            return;
        QQuickItem *hit = pickThrough(content, content->mapFromScene(m_pos));
        QCursor cursor;
        if (resolveCursor(hit, &cursor))
            m_window->setCursor(cursor);
        else
            m_window->unsetCursor();
    }

    QPointer<QQuickWindow> m_window;
    QPointF m_pos;
    bool m_pending = false;
    bool m_left = false;
    bool m_havePos = false;
};

void installWatch(QQuickWindow *window)
{
    if (!window || window->property(kFilterProp).toBool())
        return;
    window->setProperty(kFilterProp, true);
    new CursorHandWindowFilter(window);
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
