/**
 * @file MainWindow.cpp
 * @brief Main window implementation for skrat.
 *
 * This translation unit implements:
 * - File-tree selection and preview routing (PDF/text/placeholder).
 * - PDF navigation/search/print behavior.
 * - Toolbar + tabbed pane wiring and state updates.
 * - File-system watcher based reload handling.
 */

#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFormLayout>
#include <QGuiApplication>
#include <QInputDialog>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QComboBox>
#include <QResizeEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QModelIndex>
#include <QPdfDocument>
#include <QPdfBookmarkModel>
#include <QPdfPageNavigator>
#include <QPdfSearchModel>
#include <QPdfView>
#include <QPainter>
#include <QPen>
#include <QPlainTextEdit>
#include <QPrintDialog>
#include <QPrinter>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QWidget>
#include <QUrl>

namespace {

constexpr int kPlaceholderPage = 0;
constexpr int kPdfPage = 1;
constexpr int kTextPage = 2;
constexpr int kRasterDpiDefault = 300;

enum class PrintOutputMode : int {
    VectorNative = 0,
    RasterQt = 1,
};

/** Return true when suffix belongs to allowlisted textual extensions. */
bool hasTextualSuffix(const QString &suffixLower)
{
    static const QStringList kSuffixes = {
        QStringLiteral("txt"),
        QStringLiteral("text"),
        QStringLiteral("md"),
        QStringLiteral("markdown"),
        QStringLiteral("log"),
        QStringLiteral("csv"),
        QStringLiteral("tsv"),
        QStringLiteral("json"),
        QStringLiteral("xml"),
        QStringLiteral("html"),
        QStringLiteral("htm"),
        QStringLiteral("css"),
        QStringLiteral("js"),
        QStringLiteral("mjs"),
        QStringLiteral("cjs"),
        QStringLiteral("ts"),
        QStringLiteral("tsx"),
        QStringLiteral("jsx"),
        QStringLiteral("c"),
        QStringLiteral("cc"),
        QStringLiteral("cpp"),
        QStringLiteral("cxx"),
        QStringLiteral("h"),
        QStringLiteral("hh"),
        QStringLiteral("hpp"),
        QStringLiteral("hxx"),
        QStringLiteral("py"),
        QStringLiteral("sh"),
        QStringLiteral("bash"),
        QStringLiteral("zsh"),
        QStringLiteral("yaml"),
        QStringLiteral("yml"),
        QStringLiteral("toml"),
        QStringLiteral("ini"),
        QStringLiteral("cfg"),
        QStringLiteral("conf"),
        QStringLiteral("properties"),
        QStringLiteral("cmake"),
        QStringLiteral("pro"),
        QStringLiteral("pri"),
        QStringLiteral("qml"),
        QStringLiteral("rst"),
        QStringLiteral("svg"),
    };
    return kSuffixes.contains(suffixLower);
}

bool looksLikeSourceSuffix(const QString &suffixLower)
{
    static const QStringList kCode = {
        QStringLiteral("c"),   QStringLiteral("cc"), QStringLiteral("cpp"), QStringLiteral("cxx"),
        QStringLiteral("h"),   QStringLiteral("hh"), QStringLiteral("hpp"), QStringLiteral("hxx"),
        QStringLiteral("py"),  QStringLiteral("js"),  QStringLiteral("ts"),   QStringLiteral("tsx"),
        QStringLiteral("jsx"), QStringLiteral("qml"), QStringLiteral("cmake"),
    };
    return kCode.contains(suffixLower);
}

QIcon iconThemedOrStandard(const QString &themeName, QStyle::StandardPixmap fallback)
{
    const QIcon fromTheme = QIcon::fromTheme(themeName);
    if (!fromTheme.isNull()) {
        return fromTheme;
    }
    if (QApplication::style() != nullptr) {
        return QApplication::style()->standardIcon(fallback);
    }
    return {};
}

QIcon chevronFallbackIcon(bool forward, bool doubled)
{
    QPixmap pm(22, 22);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QApplication::palette().buttonText().color());
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);

    const int cy = 11;
    auto drawChevron = [&](int x) {
        if (forward) {
            p.drawLine(x - 3, cy - 5, x + 2, cy);
            p.drawLine(x + 2, cy, x - 3, cy + 5);
        } else {
            p.drawLine(x + 3, cy - 5, x - 2, cy);
            p.drawLine(x - 2, cy, x + 3, cy + 5);
        }
    };

    if (doubled) {
        drawChevron(forward ? 8 : 14);
        drawChevron(forward ? 14 : 8);
    } else {
        drawChevron(11);
    }
    return QIcon(pm);
}

QIcon navIcon(const QString &themeName,
              QStyle::StandardPixmap fallback,
              bool forward,
              bool doubled)
{
    QIcon icon = iconThemedOrStandard(themeName, fallback);
    if (icon.isNull()) {
        icon = chevronFallbackIcon(forward, doubled);
    }
    return icon;
}

void applyToolbarChrome(QToolBar *bar, int iconSize)
{
    if (!bar) {
        return;
    }
    bar->setIconSize(QSize(iconSize, iconSize));
    bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setContentsMargins(4, 2, 4, 2);
}

QString stripBackgroundStyles(QString html)
{
    // Remove inline background styling while keeping other formatting.
    html.remove(QRegularExpression(
        QStringLiteral(R"((?:^|;)\s*background(?:-color)?\s*:\s*[^;\"']+\s*;?)"),
        QRegularExpression::CaseInsensitiveOption));
    html.remove(QRegularExpression(
        QStringLiteral(R"(background(?:-color)?\s*=\s*["'][^"']*["'])"),
        QRegularExpression::CaseInsensitiveOption));
    return html;
}

bool supportsNativeVectorPrinting()
{
#if defined(Q_OS_WIN)
    return !QStandardPaths::findExecutable(QStringLiteral("powershell.exe")).isEmpty();
#elif defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    return !QStandardPaths::findExecutable(QStringLiteral("lp")).isEmpty();
#else
    return false;
#endif
}

bool runNativeVectorPrint(const QString &filePath)
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    if (filePath.isEmpty()) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
#else
    Q_UNUSED(filePath);
    return false;
#endif
}

} // namespace

