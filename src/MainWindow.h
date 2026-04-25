#pragma once

#include <QFileInfo>
#include <QMainWindow>
#include <QPixmap>

/**
 * @file MainWindow.h
 * @brief Main application window for the skrat desktop viewer.
 *
 * The main window coordinates file browsing, preview rendering for PDF/text,
 * PDF navigation/search/print actions, live file reload behavior, and
 * left-pane tab content (files + PDF table of contents + PDF thumbnails).
 */

class QAction;
class QEvent;
class QFileSystemModel;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QIntValidator;
class QListWidget;
class QListWidgetItem;
class QPoint;
class QPdfBookmarkModel;
class QPdfDocument;
class QPdfSearchModel;
class QPlainTextEdit;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QToolBar;
class QTreeView;
class PdfGraphicsView;

namespace skrat {
class PdfDocumentRibbon;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the main window and wires models, views, menus, and actions.
     * @param[in] parent Optional parent widget; typically @c nullptr for a top-level window.
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief Destroys the window and blocks disconnects PDF signals before child teardown.
     */
    ~MainWindow() override;

    /**
     * @brief Sets the file system model root to an existing directory.
     * @param[in] absolutePath Absolute directory path; if invalid, shows a warning and returns.
     */
    void setRootFolder(const QString &absolutePath);

    /**
     * @brief Selects a file or folder in the tree and previews it when under the current root.
     * @param[in] absoluteFilePath Absolute path to a file or directory visible from the root.
     */
    void selectPath(const QString &absoluteFilePath);

protected:
    /**
     * @brief Handles resize events on the image preview viewport to rescale the pixmap.
     * @param[in] watched Object receiving the event (image scroll viewport).
     * @param[in] event Qt event; @c QEvent::Resize triggers image scale refresh.
     * @return @c true if the event was handled here; otherwise base-class result.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    /**
     * @brief Previews the path for the newly selected tree item.
     * @param[in] current New selection index; invalid shows an empty-state placeholder.
     * @param[in] previous Prior selection (unused).
     */
    void onTreeCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

    /**
     * @brief Shows a context menu for Open in skrat / default app / Open With.
     * @param[in] pos Viewport-local position for menu placement.
     */
    void onTreeContextMenuRequested(const QPoint &pos);

    /** @brief Prompts for a folder and calls setRootFolder() when accepted. */
    void openFolderDialog();

    /** @brief Zooms in the active PDF or image preview by a fixed factor. */
    void zoomInPdf();

    /** @brief Zooms out the active PDF or image preview by a fixed factor. */
    void zoomOutPdf();

    /** @brief Sets PDF view zoom mode to fit-to-width when a PDF is active. */
    void pdfFitWidth();

    /** @brief Resets PDF zoom to fit width or image zoom to fit the viewport. */
    void zoomResetToViewport();

    /** @brief Jumps the PDF view to the first page when a document is loaded. */
    void pdfGoFirstPage();

    /** @brief Jumps the PDF view to the previous page if one exists. */
    void pdfGoPrevPage();

    /** @brief Jumps the PDF view to the next page if one exists. */
    void pdfGoNextPage();

    /** @brief Jumps the PDF view to the last page when a document is loaded. */
    void pdfGoLastPage();

    /** @brief Shows the find toolbar and focuses the search field for the active PDF. */
    void openPdfFind();

    /** @brief Selects the next row in the PDF search model and scrolls to that match. */
    void pdfFindNext();

    /** @brief Selects the previous row in the PDF search model and scrolls to that match. */
    void pdfFindPrev();

    /**
     * @brief Updates QPdfSearchModel::setSearchString and refreshes find UI state.
     * @param[in] text New query string (may be empty to clear matches).
     */
    void onPdfFindTextChanged(const QString &text);

    /** @brief Updates match counts, labels, and action enablement after search model changes. */
    void onPdfSearchResultsChanged();

    /** @brief Runs the print-options dialog then raster or native-vector print pipeline. */
    void printCurrentPdf();

    /** @brief Copies from the focused text/PDF widget and strips background HTML from clipboard. */
    void copyCurrentSelection();

    /** @brief Shows the About box with version, license, and project links. */
    void showAboutDialog();

