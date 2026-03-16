#include "keymapeditordocument.h"

#include <cmath>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include "keymapkeycodec.h"

namespace {
QString primaryJsonKeyForNode(const QJsonObject &object)
{
    return object.value(QStringLiteral("key")).toString();
}

double roundNormalizedComponent(double value)
{
    constexpr double kNormalizedPrecision = 1000000.0;
    const double clamped = qBound(0.0, value, 1.0);
    return std::round(clamped * kNormalizedPrecision) / kNormalizedPrecision;
}
}

KeymapEditorDocument::KeymapEditorDocument(QObject *parent)
    : QObject(parent)
{
}

bool KeymapEditorDocument::loadFromJson(const QString &json, const QString &filePath, const QString &displayName, QString *errorString)
{
    clear();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorString) {
            *errorString = parseError.error != QJsonParseError::NoError
                ? parseError.errorString()
                : QStringLiteral("Root JSON must be an object");
        }
        return false;
    }

    m_rootObject = document.object();
    m_filePath = filePath;
    m_displayName = displayName;

    if (m_rootObject.contains(QStringLiteral("mouseMoveMap")) && m_rootObject.value(QStringLiteral("mouseMoveMap")).isObject()) {
        NodeEntry entry;
        entry.id = m_nextNodeId++;
        entry.rootNode = true;
        entry.object = m_rootObject.value(QStringLiteral("mouseMoveMap")).toObject();
        entry.type = parseNodeType(entry.object);
        if (entry.type == NodeUnknown) {
            entry.type = NodeMouseMove;
        }
        entry.readOnly = false;
        m_entries.push_back(entry);
    }

    const QJsonArray nodes = m_rootObject.value(QStringLiteral("keyMapNodes")).toArray();
    for (int index = 0; index < nodes.size(); ++index) {
        if (!nodes.at(index).isObject()) {
            continue;
        }
        NodeEntry entry;
        entry.id = m_nextNodeId++;
        entry.object = nodes.at(index).toObject();
        entry.type = parseNodeType(entry.object);
        entry.readOnly = !supportsEditing(entry.type);
        m_entries.push_back(entry);
    }

    m_dirty = false;
    emit documentReset();
    emit nodeListChanged();
    emit dirtyChanged(false);
    return true;
}

bool KeymapEditorDocument::save(QString *errorString)
{
    if (m_filePath.isEmpty()) {
        if (errorString) {
            *errorString = QStringLiteral("Script file path is empty");
        }
        return false;
    }

    QSaveFile saveFile(m_filePath);
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorString) {
            *errorString = saveFile.errorString();
        }
        return false;
    }

    const QByteArray encoded = toJsonString().toUtf8();
    if (saveFile.write(encoded) != encoded.size()) {
        if (errorString) {
            *errorString = saveFile.errorString();
        }
        return false;
    }

    if (!saveFile.commit()) {
        if (errorString) {
            *errorString = saveFile.errorString();
        }
        return false;
    }

    setDirty(false);
    return true;
}

QString KeymapEditorDocument::toJsonString() const
{
    QJsonObject root = m_rootObject;
    bool hasMouseMove = false;
    QJsonArray keyMapNodes;

    for (int i = 0; i < m_entries.size(); ++i) {
        const NodeEntry &entry = m_entries.at(i);
        if (entry.rootNode) {
            root.insert(QStringLiteral("mouseMoveMap"), entry.object);
            hasMouseMove = true;
        } else {
            keyMapNodes.push_back(entry.object);
        }
    }

    if (!hasMouseMove) {
        root.remove(QStringLiteral("mouseMoveMap"));
    }
    root.insert(QStringLiteral("keyMapNodes"), keyMapNodes);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString KeymapEditorDocument::filePath() const
{
    return m_filePath;
}

QString KeymapEditorDocument::displayName() const
{
    return m_displayName;
}

bool KeymapEditorDocument::isDirty() const
{
    return m_dirty;
}

bool KeymapEditorDocument::hasLoadedDocument() const
{
    return !m_filePath.isEmpty() || !m_entries.isEmpty() || !m_rootObject.isEmpty();
}

QVector<KeymapEditorDocument::NodeInfo> KeymapEditorDocument::nodeInfos() const
{
    QVector<NodeInfo> nodes;
    nodes.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        nodes.push_back(buildNodeInfo(m_entries.at(i)));
    }
    return nodes;
}