namespace skrat {

class PdfDocumentRibbon final : public QFrame
{
public:
    explicit PdfDocumentRibbon(QToolBar *toolBar, QLabel *pageLabel, QWidget *parent = nullptr)
        : QFrame(parent)
        , m_toolBar(toolBar)
        , m_pageLabel(pageLabel)
    {
        setFrameShape(QFrame::NoFrame);
        setObjectName(QStringLiteral("pdfDocumentRibbon"));
        if (m_toolBar) {
            m_toolBar->setParent(this);
        }
        if (m_pageLabel) {
            m_pageLabel->setParent(this);
            m_pageLabel->setAlignment(Qt::AlignCenter);
            m_pageLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void recenter() { doLayout(); }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QFrame::resizeEvent(event);
        doLayout();
    }

    QSize sizeHint() const override
    {
        int h = 34;
        if (m_toolBar) {
            h = qMax(h, m_toolBar->sizeHint().height() + 6);
        }
        if (m_pageLabel) {
            h = qMax(h, m_pageLabel->sizeHint().height() + 6);
        }
        return QSize(0, h);
    }

private:
    void doLayout()
    {
        if (!m_toolBar || !m_pageLabel) {
            return;
        }
        const int rowH = qMax(1, height());
        m_toolBar->adjustSize();
        const int tw = m_toolBar->sizeHint().width();
        const int th = m_toolBar->sizeHint().height();
        const int ty = qMax(0, (rowH - th) / 2);
        m_toolBar->setGeometry(0, ty, tw, th);

        m_pageLabel->adjustSize();
        const int lw = m_pageLabel->sizeHint().width();
        const int lh = m_pageLabel->sizeHint().height();
        const int lx = qMax(0, (width() - lw) / 2);
        const int ly = qMax(0, (rowH - lh) / 2);
        m_pageLabel->setGeometry(lx, ly, lw, lh);
        m_pageLabel->raise();
    }

    QToolBar *m_toolBar = nullptr;
    QLabel *m_pageLabel = nullptr;
};

} // namespace skrat

bool MainWindow::isProbablyTextFile(const QFileInfo &fi)
{
    if (!fi.isFile()) {
        return false;
    }
    const QString suf = fi.suffix().toLower();
    if (suf.isEmpty()) {
        return false;
    }
    return hasTextualSuffix(suf);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    m_rootPath = QDir::homePath();
    m_fsModel->setRootPath(m_rootPath);
    m_tree->setRootIndex(m_fsModel->index(m_rootPath));

    connect(m_tree->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &MainWindow::onTreeCurrentChanged);

    resize(1280, 800);
    if (QScreen *scr = screen()) {
        const QRect avail = scr->availableGeometry();
        move((avail.width() - width()) / 2, (avail.height() - height()) / 2);
    }
}

MainWindow::~MainWindow()
{
    m_shuttingDown = true;

    // QPdfDocument emits pageCountChanged while clearing in its destructor. If that runs
    // after QAction children have already been destroyed, updatePdfPageUi() crashes.
    if (m_pdfDocument) {
        m_pdfDocument->blockSignals(true);
        QObject::disconnect(m_pdfDocument, nullptr, this, nullptr);
    }
    if (m_pdfView) {
        QObject::disconnect(m_pdfView->pageNavigator(), nullptr, this, nullptr);
        m_pdfView->setDocument(nullptr);
    }
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("skrat — preview"));

    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onWatchedFileChanged);
    m_reloadDebounceTimer = new QTimer(this);
    m_reloadDebounceTimer->setSingleShot(true);
    m_reloadDebounceTimer->setInterval(300);
    connect(m_reloadDebounceTimer, &QTimer::timeout, this, &MainWindow::onReloadDebounceTimeout);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *openFolder = new QAction(tr("&Open Folder…"), this);
    openFolder->setShortcut(QKeySequence::Open);
    connect(openFolder, &QAction::triggered, this, &MainWindow::openFolderDialog);
    fileMenu->addAction(openFolder);
    m_actPdfPrint = new QAction(tr("&Print PDF…"), this);
    m_actPdfPrint->setShortcut(QKeySequence::Print);
    m_actPdfPrint->setIcon(
        iconThemedOrStandard(QStringLiteral("document-print"), QStyle::SP_DialogSaveButton));
    m_actPdfPrint->setToolTip(tr("Print the current PDF (%1)")
                                  .arg(QKeySequence(QKeySequence::Print)
                                           .toString(QKeySequence::NativeText)));
    connect(m_actPdfPrint, &QAction::triggered, this, &MainWindow::printCurrentPdf);
    fileMenu->addAction(m_actPdfPrint);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    m_actCopy = new QAction(tr("&Copy"), this);
    m_actCopy->setShortcut(QKeySequence::Copy);
    connect(m_actCopy, &QAction::triggered, this, &MainWindow::copyCurrentSelection);
    editMenu->addAction(m_actCopy);
    editMenu->addSeparator();

    m_actPdfFind = new QAction(tr("&Find in PDF…"), this);
    m_actPdfFind->setShortcut(QKeySequence::Find);
    m_actPdfFind->setIcon(
        iconThemedOrStandard(QStringLiteral("edit-find"), QStyle::SP_FileDialogContentsView));
    m_actPdfFind->setToolTip(
        tr("Show the find bar and search in the PDF (%1)")
            .arg(QKeySequence(QKeySequence::Find).toString(QKeySequence::NativeText)));
    connect(m_actPdfFind, &QAction::triggered, this, &MainWindow::openPdfFind);
    editMenu->addAction(m_actPdfFind);

    m_actPdfFindNext = new QAction(tr("Find &Next"), this);
    m_actPdfFindNext->setShortcut(QKeySequence::FindNext);
    m_actPdfFindNext->setIcon(
        iconThemedOrStandard(QStringLiteral("go-down"), QStyle::SP_ArrowDown));
    m_actPdfFindNext->setToolTip(
        tr("Find next match (%1)")
            .arg(QKeySequence(QKeySequence::FindNext).toString(QKeySequence::NativeText)));
    connect(m_actPdfFindNext, &QAction::triggered, this, &MainWindow::pdfFindNext);
    editMenu->addAction(m_actPdfFindNext);

    m_actPdfFindPrev = new QAction(tr("Find &Previous"), this);
    m_actPdfFindPrev->setShortcut(QKeySequence::FindPrevious);
    m_actPdfFindPrev->setIcon(
        iconThemedOrStandard(QStringLiteral("go-up"), QStyle::SP_ArrowUp));
    m_actPdfFindPrev->setToolTip(
        tr("Find previous match (%1)")
            .arg(QKeySequence(QKeySequence::FindPrevious).toString(QKeySequence::NativeText)));
    connect(m_actPdfFindPrev, &QAction::triggered, this, &MainWindow::pdfFindPrev);
    editMenu->addAction(m_actPdfFindPrev);

    addAction(m_actCopy);
    addAction(m_actPdfFind);
    addAction(m_actPdfFindNext);
    addAction(m_actPdfFindPrev);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    auto *actFitWidth = new QAction(tr("PDF Fit &Width"), this);
    connect(actFitWidth, &QAction::triggered, this, &MainWindow::pdfFitWidth);
    viewMenu->addAction(actFitWidth);

    auto *actZoomIn = new QAction(tr("PDF Zoom &In"), this);
    actZoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(actZoomIn, &QAction::triggered, this, &MainWindow::zoomInPdf);
    viewMenu->addAction(actZoomIn);

    auto *actZoomOut = new QAction(tr("PDF Zoom &Out"), this);
    actZoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(actZoomOut, &QAction::triggered, this, &MainWindow::zoomOutPdf);
    viewMenu->addAction(actZoomOut);

    viewMenu->addSeparator();
    m_actGoToPageOrLine = new QAction(tr("&Go to page / line…"), this);
    m_actGoToPageOrLine->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    m_actGoToPageOrLine->setIcon(
        iconThemedOrStandard(QStringLiteral("go-jump"), QStyle::SP_FileDialogInfoView));
    m_actGoToPageOrLine->setToolTip(tr("In a PDF, jump to a page by number; in a text file, jump to a line. "
                                        "Shortcut: %1")
                                        .arg(QKeySequence(QStringLiteral("Ctrl+G"))
                                                 .toString(QKeySequence::NativeText)));
    connect(m_actGoToPageOrLine, &QAction::triggered, this, &MainWindow::goToPageOrLine);
    viewMenu->addAction(m_actGoToPageOrLine);
    addAction(m_actGoToPageOrLine);

    viewMenu->addSeparator();
    QMenu *const pdfPageMenu = viewMenu->addMenu(tr("PDF &pages"));

    m_pdfActFirst = new QAction(tr("&First page"), this);
    m_pdfActFirst->setShortcuts(
        {QKeySequence(QStringLiteral("Ctrl+Home")), QKeySequence(QStringLiteral("Meta+Up"))});
    m_pdfActFirst->setIcon(navIcon(QStringLiteral("media-skip-backward"),
                                   QStyle::SP_MediaSkipBackward,
                                   false,
                                   true));
    m_pdfActFirst->setToolTip(
        tr("First page (%1 / %2)")
            .arg(QKeySequence(QStringLiteral("Ctrl+Home")).toString(QKeySequence::NativeText),
                 QKeySequence(QStringLiteral("Meta+Up")).toString(QKeySequence::NativeText)));
    connect(m_pdfActFirst, &QAction::triggered, this, &MainWindow::pdfGoFirstPage);

    m_pdfActPrev = new QAction(tr("&Previous page"), this);
    m_pdfActPrev->setShortcut(QKeySequence(QStringLiteral("Alt+PgUp")));
    m_pdfActPrev->setIcon(
        navIcon(QStringLiteral("go-previous"), QStyle::SP_MediaSeekBackward, false, false));
    m_pdfActPrev->setToolTip(
        tr("Previous page (%1)").arg(QKeySequence(QStringLiteral("Alt+PgUp"))
                                         .toString(QKeySequence::NativeText)));
    connect(m_pdfActPrev, &QAction::triggered, this, &MainWindow::pdfGoPrevPage);

    m_pdfActNext = new QAction(tr("&Next page"), this);
    m_pdfActNext->setShortcut(QKeySequence(QStringLiteral("Alt+PgDown")));
    m_pdfActNext->setIcon(
        navIcon(QStringLiteral("go-next"), QStyle::SP_MediaSeekForward, true, false));
    m_pdfActNext->setToolTip(
        tr("Next page (%1)").arg(QKeySequence(QStringLiteral("Alt+PgDown"))
                                     .toString(QKeySequence::NativeText)));
    connect(m_pdfActNext, &QAction::triggered, this, &MainWindow::pdfGoNextPage);

    m_pdfActLast = new QAction(tr("&Last page"), this);
    m_pdfActLast->setShortcuts(
        {QKeySequence(QStringLiteral("Ctrl+End")), QKeySequence(QStringLiteral("Meta+Down"))});
    m_pdfActLast->setIcon(
        navIcon(QStringLiteral("media-skip-forward"), QStyle::SP_MediaSkipForward, true, true));
    m_pdfActLast->setToolTip(
        tr("Last page (%1 / %2)")
            .arg(QKeySequence(QStringLiteral("Ctrl+End")).toString(QKeySequence::NativeText),
                 QKeySequence(QStringLiteral("Meta+Down")).toString(QKeySequence::NativeText)));
    connect(m_pdfActLast, &QAction::triggered, this, &MainWindow::pdfGoLastPage);

    pdfPageMenu->addAction(m_pdfActFirst);
    pdfPageMenu->addAction(m_pdfActPrev);
    pdfPageMenu->addAction(m_pdfActNext);
    pdfPageMenu->addAction(m_pdfActLast);
    pdfPageMenu->addSeparator();
    pdfPageMenu->addAction(m_actGoToPageOrLine);

    addAction(m_pdfActFirst);
    addAction(m_pdfActPrev);
    addAction(m_pdfActNext);
    addAction(m_pdfActLast);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto *actHelp = new QAction(tr("&Help / Shortcuts…"), this);
    actHelp->setShortcut(QKeySequence::HelpContents);
    connect(actHelp, &QAction::triggered, this, &MainWindow::showHelpDialog);
    helpMenu->addAction(actHelp);

    auto *actAbout = new QAction(tr("&About skrat…"), this);
    connect(actAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
    helpMenu->addAction(actAbout);

    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_fsModel->setNameFilterDisables(false);

    m_tree = new QTreeView;
    m_tree->setModel(m_fsModel);
    m_tree->setHeaderHidden(false);
    m_tree->setAnimated(true);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::AscendingOrder);
    m_tree->setColumnWidth(0, 320);
    m_tree->setMinimumWidth(240);

    m_pdfDocument = new QPdfDocument(this);
    m_pdfBookmarkModel = new QPdfBookmarkModel(this);
    m_pdfBookmarkModel->setDocument(m_pdfDocument);
    m_pdfSearchModel = new QPdfSearchModel(this);
    m_pdfSearchModel->setDocument(m_pdfDocument);
    m_pdfView = new QPdfView;
    m_pdfView->setDocument(m_pdfDocument);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    m_pdfView->setSearchModel(m_pdfSearchModel);
