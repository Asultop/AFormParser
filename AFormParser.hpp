#pragma once

#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

#include "LuaRuntime.hpp"

namespace AFormParser {

enum class NodeKind {
	Unknown,
	Form,
	Group,
	Field,
	KeyBind,
	MustField,
	TextField,
	LineField,
	Arg,
	OptionField,
	Option,
	Scripts
};

QString nodeKindToString(NodeKind kind);

struct ParseError {
	int line = -1;
	int column = -1;
	QString message;

	bool hasError() const { return !message.isEmpty(); }
};

class Node;
class FormNode;
class GroupNode;
class FieldNode;
class KeyBindNode;
class MustFieldNode;
class TextFieldNode;
class LineFieldNode;
class ArgNode;
class OptionFieldNode;
class OptionNode;
class ScriptsNode;
class Document;

using NodePtr = std::shared_ptr<Node>;

class Node : public std::enable_shared_from_this<Node> {
public:
	virtual ~Node() = default;

	NodeKind kind() const;
	bool is(NodeKind target) const;

	virtual QString dump(int indent = 0) const = 0;
	virtual QString toCFG() const;

	virtual std::shared_ptr<FormNode> toForm();
	virtual std::shared_ptr<GroupNode> toGroup();
	virtual std::shared_ptr<FieldNode> toField();
	virtual std::shared_ptr<KeyBindNode> toKeyBind();
	virtual std::shared_ptr<MustFieldNode> toMustField();
	virtual std::shared_ptr<TextFieldNode> toTextField();
	virtual std::shared_ptr<LineFieldNode> toLineField();
	virtual std::shared_ptr<ArgNode> toArg();
	virtual std::shared_ptr<OptionFieldNode> toOptionField();
	virtual std::shared_ptr<OptionNode> toOption();
	virtual std::shared_ptr<ScriptsNode> toScripts();

	template <typename T>
	std::shared_ptr<T> to() {
		return std::dynamic_pointer_cast<T>(shared_from_this());
	}

	template <typename T>
	static std::shared_ptr<T> from(const NodePtr &node) {
		return std::dynamic_pointer_cast<T>(node);
	}

	QMap<QString, QString> extraProperties;

protected:
	explicit Node(NodeKind nodeKind);

private:
	NodeKind kind_ = NodeKind::Unknown;
};

class FieldNode : public Node {
public:
	using Ptr = std::shared_ptr<FieldNode>;

	static Ptr from(const NodePtr &node);
	std::shared_ptr<FieldNode> toField() override;

	QString id;
	QString enabledExpression = QStringLiteral("true");
	QString description;
	QString subDescription;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

protected:
	explicit FieldNode(NodeKind kind);
	QString dumpCommonProperties(int indent) const;
};

class KeyBindNode final : public FieldNode {
public:
	using Ptr = std::shared_ptr<KeyBindNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<KeyBindNode> toKeyBind() override;

	QString command;
	QString bind = QStringLiteral("None");

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	KeyBindNode();
};

class MustFieldNode final : public FieldNode {
public:
	using Ptr = std::shared_ptr<MustFieldNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<MustFieldNode> toMustField() override;

	QString command;
	QString bind = QStringLiteral("None");

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	MustFieldNode();
};

class TextFieldNode final : public FieldNode {
public:
	using Ptr = std::shared_ptr<TextFieldNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<TextFieldNode> toTextField() override;

	QString text;
	QString command;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	TextFieldNode();
};

class ArgNode final : public Node {
public:
	using Ptr = std::shared_ptr<ArgNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<ArgNode> toArg() override;

	QString id;
	QString description;
	QString value;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	ArgNode();
};

class LineFieldNode final : public FieldNode {
public:
	using Ptr = std::shared_ptr<LineFieldNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<LineFieldNode> toLineField() override;

	QVector<ArgNode::Ptr> args;
	QString expression;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	LineFieldNode();
};

class OptionNode final : public Node {
public:
	using Ptr = std::shared_ptr<OptionNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<OptionNode> toOption() override;

	QString id;
	QString description;
	QString command;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	OptionNode();
};

class OptionFieldNode final : public FieldNode {
public:
	using Ptr = std::shared_ptr<OptionFieldNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<OptionFieldNode> toOptionField() override;

	QVector<OptionNode::Ptr> options;
	QString selected;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	OptionFieldNode();
};

class GroupNode final : public Node {
public:
	using Ptr = std::shared_ptr<GroupNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<GroupNode> toGroup() override;

	QString title;
	QVector<FieldNode::Ptr> fields;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	GroupNode();
};

class FormNode final : public Node {
public:
	using Ptr = std::shared_ptr<FormNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<FormNode> toForm() override;

	QString id;
	QVector<GroupNode::Ptr> groups;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	FormNode();
};

class ScriptsNode final : public Node {
public:
	using Ptr = std::shared_ptr<ScriptsNode>;

	static Ptr create();
	static Ptr from(const NodePtr &node);
	std::shared_ptr<ScriptsNode> toScripts() override;

	QString rawCode;

	QString dump(int indent = 0) const override;
	QString toCFG() const override;

private:
	ScriptsNode();
};

class Document : public std::enable_shared_from_this<Document> {
public:
    using Ptr = std::shared_ptr<Document>;
    using LuaFunction = std::function<QString(QStringList)>;

    static Ptr create();
    static Ptr from(const QString &formText, ParseError *error = nullptr);

    bool parse(const QString &formText, ParseError *error = nullptr);
    QString dump() const;
    QString toCFG() const;

    void clear();

    QString metaValue(const QString &key) const;
    void setMetaValue(const QString &key, const QString &value);
    QVector<QPair<QString, QString>> metaEntries() const;

    QVector<NodePtr> allNodes() const;
    NodePtr findById(const QString &id) const;

    void registerFunction(const QString &name, LuaFunction func);
    void registerGlobalVariable(const QString &name, const QString &value);
    void setGlobalVariables(const QMap<QString, QString> &vars);
    QMap<QString, QString> globalVariables() const { return globalVars_; }
    QString executeFunction(const QString &fnName, const QStringList &args, QString *error = nullptr) const;
    QString executeScriptFunction(const QString &fnName, const QStringList &args, QString *error = nullptr) const;

    FormNode::Ptr form;
    ScriptsNode::Ptr scripts;

private:
    QVector<QPair<QString, QString>> meta_;
    mutable std::shared_ptr<LuaRuntime> luaRuntime_;
    mutable bool luaScriptLoaded_ = false;
    QMap<QString, QString> globalVars_;
};

} // namespace AFormParser

