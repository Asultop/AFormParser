#include "PreviewWindow.h"

#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QFontDatabase>

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

class FieldWidgetFactory {
public:
    static QWidget *createKeyBindWidget(const AFormParser::FieldNode::Ptr &field, QWidget *parent)
    {
        auto key = field->toKeyBind();
        if (!key) {
            return nullptr;
        }

        auto *container = new QWidget(parent);
        auto *layout = new QHBoxLayout(container);

        auto *bindEdit = new QLineEdit(key->bind, container);
        bindEdit->setPlaceholderText(QStringLiteral("Bind"));
        layout->addWidget(new QLabel(QStringLiteral("Bind:")));
        layout->addWidget(bindEdit);

        auto *cmdEdit = new QLineEdit(key->command, container);
        cmdEdit->setPlaceholderText(QStringLiteral("Command"));
        layout->addWidget(new QLabel(QStringLiteral("Command:")));
        layout->addWidget(cmdEdit);

        QObject::connect(bindEdit, &QLineEdit::textChanged, [key](const QString &text) {
            key->bind = text;
        });
        QObject::connect(cmdEdit, &QLineEdit::textChanged, [key](const QString &text) {
            key->command = text;
        });

        return container;
    }

    static QWidget *createMustFieldWidget(const AFormParser::FieldNode::Ptr &field, QWidget *parent)
    {
        auto must = field->toMustField();
        if (!must) {
            return nullptr;
        }

        auto *container = new QWidget(parent);
        auto *layout = new QHBoxLayout(container);

        auto *bindEdit = new QLineEdit(must->bind, container);
        bindEdit->setPlaceholderText(QStringLiteral("Bind (ReadOnly)"));
        bindEdit->setEnabled(false);
        layout->addWidget(new QLabel(QStringLiteral("Bind:")));
        layout->addWidget(bindEdit);

        auto *cmdEdit = new QLineEdit(must->command, container);
        cmdEdit->setPlaceholderText(QStringLiteral("Command"));
        layout->addWidget(new QLabel(QStringLiteral("Command:")));
        layout->addWidget(cmdEdit);

        QObject::connect(cmdEdit, &QLineEdit::textChanged, [must](const QString &text) {
            must->command = text;
        });

        return container;
    }

    static QWidget *createTextFieldWidget(const AFormParser::FieldNode::Ptr &field, QWidget *parent)
    {
        auto textField = field->toTextField();
        if (!textField) {
            return nullptr;
        }

        auto *container = new QWidget(parent);
        auto *layout = new QVBoxLayout(container);

        auto *textEdit = new QLineEdit(textField->text, container);
        textEdit->setPlaceholderText(QStringLiteral("Text"));
        layout->addWidget(new QLabel(QStringLiteral("Text:")));
        layout->addWidget(textEdit);

        auto *cmdEdit = new QLineEdit(textField->command, container);
        cmdEdit->setPlaceholderText(QStringLiteral("Command"));
        layout->addWidget(new QLabel(QStringLiteral("Command:")));
        layout->addWidget(cmdEdit);

        QObject::connect(textEdit, &QLineEdit::textChanged, [textField](const QString &val) {
            textField->text = val;
        });
        QObject::connect(cmdEdit, &QLineEdit::textChanged, [textField](const QString &val) {
            textField->command = val;
        });

        return container;
    }

    static QWidget *createLineFieldWidget(const AFormParser::FieldNode::Ptr &field, QWidget *parent)
    {
        auto line = field->toLineField();
        if (!line) {
            return nullptr;
        }

        auto *container = new QWidget(parent);
        auto *layout = new QVBoxLayout(container);

        auto *exprEdit = new QLineEdit(line->expression, container);
        exprEdit->setPlaceholderText(QStringLiteral("Expression"));
        layout->addWidget(new QLabel(QStringLiteral("Expression:")));
        layout->addWidget(exprEdit);

        QObject::connect(exprEdit, &QLineEdit::textChanged, [line](const QString &expr) {
            line->expression = expr;
        });

        if (!line->args.isEmpty()) {
            layout->addWidget(new QLabel(QStringLiteral("Args:")));
            for (const auto &arg : line->args) {
                if (!arg) {
                    continue;
                }
                auto *argContainer = new QWidget(container);
                auto *argLayout = new QHBoxLayout(argContainer);
                argLayout->setContentsMargins(0, 0, 0, 0);

                auto *argLabel = new QLabel(arg->description.isEmpty() ? arg->id : arg->description, argContainer);
                auto *argEdit = new QLineEdit(arg->value, argContainer);
                argEdit->setPlaceholderText(QStringLiteral("Value"));

                argLayout->addWidget(argLabel);
                argLayout->addWidget(argEdit, 1);

                QObject::connect(argEdit, &QLineEdit::textChanged, [arg](const QString &val) {
                    arg->value = val;
                });

                layout->addWidget(argContainer);
            }
        }

        return container;
    }