#endif
    // Default is SinglePage (one page, no vertical scroll through the document).
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);

    m_textView = new QPlainTextEdit;
    m_textView->setReadOnly(true);
    m_textView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_textView->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    m_placeholder = new QLabel;
    m_placeholder->setWordWrap(true);
    m_placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_placeholder->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_placeholder->setOpenExternalLinks(true);
    m_placeholder->setMargin(12);

    m_tocView = new QTreeView;
    m_tocView->setModel(m_pdfBookmarkModel);
    m_tocView->setHeaderHidden(true);
    m_tocView->setRootIsDecorated(true);
    m_tocView->setItemsExpandable(true);
    m_tocView->setUniformRowHeights(true);

    m_tocPlaceholder = new QLabel;
    m_tocPlaceholder->setWordWrap(true);
    m_tocPlaceholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_tocPlaceholder->setMargin(8);
    m_tocPlaceholder->setText(tr("<b>Table of contents</b><br/>Open a PDF with bookmarks to view outline entries."));

    m_tocStack = new QStackedWidget;
    m_tocStack->addWidget(m_tocPlaceholder);
    m_tocStack->addWidget(m_tocView);
    m_tocStack->setCurrentWidget(m_tocPlaceholder);

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_placeholder); // kPlaceholderPage
    m_stack->addWidget(m_pdfView);     // kPdfPage
    m_stack->addWidget(m_textView);    // kTextPage
    m_stack->setCurrentIndex(kPlaceholderPage);

    m_leftTabs = new QTabWidget;
    m_leftTabs->addTab(m_tree, tr("Files"));
    m_leftTabs->addTab(m_tocStack, tr("TOC"));
    m_leftTabs->setTabEnabled(1, false);
    m_leftTabs->setCurrentWidget(m_tree);

    m_previewPane = new QWidget;
    auto *previewLayout = new QVBoxLayout(m_previewPane);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    m_pdfPageLabel = new QLabel(tr("—"));
    m_pdfPageLabel->setMinimumWidth(120);
    m_pdfPageLabel->setToolTip(
        tr("Current page and page count, centered over the preview. Use the icons on the left to move between pages or go to a page."));

    m_pdfToolBar = new QToolBar;
    applyToolbarChrome(m_pdfToolBar, 22);
    m_pdfToolBar->addAction(m_pdfActFirst);
    m_pdfToolBar->addAction(m_pdfActPrev);
    m_pdfPageEdit = new QLineEdit;
    m_pdfPageEdit->setMinimumWidth(64);
    m_pdfPageEdit->setMaximumWidth(84);
    m_pdfPageEdit->setAlignment(Qt::AlignCenter);
    m_pdfPageEdit->setPlaceholderText(tr("Page"));
    m_pdfPageEdit->setToolTip(tr("Enter a page number and press Return to jump."));
    m_pdfPageValidator = new QIntValidator(1, 1, m_pdfPageEdit);
    m_pdfPageEdit->setValidator(m_pdfPageValidator);
    m_pdfToolBar->addWidget(m_pdfPageEdit);
    m_pdfToolBar->addAction(m_pdfActNext);
    m_pdfToolBar->addAction(m_pdfActLast);
    m_pdfToolBar->addSeparator();
    m_pdfToolBar->addAction(m_actPdfPrint);

    m_pdfDocumentRibbon = new skrat::PdfDocumentRibbon(m_pdfToolBar, m_pdfPageLabel, m_previewPane);
    previewLayout->addWidget(m_pdfDocumentRibbon);

    m_pdfFindToolBar = new QToolBar(m_previewPane);
    m_pdfFindToolBar->setWindowTitle(tr("Find"));
    applyToolbarChrome(m_pdfFindToolBar, 22);
    m_pdfFindToolBar->addAction(m_actPdfFind);
    m_pdfFindToolBar->addSeparator();
    m_pdfFindToolBar->addAction(m_actPdfFindPrev);
    m_pdfFindToolBar->addAction(m_actPdfFindNext);
    m_pdfFindToolBar->addSeparator();
    m_pdfFindEdit = new QLineEdit;
    m_pdfFindEdit->setClearButtonEnabled(true);
    m_pdfFindEdit->setPlaceholderText(tr("Search text in the active PDF"));
    m_pdfFindEdit->setToolTip(
        tr("Type your search, then use the arrows to step between matches. Press Return to jump to the next one."));
    m_pdfFindEdit->setMinimumWidth(280);
    m_pdfFindEdit->setMinimumHeight(24);
    m_pdfFindToolBar->addWidget(m_pdfFindEdit);
    m_pdfFindToolBar->addSeparator();
    m_pdfFindCountLabel = new QLabel(tr("—"));
    m_pdfFindCountLabel->setMinimumWidth(140);
    m_pdfFindCountLabel->setToolTip(
        tr("Number of the active match and the total number of matches for the current search."));
    m_pdfFindToolBar->addWidget(m_pdfFindCountLabel);
    m_pdfFindToolBar->setVisible(false);

    previewLayout->addWidget(m_pdfFindToolBar);
    previewLayout->addWidget(m_stack, 1);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->addWidget(m_leftTabs);
    m_splitter->addWidget(m_previewPane);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    setCentralWidget(m_splitter);

    connect(m_pdfView->pageNavigator(),
            &QPdfPageNavigator::currentPageChanged,
            this,
            &MainWindow::updatePdfPageUi);
    connect(m_pdfDocument, &QPdfDocument::pageCountChanged, this, &MainWindow::updatePdfPageUi);
    connect(m_pdfSearchModel, &QAbstractItemModel::modelReset, this, &MainWindow::onPdfSearchResultsChanged);
    connect(m_pdfSearchModel,
            &QAbstractItemModel::rowsInserted,
            this,
            &MainWindow::onPdfSearchResultsChanged);
    connect(m_pdfSearchModel,
            &QAbstractItemModel::rowsRemoved,
            this,
            &MainWindow::onPdfSearchResultsChanged);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    connect(m_pdfView,
            &QPdfView::currentSearchResultIndexChanged,
            this,
            &MainWindow::onPdfSearchResultsChanged);
