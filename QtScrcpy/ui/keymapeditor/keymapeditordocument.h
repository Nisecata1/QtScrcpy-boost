#ifndef KEYMAPEDITORDOCUMENT_H
#define KEYMAPEDITORDOCUMENT_H

#include <QObject>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "../../QtScrcpyCore/src/device/controller/inputconvert/keymap/keymap.h"
#include "../../QtScrcpyCore/src/device/android/keycodes.h"

class KeymapEditorDocument : public QObject
{
    Q_OBJECT
public:
    enum NodeType {
        NodeUnknown = -1,
        NodeMouseMove = KeyMap::KMT_MOUSE_MOVE,
        NodeClick = KeyMap::KMT_CLICK,
        NodeClickTwice = KeyMap::KMT_CLICK_TWICE,
        NodeClickMulti = KeyMap::KMT_CLICK_MULTI,
        NodeSteerWheel = KeyMap::KMT_STEER_WHEEL,
        NodeDrag = KeyMap::KMT_DRAG,
        NodeAndroidKey = KeyMap::KMT_ANDROID_KEY
    };
    Q_ENUM(NodeType)

    enum HandleRole {
        HandlePrimaryPos = 0,
        HandleSecondaryPos,
        HandleSmallEyesPos
    };
    Q_ENUM(HandleRole)

    enum KeyBindingField {
        BindingPrimary = 0,
        BindingSteerLeft,
        BindingSteerRight,
        BindingSteerUp,
        BindingSteerDown
    };
    Q_ENUM(KeyBindingField)

    struct NodeInfo {
        int id = -1;
        NodeType type = NodeUnknown;
        QString typeName;
        QString displayName;
        QString comment;
        bool readOnly = false;
        bool rootNode = false;
        bool canDelete = false;
        bool supportsComment = false;
        bool supportsAlwaysActive = false;
        bool supportsPrimaryKey = false;
        bool supportsSwitchMap = false;
        bool supportsAndroidKey = false;
        bool supportsDirectionalKeys = false;
        bool alwaysActive = false;
        bool switchMap = false;
        QString primaryKey;
        QString leftKey;
        QString rightKey;
        QString upKey;
        QString downKey;
        int androidKey = AKEYCODE_UNKNOWN;
        bool hasPrimaryPos = false;
        bool hasSecondaryPos = false;
        bool hasSmallEyesPos = false;
        QPointF primaryPos;
        QPointF secondaryPos;
        QPointF smallEyesPos;
    };

    struct HandleInfo {
        int nodeId = -1;
        HandleRole role = HandlePrimaryPos;
        QPointF normalizedPos;
        bool movable = false;
        QString label;
        bool selected = false;
        bool readOnly = false;
    };

    explicit KeymapEditorDocument(QObject *parent = nullptr);

    bool loadFromJson(const QString &json, const QString &filePath, const QString &displayName, QString *errorString = nullptr);
    bool save(QString *errorString = nullptr);
    QString toJsonString() const;

    QString filePath() const;
    QString displayName() const;
    bool isDirty() const;
    bool hasLoadedDocument() const;

    QVector<NodeInfo> nodeInfos() const;
    QVector<HandleInfo> handleInfos(int selectedNodeId) const;
    NodeInfo nodeInfo(int nodeId) const;

    int createNode(NodeType type, const QPointF &anchorNormalized = QPointF(0.5, 0.5));
    bool deleteNode(int nodeId);

    bool setComment(int nodeId, const QString &comment);
    bool setAlwaysActive(int nodeId, bool enabled);
    bool setSwitchMap(int nodeId, bool enabled);
    bool setAndroidKey(int nodeId, int androidKey);
    bool setKeyBinding(int nodeId, KeyBindingField field, const QString &jsonKey);
    bool setHandlePosition(int nodeId, HandleRole role, const QPointF &normalizedPos);

signals:
    void documentReset();
    void nodeListChanged();
    void nodeChanged(int nodeId);
    void dirtyChanged(bool dirty);

private:
    struct NodeEntry {
        int id = -1;
        NodeType type = NodeUnknown;
        bool readOnly = true;
        bool rootNode = false;
        QJsonObject object;
    };

    void clear();
    void setDirty(bool dirty);
    static QPointF clampNormalized(const QPointF &point);
    static QJsonObject makePosObject(const QPointF &point);
    static QString nodeTypeName(NodeType type);
    static QString nodeDisplayName(const NodeEntry &entry);
    static NodeType parseNodeType(const QJsonObject &object);
    static bool supportsEditing(NodeType type);
    static bool supportsOverlayHandle(NodeType type);
    NodeEntry *findEntry(int nodeId);
    const NodeEntry *findEntry(int nodeId) const;
    NodeInfo buildNodeInfo(const NodeEntry &entry) const;
    void emitNodeChanged(NodeEntry *entry);

    QJsonObject m_rootObject;
    QVector<NodeEntry> m_entries;
    QString m_filePath;
    QString m_displayName;
    bool m_dirty = false;
    int m_nextNodeId = 1;
};

#endif // KEYMAPEDITORDOCUMENT_H
