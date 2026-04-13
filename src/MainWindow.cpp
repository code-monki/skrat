#include "MainWindow.h"

#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QFont>
#include <QInputDialog>
#include <QLabel>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QModelIndex>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QPlainTextEdit>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>

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
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

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
    m_actGoToPageOrLine->setToolTip(
        tr("In a PDF: jump to a page by number. In a text file: jump to a line (there are no PDF-style pages)."));
    connect(m_actGoToPageOrLine, &QAction::triggered, this, &MainWindow::goToPageOrLine);
    viewMenu->addAction(m_actGoToPageOrLine);
    addAction(m_actGoToPageOrLine);

    viewMenu->addSeparator();
    QMenu *const pdfPageMenu = viewMenu->addMenu(tr("PDF &pages"));

    m_pdfActFirst = new QAction(tr("&First page"), this);
    m_pdfActFirst->setShortcut(QKeySequence(QStringLiteral("Ctrl+Home")));
    m_pdfActFirst->setToolTip(tr("Go to first page (Ctrl+Home)"));
    connect(m_pdfActFirst, &QAction::triggered, this, &MainWindow::pdfGoFirstPage);

    m_pdfActPrev = new QAction(tr("&Previous page"), this);
    m_pdfActPrev->setShortcut(QKeySequence(QStringLiteral("Alt+PgUp")));
    m_pdfActPrev->setToolTip(tr("Go to previous page (Alt+PgUp)"));
    connect(m_pdfActPrev, &QAction::triggered, this, &MainWindow::pdfGoPrevPage);

    m_pdfActNext = new QAction(tr("&Next page"), this);
    m_pdfActNext->setShortcut(QKeySequence(QStringLiteral("Alt+PgDown")));
    m_pdfActNext->setToolTip(tr("Go to next page (Alt+PgDown)"));
    connect(m_pdfActNext, &QAction::triggered, this, &MainWindow::pdfGoNextPage);

    m_pdfActLast = new QAction(tr("&Last page"), this);
    m_pdfActLast->setShortcut(QKeySequence(QStringLiteral("Ctrl+End")));
    m_pdfActLast->setToolTip(tr("Go to last page (Ctrl+End)"));
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
    m_pdfView = new QPdfView;
    m_pdfView->setDocument(m_pdfDocument);
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

    m_pdfToolBar = addToolBar(tr("PDF navigation"));
    m_pdfToolBar->setMovable(false);
    m_pdfToolBar->addAction(m_pdfActFirst);
    m_pdfToolBar->addAction(m_pdfActPrev);
    m_pdfToolBar->addAction(m_pdfActNext);
    m_pdfToolBar->addAction(m_pdfActLast);
    m_pdfToolBar->addAction(m_actGoToPageOrLine);
    m_pdfPageLabel = new QLabel(tr("—"));
    m_pdfPageLabel->setMinimumWidth(140);
    m_pdfPageLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_pdfToolBar->addWidget(m_pdfPageLabel);

    connect(m_pdfView->pageNavigator(),
            &QPdfPageNavigator::currentPageChanged,
            this,
            &MainWindow::updatePdfPageUi);
    connect(m_pdfDocument, &QPdfDocument::pageCountChanged, this, &MainWindow::updatePdfPageUi);

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
        if (m_pdfActFirst) {
            m_pdfActFirst->setEnabled(false);
            m_pdfActPrev->setEnabled(false);
            m_pdfActNext->setEnabled(false);
            m_pdfActLast->setEnabled(false);
        }
        statusBar()->clearMessage();
        updateGoToNavigationAction();
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

    updateGoToNavigationAction();
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