#endif
    connect(m_pdfFindEdit, &QLineEdit::textChanged, this, &MainWindow::onPdfFindTextChanged);
    connect(m_pdfFindEdit, &QLineEdit::returnPressed, this, &MainWindow::pdfFindNext);
    connect(m_pdfPageEdit, &QLineEdit::returnPressed, this, &MainWindow::onPdfPageEditReturnPressed);
    connect(m_tocView, &QTreeView::activated, this, &MainWindow::onTocActivated);
    connect(m_tocView, &QTreeView::clicked, this, &MainWindow::onTocActivated);
    connect(m_pdfBookmarkModel, &QAbstractItemModel::modelReset, this, &MainWindow::onPdfBookmarksChanged);
    connect(m_pdfBookmarkModel, &QAbstractItemModel::rowsInserted, this, &MainWindow::onPdfBookmarksChanged);
    connect(m_pdfBookmarkModel, &QAbstractItemModel::rowsRemoved, this, &MainWindow::onPdfBookmarksChanged);

    statusBar()->setSizeGripEnabled(true);

    showPlaceholder(tr("<p><b>skrat</b> — pick a file in the tree to preview PDFs and text.</p>"
                        "<p>Use <b>File → Open Folder…</b> to point the tree at a project directory.</p>"));
}

void MainWindow::setRootFolder(const QString &absolutePath)
{
    const QFileInfo fi(absolutePath);
    if (!fi.exists() || !fi.isDir()) {
        QMessageBox::warning(this, tr("Open Folder"), tr("Not a directory: %1").arg(absolutePath));
        return;
    }

    m_rootPath = fi.absoluteFilePath();
    m_fsModel->setRootPath(m_rootPath);
    m_tree->setRootIndex(m_fsModel->index(m_rootPath));
}

void MainWindow::selectPath(const QString &absoluteFilePath)
{
    const QFileInfo fi(absoluteFilePath);
    if (!fi.exists()) {
        return;
    }

    const QString abs = fi.absoluteFilePath();
    const QModelIndex idx = m_fsModel->index(abs);
    if (!idx.isValid()) {
        QMessageBox::information(
            this,
            tr("Select File"),
            tr("That path is not visible under the current tree root.\n"
               "Use File → Open Folder… to choose a root that contains the file.\n\n%1")
                .arg(abs));
        return;
    }

    m_tree->setCurrentIndex(idx);
    m_tree->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    previewPath(abs);
}

void MainWindow::onTreeCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    if (!current.isValid()) {
        showPlaceholder(tr("<p>No selection.</p>"));
        return;
    }

    const QString path = m_fsModel->filePath(current);
    previewPath(path);
}

void MainWindow::openFolderDialog()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Open Folder"),
        m_rootPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                             : m_rootPath);

    if (dir.isEmpty()) {
        return;
    }
    setRootFolder(dir);
}

void MainWindow::zoomInPdf()
{
    if (m_stack->currentWidget() != m_pdfView) {
        return;
    }
    m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
    m_pdfView->setZoomFactor(m_pdfView->zoomFactor() * 1.15);
}

void MainWindow::zoomOutPdf()
{
    if (m_stack->currentWidget() != m_pdfView) {
        return;
    }
    m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
    m_pdfView->setZoomFactor(qMax(0.1, m_pdfView->zoomFactor() / 1.15));
}

void MainWindow::pdfFitWidth()
{
    if (m_stack->currentWidget() != m_pdfView) {
        return;
    }
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
}

void MainWindow::pdfGoFirstPage()
{
    if (m_stack->currentWidget() != m_pdfView || !m_pdfDocument) {
        return;
    }
    const int pages = m_pdfDocument->pageCount();
    if (pages <= 0) {
        return;
    }
    m_pdfView->pageNavigator()->jump(0, QPointF(0, 0), 0);
}

void MainWindow::pdfGoPrevPage()
{
    if (m_stack->currentWidget() != m_pdfView || !m_pdfDocument) {
        return;
    }
    QPdfPageNavigator *const nav = m_pdfView->pageNavigator();
    const int cur = nav->currentPage();
    if (cur > 0) {
        nav->jump(cur - 1, QPointF(0, 0), 0);
    }
}

void MainWindow::pdfGoNextPage()
{
    if (m_stack->currentWidget() != m_pdfView || !m_pdfDocument) {
        return;
    }
    const int pages = m_pdfDocument->pageCount();
    if (pages <= 0) {
        return;
    }
    QPdfPageNavigator *const nav = m_pdfView->pageNavigator();
    const int cur = nav->currentPage();
    if (cur < pages - 1) {
        nav->jump(cur + 1, QPointF(0, 0), 0);
    }
}

void MainWindow::pdfGoLastPage()
{
    if (m_stack->currentWidget() != m_pdfView || !m_pdfDocument) {
        return;
    }
    const int pages = m_pdfDocument->pageCount();
    if (pages <= 0) {
        return;
    }
    m_pdfView->pageNavigator()->jump(pages - 1, QPointF(0, 0), 0);
}

