#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>

#include <functional>

#include "AFormParser/AFormParser.hpp"

namespace {

QString fieldTypeName(const AFormParser::FieldNode::Ptr &field)
{
    if (!field) {
        return QStringLiteral("Field");
    }
    if (field->toKeyBind()) {
        return QStringLiteral("KeyBind");
    }
    if (field->toMustField()) {
        return QStringLiteral("MustField");
    }
    if (field->toTextField()) {
        return QStringLiteral("TextField");
    }
    if (field->toLineField()) {
        return QStringLiteral("LineField");
    }
    if (field->toOptionField()) {
        return QStringLiteral("OptionField");
    }
    return QStringLiteral("Field");
}

QString safeSummary(const QString &value, int maxLen = 42)
{
    QString s = value;
    s.replace(QLatin1Char('\n'), QStringLiteral(" "));
    if (s.size() > maxLen) {
        s = s.left(maxLen) + QStringLiteral("...");
    }
    return s;
}

class FormTreeViewer final : public QWidget {
public:
    explicit FormTreeViewer(const QString &defaultFormPath, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle(QStringLiteral("AFormParser - Node Tree Viewer"));
        resize(1400, 880);

        auto *mainLayout = new QVBoxLayout(this);
        auto *toolbar = new QHBoxLayout();

        fileEdit_ = new QLineEdit(this);
        fileEdit_->setPlaceholderText(QStringLiteral("Form 文件路径"));
        fileEdit_->setText(defaultFormPath);

        auto *browseBtn = new QPushButton(QStringLiteral("浏览"), this);
        auto *loadBtn = new QPushButton(QStringLiteral("加载"), this);

        toolbar->addWidget(new QLabel(QStringLiteral("Form:"), this));
        toolbar->addWidget(fileEdit_, 1);
        toolbar->addWidget(browseBtn);
        toolbar->addWidget(loadBtn);

        auto *globalVarsBtn = new QPushButton(QStringLiteral("全局变量表"), this);
        toolbar->addWidget(globalVarsBtn);

        statusLabel_ = new QLabel(QStringLiteral("等待加载"), this);
        statusLabel_->setWordWrap(true);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);

        auto *leftSplitter = new QSplitter(Qt::Vertical, splitter);
        leftSplitter->setChildrenCollapsible(false);

        auto *previewBox = new QGroupBox(QStringLiteral("控件预览"), leftSplitter);
        auto *previewBoxLayout = new QVBoxLayout(previewBox);
        previewScroll_ = new QScrollArea(previewBox);
        previewScroll_->setWidgetResizable(true);
        previewBoxLayout->addWidget(previewScroll_);

        auto *treeBox = new QGroupBox(QStringLiteral("节点树"), leftSplitter);
        auto *treeBoxLayout = new QVBoxLayout(treeBox);
        tree_ = new QTreeWidget(treeBox);
        tree_->setColumnCount(2);
        tree_->setHeaderLabels(QStringList{QStringLiteral("节点/属性"), QStringLiteral("值")});
        tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        tree_->setAlternatingRowColors(true);
        treeBoxLayout->addWidget(tree_);

        auto *rightPane = new QWidget(splitter);
        auto *rightLayout = new QVBoxLayout(rightPane);

        auto *dumpBox = new QGroupBox(QStringLiteral("Dump (Form)"), rightPane);
        auto *dumpLayout = new QVBoxLayout(dumpBox);
        dumpEdit_ = new QPlainTextEdit(dumpBox);
        dumpEdit_->setReadOnly(true);
        dumpEdit_->setPlaceholderText(QStringLiteral("节点树变更后将实时刷新 Dump"));

        auto *cfgBox = new QGroupBox(QStringLiteral("toCFG"), rightPane);
        auto *cfgLayout = new QVBoxLayout(cfgBox);
        cfgEdit_ = new QPlainTextEdit(cfgBox);
        cfgEdit_->setReadOnly(true);
        cfgEdit_->setPlaceholderText(QStringLiteral("节点树变更后将实时刷新 toCFG"));

        const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        dumpEdit_->setFont(mono);
        cfgEdit_->setFont(mono);