    static QWidget *createOptionFieldWidget(const AFormParser::FieldNode::Ptr &field, QWidget *parent)
    {
        auto optField = field->toOptionField();
        if (!optField) {
            return nullptr;
        }

        auto *container = new QWidget(parent);
        auto *layout = new QHBoxLayout(container);

        auto *combo = new QComboBox(container);
        for (const auto &opt : optField->options) {
            if (!opt) {
                continue;
            }
            const QString display = opt->description.isEmpty() ? opt->id : opt->description;
            combo->addItem(display, opt->id);
        }

        int selectedIdx = 0;
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == optField->selected) {
                selectedIdx = i;
                break;
            }
        }
        combo->setCurrentIndex(selectedIdx);

        layout->addWidget(new QLabel(QStringLiteral("Selected:")));
        layout->addWidget(combo, 1);

        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         [optField, combo](int idx) {
                             if (idx >= 0) {
                                 optField->selected = combo->itemData(idx).toString();
                             }
                         });

        return container;
    }

    static QWidget *createWidget(const AFormParser::FieldNode::Ptr &field, QWidget *parent)
    {
        if (!field) {
            return new QLabel(QStringLiteral("(null field)"), parent);
        }

        if (field->toKeyBind()) {
            return createKeyBindWidget(field, parent);
        }

        if (field->toMustField()) {
            return createMustFieldWidget(field, parent);
        }

        if (field->toTextField()) {
            return createTextFieldWidget(field, parent);
        }

        if (field->toLineField()) {
            return createLineFieldWidget(field, parent);
        }

        if (field->toOptionField()) {
            return createOptionFieldWidget(field, parent);
        }

        return new QLabel(QStringLiteral("Unknown field type: ") + fieldTypeName(field), parent);
    }
};

} // namespace

PreviewDialog::PreviewDialog(const AFormParser::Document::Ptr &doc, QWidget *parent)
    : QWidget(parent)
    , doc_(doc)
{
    setWindowTitle(QStringLiteral("Preview - toCFGs & Dump"));
    resize(900, 700);

    setupUi();
    populateTabs();
}

void PreviewDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    tabWidget_ = new QTabWidget(this);
    mainLayout->addWidget(tabWidget_);
}

void PreviewDialog::populateTabs()
{
    if (!doc_) {
        return;
    }

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    auto cfgItems = doc_->toCFGs();

    for (int i = 0; i < cfgItems.size(); ++i) {
        const auto &item = cfgItems.at(i);

        auto *tab = new QWidget();
        auto *tabLayout = new QVBoxLayout(tab);

        auto *outputLayout = new QHBoxLayout();
        outputLayout->addWidget(new QLabel(QStringLiteral("Output:")));
        auto *outputEdit = new QLineEdit(item.absolutePath);
        outputEdit->setReadOnly(true);
        outputEdit->setFont(mono);
        outputLayout->addWidget(outputEdit, 1);
        tabLayout->addLayout(outputLayout);

        auto *cfgGroup = new QGroupBox(QStringLiteral("CFG Content"), tab);
        auto *cfgLayout = new QVBoxLayout(cfgGroup);
        auto *cfgEdit = new QPlainTextEdit(item.content, cfgGroup);
        cfgEdit->setReadOnly(true);
        cfgEdit->setFont(mono);
        cfgLayout->addWidget(cfgEdit);
        tabLayout->addWidget(cfgGroup, 1);

        QString tabLabel = item.sourceFormId.isEmpty()
                               ? QStringLiteral("Form %1").arg(i + 1)
                               : item.sourceFormId;
        if (!item.description.isEmpty()) {
            tabLabel += QStringLiteral(" - %1").arg(item.description);
        }

        tabWidget_->addTab(tab, tabLabel);
    }
}

void PreviewDialog::updateTab(int index)
{
    Q_UNUSED(index)
}

PreviewWindow::PreviewWindow(const QString &formPath, QWidget *parent)
    : QWidget(parent)
    , currentFormIndex_(0)
{
    setWindowTitle(QStringLiteral("Widget Preview - AFormParser SDK Demo"));
    resize(800, 600);

    auto *mainLayout = new QVBoxLayout(this);

    auto *navLayout = new QHBoxLayout();

    formSelector_ = new QComboBox(this);
    navLayout->addWidget(new QLabel(QStringLiteral("Form:"), this));
    navLayout->addWidget(formSelector_, 1);

    prevBtn_ = new QPushButton(QStringLiteral("上一页"), this);
    nextBtn_ = new QPushButton(QStringLiteral("下一页"), this);
    previewBtn_ = new QPushButton(QStringLiteral("预览"), this);

    navLayout->addWidget(prevBtn_);
    navLayout->addWidget(nextBtn_);
    navLayout->addWidget(previewBtn_);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    auto *scrollContent = new QWidget(scrollArea);
    previewLayout_ = new QVBoxLayout(scrollContent);

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    mainLayout->addLayout(navLayout);

    connect(formSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index >= 0) {
                    currentFormIndex_ = index;
                    updateNavigation();
                    if (doc_ && !doc_->forms.isEmpty() && currentFormIndex_ < doc_->forms.size()) {
                        generatePreview(doc_->forms.at(currentFormIndex_));
                    }
                }
            });

    connect(prevBtn_, &QPushButton::clicked, this, &PreviewWindow::onPrevClicked);
    connect(nextBtn_, &QPushButton::clicked, this, &PreviewWindow::onNextClicked);
    connect(previewBtn_, &QPushButton::clicked, this, &PreviewWindow::onPreviewClicked);

    if (!formPath.isEmpty()) {
        loadForm(formPath);
    }
}