void MainWindow::openPdfFind()
{
    if (m_stack->currentWidget() != m_pdfView) {
        return;
    }
    if (m_pdfFindToolBar) {
        m_pdfFindToolBar->setVisible(true);
    }
    if (m_pdfFindEdit) {
        m_pdfFindEdit->setFocus(Qt::ShortcutFocusReason);
        m_pdfFindEdit->selectAll();
    }
    updatePdfFindActions();
}

void MainWindow::onPdfFindTextChanged(const QString &text)
{
    if (!m_pdfSearchModel) {
        return;
    }
    m_pdfSearchModel->setSearchString(text);
    setCurrentPdfSearchResultIndexCompat(-1);
    updatePdfFindActions();
    updatePdfSearchStatus();
}

void MainWindow::onPdfSearchResultsChanged()
{
    if (!m_pdfAutoSelectingFirstResult && m_pdfFindEdit
        && !m_pdfFindEdit->text().trimmed().isEmpty()) {
        const int total = pdfSearchResultCount();
        const int current = currentPdfSearchResultIndex();
        if (total > 0 && (current < 0 || current >= total)) {
            m_pdfAutoSelectingFirstResult = true;
            selectPdfSearchResult(0);
            m_pdfAutoSelectingFirstResult = false;
        }
    }
    updatePdfFindActions();
    updatePdfSearchStatus();
}

int MainWindow::pdfSearchResultCount() const
{
    if (!m_pdfSearchModel) {
        return 0;
    }
    return m_pdfSearchModel->rowCount(QModelIndex());
}

int MainWindow::currentPdfSearchResultIndex() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (m_pdfView) {
        return m_pdfView->currentSearchResultIndex();
    }
#endif
    return m_pdfSearchCurrentIndex;
}

void MainWindow::setCurrentPdfSearchResultIndexCompat(int index)
{
    m_pdfSearchCurrentIndex = index;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    // Keep Qt's bright cyan "current result" frame disabled; we use the softer
    // rectangular background boxes that QPdfView draws for all matches.
    if (m_pdfView) {
        m_pdfView->setCurrentSearchResultIndex(-1);
    }
#endif
}

void MainWindow::selectPdfSearchResult(int index)
{
    if (!m_pdfView || !m_pdfSearchModel) {
        return;
    }
    const int total = pdfSearchResultCount();
    if (index < 0 || index >= total) {
        return;
    }
    setCurrentPdfSearchResultIndexCompat(index);
    const QPdfLink link = m_pdfSearchModel->resultAtIndex(index);
    if (link.isValid()) {
        m_pdfView->pageNavigator()->jump(link.page(), link.location(), link.zoom());
    }
}

void MainWindow::pdfFindNext()
{
    if (m_stack->currentWidget() != m_pdfView || !m_pdfSearchModel || !m_pdfView) {
        return;
    }
    const int total = pdfSearchResultCount();
    if (total <= 0) {
        return;
    }
    const int current = currentPdfSearchResultIndex();
    const int next = (current + 1 + total) % total;
    selectPdfSearchResult(next);
}

void MainWindow::pdfFindPrev()
{
    if (m_stack->currentWidget() != m_pdfView || !m_pdfSearchModel || !m_pdfView) {
        return;
    }
    const int total = pdfSearchResultCount();
    if (total <= 0) {
        return;
    }
    const int current = currentPdfSearchResultIndex();
    const int prev = (current - 1 + total) % total;
    selectPdfSearchResult(prev);
}

void MainWindow::printCurrentPdf()
{
    if (m_stack->currentWidget() != m_pdfView || !m_pdfDocument || m_pdfDocument->pageCount() <= 0) {
        return;
    }

    PrintOutputMode outputMode = PrintOutputMode::RasterQt;
    int rasterDpi = kRasterDpiDefault;
    {
        QDialog optionsDialog(this);
        optionsDialog.setWindowTitle(tr("Print Options"));

        auto *layout = new QFormLayout(&optionsDialog);
        auto *modeCombo = new QComboBox(&optionsDialog);
        const bool nativeVectorSupported = supportsNativeVectorPrinting();
        modeCombo->addItem(tr("Rasterized by skrat (Qt print dialog)"),
                           static_cast<int>(PrintOutputMode::RasterQt));
        if (nativeVectorSupported) {
            modeCombo->addItem(tr("Native PDF (vector, open in system viewer to print)"),
                               static_cast<int>(PrintOutputMode::VectorNative));
        }
        modeCombo->setToolTip(
            tr("Raster mode prints in-app. Native mode opens the PDF in your system viewer so you can use native print options, including page ranges."));
        modeCombo->setCurrentIndex(0);

        auto *dpiCombo = new QComboBox(&optionsDialog);
        dpiCombo->addItem(tr("300 DPI"), 300);
        dpiCombo->addItem(tr("600 DPI"), 600);
        dpiCombo->setCurrentIndex(0);
        dpiCombo->setToolTip(tr("Used only when rasterized printing is selected."));

        layout->addRow(tr("Output mode:"), modeCombo);
        layout->addRow(tr("Raster DPI:"), dpiCombo);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                             Qt::Horizontal,
                                             &optionsDialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &optionsDialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &optionsDialog, &QDialog::reject);
        connect(modeCombo,
                &QComboBox::currentIndexChanged,
                &optionsDialog,
                [modeCombo, dpiCombo](int) {
                    const auto mode =
                        static_cast<PrintOutputMode>(modeCombo->currentData().toInt());
                    dpiCombo->setEnabled(mode == PrintOutputMode::RasterQt);
                });
        dpiCombo->setEnabled(true);

        if (optionsDialog.exec() != QDialog::Accepted) {
            statusBar()->showMessage(tr("Print canceled."), 2000);
            return;
        }

        outputMode = static_cast<PrintOutputMode>(modeCombo->currentData().toInt());
        rasterDpi = dpiCombo->currentData().toInt();
    }

    if (outputMode == PrintOutputMode::VectorNative) {
        if (runNativeVectorPrint(m_previewFilePath)) {
            statusBar()->showMessage(tr("Opened PDF in system viewer for vector print."), 3500);
        } else {
            QMessageBox::warning(this,
                                 tr("Print PDF"),
                                 tr("Vector print mode is not available on this system."));
            statusBar()->showMessage(tr("Vector print failed."), 3000);
        }
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QFileInfo(m_previewFilePath).fileName());
    printer.setResolution(qMax(kRasterDpiDefault, rasterDpi));
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print PDF (Raster)"));
    dialog.setOption(QAbstractPrintDialog::PrintPageRange, true);
    if (dialog.exec() != QDialog::Accepted) {
        statusBar()->showMessage(tr("Print canceled."), 2000);
        return;
    }

    QPainter painter(&printer);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (!painter.isActive()) {
        QMessageBox::warning(this, tr("Print PDF"), tr("Could not start printing."));
        statusBar()->showMessage(tr("Print failed."), 3000);
        return;
    }

    const int pages = m_pdfDocument->pageCount();
    int startPage = 0;
    int endPage = pages - 1;
    if (printer.printRange() == QPrinter::PageRange) {
        startPage = qMax(0, printer.fromPage() - 1);
        endPage = qMin(pages - 1, printer.toPage() - 1);
        if (startPage > endPage) {
            std::swap(startPage, endPage);
        }
    }

    bool firstPrinted = true;
    for (int page = startPage; page <= endPage; ++page) {
        if (!firstPrinted) {
            printer.newPage();
        }
        firstPrinted = false;
        const QSizeF pagePt = m_pdfDocument->pagePointSize(page);
        const qreal scale = static_cast<qreal>(printer.resolution()) / 72.0;
        const QSize pagePx(qMax(1, qRound(pagePt.width() * scale)),
                           qMax(1, qRound(pagePt.height() * scale)));
        if (pagePx.isEmpty()) {
            continue;
        }
        const QImage image = m_pdfDocument->render(page, pagePx);
        if (image.isNull()) {
            continue;
        }
        const QRect target = painter.viewport();
        const QSize scaled = image.size().scaled(target.size(), Qt::KeepAspectRatio);
        const QRect centered((target.width() - scaled.width()) / 2,
                             (target.height() - scaled.height()) / 2,
                             scaled.width(),
                             scaled.height());
        painter.drawImage(centered, image);
    }
    statusBar()->showMessage(tr("Sent PDF to printer (raster mode)."), 3000);
}