    /** @brief Shows an in-window help browser with shortcuts and usage summary. */
    void showHelpDialog();

    /** @brief Writes a small launcher script to the user’s CLI path and explains PATH setup. */
    void installCommandLineTool();

    /** @brief Opens the theme and UI font preferences dialog. */
    void showThemeSettingsDialog();

    /** @brief Parses the toolbar page field and jumps the PDF to that 1-based page. */
    void onPdfPageEditReturnPressed();

    /**
     * @brief Navigates the PDF view to the bookmark’s page, location, and zoom.
     * @param[in] index Activated item in the TOC tree backed by QPdfBookmarkModel.
     */
    void onTocActivated(const QModelIndex &index);

    /** @brief Expands TOC rows and refreshes TOC/thumbnail tab enablement after bookmark changes. */
    void onPdfBookmarksChanged();

    /**
     * @brief Jumps the PDF to the page encoded on the clicked thumbnail item.
     * @param[in] item Thumbnail list item whose UserRole holds the 0-based page index.
     */
    void onPdfThumbnailActivated(QListWidgetItem *item);

    /** @brief Prompts for a PDF page or plain-text line depending on the active preview mode. */
    void goToPageOrLine();

    /** @brief Refreshes page label, toolbar state, find bar visibility, and TOC/thumbnail tabs. */
    void updatePdfPageUi();

    /**
     * @brief Starts a short debounce timer after QFileSystemWatcher reports a file change.
     * @param[in] path Path that changed (unused; reload uses the tracked preview path).
     */
    void onWatchedFileChanged(const QString &path);

    /** @brief Reloads the preview path after debounce, or shows unavailable if the file vanished. */
    void onReloadDebounceTimeout();

private:
    /** @brief Builds menus, toolbars, splitters, preview stack, and signal connections. */
    void setupUi();

    /**
     * @brief Loads and displays PDF, text, image, or placeholder content for @a absolutePath.
     * @param[in] absolutePath Absolute filesystem path to preview.
     */
    void previewPath(const QString &absolutePath);

    /**
     * @brief Shows the stacked placeholder page with rich HTML and clears file watching.
     * @param[in] html Rich text / HTML body for the placeholder QLabel.
     */
    void showPlaceholder(const QString &html);

    /**
     * @brief Heuristically classifies a path as plain text preview by extension.
     * @param[in] fi File metadata for the candidate path.
     * @return @c true if the suffix is in the allowlisted text extensions.
     */
    static bool isProbablyTextFile(const QFileInfo &fi);

    /**
     * @brief Heuristically classifies a path as image preview by suffix or QImageReader.
     * @param[in] fi File metadata for the candidate path.
     * @return @c true if the file is likely displayable as a raster or vector image.
     */
    static bool isProbablyImageFile(const QFileInfo &fi);

    /**
     * @brief Opens @a absolutePath with QDesktopServices (OS default application).
     * @param[in] absolutePath Absolute path to a file or folder.
     * @return @c true if the URL open request succeeded.
     */
    bool openPathInDefaultApp(const QString &absolutePath);

    /**
     * @brief Launches @a appPath with @a absolutePath as its argument (platform-specific).
     * @param[in] absolutePath File to open.
     * @param[in] appPath Executable or macOS .app bundle path.
     * @return @c true if QProcess::startDetached reported success.
     */
    bool openPathWithApp(const QString &absolutePath, const QString &appPath);

    /**
     * @brief Presents ranked “Open With” choices and records usage for future ranking.
     * @param[in] absolutePath File to open with a chosen handler.
     */
    void openWithForPath(const QString &absolutePath);

    /**
     * @brief Returns a stable QSettings subgroup key for a file type (usually lower-case suffix).
     * @param[in] absolutePath Path whose extension (or @c __noext__) defines the key.
     * @return Non-empty type key used under Open With settings.
     */
    QString fileTypeKeyForPath(const QString &absolutePath) const;

