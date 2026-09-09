#ifndef CURSORHAND_H
#define CURSORHAND_H

#include <QCursor>
#include <QObject>
#include <QUrl>
#include <QtQml/qqml.h>

class QQmlEngine;
class QJSEngine;

class CursorHandAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(Qt::CursorShape shape READ shape WRITE setShape NOTIFY shapeChanged FINAL)
    Q_PROPERTY(bool hovered READ hovered NOTIFY hoveredChanged FINAL)
    QML_ANONYMOUS

public:
    explicit CursorHandAttached(QObject *parent = nullptr);
    ~CursorHandAttached() override;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    Qt::CursorShape shape() const { return m_shape; }
    void setShape(Qt::CursorShape shape);

    bool hovered() const { return m_hovered; }

signals:
    void enabledChanged();
    void shapeChanged();
    void hoveredChanged();

private slots:
    void syncHovered();

private:
    void apply();
    void ensureHandler();
    void clearItemCursor();

    bool m_enabled = true;
    bool m_hovered = false;
    Qt::CursorShape m_shape = Qt::PointingHandCursor;
    QObject *m_handler = nullptr;
};

class CursorHand : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CursorHand)
    QML_ATTACHED(CursorHandAttached)
    QML_SINGLETON

public:
    explicit CursorHand(QObject *parent = nullptr);

    static CursorHand *create(QQmlEngine *, QJSEngine *);
    static CursorHandAttached *qmlAttachedProperties(QObject *object);

    Q_INVOKABLE static void setOverride(Qt::CursorShape shape);
    Q_INVOKABLE static void setPixmapOverride(const QUrl &url, int hotX = -1, int hotY = -1);
    Q_INVOKABLE static void restoreOverride();
};

#endif