void MainWindow::copyCurrentSelection()
{
    QWidget *target = QApplication::focusWidget();
    if (!target) {
        target = m_stack->currentWidget();
    }
    if (!target) {
        statusBar()->showMessage(tr("Nothing to copy."), 2000);
        return;
    }

    bool attemptedCopy = false;
    if (target == m_textView || m_textView->isAncestorOf(target)) {
        m_textView->copy();
        attemptedCopy = true;
    } else if ((target == m_pdfView || m_pdfView->isAncestorOf(target))
               && m_stack->currentWidget() == m_pdfView) {
        if (!m_pdfView->hasFocus()) {
            m_pdfView->setFocus(Qt::ShortcutFocusReason);
        }
        QKeyEvent press(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier, QStringLiteral("c"));
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_C, Qt::ControlModifier, QStringLiteral("c"));
        QApplication::sendEvent(m_pdfView, &press);
        QApplication::sendEvent(m_pdfView, &release);
        attemptedCopy = true;
    } else if (target) {
        if (auto *plain = qobject_cast<QPlainTextEdit *>(target)) {
            plain->copy();
            attemptedCopy = true;
        } else {
            attemptedCopy = QMetaObject::invokeMethod(target, "copy", Qt::DirectConnection);
        }
    }

    QClipboard *const clipboard = QGuiApplication::clipboard();
    if (!clipboard || !clipboard->mimeData()) {
        if (attemptedCopy) {
            statusBar()->showMessage(tr("Copied selection."), 2000);
        }
        return;
    }
    const QMimeData *const src = clipboard->mimeData();
    if (!src->hasHtml()) {
        if (attemptedCopy) {
            statusBar()->showMessage(tr("Copied selection."), 2000);
        }
        return;
    }
    auto *clean = new QMimeData;
    clean->setHtml(stripBackgroundStyles(src->html()));
    if (src->hasText()) {
        clean->setText(src->text());
    }
    if (src->hasImage()) {
        clean->setImageData(src->imageData());
    }
    clipboard->setMimeData(clean);
    if (attemptedCopy) {
        statusBar()->showMessage(tr("Copied selection (background colors removed)."), 2500);
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(
        this,
        tr("About skrat"),
        tr("<p><b>skrat</b> %1</p>"
           "<p>Read-only desktop viewer for PDFs and text files.</p>"
           "<p><b>Author / Maintainer:</b> CodeMonki</p>"
           "<p><b>License:</b> GPL-3.0-or-later (code), CC-BY-SA-4.0 (docs)</p>"
           "<p><b>Third-party:</b> Uses Qt framework under LGPLv3/GPL/commercial terms as applicable "
           "(see <a href='https://www.qt.io/licensing'>qt.io/licensing</a>).</p>"
           "<p>Project: <a href='https://github.com/code-monki/skrat'>github.com/code-monki/skrat</a></p>")
            .arg(QString::fromUtf8(SKRAT_APP_VERSION)));
}

void MainWindow::showHelpDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("skrat Help"));
    dlg.resize(760, 560);
    auto *layout = new QVBoxLayout(&dlg);
    auto *browser = new QTextBrowser(&dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        tr("<h2>skrat Help</h2>"
           "<p><b>Purpose:</b> read-only preview of PDFs and common text files.</p>"
           "<h3>Basics</h3>"
           "<ul>"
           "<li>Use the <b>Files</b> tab on the left to browse and select files.</li>"
           "<li>When a PDF has bookmarks, the <b>TOC</b> tab becomes available.</li>"
           "<li>Use toolbar controls to navigate pages, print, and search.</li>"
           "</ul>"
           "<h3>Printing</h3>"
           "<ul>"
           "<li><b>File → Print PDF…</b> opens print options first.</li>"
           "<li><b>Rasterized by skrat</b> prints in-app and supports page ranges.</li>"
           "<li><b>Native PDF</b> opens the file in the system viewer for vector-quality printing and native controls.</li>"
           "</ul>"
           "<h3>Keyboard shortcuts</h3>"
           "<ul>"
           "<li><b>Open folder:</b> %1</li>"
           "<li><b>Print PDF:</b> %2</li>"
           "<li><b>Find in PDF:</b> %3</li>"
           "<li><b>Find next / previous:</b> %4 / %5</li>"
           "<li><b>Copy:</b> %6</li>"
           "<li><b>Go to page / line:</b> Ctrl+G</li>"
           "<li><b>First / Last page:</b> Ctrl+Home or Cmd+Up / Ctrl+End or Cmd+Down</li>"
           "<li><b>Prev / Next page:</b> Alt+PgUp / Alt+PgDown</li>"
           "<li><b>Zoom in / out:</b> %7 / %8</li>"
           "</ul>"
           "<p>More details: <a href='https://github.com/code-monki/skrat#readme'>README</a></p>")
            .arg(QKeySequence(QKeySequence::Open).toString(QKeySequence::NativeText),
                 QKeySequence(QKeySequence::Print).toString(QKeySequence::NativeText),
                 QKeySequence(QKeySequence::Find).toString(QKeySequence::NativeText),
                 QKeySequence(QKeySequence::FindNext).toString(QKeySequence::NativeText),
                 QKeySequence(QKeySequence::FindPrevious).toString(QKeySequence::NativeText),
                 QKeySequence(QKeySequence::Copy).toString(QKeySequence::NativeText),
                 QKeySequence(QKeySequence::ZoomIn).toString(QKeySequence::NativeText),
                 QKeySequence(QKeySequence::ZoomOut).toString(QKeySequence::NativeText)));
    layout->addWidget(browser);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    layout->addWidget(buttons);
    dlg.exec();
}

void MainWindow::onPdfPageEditReturnPressed()
{
    if (!m_pdfPageEdit || !m_pdfDocument || !m_pdfView || m_stack->currentWidget() != m_pdfView) {
        return;
    }
    if (m_updatingPdfPageEdit) {
        return;
    }
    const int pages = m_pdfDocument->pageCount();
    if (pages <= 0) {
        return;
    }

    bool ok = false;
    const int pageOneBased = m_pdfPageEdit->text().trimmed().toInt(&ok);
    if (!ok || pageOneBased < 1 || pageOneBased > pages) {
        QMessageBox::warning(this,
                             tr("Go to Page"),
                             tr("Page number out of range. Enter a value between 1 and %1.").arg(pages));
        m_updatingPdfPageEdit = true;
        m_pdfPageEdit->setText(QString::number(m_pdfView->pageNavigator()->currentPage() + 1));
        m_updatingPdfPageEdit = false;
        m_pdfPageEdit->selectAll();
        return;
    }
    m_pdfView->pageNavigator()->jump(pageOneBased - 1, QPointF(0, 0), 0);
}