void PreviewWindow::loadForm(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             QStringLiteral("Error"),
                             QStringLiteral("Failed to open file: ") + path);
        return;
    }

    QTextStream stream(&file);
    const QString text = stream.readAll();
    file.close();

    AFormParser::ParseError err;
    doc_ = AFormParser::Document::from(text, &err);
    if (!doc_) {
        QMessageBox::critical(this,
                              QStringLiteral("Parse Error"),
                              QStringLiteral("Line %1, Col %2:\n%3").arg(err.line).arg(err.column).arg(err.message));
        return;
    }

    formSelector_->blockSignals(true);
    formSelector_->clear();
    for (int i = 0; i < doc_->forms.size(); ++i) {
        const auto &form = doc_->forms.at(i);
        QString label;
        if (!form->description.isEmpty()) {
            label = form->description;
        } else if (!form->id.isEmpty()) {
            label = form->id;
        } else {
            label = QStringLiteral("Form %1").arg(i + 1);
        }
        formSelector_->addItem(label, i);
    }
    formSelector_->blockSignals(false);

    if (!doc_->forms.isEmpty()) {
        currentFormIndex_ = 0;
        formSelector_->setCurrentIndex(0);
        generatePreview(doc_->forms.first());
    }

    updateNavigation();
}

void PreviewWindow::generatePreview(const AFormParser::FormNode::Ptr &form)
{
    QLayoutItem *child;
    while ((child = previewLayout_->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    if (!form) {
        previewLayout_->addWidget(new QLabel(QStringLiteral("No form to display")));
        previewLayout_->addStretch();
        return;
    }

    for (const auto &group : form->groups) {
        if (!group) {
            continue;
        }

        auto *groupBox = new QGroupBox(
            group->title.isEmpty() ? QStringLiteral("Group") : group->title,
            this);
        auto *groupLayout = new QVBoxLayout(groupBox);

        for (const auto &field : group->fields) {
            if (!field) {
                continue;
            }

            auto *fieldContainer = new QWidget(groupBox);
            auto *fieldLayout = new QVBoxLayout(fieldContainer);
            fieldLayout->setContentsMargins(4, 4, 4, 4);

            auto *titleLabel = new QLabel(field->description, fieldContainer);
            QFont titleFont = titleLabel->font();
            titleFont.setBold(true);
            titleLabel->setFont(titleFont);

            auto *typeLabel = new QLabel(QStringLiteral("Type: ") + fieldTypeName(field), fieldContainer);

            auto *editorWidget = FieldWidgetFactory::createWidget(field, fieldContainer);

            fieldLayout->addWidget(titleLabel);
            fieldLayout->addWidget(typeLabel);
            fieldLayout->addWidget(editorWidget);

            groupLayout->addWidget(fieldContainer);
        }

        previewLayout_->addWidget(groupBox);
    }

    previewLayout_->addStretch();
}

QWidget *PreviewWindow::createFieldWidget(const AFormParser::FieldNode::Ptr &field)
{
    return FieldWidgetFactory::createWidget(field, this);
}

void PreviewWindow::updateNavigation()
{
    if (!doc_ || doc_->forms.isEmpty()) {
        prevBtn_->setVisible(false);
        nextBtn_->setVisible(false);
        previewBtn_->setText(QStringLiteral("预览"));
        return;
    }

    const int totalForms = doc_->forms.size();
    const bool hasMultipleForms = totalForms > 1;
    const bool hasPrev = currentFormIndex_ > 0;
    const bool hasNext = currentFormIndex_ < totalForms - 1;

    prevBtn_->setVisible(hasMultipleForms && hasPrev);
    nextBtn_->setVisible(hasMultipleForms && hasNext);

    if (!hasMultipleForms) {
        previewBtn_->setText(QStringLiteral("预览"));
    } else if (!hasNext && hasPrev) {
        previewBtn_->setText(QStringLiteral("预览"));
    } else {
        previewBtn_->setText(QStringLiteral("预览"));
    }
}

void PreviewWindow::onPrevClicked()
{
    if (!doc_ || currentFormIndex_ <= 0) {
        return;
    }

    currentFormIndex_--;
    formSelector_->setCurrentIndex(currentFormIndex_);
    updateNavigation();
    generatePreview(doc_->forms.at(currentFormIndex_));
}

void PreviewWindow::onNextClicked()
{
    if (!doc_ || currentFormIndex_ >= doc_->forms.size() - 1) {
        return;
    }

    currentFormIndex_++;
    formSelector_->setCurrentIndex(currentFormIndex_);
    updateNavigation();
    generatePreview(doc_->forms.at(currentFormIndex_));
}

void PreviewWindow::onPreviewClicked()
{
    if (!doc_) {
        return;
    }

    auto *dialog = new PreviewDialog(doc_, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