        dumpLayout->addWidget(dumpEdit_);
        cfgLayout->addWidget(cfgEdit_);
        rightLayout->addWidget(dumpBox, 1);
        rightLayout->addWidget(cfgBox, 1);

        splitter->addWidget(leftSplitter);
        splitter->addWidget(rightPane);
        splitter->setStretchFactor(0, 6);
        splitter->setStretchFactor(1, 5);

        leftSplitter->setStretchFactor(0, 6);
        leftSplitter->setStretchFactor(1, 4);

        mainLayout->addLayout(toolbar);
        mainLayout->addWidget(splitter, 1);
        mainLayout->addWidget(statusLabel_);

        connect(browseBtn, &QPushButton::clicked, this, [this]() {
            const QString selected = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择 Form 文件"),
                fileEdit_->text().isEmpty() ? QStringLiteral(".") : fileEdit_->text(),
                QStringLiteral("ASUL Form (*.asul);;All Files (*.*)"));
            if (!selected.isEmpty()) {
                fileEdit_->setText(selected);
            }
        });

        connect(loadBtn, &QPushButton::clicked, this, [this]() {
            loadFromFile(fileEdit_->text().trimmed());
        });

        connect(globalVarsBtn, &QPushButton::clicked, this, [this]() {
            showGlobalVarsDialog();
        });

        connect(fileEdit_, &QLineEdit::returnPressed, this, [this]() {
            loadFromFile(fileEdit_->text().trimmed());
        });

        connect(tree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
            if (rebuilding_ || column != 1 || !item) {
                return;
            }
            const auto it = valueSetters_.find(item);
            if (it == valueSetters_.end()) {
                return;
            }

            it.value()(item->text(1));
            rebuildAllViews();
        });
        loadFromFile(defaultFormPath);
    }