void MainWindow::onTocActivated(const QModelIndex &index)
{
    if (!index.isValid() || !m_pdfView || !m_pdfBookmarkModel || m_stack->currentWidget() != m_pdfView) {
        return;
    }
    const QVariant pageVar = m_pdfBookmarkModel->data(index, static_cast<int>(QPdfBookmarkModel::Role::Page));
    if (!pageVar.isValid()) {
        return;
    }
    const int page = pageVar.toInt();
    const QVariant locVar =
        m_pdfBookmarkModel->data(index, static_cast<int>(QPdfBookmarkModel::Role::Location));
    const QVariant zoomVar = m_pdfBookmarkModel->data(index, static_cast<int>(QPdfBookmarkModel::Role::Zoom));
    const QPointF location = locVar.isValid() ? locVar.toPointF() : QPointF(0, 0);
    const qreal zoom = zoomVar.isValid() ? zoomVar.toReal() : 0.0;
    m_pdfView->pageNavigator()->jump(page, location, zoom);
}

void MainWindow::onPdfBookmarksChanged()
{
    if (m_tocView) {
        m_tocView->expandToDepth(1);
    }
    updateTocPaneUi();
}

void MainWindow::updateTocPaneUi()
{
    if (!m_leftTabs || !m_tocStack || !m_tocPlaceholder || !m_tocView || !m_pdfBookmarkModel) {
        return;
    }
    const bool pdfActive = (m_stack && m_stack->currentWidget() == m_pdfView);
    const int bookmarks = m_pdfBookmarkModel->rowCount(QModelIndex());
    if (!pdfActive) {
        m_tocPlaceholder->setText(
            tr("<b>Table of contents</b><br/>Open a PDF with bookmarks to view outline entries."));
        m_tocStack->setCurrentWidget(m_tocPlaceholder);
        m_leftTabs->setTabEnabled(1, false);
        return;
    }
    m_leftTabs->setTabEnabled(1, true);
    if (bookmarks <= 0) {
        m_tocPlaceholder->setText(
            tr("<b>Table of contents</b><br/>No outline entries were found in this PDF."));
        m_tocStack->setCurrentWidget(m_tocPlaceholder);
        return;
    }
    m_tocStack->setCurrentWidget(m_tocView);
}

void MainWindow::goToPageOrLine()
{
    if (m_stack->currentWidget() == m_pdfView && m_pdfDocument) {
        const int pages = m_pdfDocument->pageCount();
        if (pages <= 0) {
            return;
        }
        QPdfPageNavigator *const nav = m_pdfView->pageNavigator();
        const int cur = nav->currentPage();
        bool ok = false;
        const int pageOneBased = QInputDialog::getInt(
            this,
            tr("Go to Page"),
            tr("Page number (1–%1):").arg(pages),
            cur + 1,
            1,
            pages,
            1,
            &ok);
        if (!ok) {
            return;
        }
        nav->jump(pageOneBased - 1, QPointF(0, 0), 0);
        return;
    }

    if (m_stack->currentWidget() == m_textView && m_textView) {
        QTextDocument *const doc = m_textView->document();
        const int lineCount = doc->blockCount();
        if (lineCount <= 0) {
            return;
        }
        const int currentLine = m_textView->textCursor().block().blockNumber() + 1;
        bool ok = false;
        const int lineOneBased = QInputDialog::getInt(
            this,
            tr("Go to Line"),
            tr("Plain text has no fixed pages; enter a line number (1–%1):").arg(lineCount),
            qBound(1, currentLine, lineCount),
            1,
            lineCount,
            1,
            &ok);
        if (!ok) {
            return;
        }
        const QTextBlock block = doc->findBlockByNumber(lineOneBased - 1);
        if (!block.isValid()) {
            return;
        }
        QTextCursor cursor(block);
        cursor.movePosition(QTextCursor::StartOfLine);
        m_textView->setTextCursor(cursor);
        m_textView->centerCursor();
        m_textView->setFocus(Qt::OtherFocusReason);
    }
}

void MainWindow::updatePdfPageUi()
{
    if (m_shuttingDown) {
        return;
    }

    const bool pdfActive = (m_stack->currentWidget() == m_pdfView);
    const int pages = m_pdfDocument ? m_pdfDocument->pageCount() : 0;
    const bool showBar = pdfActive && pages > 0;

    if (m_pdfDocumentRibbon) {
        m_pdfDocumentRibbon->setVisible(showBar);
    }

    if (!showBar) {
        if (m_pdfPageLabel) {
            m_pdfPageLabel->setText(tr("—"));
        }
        if (m_pdfPageEdit) {
            m_updatingPdfPageEdit = true;
            m_pdfPageEdit->clear();
            m_pdfPageEdit->setEnabled(false);
            m_updatingPdfPageEdit = false;
        }
        if (m_pdfDocumentRibbon) {
            m_pdfDocumentRibbon->recenter();
        }
        if (m_pdfFindToolBar) {
            m_pdfFindToolBar->setVisible(false);
        }
        if (m_pdfActFirst) {
            m_pdfActFirst->setEnabled(false);
            m_pdfActPrev->setEnabled(false);
            m_pdfActNext->setEnabled(false);
            m_pdfActLast->setEnabled(false);
        }
        statusBar()->clearMessage();
        updateGoToNavigationAction();
        updatePdfFindActions();
        updatePdfSearchStatus();
        updateTocPaneUi();
        return;
    }

    QPdfPageNavigator *const nav = m_pdfView->pageNavigator();
    const int cur = nav->currentPage();
    if (m_pdfPageLabel) {
        m_pdfPageLabel->setText(tr("Page %1 of %2").arg(cur + 1).arg(pages));
    }
    if (m_pdfPageEdit) {
        m_updatingPdfPageEdit = true;
        m_pdfPageEdit->setEnabled(true);
        m_pdfPageEdit->setText(QString::number(cur + 1));
        if (m_pdfPageValidator) {
            m_pdfPageValidator->setRange(1, pages);
        }
        m_updatingPdfPageEdit = false;
    }
    if (m_pdfDocumentRibbon) {
        m_pdfDocumentRibbon->recenter();
    }
    m_pdfActFirst->setEnabled(cur > 0);
    m_pdfActPrev->setEnabled(cur > 0);
    m_pdfActNext->setEnabled(cur < pages - 1);
    m_pdfActLast->setEnabled(cur < pages - 1);

    if (pages > 1) {
        statusBar()->showMessage(
            tr("PDF: %1 pages — scroll for continuous view, or use toolbar / View → PDF pages "
               "(Alt+PgUp / Alt+PgDown, Ctrl+Home / Ctrl+End).")
                .arg(pages),
            0);
    } else {
        statusBar()->showMessage(
            tr("PDF: 1 page — use View → PDF pages or toolbar if you switch to single-page layout later."),
            0);
    }

    if (m_actPdfPrint) {
        m_actPdfPrint->setEnabled(showBar);
    }
    updateGoToNavigationAction();
    updatePdfFindActions();
    updatePdfSearchStatus();
    updateTocPaneUi();
}

void MainWindow::updateGoToNavigationAction()
{
    if (!m_actGoToPageOrLine) {
        return;
    }

    if (m_stack->currentWidget() == m_pdfView && m_pdfDocument && m_pdfDocument->pageCount() > 0) {
        m_actGoToPageOrLine->setEnabled(true);
        return;
    }
    if (m_stack->currentWidget() == m_textView && m_textView && m_textView->document()
        && m_textView->document()->blockCount() > 0) {
        m_actGoToPageOrLine->setEnabled(true);
        return;
    }

    m_actGoToPageOrLine->setEnabled(false);
}

