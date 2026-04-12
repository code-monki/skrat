#pragma once

#include <QFileInfo>
#include <QMainWindow>

class QAction;
class QFileSystemModel;
class QLabel;
class QPdfDocument;
class QPdfView;
class QPlainTextEdit;
class QSplitter;
class QStackedWidget;
class QToolBar;
class QTreeView;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void setRootFolder(const QString &absolutePath);
    void selectPath(const QString &absoluteFilePath);

private slots:
    void onTreeCurrentChanged(const QModelIndex &current, const QModelIndex &previous);
    void openFolderDialog();
    void zoomInPdf();
    void zoomOutPdf();
    void pdfFitWidth();
    void pdfGoFirstPage();
    void pdfGoPrevPage();
    void pdfGoNextPage();
    void pdfGoLastPage();
    void updatePdfPageUi();

private:
    void setupUi();
    void previewPath(const QString &absolutePath);
    void showPlaceholder(const QString &html);
    static bool isProbablyTextFile(const QFileInfo &fi);

    QString m_rootPath;
    QFileSystemModel *m_fsModel = nullptr;
    QTreeView *m_tree = nullptr;
    QSplitter *m_splitter = nullptr;
    QStackedWidget *m_stack = nullptr;
    QPdfView *m_pdfView = nullptr;
    QPdfDocument *m_pdfDocument = nullptr;
    QPlainTextEdit *m_textView = nullptr;
    QLabel *m_placeholder = nullptr;

    QToolBar *m_pdfToolBar = nullptr;
    QLabel *m_pdfPageLabel = nullptr;
    QAction *m_pdfActFirst = nullptr;
    QAction *m_pdfActPrev = nullptr;
    QAction *m_pdfActNext = nullptr;
    QAction *m_pdfActLast = nullptr;
};