QVector<KeymapEditorDocument::HandleInfo> KeymapEditorDocument::handleInfos(int selectedNodeId) const
{
    QVector<HandleInfo> handles;
    for (int i = 0; i < m_entries.size(); ++i) {
        const NodeInfo info = buildNodeInfo(m_entries.at(i));
        if (info.hasPrimaryPos) {
            HandleInfo handle;
            handle.nodeId = info.id;
            handle.role = HandlePrimaryPos;
            handle.normalizedPos = info.primaryPos;
            handle.movable = !info.readOnly;
            handle.readOnly = info.readOnly;
            handle.selected = info.id == selectedNodeId;
            handle.label = info.rootNode ? QStringLiteral("look") : info.displayName;
            handles.push_back(handle);
        }
        if (info.hasSecondaryPos) {
            HandleInfo handle;
            handle.nodeId = info.id;
            handle.role = HandleSecondaryPos;
            handle.normalizedPos = info.secondaryPos;
            handle.movable = !info.readOnly;
            handle.readOnly = info.readOnly;
            handle.selected = info.id == selectedNodeId;
            handle.label = QStringLiteral("end");
            handles.push_back(handle);
        }
        if (info.hasSmallEyesPos) {
            HandleInfo handle;
            handle.nodeId = info.id;
            handle.role = HandleSmallEyesPos;
            handle.normalizedPos = info.smallEyesPos;
            handle.movable = !info.readOnly;
            handle.readOnly = info.readOnly;
            handle.selected = info.id == selectedNodeId;
            handle.label = QStringLiteral("smallEyes");
            handles.push_back(handle);
        }
    }
    return handles;
}

KeymapEditorDocument::NodeInfo KeymapEditorDocument::nodeInfo(int nodeId) const
{
    const NodeEntry *entry = findEntry(nodeId);
    return entry ? buildNodeInfo(*entry) : NodeInfo();
}

int KeymapEditorDocument::createNode(NodeType type, const QPointF &anchorNormalized)
{
    if (type == NodeUnknown || type == NodeClickTwice || type == NodeClickMulti) {
        return -1;
    }

    const QPointF anchor = clampNormalized(anchorNormalized);
    NodeEntry entry;
    entry.id = m_nextNodeId++;
    entry.type = type;
    entry.readOnly = false;

    switch (type) {
    case NodeMouseMove:
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries.at(i).rootNode) {
                return m_entries.at(i).id;
            }
        }
        entry.rootNode = true;
        entry.object.insert(QStringLiteral("type"), QStringLiteral("KMT_MOUSE_MOVE"));
        entry.object.insert(QStringLiteral("speedRatioX"), 3.0);
        entry.object.insert(QStringLiteral("speedRatioY"), 2.0);
        entry.object.insert(QStringLiteral("startPos"), makePosObject(anchor));
        m_entries.prepend(entry);
        break;
    case NodeClick:
        entry.object.insert(QStringLiteral("type"), QStringLiteral("KMT_CLICK"));
        entry.object.insert(QStringLiteral("key"), QStringLiteral("Key_F1"));
        entry.object.insert(QStringLiteral("pos"), makePosObject(anchor));
        entry.object.insert(QStringLiteral("switchMap"), false);
        m_entries.push_back(entry);
        break;
    case NodeSteerWheel:
        entry.object.insert(QStringLiteral("type"), QStringLiteral("KMT_STEER_WHEEL"));
        entry.object.insert(QStringLiteral("centerPos"), makePosObject(anchor));
        entry.object.insert(QStringLiteral("leftKey"), QStringLiteral("Key_A"));
        entry.object.insert(QStringLiteral("rightKey"), QStringLiteral("Key_D"));
        entry.object.insert(QStringLiteral("upKey"), QStringLiteral("Key_W"));
        entry.object.insert(QStringLiteral("downKey"), QStringLiteral("Key_S"));
        entry.object.insert(QStringLiteral("leftOffset"), 0.1);
        entry.object.insert(QStringLiteral("rightOffset"), 0.1);
        entry.object.insert(QStringLiteral("upOffset"), 0.1);
        entry.object.insert(QStringLiteral("downOffset"), 0.1);
        m_entries.push_back(entry);
        break;
    case NodeDrag:
        entry.object.insert(QStringLiteral("type"), QStringLiteral("KMT_DRAG"));
        entry.object.insert(QStringLiteral("key"), QStringLiteral("Key_F2"));
        entry.object.insert(QStringLiteral("startPos"), makePosObject(anchor));
        entry.object.insert(QStringLiteral("endPos"), makePosObject(clampNormalized(anchor + QPointF(0.08, 0.0))));
        m_entries.push_back(entry);
        break;
    case NodeAndroidKey:
        entry.object.insert(QStringLiteral("type"), QStringLiteral("KMT_ANDROID_KEY"));
        entry.object.insert(QStringLiteral("key"), QStringLiteral("Key_F3"));
        entry.object.insert(QStringLiteral("androidKey"), static_cast<int>(AKEYCODE_BACK));
        m_entries.push_back(entry);
        break;
    default:
        return -1;
    }

    setDirty(true);
    emit nodeListChanged();
    emit nodeChanged(entry.id);
    return entry.id;
}

