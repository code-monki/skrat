#pragma once

#include <QFileInfo>
#include <QMainWindow>

/**
 * @file MainWindow.h
 * @brief Main application window for the skrat desktop viewer.
 *
 * The main window coordinates file browsing, preview rendering for PDF/text,
 * PDF navigation/search/print actions, live file reload behavior, and
 * left-pane tab content (files + PDF table of contents).
 */

class QAction;
class QFileSystemModel;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QIntValidator;
class QPdfBookmarkModel;
class QPdfDocument;
class QPdfSearchModel;
class QPdfView;
class QPlainTextEdit;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QToolBar;
class QTreeView;

namespace skrat {
class PdfDocumentRibbon;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    /** Construct the main window and initialize all UI/state wiring. */
    explicit MainWindow(QWidget *parent = nullptr);
    /** Tear down the window and safely disconnect late PDF signals. */
    ~MainWindow() override;

    /** Set the file-tree root folder using an absolute path. */
    void setRootFolder(const QString &absolutePath);
    /** Select and preview a specific file path under the current root. */
    void selectPath(const QString &absoluteFilePath);

private slots:
    /** React to tree selection changes and preview the selected path. */
    void onTreeCurrentChanged(const QModelIndex &current, const QModelIndex &previous);
    /** Show folder picker and update the tree root. */
    void openFolderDialog();
    /** Increase PDF zoom while in PDF preview mode. */
    void zoomInPdf();
    /** Decrease PDF zoom while in PDF preview mode. */
    void zoomOutPdf();
    /** Switch PDF zoom behavior to fit width. */
    void pdfFitWidth();
    /** Navigate to first page in current PDF. */
    void pdfGoFirstPage();
    /** Navigate to previous page in current PDF. */
    void pdfGoPrevPage();
    /** Navigate to next page in current PDF. */
    void pdfGoNextPage();
    /** Navigate to last page in current PDF. */
    void pdfGoLastPage();
    /** Reveal/focus PDF find controls. */
    void openPdfFind();
    /** Select the next search result in the active PDF. */
    void pdfFindNext();
    /** Select the previous search result in the active PDF. */
    void pdfFindPrev();
    /** Update search model state when query text changes. */
    void onPdfFindTextChanged(const QString &text);
    /** Refresh search UI when result model updates. */
    void onPdfSearchResultsChanged();
    /** Start PDF print flow (mode selection + print pipeline). */
    void printCurrentPdf();
    /** Copy selection from active preview widget with clipboard cleanup. */
    void copyCurrentSelection();
    /** Show About dialog with app and licensing summary. */
    void showAboutDialog();
    /** Show help/shortcuts dialog. */
    void showHelpDialog();
    /** Jump to page number entered in the toolbar page input. */
    void onPdfPageEditReturnPressed();
    /** Handle click/activation from PDF table-of-contents tree. */
    void onTocActivated(const QModelIndex &index);
    /** React to bookmark model changes and refresh TOC pane state. */
    void onPdfBookmarksChanged();
    /** Context-aware "go to page/line" action handler. */
    void goToPageOrLine();
    /** Recompute PDF navigation state, labels, and action enablement. */
    void updatePdfPageUi();
    /** Queue reload after watched file changes on disk. */
    void onWatchedFileChanged(const QString &path);
    /** Perform debounced preview reload for watched files. */
    void onReloadDebounceTimeout();

private:
    /** Build menus, toolbars, splitters, views, and signal wiring. */
    void setupUi();
    /** Preview a path as PDF/text/placeholder depending on file type/state. */
    void previewPath(const QString &absolutePath);
    /** Show placeholder page with provided rich-text message. */
    void showPlaceholder(const QString &html);
    /** Heuristic text-file classifier based on extension. */
    static bool isProbablyTextFile(const QFileInfo &fi);
    /** Remove all active file-system watch registrations. */
    void pauseWatching();
    /** Set currently watched preview file (or clear when empty). */
    void setWatchedPreviewFile(const QString &absoluteFilePath);
    /** Show "file unavailable" placeholder for deleted/renamed previews. */
    void showPreviewFileUnavailable(const QString &lostPath);
    /** Enable/disable Go-To action according to active preview context. */
    void updateGoToNavigationAction();
    /** Enable/disable find-related actions based on current state. */
    void updatePdfFindActions();
    /** Update find-match summary label (e.g. "Match X of Y"). */
    void updatePdfSearchStatus();
    /** Return number of current PDF search matches. */
    int pdfSearchResultCount() const;
    /** Select and navigate to a search result by index. */
    void selectPdfSearchResult(int index);
    /** Read currently selected search-result index (Qt-version compatible). */
    int currentPdfSearchResultIndex() const;
    /** Set current search-result index using Qt-version-safe behavior. */
    void setCurrentPdfSearchResultIndexCompat(int index);
    /** Toggle and populate TOC tab content based on active preview. */
    void updateTocPaneUi();

    QString m_rootPath;
    QFileSystemModel *m_fsModel = nullptr;
    QTreeView *m_tree = nullptr;
    QTabWidget *m_leftTabs = nullptr;
    QSplitter *m_splitter = nullptr;
    QStackedWidget *m_stack = nullptr;
    QPdfView *m_pdfView = nullptr;
    QPdfDocument *m_pdfDocument = nullptr;
    QPlainTextEdit *m_textView = nullptr;
    QLabel *m_placeholder = nullptr;

    QToolBar *m_pdfToolBar = nullptr;
    skrat::PdfDocumentRibbon *m_pdfDocumentRibbon = nullptr;
    QWidget *m_previewPane = nullptr;
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
    QLineEdit *m_pdfPageEdit = nullptr;
    QIntValidator *m_pdfPageValidator = nullptr;
    QLineEdit *m_pdfFindEdit = nullptr;
    QLabel *m_pdfFindCountLabel = nullptr;
    QPdfSearchModel *m_pdfSearchModel = nullptr;
    QPdfBookmarkModel *m_pdfBookmarkModel = nullptr;
    QTreeView *m_tocView = nullptr;
    QStackedWidget *m_tocStack = nullptr;
    QLabel *m_tocPlaceholder = nullptr;

    QString m_previewFilePath;
    QString m_pendingReloadPath;
    int m_pdfSearchCurrentIndex = -1;
    bool m_updatingPdfPageEdit = false;
    bool m_pdfAutoSelectingFirstResult = false;
    QFileSystemWatcher *m_fileWatcher = nullptr;
    QTimer *m_reloadDebounceTimer = nullptr;

    /** Set for the duration of destruction so slots ignore late PDF/signal callbacks. */
    bool m_shuttingDown = false;
};