    /**
     * @brief Increments launch counters and timestamps for Open With ranking.
     * @param[in] fileTypeKey Result of fileTypeKeyForPath().
     * @param[in] appKey Stable identifier for the chosen application.
     * @param[in] label Human-readable label stored for menus.
     * @param[in] appPath Executable path stored for relaunch.
     */
    void recordOpenWithUsage(const QString &fileTypeKey,
                             const QString &appKey,
                             const QString &label,
                             const QString &appPath);

    /**
     * @brief Returns the platform-specific path where the CLI launcher script is installed.
     * @return Native path to @c skrat or @c skrat.cmd under the user’s local bin directory.
     */
    QString cliLauncherPath() const;

    /**
     * @brief Returns the shell or batch script body that forwards arguments to this binary.
     * @return UTF-8 script contents appropriate for the host OS.
     */
    QString cliLauncherContent() const;

    /** @brief Clears all paths from QFileSystemWatcher without changing the logical preview path. */
    void pauseWatching();

    /**
     * @brief Registers @a absoluteFilePath with QFileSystemWatcher for live reload.
     * @param[in] absoluteFilePath File to watch, or empty to clear watching only.
     */
    void setWatchedPreviewFile(const QString &absoluteFilePath);

    /**
     * @brief Shows a placeholder explaining the preview file disappeared (rename/delete).
     * @param[in] lostPath Absolute path that is no longer available on disk.
     */
    void showPreviewFileUnavailable(const QString &lostPath);

    /** @brief Enables “Go to page/line” when PDF or non-empty text preview is active. */
    void updateGoToNavigationAction();

    /** @brief Enables find/copy/print actions according to stack mode and model state. */
    void updatePdfFindActions();

    /** @brief Updates the “Match X of Y” (or similar) label next to the find field. */
    void updatePdfSearchStatus();

    /**
     * @brief Returns the number of rows in the active QPdfSearchModel.
     * @return Match count, or 0 if no model or empty query.
     */
    int pdfSearchResultCount() const;

    /**
     * @brief Selects search result @a index, jumps the PDF view, and syncs highlight state.
     * @param[in] index 0-based row in QPdfSearchModel (must be in range).
     */
    void selectPdfSearchResult(int index);

    /**
     * @brief Returns the current match index from the view or compatibility cache.
     * @return 0-based index, or -1 if none / invalid.
     */
    int currentPdfSearchResultIndex() const;

    /**
     * @brief Sets the active search-result index on the view and mirrored member state.
     * @param[in] index New current index, or -1 to clear selection.
     */
    void setCurrentPdfSearchResultIndexCompat(int index);

    /** @brief Shows or hides TOC/thumbnail tabs and rebuilds thumbnails when page count changes. */
    void updateTocPaneUi();

    /** @brief Regenerates QListWidget thumbnail icons for every PDF page. */
    void rebuildPdfThumbnails();

    /** @brief Selects the thumbnail row matching PdfGraphicsView::currentPage(). */
    void syncPdfThumbnailSelection();

    /** @brief Rescales the image QLabel from m_imageOriginalPixmap and m_imageZoomFactor. */
    void updateImagePreviewScale();

    QString m_rootPath;
    QFileSystemModel *m_fsModel = nullptr;
    QTreeView *m_tree = nullptr;
    QTabWidget *m_leftTabs = nullptr;
    QSplitter *m_splitter = nullptr;
    QStackedWidget *m_stack = nullptr;
    PdfGraphicsView *m_pdfView = nullptr;
    QPdfDocument *m_pdfDocument = nullptr;
    QPlainTextEdit *m_textView = nullptr;
    QScrollArea *m_imageScroll = nullptr;
    QLabel *m_imageView = nullptr;
    QPixmap m_imageOriginalPixmap;
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
    QListWidget *m_pdfThumbList = nullptr;
    int m_pdfThumbPageCount = -1;

    QString m_previewFilePath;
    qreal m_imageZoomFactor = 1.0;
    QString m_pendingReloadPath;
    int m_pdfSearchCurrentIndex = -1;
    bool m_updatingPdfPageEdit = false;
    bool m_pdfAutoSelectingFirstResult = false;
    QFileSystemWatcher *m_fileWatcher = nullptr;
    QTimer *m_reloadDebounceTimer = nullptr;

    bool m_shuttingDown = false; ///< True while destroying; suppresses late PDF UI updates.
};