bool KeymapEditorDocument::deleteNode(int nodeId)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id != nodeId) {
            continue;
        }
        m_entries.remove(i);
        setDirty(true);
        emit nodeListChanged();
        return true;
    }
    return false;
}

bool KeymapEditorDocument::setComment(int nodeId, const QString &comment)
{
    NodeEntry *entry = findEntry(nodeId);
    if (!entry || entry->readOnly) {
        return false;
    }
    if (comment.trimmed().isEmpty()) {
        entry->object.remove(QStringLiteral("comment"));
    } else {
        entry->object.insert(QStringLiteral("comment"), comment);
    }
    emitNodeChanged(entry);
    emit nodeListChanged();
    return true;
}

bool KeymapEditorDocument::setAlwaysActive(int nodeId, bool enabled)
{
    NodeEntry *entry = findEntry(nodeId);
    if (!entry || entry->readOnly) {
        return false;
    }
    if (enabled) {
        entry->object.insert(QStringLiteral("alwaysActive"), true);
    } else {
        entry->object.remove(QStringLiteral("alwaysActive"));
    }
    emitNodeChanged(entry);
    return true;
}

bool KeymapEditorDocument::setSwitchMap(int nodeId, bool enabled)
{
    NodeEntry *entry = findEntry(nodeId);
    if (!entry || entry->readOnly || entry->type != NodeClick) {
        return false;
    }
    entry->object.insert(QStringLiteral("switchMap"), enabled);
    emitNodeChanged(entry);
    return true;
}

bool KeymapEditorDocument::setAndroidKey(int nodeId, int androidKey)
{
    NodeEntry *entry = findEntry(nodeId);
    if (!entry || entry->readOnly || entry->type != NodeAndroidKey) {
        return false;
    }
    entry->object.insert(QStringLiteral("androidKey"), androidKey);
    emitNodeChanged(entry);
    return true;
}

bool KeymapEditorDocument::setKeyBinding(int nodeId, KeyBindingField field, const QString &jsonKey)
{
    NodeEntry *entry = findEntry(nodeId);
    if (!entry || entry->readOnly) {
        return false;
    }

    KeyMap::ActionType type = KeyMap::AT_INVALID;
    int value = 0;
    if (!KeymapKeyCodec::decodeJsonKey(jsonKey, type, value)) {
        return false;
    }
    Q_UNUSED(type)
    Q_UNUSED(value)

    switch (field) {
    case BindingPrimary:
        entry->object.insert(QStringLiteral("key"), jsonKey);
        break;
    case BindingSteerLeft:
        if (entry->type != NodeSteerWheel) {
            return false;
        }
        entry->object.insert(QStringLiteral("leftKey"), jsonKey);
        break;
    case BindingSteerRight:
        if (entry->type != NodeSteerWheel) {
            return false;
        }
        entry->object.insert(QStringLiteral("rightKey"), jsonKey);
        break;
    case BindingSteerUp:
        if (entry->type != NodeSteerWheel) {
            return false;
        }
        entry->object.insert(QStringLiteral("upKey"), jsonKey);
        break;
    case BindingSteerDown:
        if (entry->type != NodeSteerWheel) {
            return false;
        }
        entry->object.insert(QStringLiteral("downKey"), jsonKey);
        break;
    default:
        return false;
    }

    emitNodeChanged(entry);
    emit nodeListChanged();
    return true;
}