void MainWindow::updatePdfFindActions()
{
    const bool pdfActive = (m_stack->currentWidget() == m_pdfView);
    const bool hasDocument = pdfActive && m_pdfDocument && m_pdfDocument->pageCount() > 0;
    const bool hasQuery = m_pdfFindEdit && !m_pdfFindEdit->text().trimmed().isEmpty();
    const bool hasResults = (pdfSearchResultCount() > 0);

    if (m_actPdfFind) {
        m_actPdfFind->setEnabled(hasDocument);
    }
    if (m_actPdfFindNext) {
        m_actPdfFindNext->setEnabled(hasDocument && hasQuery && hasResults);
    }
    if (m_actPdfFindPrev) {
        m_actPdfFindPrev->setEnabled(hasDocument && hasQuery && hasResults);
    }
    if (m_actPdfPrint) {
        m_actPdfPrint->setEnabled(hasDocument);
    }
    if (m_actCopy) {
        m_actCopy->setEnabled(m_stack->currentWidget() == m_pdfView || m_stack->currentWidget() == m_textView);
    }
}

void MainWindow::updatePdfSearchStatus()
{
    if (!m_pdfFindCountLabel || !m_pdfView || !m_pdfSearchModel) {
        return;
    }
    const QString query = m_pdfFindEdit ? m_pdfFindEdit->text().trimmed() : QString();
    const int total = pdfSearchResultCount();
    if (query.isEmpty()) {
        m_pdfFindCountLabel->setText(tr("Type to search"));
        return;
    }
    if (total <= 0) {
        m_pdfFindCountLabel->setText(tr("No matches"));
        return;
    }
    const int current = currentPdfSearchResultIndex();
    const int oneBased = (current >= 0 && current < total) ? current + 1 : 0;
    m_pdfFindCountLabel->setText(tr("Match %1 of %2").arg(oneBased).arg(total));
}

void MainWindow::showPlaceholder(const QString &html)
{
    setWatchedPreviewFile(QString());
    m_placeholder->setText(html);
    m_stack->setCurrentIndex(kPlaceholderPage);
    updatePdfPageUi();
}

void MainWindow::pauseWatching()
{
    if (!m_fileWatcher) {
        return;
    }
    const QStringList watched = m_fileWatcher->files();
    if (!watched.isEmpty()) {
        m_fileWatcher->removePaths(watched);
    }
}

void MainWindow::setWatchedPreviewFile(const QString &absoluteFilePath)
{
    pauseWatching();
    m_previewFilePath = absoluteFilePath;
    if (absoluteFilePath.isEmpty()) {
        return;
    }
    const QFileInfo fi(absoluteFilePath);
    if (!fi.exists() || !fi.isFile()) {
        return;
    }
    if (!m_fileWatcher->addPath(absoluteFilePath)) {
        qWarning() << "QFileSystemWatcher::addPath failed for" << absoluteFilePath;
    }
}

void MainWindow::showPreviewFileUnavailable(const QString &lostPath)
{
    setWatchedPreviewFile(QString());
    if (m_pdfDocument) {
        m_pdfDocument->close();
    }
    m_placeholder->setText(
        tr("<p><b>File no longer available</b></p><p>%1</p>"
           "<p>It was removed or renamed on disk while the preview was open.</p>")
            .arg(lostPath.toHtmlEscaped()));
    m_stack->setCurrentIndex(kPlaceholderPage);
    updatePdfPageUi();
}

void MainWindow::onWatchedFileChanged(const QString &path)
{
    Q_UNUSED(path);
    if (m_previewFilePath.isEmpty()) {
        return;
    }
    m_pendingReloadPath = m_previewFilePath;
    m_reloadDebounceTimer->start();
}

void MainWindow::onReloadDebounceTimeout()
{
    const QString path = m_pendingReloadPath;
    m_pendingReloadPath.clear();
    if (path.isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(path)) {
        showPreviewFileUnavailable(path);
        return;
    }
    previewPath(path);
}

void MainWindow::previewPath(const QString &absolutePath)
{
    m_reloadDebounceTimer->stop();
    m_pendingReloadPath.clear();
    pauseWatching();

    const QFileInfo fi(absolutePath);

    if (!fi.exists()) {
        showPlaceholder(tr("<p><b>File not found</b></p><p>%1</p>").arg(absolutePath.toHtmlEscaped()));
        return;
    }

    if (fi.isDir()) {
        showPlaceholder(tr("<p><b>Folder</b></p><p>%1</p><p>Select a file to preview.</p>")
                            .arg(fi.absoluteFilePath().toHtmlEscaped()));
        return;
    }

    if (!fi.isFile()) {
        showPlaceholder(tr("<p>Not a file: %1</p>").arg(absolutePath.toHtmlEscaped()));
        return;
    }

    const QString suffix = fi.suffix().toLower();
    if (suffix == QStringLiteral("pdf")) {
        m_pdfDocument->close();
        const QPdfDocument::Error loadError = m_pdfDocument->load(fi.absoluteFilePath());
        if (loadError != QPdfDocument::Error::None) {
            QString errStr;
            switch (loadError) {
            case QPdfDocument::Error::Unknown:
                errStr = QStringLiteral("Unknown");
                break;
            case QPdfDocument::Error::DataNotYetAvailable:
                errStr = QStringLiteral("DataNotYetAvailable");
                break;
            case QPdfDocument::Error::FileNotFound:
                errStr = QStringLiteral("FileNotFound");
                break;
            case QPdfDocument::Error::InvalidFileFormat:
                errStr = QStringLiteral("InvalidFileFormat");
                break;
            case QPdfDocument::Error::IncorrectPassword:
                errStr = QStringLiteral("IncorrectPassword");
                break;
            case QPdfDocument::Error::UnsupportedSecurityScheme:
                errStr = QStringLiteral("UnsupportedSecurityScheme");
                break;
            default:
                errStr = QStringLiteral("Other");
                break;
            }
            showPlaceholder(tr("<p><b>Failed to load PDF</b></p><p>%1</p><p>Error: %2</p>")
                                .arg(fi.absoluteFilePath().toHtmlEscaped(), errStr.toHtmlEscaped()));
            qWarning() << "QPdfDocument::load failed:" << fi.absoluteFilePath() << "error" << errStr;
            return;
        }

        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        m_stack->setCurrentIndex(kPdfPage);
        setWatchedPreviewFile(fi.absoluteFilePath());
        updatePdfPageUi();
        return;
    }

    if (isProbablyTextFile(fi)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) {
            showPlaceholder(tr("<p><b>Could not open file</b></p><p>%1</p>")
                                .arg(fi.absoluteFilePath().toHtmlEscaped()));
            return;
        }

        const QByteArray bytes = f.readAll();
        f.close();

        // Prefer UTF-8; invalid sequences become replacement characters.
        const QString text = QString::fromUtf8(bytes);

        QFont font = m_textView->font();
        if (looksLikeSourceSuffix(suffix)) {
            const QFont fixed = QFont(QStringLiteral("Monospace"));
            if (fixed.exactMatch() || !fixed.family().isEmpty()) {
                font = fixed;
            }
        }
        m_textView->setFont(font);

        m_textView->setPlainText(text);
        m_stack->setCurrentIndex(kTextPage);
        setWatchedPreviewFile(fi.absoluteFilePath());
        updatePdfPageUi();
        return;
    }

    showPlaceholder(
        tr("<p><b>Unsupported file type</b> for preview.</p>"
           "<p>%1</p>"
           "<p>Supported: <b>PDF</b> and common <b>text</b> extensions (txt, md, json, code, …).</p>")
            .arg(fi.absoluteFilePath().toHtmlEscaped()));
}
