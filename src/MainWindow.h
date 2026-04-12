#pragma once

#include <QFileInfo>
#include <QMainWindow>

class QAction;
class QFileSystemModel;
class QFileSystemWatcher;
class QLabel;
class QPdfDocument;
class QPdfView;
class QPlainTextEdit;
class QSplitter;
class QStackedWidget;
class QTimer;
class QToolBar;
class QTreeView;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

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
    void onWatchedFileChanged(const QString &path);
    void onReloadDebounceTimeout();

private:
    void setupUi();
    void previewPath(const QString &absolutePath);
    void showPlaceholder(const QString &html);
    static bool isProbablyTextFile(const QFileInfo &fi);
    void pauseWatching();
    void setWatchedPreviewFile(const QString &absoluteFilePath);
    void showPreviewFileUnavailable(const QString &lostPath);

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

    QString m_previewFilePath;
    QString m_pendingReloadPath;
    QFileSystemWatcher *m_fileWatcher = nullptr;
    QTimer *m_reloadDebounceTimer = nullptr;

    /** Set for the duration of destruction so slots ignore late PDF/signal callbacks. */
    bool m_shuttingDown = false;
};