bool KeymapEditorDocument::setHandlePosition(int nodeId, HandleRole role, const QPointF &normalizedPos)
{
    NodeEntry *entry = findEntry(nodeId);
    if (!entry || entry->readOnly) {
        return false;
    }

    const QPointF clamped = clampNormalized(normalizedPos);
    switch (entry->type) {
    case NodeMouseMove:
        if (role == HandlePrimaryPos) {
            entry->object.insert(QStringLiteral("startPos"), makePosObject(clamped));
        } else if (role == HandleSmallEyesPos) {
            QJsonObject smallEyes = entry->object.value(QStringLiteral("smallEyes")).toObject();
            if (smallEyes.isEmpty()) {
                return false;
            }
            smallEyes.insert(QStringLiteral("pos"), makePosObject(clamped));
            entry->object.insert(QStringLiteral("smallEyes"), smallEyes);
        } else {
            return false;
        }
        break;
    case NodeClick:
    case NodeClickTwice:
        if (role != HandlePrimaryPos) {
            return false;
        }
        entry->object.insert(QStringLiteral("pos"), makePosObject(clamped));
        break;
    case NodeDrag:
        if (role == HandlePrimaryPos) {
            entry->object.insert(QStringLiteral("startPos"), makePosObject(clamped));
        } else if (role == HandleSecondaryPos) {
            entry->object.insert(QStringLiteral("endPos"), makePosObject(clamped));
        } else {
            return false;
        }
        break;
    case NodeSteerWheel:
        if (role != HandlePrimaryPos) {
            return false;
        }
        entry->object.insert(QStringLiteral("centerPos"), makePosObject(clamped));
        break;
    default:
        return false;
    }

    emitNodeChanged(entry);
    return true;
}

void KeymapEditorDocument::clear()
{
    m_rootObject = QJsonObject();
    m_entries.clear();
    m_filePath.clear();
    m_displayName.clear();
    m_dirty = false;
    m_nextNodeId = 1;
}

void KeymapEditorDocument::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        return;
    }
    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}

QPointF KeymapEditorDocument::clampNormalized(const QPointF &point)
{
    return QPointF(qBound(0.0, point.x(), 1.0), qBound(0.0, point.y(), 1.0));
}

QJsonObject KeymapEditorDocument::makePosObject(const QPointF &point)
{
    const QPointF clamped = clampNormalized(point);
    QJsonObject pos;
    pos.insert(QStringLiteral("x"), roundNormalizedComponent(clamped.x()));
    pos.insert(QStringLiteral("y"), roundNormalizedComponent(clamped.y()));
    return pos;
}

QString KeymapEditorDocument::nodeTypeName(NodeType type)
{
    switch (type) {
    case NodeMouseMove:
        return QStringLiteral("KMT_MOUSE_MOVE");
    case NodeClick:
        return QStringLiteral("KMT_CLICK");
    case NodeClickTwice:
        return QStringLiteral("KMT_CLICK_TWICE");
    case NodeClickMulti:
        return QStringLiteral("KMT_CLICK_MULTI");
    case NodeSteerWheel:
        return QStringLiteral("KMT_STEER_WHEEL");
    case NodeDrag:
        return QStringLiteral("KMT_DRAG");
    case NodeAndroidKey:
        return QStringLiteral("KMT_ANDROID_KEY");
    default:
        return QStringLiteral("UNKNOWN");
    }
}

QString KeymapEditorDocument::nodeDisplayName(const NodeEntry &entry)
{
    const QString comment = entry.object.value(QStringLiteral("comment")).toString().trimmed();
    if (!comment.isEmpty()) {
        return comment;
    }

    if (entry.rootNode) {
        return QStringLiteral("Mouse Look");
    }

    const QString key = primaryJsonKeyForNode(entry.object);
    if (!key.isEmpty()) {
        return KeymapKeyCodec::displayStringForJsonKey(key);
    }

    return nodeTypeName(entry.type);
}

