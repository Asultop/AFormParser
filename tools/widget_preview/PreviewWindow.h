#ifndef PREVIEWWINDOW_H
#define PREVIEWWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include "AFormParser/AFormParser.hpp"

class PreviewDialog : public QWidget {
    Q_OBJECT
public:
    explicit PreviewDialog(const AFormParser::Document::Ptr &doc, QWidget *parent = nullptr);
    ~PreviewDialog() override = default;

private:
    void setupUi();
    void populateTabs();
    void updateTab(int index);

    AFormParser::Document::Ptr doc_;
    QTabWidget *tabWidget_ = nullptr;
    QVector<QPair<QString, QString>> formOutputs_;
};

class PreviewWindow : public QWidget {
    Q_OBJECT
public:
    explicit PreviewWindow(const QString &formPath, QWidget *parent = nullptr);
    ~PreviewWindow() override = default;

private slots:
    void onPrevClicked();
    void onNextClicked();
    void onPreviewClicked();

private:
    void loadForm(const QString &path);
    void generatePreview(const AFormParser::FormNode::Ptr &form);
    QWidget *createFieldWidget(const AFormParser::FieldNode::Ptr &field);
    void updateNavigation();

    AFormParser::Document::Ptr doc_;
    QVBoxLayout *previewLayout_ = nullptr;
    QComboBox *formSelector_ = nullptr;
    QPushButton *prevBtn_ = nullptr;
    QPushButton *nextBtn_ = nullptr;
    QPushButton *previewBtn_ = nullptr;
    int currentFormIndex_ = 0;
};

#endif