private:
    QTreeWidgetItem *addEditable(QTreeWidgetItem *parent,
                                 const QString &name,
                                 const QString &value,
                                 const std::function<void(const QString &)> &setter)
    {
        auto *item = new QTreeWidgetItem(parent);
        item->setText(0, name);
        item->setText(1, value);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        valueSetters_.insert(item, setter);
        return item;
    }

    QTreeWidgetItem *addReadOnly(QTreeWidgetItem *parent,
                                 const QString &name,
                                 const QString &value)
    {
        auto *item = new QTreeWidgetItem(parent);
        item->setText(0, name);
        item->setText(1, value);
        return item;
    }

    void addFieldCommonProperties(QTreeWidgetItem *fieldItem,
                                  const AFormParser::FieldNode::Ptr &field)
    {
        addEditable(fieldItem,
                    QStringLiteral(".Id"),
                    field->id,
                    [field](const QString &v) { field->id = v; });

        addEditable(fieldItem,
                    QStringLiteral(".Enabled"),
                    field->enabledExpression,
                    [field](const QString &v) { field->enabledExpression = v; });

        addEditable(fieldItem,
                    QStringLiteral(".Description"),
                    field->description,
                    [field](const QString &v) { field->description = v; });

        addEditable(fieldItem,
                    QStringLiteral(".SubDescription"),
                    field->subDescription,
                    [field](const QString &v) { field->subDescription = v; });

        for (auto it = field->extraProperties.begin(); it != field->extraProperties.end(); ++it) {
            const QString k = it.key();
            addEditable(fieldItem,
                        QStringLiteral(".") + k,
                        it.value(),
                        [field, k](const QString &v) { field->extraProperties[k] = v; });
        }
    }

    void addArgNode(QTreeWidgetItem *parent, const AFormParser::ArgNode::Ptr &arg)
    {
        auto *item = addReadOnly(parent,
                                 QStringLiteral("Arg[%1]").arg(arg->id.isEmpty() ? QStringLiteral("-") : arg->id),
                                 QString());

        addEditable(item,
                    QStringLiteral(".Id"),
                    arg->id,
                    [arg](const QString &v) { arg->id = v; });

        addEditable(item,
                    QStringLiteral(".Description"),
                    arg->description,
                    [arg](const QString &v) { arg->description = v; });

        addEditable(item,
                    QStringLiteral(".Value"),
                    arg->value,
                    [arg](const QString &v) { arg->value = v; });

        for (auto it = arg->extraProperties.begin(); it != arg->extraProperties.end(); ++it) {
            const QString k = it.key();
            addEditable(item,
                        QStringLiteral(".") + k,
                        it.value(),
                        [arg, k](const QString &v) { arg->extraProperties[k] = v; });
        }
    }

    void addOptionNode(QTreeWidgetItem *parent, const AFormParser::OptionNode::Ptr &opt)
    {
        auto *item = addReadOnly(parent,
                                 QStringLiteral("Option[%1]").arg(opt->id.isEmpty() ? QStringLiteral("-") : opt->id),
                                 safeSummary(opt->description));

        addEditable(item,
                    QStringLiteral(".Id"),
                    opt->id,
                    [opt](const QString &v) { opt->id = v; });

        addEditable(item,
                    QStringLiteral(".Description"),
                    opt->description,
                    [opt](const QString &v) { opt->description = v; });

        addEditable(item,
                    QStringLiteral(".Command"),
                    opt->command,
                    [opt](const QString &v) { opt->command = v; });

        for (auto it = opt->extraProperties.begin(); it != opt->extraProperties.end(); ++it) {
            const QString k = it.key();
            addEditable(item,
                        QStringLiteral(".") + k,
                        it.value(),
                        [opt, k](const QString &v) { opt->extraProperties[k] = v; });
        }
    }

    void bindLineEditCommit(QLineEdit *edit,
                            const std::function<void(const QString &)> &setter)
    {
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, setter]() {
            if (rebuilding_) {
                return;
            }
            setter(edit->text());
            rebuildAllViews();
        });
    }

    QWidget *createPreviewRow(const QString &title,
                              const QString &subTitle,
                              QWidget *editor,
                              QWidget *parent)
    {
        auto *row = new QWidget(parent);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(8, 8, 8, 8);

        auto *left = new QVBoxLayout();
        auto *titleLabel = new QLabel(title.isEmpty() ? QStringLiteral("(无描述)") : title, row);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);

        left->addWidget(titleLabel);
        if (!subTitle.isEmpty()) {
            auto *subLabel = new QLabel(subTitle, row);
            subLabel->setWordWrap(true);
            left->addWidget(subLabel);
        }

        left->addStretch();
        layout->addLayout(left, 5);
        layout->addWidget(editor, 4);
        return row;
    }

    void addPreviewField(QVBoxLayout *groupLayout,
                         const AFormParser::FieldNode::Ptr &field,
                         QWidget *parent)
    {
        const QString title = field->description;
        const QString subTitle = field->subDescription;

        if (auto key = field->toKeyBind()) {
            auto *editorHost = new QWidget(parent);
            auto *editorLayout = new QHBoxLayout(editorHost);
            editorLayout->setContentsMargins(0, 0, 0, 0);

            auto *bindEdit = new QLineEdit(key->bind, editorHost);
            bindEdit->setPlaceholderText(QStringLiteral("Bind"));
            bindLineEditCommit(bindEdit, [key](const QString &v) { key->bind = v; });

            auto *commandEdit = new QLineEdit(key->command, editorHost);
            commandEdit->setPlaceholderText(QStringLiteral("Command"));
            bindLineEditCommit(commandEdit, [key](const QString &v) { key->command = v; });

            editorLayout->addWidget(bindEdit);
            editorLayout->addWidget(commandEdit);
            groupLayout->addWidget(createPreviewRow(title, subTitle, editorHost, parent));
            return;
        }

        if (auto must = field->toMustField()) {
            auto *editorHost = new QWidget(parent);
            auto *editorLayout = new QHBoxLayout(editorHost);
            editorLayout->setContentsMargins(0, 0, 0, 0);

            auto *bindEdit = new QLineEdit(must->bind, editorHost);
            bindEdit->setPlaceholderText(QStringLiteral("Bind (Must)"));
            bindEdit->setEnabled(false);

            auto *commandEdit = new QLineEdit(must->command, editorHost);
            commandEdit->setPlaceholderText(QStringLiteral("Command"));
            bindLineEditCommit(commandEdit, [must](const QString &v) { must->command = v; });

            editorLayout->addWidget(bindEdit);
            editorLayout->addWidget(commandEdit);
            groupLayout->addWidget(createPreviewRow(title, subTitle + QStringLiteral(" (MustField)"), editorHost, parent));
            return;
        }

        if (auto text = field->toTextField()) {
            auto *editorHost = new QWidget(parent);
            auto *editorLayout = new QVBoxLayout(editorHost);
            editorLayout->setContentsMargins(0, 0, 0, 0);

            auto *textEdit = new QLineEdit(text->text, editorHost);
            textEdit->setPlaceholderText(QStringLiteral("Text"));
            bindLineEditCommit(textEdit, [text](const QString &v) { text->text = v; });

            auto *commandEdit = new QLineEdit(text->command, editorHost);
            commandEdit->setPlaceholderText(QStringLiteral("Command"));
            bindLineEditCommit(commandEdit, [text](const QString &v) { text->command = v; });

            editorLayout->addWidget(textEdit);
            editorLayout->addWidget(commandEdit);
            groupLayout->addWidget(createPreviewRow(title, subTitle, editorHost, parent));
            return;
        }

        if (auto line = field->toLineField()) {
            auto *editorHost = new QWidget(parent);
            auto *editorLayout = new QVBoxLayout(editorHost);
            editorLayout->setContentsMargins(0, 0, 0, 0);

            auto *exprEdit = new QLineEdit(line->expression, editorHost);
            exprEdit->setPlaceholderText(QStringLiteral("Expression"));
            bindLineEditCommit(exprEdit, [line](const QString &v) { line->expression = v; });
            editorLayout->addWidget(exprEdit);

            for (const auto &arg : line->args) {
                if (!arg) {
                    continue;
                }
                auto *argHost = new QWidget(editorHost);
                auto *argLayout = new QHBoxLayout(argHost);
                argLayout->setContentsMargins(0, 0, 0, 0);
                auto *argLabel = new QLabel(arg->id.isEmpty() ? QStringLiteral("arg") : arg->description, argHost);
                auto *argEdit = new QLineEdit(arg->value, argHost);
                bindLineEditCommit(argEdit, [arg](const QString &v) { arg->value = v; });
                argLayout->addWidget(argLabel);
                argLayout->addWidget(argEdit, 1);
                editorLayout->addWidget(argHost);
            }

            groupLayout->addWidget(createPreviewRow(title, subTitle, editorHost, parent));
            return;
        }

        if (auto optField = field->toOptionField()) {
            auto *combo = new QComboBox(parent);
            int selectedIdx = 0;

            for (int i = 0; i < optField->options.size(); ++i) {
                const auto &opt = optField->options[i];
                if (!opt) {
                    continue;
                }
                const QString display = opt->description.isEmpty() ? opt->id : opt->description;
                combo->addItem(display, opt->id);
                if (opt->id == optField->selected) {
                    selectedIdx = combo->count() - 1;
                }
            }

            if (combo->count() > 0) {
                combo->setCurrentIndex(selectedIdx);
            }

            connect(combo,
                    qOverload<int>(&QComboBox::currentIndexChanged),
                    this,
                    [this, combo, optField](int idx) {
                        if (rebuilding_ || idx < 0) {
                            return;
                        }
                        optField->selected = combo->itemData(idx).toString();
                        rebuildAllViews();
                    });

            groupLayout->addWidget(createPreviewRow(title, subTitle, combo, parent));
            return;
        }
    }

    void rebuildPreviewControls()
    {
        QWidget *old = previewScroll_->takeWidget();
        if (old) {
            old->deleteLater();
        }

        auto *content = new QWidget(previewScroll_);
        auto *layout = new QVBoxLayout(content);

        if (!doc_ || !doc_->form) {
            layout->addWidget(new QLabel(QStringLiteral("未加载 Form"), content));
            layout->addStretch();
            previewScroll_->setWidget(content);
            return;
        }

        for (const auto &group : doc_->form->groups) {
            if (!group) {
                continue;
            }

            auto *groupBox = new QGroupBox(group->title.isEmpty() ? QStringLiteral("Group") : group->title, content);
            auto *groupLayout = new QVBoxLayout(groupBox);

            for (const auto &field : group->fields) {
                if (field) {
                    addPreviewField(groupLayout, field, groupBox);
                }
            }

            layout->addWidget(groupBox);
        }

        layout->addStretch();
        previewScroll_->setWidget(content);
    }

    void addFieldNode(QTreeWidgetItem *groupItem, const AFormParser::FieldNode::Ptr &field)
    {
        const QString typeName = fieldTypeName(field);
        auto *fieldItem = addReadOnly(groupItem,
                                      QStringLiteral("%1[%2]").arg(typeName,
                                                                     field->id.isEmpty() ? QStringLiteral("-") : field->id),
                                      safeSummary(field->description));

        addFieldCommonProperties(fieldItem, field);

        if (auto key = field->toKeyBind()) {
            addEditable(fieldItem,
                        QStringLiteral(".Command"),
                        key->command,
                        [key](const QString &v) { key->command = v; });

            addEditable(fieldItem,
                        QStringLiteral(".Bind"),
                        key->bind,
                        [key](const QString &v) { key->bind = v; });
            return;
        }

        if (auto must = field->toMustField()) {
            addEditable(fieldItem,
                        QStringLiteral(".Command"),
                        must->command,
                        [must](const QString &v) { must->command = v; });

            addEditable(fieldItem,
                        QStringLiteral(".Bind"),
                        must->bind,
                        [must](const QString &v) { must->bind = v; });
            return;
        }

        if (auto text = field->toTextField()) {
            addEditable(fieldItem,
                        QStringLiteral(".Text"),
                        text->text,
                        [text](const QString &v) { text->text = v; });

            addEditable(fieldItem,
                        QStringLiteral(".Command"),
                        text->command,
                        [text](const QString &v) { text->command = v; });
            return;
        }

        if (auto line = field->toLineField()) {
            addEditable(fieldItem,
                        QStringLiteral(".Expression"),
                        line->expression,
                        [line](const QString &v) { line->expression = v; });

            auto *argsItem = addReadOnly(fieldItem, QStringLiteral(".Args"), QString());
            for (const auto &arg : line->args) {
                if (arg) {
                    addArgNode(argsItem, arg);
                }
            }
            return;
        }

        if (auto optField = field->toOptionField()) {
            addEditable(fieldItem,
                        QStringLiteral(".Selected"),
                        optField->selected,
                        [optField](const QString &v) { optField->selected = v; });

            auto *optsItem = addReadOnly(fieldItem, QStringLiteral("Options"), QString());
            for (const auto &opt : optField->options) {
                if (opt) {
                    addOptionNode(optsItem, opt);
                }
            }
            return;
        }
    }

    void addFormNode(QTreeWidgetItem *root)
    {
        if (!doc_ || !doc_->form) {
            addReadOnly(root, QStringLiteral("Form"), QStringLiteral("<null>"));
            return;
        }

        auto form = doc_->form;
        auto *formItem = addReadOnly(root,
                                     QStringLiteral("Form[%1]").arg(form->id.isEmpty() ? QStringLiteral("-") : form->id),
                                     QString());

        addEditable(formItem,
                    QStringLiteral(".Id"),
                    form->id,
                    [form](const QString &v) { form->id = v; });

        for (auto it = form->extraProperties.begin(); it != form->extraProperties.end(); ++it) {
            const QString k = it.key();
            addEditable(formItem,
                        QStringLiteral(".") + k,
                        it.value(),
                        [form, k](const QString &v) { form->extraProperties[k] = v; });
        }

        for (const auto &group : form->groups) {
            if (!group) {
                continue;
            }

            auto *groupItem = addReadOnly(formItem,
                                          QStringLiteral("Group[%1]").arg(group->title.isEmpty() ? QStringLiteral("-") : group->title),
                                          QStringLiteral("fields=%1").arg(group->fields.size()));

            addEditable(groupItem,
                        QStringLiteral(".Title"),
                        group->title,
                        [group](const QString &v) { group->title = v; });

            for (auto it = group->extraProperties.begin(); it != group->extraProperties.end(); ++it) {
                const QString k = it.key();
                addEditable(groupItem,
                            QStringLiteral(".") + k,
                            it.value(),
                            [group, k](const QString &v) { group->extraProperties[k] = v; });
            }

            for (const auto &field : group->fields) {
                if (field) {
                    addFieldNode(groupItem, field);
                }
            }
        }
    }

    void addMetaAndScripts(QTreeWidgetItem *root)
    {
        if (!doc_) {
            return;
        }

        auto *metaItem = addReadOnly(root, QStringLiteral("Meta"), QString());
        const auto entries = doc_->metaEntries();
        for (const auto &entry : entries) {
            const QString key = entry.first;
            addEditable(metaItem,
                        QStringLiteral("@") + key,
                        entry.second,
                        [this, key](const QString &v) { doc_->setMetaValue(key, v); });
        }

        auto *scriptsItem = addReadOnly(root, QStringLiteral("Scripts"), QString());
        if (doc_->scripts) {
            QString escaped = doc_->scripts->rawCode;
            addEditable(scriptsItem,
                        QStringLiteral("rawCode (use \\n for newline)"),
                        escaped.replace(QStringLiteral("\n"), QStringLiteral("\\n")),
                        [scripts = doc_->scripts](const QString &v) {
                            QString code = v;
                            code.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
                            scripts->rawCode = code;
                        });
        } else {
            addReadOnly(scriptsItem, QStringLiteral("rawCode"), QStringLiteral("<none>"));
        }
    }

    void rebuildTreeView()
    {
        valueSetters_.clear();
        tree_->clear();

        auto *root = new QTreeWidgetItem(tree_);
        root->setText(0, QStringLiteral("Document"));
        root->setText(1, doc_ ? QStringLiteral("loaded") : QStringLiteral("empty"));

        addFormNode(root);
        addMetaAndScripts(root);

        tree_->expandToDepth(2);
    }

    void rebuildAllViews()
    {
        rebuilding_ = true;
        rebuildTreeView();
        rebuildPreviewControls();
        refreshOutputs();
        rebuilding_ = false;
    }

    void refreshOutputs()
    {
        if (!doc_) {
            dumpEdit_->clear();
            cfgEdit_->clear();
            return;
        }

        dumpEdit_->setPlainText(doc_->dump());
        cfgEdit_->setPlainText(doc_->toCFG());
    }

    bool loadFromFile(const QString &path)
    {
        if (path.isEmpty()) {
            statusLabel_->setText(QStringLiteral("请输入 Form 文件路径"));
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            statusLabel_->setText(QStringLiteral("读取失败: ") + file.errorString());
            QMessageBox::warning(this,
                                 QStringLiteral("加载失败"),
                                 QStringLiteral("无法读取文件:\n%1\n\n%2").arg(path, file.errorString()));
            return false;
        }

        QTextStream stream(&file);
        const QString text = stream.readAll();
        file.close();

        AFormParser::ParseError err;
        auto nextDoc = AFormParser::Document::from(text, &err);
        if (!nextDoc) {
            statusLabel_->setText(QStringLiteral("解析失败"));
            QMessageBox::critical(this,
                                  QStringLiteral("解析失败"),
                                  QStringLiteral("line=%1 col=%2\n%3").arg(err.line).arg(err.column).arg(err.message));
            return false;
        }

        doc_ = nextDoc;
        fileEdit_->setText(path);
        statusLabel_->setText(QStringLiteral("已加载: ") + path);
        if (!globalVars_.isEmpty()) {
            doc_->setGlobalVariables(globalVars_);
        }
        rebuildAllViews();
        return true;
    }