KeymapEditorDocument::NodeType KeymapEditorDocument::parseNodeType(const QJsonObject &object)
{
    const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("KMT_MOUSE_MOVE")) {
        return NodeMouseMove;
    }
    if (type == QStringLiteral("KMT_CLICK")) {
        return NodeClick;
    }
    if (type == QStringLiteral("KMT_CLICK_TWICE")) {
        return NodeClickTwice;
    }
    if (type == QStringLiteral("KMT_CLICK_MULTI")) {
        return NodeClickMulti;
    }
    if (type == QStringLiteral("KMT_STEER_WHEEL")) {
        return NodeSteerWheel;
    }
    if (type == QStringLiteral("KMT_DRAG")) {
        return NodeDrag;
    }
    if (type == QStringLiteral("KMT_ANDROID_KEY")) {
        return NodeAndroidKey;
    }
    return NodeUnknown;
}

bool KeymapEditorDocument::supportsEditing(NodeType type)
{
    switch (type) {
    case NodeMouseMove:
    case NodeClick:
    case NodeSteerWheel:
    case NodeDrag:
    case NodeAndroidKey:
        return true;
    default:
        return false;
    }
}

bool KeymapEditorDocument::supportsOverlayHandle(NodeType type)
{
    switch (type) {
    case NodeMouseMove:
    case NodeClick:
    case NodeClickTwice:
    case NodeDrag:
    case NodeSteerWheel:
        return true;
    default:
        return false;
    }
}

KeymapEditorDocument::NodeEntry *KeymapEditorDocument::findEntry(int nodeId)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == nodeId) {
            return &m_entries[i];
        }
    }
    return nullptr;
}

const KeymapEditorDocument::NodeEntry *KeymapEditorDocument::findEntry(int nodeId) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == nodeId) {
            return &m_entries.at(i);
        }
    }
    return nullptr;
}

