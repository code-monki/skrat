#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QFont>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QModelIndex>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfSearchModel>
#include <QPdfView>
#include <QPainter>
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
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QWidget>

namespace {

constexpr int kPlaceholderPage = 0;
constexpr int kPdfPage = 1;
constexpr int kTextPage = 2;

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

} // namespace

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
    m_pdfActFirst->setShortcut(QKeySequence(QStringLiteral("Ctrl+Home")));
    m_pdfActFirst->setIcon(
        iconThemedOrStandard(QStringLiteral("go-first"), QStyle::SP_MediaSkipBackward));
    m_pdfActFirst->setToolTip(
        tr("First page (%1)").arg(QKeySequence(QStringLiteral("Ctrl+Home"))
                                      .toString(QKeySequence::NativeText)));
    connect(m_pdfActFirst, &QAction::triggered, this, &MainWindow::pdfGoFirstPage);

    m_pdfActPrev = new QAction(tr("&Previous page"), this);
    m_pdfActPrev->setShortcut(QKeySequence(QStringLiteral("Alt+PgUp")));
    m_pdfActPrev->setIcon(
        iconThemedOrStandard(QStringLiteral("go-previous"), QStyle::SP_MediaSeekBackward));
    m_pdfActPrev->setToolTip(
        tr("Previous page (%1)").arg(QKeySequence(QStringLiteral("Alt+PgUp"))
                                         .toString(QKeySequence::NativeText)));
    connect(m_pdfActPrev, &QAction::triggered, this, &MainWindow::pdfGoPrevPage);

    m_pdfActNext = new QAction(tr("&Next page"), this);
    m_pdfActNext->setShortcut(QKeySequence(QStringLiteral("Alt+PgDown")));
    m_pdfActNext->setIcon(
        iconThemedOrStandard(QStringLiteral("go-next"), QStyle::SP_MediaSeekForward));
    m_pdfActNext->setToolTip(
        tr("Next page (%1)").arg(QKeySequence(QStringLiteral("Alt+PgDown"))
                                     .toString(QKeySequence::NativeText)));
    connect(m_pdfActNext, &QAction::triggered, this, &MainWindow::pdfGoNextPage);

    m_pdfActLast = new QAction(tr("&Last page"), this);
    m_pdfActLast->setShortcut(QKeySequence(QStringLiteral("Ctrl+End")));
    m_pdfActLast->setIcon(
        iconThemedOrStandard(QStringLiteral("go-last"), QStyle::SP_MediaSkipForward));
    m_pdfActLast->setToolTip(
        tr("Last page (%1)").arg(QKeySequence(QStringLiteral("Ctrl+End"))
                                     .toString(QKeySequence::NativeText)));
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
    m_pdfSearchModel = new QPdfSearchModel(this);
    m_pdfSearchModel->setDocument(m_pdfDocument);
    m_pdfView = new QPdfView;
    m_pdfView->setDocument(m_pdfDocument);
    m_pdfView->setSearchModel(m_pdfSearchModel);
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

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_placeholder); // kPlaceholderPage
    m_stack->addWidget(m_pdfView);     // kPdfPage
    m_stack->addWidget(m_textView);    // kTextPage
    m_stack->setCurrentIndex(kPlaceholderPage);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->addWidget(m_tree);
    m_splitter->addWidget(m_stack);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    setCentralWidget(m_splitter);

    m_pdfToolBar = addToolBar(tr("PDF"));
    applyToolbarChrome(m_pdfToolBar, 22);
    m_pdfToolBar->addAction(m_pdfActFirst);
    m_pdfToolBar->addAction(m_pdfActPrev);
    m_pdfToolBar->addAction(m_pdfActNext);
    m_pdfToolBar->addAction(m_pdfActLast);
    m_pdfToolBar->addSeparator();
    m_pdfToolBar->addAction(m_actGoToPageOrLine);
    m_pdfToolBar->addSeparator();
    m_pdfToolBar->addAction(m_actPdfPrint);
    m_pdfToolBar->addSeparator();
    auto *pdfBarSpacer = new QWidget;
    pdfBarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_pdfToolBar->addWidget(pdfBarSpacer);
    m_pdfPageLabel = new QLabel(tr("—"));
    m_pdfPageLabel->setMinimumWidth(140);
    m_pdfPageLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_pdfPageLabel->setToolTip(
        tr("Page position in the current PDF. Use the icons to the left to move between pages, or go to a page."));
    m_pdfToolBar->addWidget(m_pdfPageLabel);

    m_pdfFindToolBar = addToolBar(tr("Find"));
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

    connect(m_pdfView->pageNavigator(),
            &QPdfPageNavigator::currentPageChanged,
            this,
            &MainWindow::updatePdfPageUi);
    connect(m_pdfDocument, &QPdfDocument::pageCountChanged, this, &MainWindow::updatePdfPageUi);
    connect(m_pdfSearchModel,
            &QPdfSearchModel::countChanged,
            this,
            &MainWindow::onPdfSearchResultsChanged);
    connect(m_pdfView,
            &QPdfView::currentSearchResultIndexChanged,
            this,
            &MainWindow::onPdfSearchResultsChanged);
    connect(m_pdfFindEdit, &QLineEdit::textChanged, this, &MainWindow::onPdfFindTextChanged);
    connect(m_pdfFindEdit, &QLineEdit::returnPressed, this, &MainWindow::pdfFindNext);

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
    if (m_pdfView) {
        m_pdfView->setCurrentSearchResultIndex(-1);
    }
    updatePdfFindActions();
    updatePdfSearchStatus();
}

void MainWindow::onPdfSearchResultsChanged()
{
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

void MainWindow::selectPdfSearchResult(int index)
{
    if (!m_pdfView || !m_pdfSearchModel) {
        return;
    }
    const int total = pdfSearchResultCount();
    if (index < 0 || index >= total) {
        return;
    }
    m_pdfView->setCurrentSearchResultIndex(index);
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
    const int current = m_pdfView->currentSearchResultIndex();
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
    const int current = m_pdfView->currentSearchResultIndex();
    const int prev = (current - 1 + total) % total;
    selectPdfSearchResult(prev);
}

void MainWindow::printCurrentPdf()
{
    if (m_stack->currentWidget() != m_pdfView || !m_pdfDocument || m_pdfDocument->pageCount() <= 0) {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QFileInfo(m_previewFilePath).fileName());
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print PDF"));
    if (dialog.exec() != QDialog::Accepted) {
        statusBar()->showMessage(tr("Print canceled."), 2000);
        return;
    }

    QPainter painter(&printer);
    if (!painter.isActive()) {
        QMessageBox::warning(this, tr("Print PDF"), tr("Could not start printing."));
        statusBar()->showMessage(tr("Print failed."), 3000);
        return;
    }

    const int pages = m_pdfDocument->pageCount();
    for (int page = 0; page < pages; ++page) {
        if (page > 0) {
            printer.newPage();
        }
        const QSize pagePx = m_pdfDocument->pagePointSize(page).toSize();
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
    statusBar()->showMessage(tr("Sent PDF to printer."), 3000);
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

    if (m_pdfToolBar) {
        m_pdfToolBar->setVisible(showBar);
    }

    if (!showBar) {
        if (m_pdfPageLabel) {
            m_pdfPageLabel->setText(tr("—"));
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
        return;
    }

    QPdfPageNavigator *const nav = m_pdfView->pageNavigator();
    const int cur = nav->currentPage();
    if (m_pdfPageLabel) {
        m_pdfPageLabel->setText(tr("Page %1 of %2").arg(cur + 1).arg(pages));
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
    const int current = m_pdfView->currentSearchResultIndex();
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