private:
    QLineEdit *fileEdit_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QScrollArea *previewScroll_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QPlainTextEdit *dumpEdit_ = nullptr;
    QPlainTextEdit *cfgEdit_ = nullptr;
    QDialog *globalVarsDialog_ = nullptr;
    QTableWidget *globalVarsTable_ = nullptr;
    QMap<QString, QString> globalVars_;

    AFormParser::Document::Ptr doc_;
    QHash<QTreeWidgetItem *, std::function<void(const QString &)>> valueSetters_;
    bool rebuilding_ = false;

    void showGlobalVarsDialog();
    void applyGlobalVarsToDocument();
};

void FormTreeViewer::showGlobalVarsDialog()
{
    if (!globalVarsDialog_) {
        globalVarsDialog_ = new QDialog(this);
        globalVarsDialog_->setWindowTitle(QStringLiteral("全局String变量表"));
        globalVarsDialog_->resize(500, 400);

        auto *layout = new QVBoxLayout(globalVarsDialog_);

        auto *descLabel = new QLabel(QStringLiteral("以 __ 开头的变量名将在Scripts中被替换为对应值"), globalVarsDialog_);
        layout->addWidget(descLabel);

        globalVarsTable_ = new QTableWidget(globalVarsDialog_);
        globalVarsTable_->setColumnCount(2);
        globalVarsTable_->setHorizontalHeaderLabels(QStringList{QStringLiteral("变量名"), QStringLiteral("值")});
        globalVarsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        globalVarsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        globalVarsTable_->setAlternatingRowColors(true);
        globalVarsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        globalVarsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(globalVarsTable_);

        auto *btnLayout = new QHBoxLayout();

        auto *addBtn = new QPushButton(QStringLiteral("添加"), globalVarsDialog_);
        auto *removeBtn = new QPushButton(QStringLiteral("删除选中"), globalVarsDialog_);
        auto *applyBtn = new QPushButton(QStringLiteral("应用并关闭"), globalVarsDialog_);
        auto *cancelBtn = new QPushButton(QStringLiteral("取消"), globalVarsDialog_);

        btnLayout->addWidget(addBtn);
        btnLayout->addWidget(removeBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(applyBtn);
        btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);

        connect(addBtn, &QPushButton::clicked, this, [this]() {
            int row = globalVarsTable_->rowCount();
            globalVarsTable_->insertRow(row);
            globalVarsTable_->setItem(row, 0, new QTableWidgetItem("__NewVar"));
            globalVarsTable_->setItem(row, 1, new QTableWidgetItem("value"));
        });

        connect(removeBtn, &QPushButton::clicked, this, [this]() {
            int row = globalVarsTable_->currentRow();
            if (row >= 0) {
                globalVarsTable_->removeRow(row);
            }
        });

        connect(applyBtn, &QPushButton::clicked, this, [this]() {
            applyGlobalVarsToDocument();
            globalVarsDialog_->accept();
        });

        connect(cancelBtn, &QPushButton::clicked, globalVarsDialog_, &QDialog::reject);
    }

    globalVarsTable_->setRowCount(0);
    for (auto it = globalVars_.constBegin(); it != globalVars_.constEnd(); ++it) {
        int row = globalVarsTable_->rowCount();
        globalVarsTable_->insertRow(row);
        globalVarsTable_->setItem(row, 0, new QTableWidgetItem(it.key()));
        globalVarsTable_->setItem(row, 1, new QTableWidgetItem(it.value()));
    }

    globalVarsDialog_->exec();
}

void FormTreeViewer::applyGlobalVarsToDocument()
{
    globalVars_.clear();
    for (int row = 0; row < globalVarsTable_->rowCount(); ++row) {
        QString name = globalVarsTable_->item(row, 0)->text().trimmed();
        QString value = globalVarsTable_->item(row, 1)->text();
        if (!name.isEmpty()) {
            globalVars_[name] = value;
        }
    }

    if (doc_) {
        doc_->setGlobalVariables(globalVars_);
    }

    rebuildAllViews();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString defaultPath = QStringLiteral("samples/Function_Preference_Form.asul");
    const QStringList args = app.arguments();
    if (args.size() >= 2) {
        defaultPath = args.at(1);
    }

    FormTreeViewer w(defaultPath);
    w.show();

    return app.exec();
}