KeymapEditorDocument::NodeInfo KeymapEditorDocument::buildNodeInfo(const NodeEntry &entry) const
{
    NodeInfo info;
    info.id = entry.id;
    info.type = entry.type;
    info.typeName = nodeTypeName(entry.type);
    info.displayName = nodeDisplayName(entry);
    info.comment = entry.object.value(QStringLiteral("comment")).toString();
    info.readOnly = entry.readOnly;
    info.rootNode = entry.rootNode;
    info.canDelete = !entry.readOnly;

    switch (entry.type) {
    case NodeMouseMove:
        info.hasPrimaryPos = true;
        info.primaryPos = QPointF(entry.object.value(QStringLiteral("startPos")).toObject().value(QStringLiteral("x")).toDouble(),
                                  entry.object.value(QStringLiteral("startPos")).toObject().value(QStringLiteral("y")).toDouble());
        if (entry.object.contains(QStringLiteral("smallEyes")) && entry.object.value(QStringLiteral("smallEyes")).isObject()) {
            const QJsonObject smallEyes = entry.object.value(QStringLiteral("smallEyes")).toObject();
            if (smallEyes.contains(QStringLiteral("pos")) && smallEyes.value(QStringLiteral("pos")).isObject()) {
                const QJsonObject pos = smallEyes.value(QStringLiteral("pos")).toObject();
                info.hasSmallEyesPos = true;
                info.smallEyesPos = QPointF(pos.value(QStringLiteral("x")).toDouble(), pos.value(QStringLiteral("y")).toDouble());
            }
        }
        break;
    case NodeClick:
    case NodeClickTwice:
        info.supportsComment = true;
        info.supportsAlwaysActive = entry.type == NodeClick;
        info.supportsPrimaryKey = true;
        info.supportsSwitchMap = entry.type == NodeClick;
        info.primaryKey = primaryJsonKeyForNode(entry.object);
        info.alwaysActive = entry.object.value(QStringLiteral("alwaysActive")).toBool(false);
        info.switchMap = entry.object.value(QStringLiteral("switchMap")).toBool(false);
        if (entry.object.contains(QStringLiteral("pos")) && entry.object.value(QStringLiteral("pos")).isObject()) {
            const QJsonObject pos = entry.object.value(QStringLiteral("pos")).toObject();
            info.hasPrimaryPos = true;
            info.primaryPos = QPointF(pos.value(QStringLiteral("x")).toDouble(), pos.value(QStringLiteral("y")).toDouble());
        }
        break;
    case NodeClickMulti:
        info.supportsComment = true;
        info.supportsPrimaryKey = true;
        info.primaryKey = primaryJsonKeyForNode(entry.object);
        if (entry.object.contains(QStringLiteral("clickNodes")) && entry.object.value(QStringLiteral("clickNodes")).isArray()) {
            const QJsonArray clickNodes = entry.object.value(QStringLiteral("clickNodes")).toArray();
            if (!clickNodes.isEmpty() && clickNodes.at(0).isObject()) {
                const QJsonObject pos = clickNodes.at(0).toObject().value(QStringLiteral("pos")).toObject();
                info.hasPrimaryPos = true;
                info.primaryPos = QPointF(pos.value(QStringLiteral("x")).toDouble(), pos.value(QStringLiteral("y")).toDouble());
            }
        }
        break;
    case NodeSteerWheel:
        info.supportsComment = true;
        info.supportsAlwaysActive = true;
        info.supportsDirectionalKeys = true;
        info.alwaysActive = entry.object.value(QStringLiteral("alwaysActive")).toBool(false);
        info.leftKey = entry.object.value(QStringLiteral("leftKey")).toString();
        info.rightKey = entry.object.value(QStringLiteral("rightKey")).toString();
        info.upKey = entry.object.value(QStringLiteral("upKey")).toString();
        info.downKey = entry.object.value(QStringLiteral("downKey")).toString();
        if (entry.object.contains(QStringLiteral("centerPos")) && entry.object.value(QStringLiteral("centerPos")).isObject()) {
            const QJsonObject pos = entry.object.value(QStringLiteral("centerPos")).toObject();
            info.hasPrimaryPos = true;
            info.primaryPos = QPointF(pos.value(QStringLiteral("x")).toDouble(), pos.value(QStringLiteral("y")).toDouble());
        }
        break;
    case NodeDrag:
        info.supportsComment = true;
        info.supportsAlwaysActive = true;
        info.supportsPrimaryKey = true;
        info.primaryKey = primaryJsonKeyForNode(entry.object);
        info.alwaysActive = entry.object.value(QStringLiteral("alwaysActive")).toBool(false);
        if (entry.object.contains(QStringLiteral("startPos")) && entry.object.value(QStringLiteral("startPos")).isObject()) {
            const QJsonObject pos = entry.object.value(QStringLiteral("startPos")).toObject();
            info.hasPrimaryPos = true;
            info.primaryPos = QPointF(pos.value(QStringLiteral("x")).toDouble(), pos.value(QStringLiteral("y")).toDouble());
        }
        if (entry.object.contains(QStringLiteral("endPos")) && entry.object.value(QStringLiteral("endPos")).isObject()) {
            const QJsonObject pos = entry.object.value(QStringLiteral("endPos")).toObject();
            info.hasSecondaryPos = true;
            info.secondaryPos = QPointF(pos.value(QStringLiteral("x")).toDouble(), pos.value(QStringLiteral("y")).toDouble());
        }
        break;
    case NodeAndroidKey:
        info.supportsComment = true;
        info.supportsAlwaysActive = true;
        info.supportsPrimaryKey = true;
        info.supportsAndroidKey = true;
        info.primaryKey = primaryJsonKeyForNode(entry.object);
        info.alwaysActive = entry.object.value(QStringLiteral("alwaysActive")).toBool(false);
        info.androidKey = entry.object.value(QStringLiteral("androidKey")).toInt(AKEYCODE_UNKNOWN);
        break;
    default:
        break;
    }

    return info;
}

void KeymapEditorDocument::emitNodeChanged(NodeEntry *entry)
{
    if (!entry) {
        return;
    }
    setDirty(true);
    emit nodeChanged(entry->id);
}
