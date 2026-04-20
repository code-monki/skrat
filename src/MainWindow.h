#pragma once

#include <QFileInfo>
#include <QMainWindow>

class QAction;
class QFileSystemModel;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QPdfDocument;
class QPdfSearchModel;
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
    void openPdfFind();
    void pdfFindNext();
    void pdfFindPrev();
    void onPdfFindTextChanged(const QString &text);
    void onPdfSearchResultsChanged();
    void printCurrentPdf();
    void copyCurrentSelection();
    void goToPageOrLine();
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
    void updateGoToNavigationAction();
    void updatePdfFindActions();
    void updatePdfSearchStatus();
    int pdfSearchResultCount() const;
    void selectPdfSearchResult(int index);

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
    QAction *m_actPdfFind = nullptr;
    QAction *m_actPdfFindNext = nullptr;
    QAction *m_actPdfFindPrev = nullptr;
    QAction *m_actPdfPrint = nullptr;
    QAction *m_actCopy = nullptr;
    QAction *m_actGoToPageOrLine = nullptr;
    QToolBar *m_pdfFindToolBar = nullptr;
    QLineEdit *m_pdfFindEdit = nullptr;
    QLabel *m_pdfFindCountLabel = nullptr;
    QPdfSearchModel *m_pdfSearchModel = nullptr;

    QString m_previewFilePath;
    QString m_pendingReloadPath;
    QFileSystemWatcher *m_fileWatcher = nullptr;
    QTimer *m_reloadDebounceTimer = nullptr;

    /** Set for the duration of destruction so slots ignore late PDF/signal callbacks. */
    bool m_shuttingDown = false;
};
