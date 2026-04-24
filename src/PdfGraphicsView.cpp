#include "PdfGraphicsView.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGuiApplication>
#include <QClipboard>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QPdfSearchModel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QtGlobal>
#include <limits>

PdfGraphicsView::PdfGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    setBackgroundBrush(QBrush(QColor(38, 38, 38)));
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        updateCurrentPageFromViewport();
    });
}

void PdfGraphicsView::setDocument(QPdfDocument *document)
{
    if (m_document == document) {
        // Same document instance can still change after reloads.
        rebuildPages();
        rebuildSearchHighlights();
        updateCurrentPageFromViewport();
        return;
    }
    if (m_document) {
        QObject::disconnect(m_document, nullptr, this, nullptr);
    }
    m_document = document;
    m_selectionAnchorPage = -1;
    m_selectionCurrentPage = -1;
    m_selectionAnchorChar = -1;
    m_selectionCurrentChar = -1;
    m_selDocFromPage = -1;
    m_selDocFromChar = -1;
    m_selDocToPage = -1;
    m_selDocToChar = -1;
    m_hasTextSelection = false;
    m_selectingText = false;
    if (m_document) {
        connect(m_document,
                &QPdfDocument::statusChanged,
                this,
                [this](QPdfDocument::Status) {
                    rebuildPages();
                    rebuildSearchHighlights();
                    updateCurrentPageFromViewport();
                });
        connect(m_document,
                &QPdfDocument::pageCountChanged,
                this,
                [this](int) {
                    rebuildPages();
                    rebuildSearchHighlights();
                    updateCurrentPageFromViewport();
                });
    }
    rebuildPages();
    rebuildSearchHighlights();
    updateCurrentPageFromViewport();
}

void PdfGraphicsView::setSearchModel(QPdfSearchModel *searchModel)
{
    if (m_searchModel == searchModel) {
        return;
    }
    m_searchModel = searchModel;
    rebuildSearchHighlights();
}

void PdfGraphicsView::setPageMode(PageMode mode)
{
    m_pageMode = mode;
}

void PdfGraphicsView::setZoomMode(ZoomMode mode)
{
    if (m_zoomMode == mode) {
        if (mode == ZoomMode::FitToWidth || mode == ZoomMode::FitInView) {
            rebuildPages();
            rebuildSearchHighlights();
            updateCurrentPageFromViewport();
        }
        return;
    }
    m_zoomMode = mode;
    const bool fitMode = (m_zoomMode == ZoomMode::FitToWidth || m_zoomMode == ZoomMode::FitInView);
    setHorizontalScrollBarPolicy(fitMode ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    rebuildPages();
    rebuildSearchHighlights();
    updateCurrentPageFromViewport();
}

void PdfGraphicsView::setZoomFactor(qreal factor)
{
    const qreal clamped = qMax<qreal>(0.05, factor);
    if (qFuzzyCompare(m_zoomFactor, clamped)) {
        return;
    }
    m_zoomFactor = clamped;
    if (m_zoomMode == ZoomMode::Custom) {
        rebuildPages();
        rebuildSearchHighlights();
    }
}

void PdfGraphicsView::jumpTo(int page, const QPointF &location, qreal zoom)
{
    if (!m_document || page < 0 || page >= m_pages.size()) {
        return;
    }
    if (zoom > 0.0 && m_zoomMode == ZoomMode::Custom) {
        setZoomFactor(zoom);
    }

    const QPointF scenePt = mapPdfPointToScenePoint(page, location);
    const int desiredY = qRound(scenePt.y() - (viewport()->height() * 0.33));
    verticalScrollBar()->setValue(qBound(verticalScrollBar()->minimum(),
                                         desiredY,
                                         verticalScrollBar()->maximum()));
    const int desiredX = qRound(scenePt.x() - (viewport()->width() * 0.5));
    horizontalScrollBar()->setValue(qBound(horizontalScrollBar()->minimum(),
                                           desiredX,
                                           horizontalScrollBar()->maximum()));
    m_currentPage = page;
    emit currentPageChanged(m_currentPage);
}

void PdfGraphicsView::setCurrentSearchResultIndex(int index)
{
    if (!m_searchModel) {
        m_currentSearchResultIndex = -1;
        return;
    }
    if (index < -1 || index >= m_searchModel->rowCount(QModelIndex())) {
        return;
    }
    if (m_currentSearchResultIndex == index) {
        return;
    }
    m_currentSearchResultIndex = index;
    rebuildSearchHighlights();
    emit currentSearchResultIndexChanged(index);
}

bool PdfGraphicsView::copySelectionToClipboard()
{
    if (!m_hasTextSelection || !m_document || m_selectionAnchorPage < 0 || m_selectionCurrentPage < 0) {
        return false;
    }
    syncDocSelectionCharRange();
    const bool forward = selectionAnchorComesBeforeCurrent();
    const int startPage = forward ? m_selectionAnchorPage : m_selectionCurrentPage;
    const int endPage = forward ? m_selectionCurrentPage : m_selectionAnchorPage;
    const QPointF startPoint = forward ? m_selectionAnchorPdf : m_selectionCurrentPdf;
    const QPointF endPoint = forward ? m_selectionCurrentPdf : m_selectionAnchorPdf;

    QStringList chunks;
    for (int page = startPage; page <= endPage; ++page) {
        if (page < 0 || page >= m_pages.size()) {
            continue;
        }
        const QPdfSelection selection =
            pdfRangeForPageInSelection(page, page == startPage, page == endPage, startPoint, endPoint);
        const QString text = selection.text().trimmed();
        if (!text.isEmpty()) {
            chunks.push_back(text);
        }
    }
    if (chunks.isEmpty()) {
        return false;
    }
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(chunks.join(QStringLiteral("\n\n")));
    }
    return true;
}

bool PdfGraphicsView::hasTextSelection() const
{
    return m_hasTextSelection;
}

void PdfGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_zoomMode == ZoomMode::FitToWidth || m_zoomMode == ZoomMode::FitInView) {
        rebuildPages();
        rebuildSearchHighlights();
        rebuildTextSelectionHighlights();
    }
}

void PdfGraphicsView::scrollContentsBy(int dx, int dy)
{
    const bool fitMode = (m_zoomMode == ZoomMode::FitToWidth || m_zoomMode == ZoomMode::FitInView);
    if (fitMode) {
        QGraphicsView::scrollContentsBy(0, dy);
        horizontalScrollBar()->setValue(horizontalScrollBar()->minimum());
        return;
    }
    QGraphicsView::scrollContentsBy(dx, dy);
    if (dy != 0 && !m_selectingText && m_hasTextSelection) {
        clearTextSelection();
    }
}

void PdfGraphicsView::wheelEvent(QWheelEvent *event)
{
    QGraphicsView::wheelEvent(event);
    if (!m_selectingText && m_hasTextSelection) {
        clearTextSelection();
    }
}

void PdfGraphicsView::clearScene()
{
    m_scene->clear();
    m_pages.clear();
    m_searchHighlights.clear();
    m_textSelectionHighlights.clear();
}

void PdfGraphicsView::rebuildPages()
{
    clearScene();
    if (!m_document || m_document->pageCount() <= 0) {
        return;
    }

    // Keep visible gutters around pages so fit-width mode doesn't look clipped.
    const qreal topPad = 24.0;
    const qreal pageMatPx = 14.0;
    const qreal fitWidthGutterPx = 0.0;
    const qreal fitOuterPad = pageMatPx + fitWidthGutterPx;
    const qreal leftPad = (m_zoomMode == ZoomMode::Custom) ? 56.0 : pageMatPx;
    const bool fitMode = (m_zoomMode == ZoomMode::FitToWidth || m_zoomMode == ZoomMode::FitInView);
    qreal y = topPad;
    qreal maxWidth = 0.0;

    const int pageCount = m_document->pageCount();
    m_pages.reserve(pageCount);

    for (int page = 0; page < pageCount; ++page) {
        const QSizeF pagePt = m_document->pagePointSize(page);
        if (pagePt.isEmpty()) {
            continue;
        }
        qreal scale = m_zoomFactor;
        if (m_zoomMode == ZoomMode::FitToWidth || m_zoomMode == ZoomMode::FitInView) {
            const qreal usableWidth = qMax<qreal>(
                80.0,
                viewport()->width() - (2.0 * fitOuterPad));
            const qreal widthScale = qMax<qreal>(0.05, usableWidth / pagePt.width());
            if (m_zoomMode == ZoomMode::FitToWidth) {
                scale = widthScale;
            } else {
                const qreal usableHeight = qMax<qreal>(80.0, viewport()->height() - 2.0 * (topPad + pageMatPx));
                const qreal heightScale = qMax<qreal>(0.05, usableHeight / pagePt.height());
                scale = qMin(widthScale, heightScale);
            }
        }
        const QSize pagePx(qMax(1, qRound(pagePt.width() * scale)),
                           qMax(1, qRound(pagePt.height() * scale)));
        const QImage img = m_document->render(page, pagePx);
        if (img.isNull()) {
            continue;
        }

        // Draw a white page mat so gutters are visually calm in dark UI chrome.
        auto *pageMat = m_scene->addRect(
            QRectF(QPointF(leftPad - pageMatPx, y - pageMatPx),
                   QSizeF(pagePx.width() + pageMatPx * 2.0, pagePx.height() + pageMatPx * 2.0)),
            Qt::NoPen,
            QBrush(QColor(255, 255, 255)));
        pageMat->setZValue(0.25);

        // Draw a subtle page edge so page bounds are visible.
        auto *pageFrame = m_scene->addRect(QRectF(QPointF(leftPad, y), QSizeF(pagePx)),
                                           QPen(QColor(155, 155, 155, 165), 1.0),
                                           Qt::NoBrush);
        pageFrame->setZValue(0.5);

        auto *item = m_scene->addPixmap(QPixmap::fromImage(img));
        item->setTransformationMode(Qt::SmoothTransformation);
        item->setPos(leftPad, y);
        item->setZValue(1.0);

        PageItem pageItem;
        pageItem.pixmapItem = item;
        pageItem.sceneRect = QRectF(QPointF(leftPad, y), QSizeF(pagePx));
        pageItem.pageSizePt = pagePt;
        pageItem.scale = scale;
        m_pages.push_back(pageItem);

        y += pagePx.height() + m_pageSpacingPx;
        maxWidth = qMax(maxWidth, static_cast<qreal>(pagePx.width()));
    }

    const qreal contentWidth = maxWidth + leftPad * 2.0;
    const qreal sceneWidth = fitMode ? qMax(contentWidth, static_cast<qreal>(viewport()->width()))
                                     : contentWidth;
    m_scene->setSceneRect(0.0, 0.0, sceneWidth, y + topPad);
    if (fitMode) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->minimum());
    }
    rebuildTextSelectionHighlights();
}

void PdfGraphicsView::rebuildSearchHighlights()
{
    for (auto *item : m_searchHighlights) {
        if (item) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    m_searchHighlights.clear();

    if (!m_searchModel || !m_document || m_pages.isEmpty()) {
        return;
    }

    const int total = m_searchModel->rowCount(QModelIndex());
    for (int i = 0; i < total; ++i) {
        const QPdfLink link = m_searchModel->resultAtIndex(i);
        if (!link.isValid()) {
            continue;
        }
        const QList<QRectF> rects = link.rectangles();
        if (!rects.isEmpty()) {
            for (const QRectF &r : rects) {
                const QRectF sr = mapPdfRectToSceneRect(link.page(), r);
                if (!sr.isValid()) {
                    continue;
                }
                auto *hl = m_scene->addRect(
                    sr,
                    Qt::NoPen,
                    QBrush(i == m_currentSearchResultIndex ? QColor(255, 170, 0, 120)
                                                           : QColor(255, 235, 120, 90)));
                hl->setZValue(10.0);
                m_searchHighlights.push_back(hl);
            }
        } else {
            const QPointF pt = mapPdfPointToScenePoint(link.page(), link.location());
            const QSizeF approx(14.0, 8.0);
            auto *hl = m_scene->addRect(QRectF(pt, approx),
                                        Qt::NoPen,
                                        QBrush(i == m_currentSearchResultIndex ? QColor(255, 170, 0, 120)
                                                                               : QColor(255, 235, 120, 90)));
            hl->setZValue(10.0);
            m_searchHighlights.push_back(hl);
        }
    }
}

void PdfGraphicsView::rebuildTextSelectionHighlights()
{
    for (auto *item : m_textSelectionHighlights) {
        if (item) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    m_textSelectionHighlights.clear();
    if (!m_document || !m_hasTextSelection || m_selectionAnchorPage < 0 || m_selectionCurrentPage < 0) {
        return;
    }
    syncDocSelectionCharRange();
    const bool forward = selectionAnchorComesBeforeCurrent();
    const int startPage = forward ? m_selectionAnchorPage : m_selectionCurrentPage;
    const int endPage = forward ? m_selectionCurrentPage : m_selectionAnchorPage;
    const QPointF startPoint = forward ? m_selectionAnchorPdf : m_selectionCurrentPdf;
    const QPointF endPoint = forward ? m_selectionCurrentPdf : m_selectionAnchorPdf;

    for (int page = startPage; page <= endPage; ++page) {
        if (page < 0 || page >= m_pages.size()) {
            continue;
        }
        const QPdfSelection selection =
            pdfRangeForPageInSelection(page, page == startPage, page == endPage, startPoint, endPoint);
        const QList<QPolygonF> bounds = selection.bounds();
        bool drew = false;
        for (const QPolygonF &poly : bounds) {
            if (poly.isEmpty()) {
                continue;
            }
            QPolygonF scenePoly;
            scenePoly.reserve(poly.size());
            for (const QPointF &pt : poly) {
                scenePoly << mapPdfPointToScenePoint(page, pt);
            }
            auto *hl = m_scene->addPolygon(scenePoly, Qt::NoPen, QBrush(QColor(64, 148, 255, 90)));
            hl->setZValue(9.0);
            m_textSelectionHighlights.push_back(hl);
            drew = true;
        }
        // Some PDFs return text but no bounds; still show a band so cross-page drags don't
        // look like the selection vanished.
        if (!drew && !selection.text().trimmed().isEmpty()) {
            const QSizeF psz = m_pages[page].pageSizePt;
            QRectF pdfBand;
            if (page == startPage && page == endPage) {
                const qreal y1 = qMin(startPoint.y(), endPoint.y());
                const qreal y2 = qMax(startPoint.y(), endPoint.y());
                pdfBand = QRectF(0.0, y1, psz.width(), qMax<qreal>(0.0, y2 - y1));
            } else if (page == startPage) {
                pdfBand = QRectF(0.0, startPoint.y(), psz.width(), qMax<qreal>(0.0, psz.height() - startPoint.y()));
            } else if (page == endPage) {
                pdfBand = QRectF(0.0, 0.0, psz.width(), qMax<qreal>(0.0, endPoint.y()));
            } else {
                pdfBand = QRectF(0.0, 0.0, psz.width(), psz.height());
            }
            const QRectF sr = mapPdfRectToSceneRect(page, pdfBand);
            if (sr.isValid() && sr.width() > 1.0 && sr.height() > 1.0) {
                QPolygonF band;
                band << sr.topLeft() << sr.topRight() << sr.bottomRight() << sr.bottomLeft();
                auto *hl = m_scene->addPolygon(band, Qt::NoPen, QBrush(QColor(64, 148, 255, 90)));
                hl->setZValue(9.0);
                m_textSelectionHighlights.push_back(hl);
            }
        }
    }
}

QRectF PdfGraphicsView::mapPdfRectToSceneRect(int page, const QRectF &pdfRect) const
{
    if (page < 0 || page >= m_pages.size()) {
        return {};
    }
    const PageItem &p = m_pages[page];
    const qreal x = p.sceneRect.left() + pdfRect.left() * p.scale;
    const qreal y = p.sceneRect.top() + pdfRect.top() * p.scale;
    return QRectF(x, y, pdfRect.width() * p.scale, pdfRect.height() * p.scale);
}

QPointF PdfGraphicsView::mapPdfPointToScenePoint(int page, const QPointF &pdfPoint) const
{
    if (page < 0 || page >= m_pages.size()) {
        return {};
    }
    const PageItem &p = m_pages[page];
    return QPointF(p.sceneRect.left() + pdfPoint.x() * p.scale,
                   p.sceneRect.top() + pdfPoint.y() * p.scale);
}

QPointF PdfGraphicsView::mapScenePointToPdfPoint(int page, const QPointF &scenePoint) const
{
    if (page < 0 || page >= m_pages.size()) {
        return {};
    }
    const PageItem &p = m_pages[page];
    return QPointF((scenePoint.x() - p.sceneRect.left()) / p.scale,
                   (scenePoint.y() - p.sceneRect.top()) / p.scale);
}

bool PdfGraphicsView::scenePointToPdfPoint(const QPointF &scenePoint, int *page, QPointF *pdfPoint) const
{
    if (!page || !pdfPoint) {
        return false;
    }
    if (m_pages.isEmpty()) {
        return false;
    }
    for (int i = 0; i < m_pages.size(); ++i) {
        if (!m_pages[i].sceneRect.contains(scenePoint)) {
            continue;
        }
        *page = i;
        *pdfPoint = clampPdfPointToPage(i, mapScenePointToPdfPoint(i, scenePoint));
        return true;
    }
    const qreal y = scenePoint.y();
    // Above first or below last page: keep continuous mapping (clamp), not a random "nearest" page.
    if (y < m_pages.first().sceneRect.top()) {
        *page = 0;
        *pdfPoint = clampPdfPointToPage(0, mapScenePointToPdfPoint(0, scenePoint));
        return true;
    }
    if (y > m_pages.last().sceneRect.bottom()) {
        const int li = m_pages.size() - 1;
        *page = li;
        *pdfPoint = clampPdfPointToPage(li, mapScenePointToPdfPoint(li, scenePoint));
        return true;
    }
    // Inter-page vertical gap: pick upper vs lower page by midpoint so the caret does not
    // jump to top/bottom edges and lose text for one frame.
    for (int i = 0; i < m_pages.size() - 1; ++i) {
        const qreal gapTop = m_pages[i].sceneRect.bottom();
        const qreal gapBot = m_pages[i + 1].sceneRect.top();
        if (y > gapTop && y < gapBot) {
            const qreal mid = (gapTop + gapBot) * 0.5;
            if (y < mid) {
                *page = i;
                *pdfPoint = clampPdfPointToPage(i, mapScenePointToPdfPoint(i, scenePoint));
            } else {
                *page = i + 1;
                *pdfPoint = clampPdfPointToPage(i + 1, mapScenePointToPdfPoint(i + 1, scenePoint));
            }
            return true;
        }
    }
    // Wider scene than a page: y overlaps a page band but x is in the left/right mat — pick
    // the row-overlapping page whose horizontal range is closest to the cursor.
    int bestPage = -1;
    qreal bestHoriz = std::numeric_limits<qreal>::max();
    for (int i = 0; i < m_pages.size(); ++i) {
        const QRectF r = m_pages[i].sceneRect;
        if (y < r.top() || y > r.bottom()) {
            continue;
        }
        const qreal d = (scenePoint.x() < r.left())   ? (r.left() - scenePoint.x())
                        : (scenePoint.x() > r.right()) ? (scenePoint.x() - r.right())
                                                       : 0.0;
        if (d < bestHoriz) {
            bestHoriz = d;
            bestPage = i;
        }
    }
    if (bestPage >= 0) {
        *page = bestPage;
        *pdfPoint = clampPdfPointToPage(bestPage, mapScenePointToPdfPoint(bestPage, scenePoint));
        return true;
    }
    // Last resort: page whose scene rect is closest in Y to the point (legacy behavior).
    int nearest = 0;
    qreal bestDistY = std::numeric_limits<qreal>::max();
    for (int i = 0; i < m_pages.size(); ++i) {
        const QRectF r = m_pages[i].sceneRect;
        const qreal clampedY = qBound(r.top(), y, r.bottom());
        const qreal dist = qAbs(y - clampedY);
        if (dist < bestDistY) {
            bestDistY = dist;
            nearest = i;
        }
    }
    *page = nearest;
    *pdfPoint = clampPdfPointToPage(nearest, mapScenePointToPdfPoint(nearest, scenePoint));
    return true;
}

QPointF PdfGraphicsView::clampPdfPointToPage(int page, const QPointF &pdfPoint) const
{
    if (page < 0 || page >= m_pages.size()) {
        return {};
    }
    const QSizeF sz = m_pages[page].pageSizePt;
    return QPointF(qBound(0.0, pdfPoint.x(), sz.width()),
                   qBound(0.0, pdfPoint.y(), sz.height()));
}

bool PdfGraphicsView::selectionRangeHasText()
{
    if (!m_document || m_pages.isEmpty() || m_selectionAnchorPage < 0 || m_selectionCurrentPage < 0) {
        return false;
    }
    if (m_selectionAnchorPage >= m_pages.size() || m_selectionCurrentPage >= m_pages.size()) {
        return false;
    }
    syncDocSelectionCharRange();
    const bool forward = selectionAnchorComesBeforeCurrent();
    const int s = forward ? m_selectionAnchorPage : m_selectionCurrentPage;
    const int e = forward ? m_selectionCurrentPage : m_selectionAnchorPage;
    const QPointF sp = forward ? m_selectionAnchorPdf : m_selectionCurrentPdf;
    const QPointF ep = forward ? m_selectionCurrentPdf : m_selectionAnchorPdf;
    for (int p = s; p <= e; ++p) {
        if (p < 0 || p >= m_pages.size()) {
            continue;
        }
        const QPdfSelection sel = pdfRangeForPageInSelection(p, p == s, p == e, sp, ep);
        if (!sel.text().trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}

void PdfGraphicsView::syncDocSelectionCharRange()
{
    m_selDocFromPage = -1;
    m_selDocFromChar = -1;
    m_selDocToPage = -1;
    m_selDocToChar = -1;
    if (m_selectionAnchorPage < 0 || m_selectionCurrentPage < 0) {
        return;
    }
    if (m_selectionAnchorChar < 0 || m_selectionCurrentChar < 0) {
        return;
    }
    if (selectionAnchorComesBeforeCurrentCharOrder()) {
        m_selDocFromPage = m_selectionAnchorPage;
        m_selDocFromChar = m_selectionAnchorChar;
        m_selDocToPage = m_selectionCurrentPage;
        m_selDocToChar = m_selectionCurrentChar;
    } else {
        m_selDocFromPage = m_selectionCurrentPage;
        m_selDocFromChar = m_selectionCurrentChar;
        m_selDocToPage = m_selectionAnchorPage;
        m_selDocToChar = m_selectionAnchorChar;
    }
}

bool PdfGraphicsView::selectionAnchorComesBeforeCurrentCharOrder() const
{
    if (m_selectionAnchorPage != m_selectionCurrentPage) {
        return m_selectionAnchorPage < m_selectionCurrentPage;
    }
    return m_selectionAnchorChar <= m_selectionCurrentChar;
}

int PdfGraphicsView::pdfCharStartIndexAtPoint(int page, const QPointF &pdfPt) const
{
    if (!m_document || page < 0 || page >= m_pages.size()) {
        return -1;
    }
    const QSizeF sz = m_pages[page].pageSizePt;
    const QPointF p(qBound(0.0, pdfPt.x(), sz.width()), qBound(0.0, pdfPt.y(), sz.height()));
    static constexpr qreal kProbeDx[] = { 1.0, 3.0, 10.0, 28.0, 80.0 };
    for (const qreal dx : kProbeDx) {
        const QPointF q(qMin(p.x() + dx, sz.width()), p.y());
        const QPdfSelection s = m_document->getSelection(page, p, q);
        if (s.startIndex() < s.endIndex()) {
            return s.startIndex();
        }
    }
    const qreal y = p.y();
    const QPdfSelection line = m_document->getSelection(
        page, QPointF(0.0, y), QPointF(sz.width(), qMin(y + 2.0, sz.height())));
    if (line.startIndex() < line.endIndex()) {
        return line.startIndex();
    }
    return -1;
}

int PdfGraphicsView::pdfCharInclusiveEndIndexAtPoint(int page, const QPointF &pdfPt) const
{
    if (!m_document || page < 0 || page >= m_pages.size()) {
        return -1;
    }
    const QSizeF sz = m_pages[page].pageSizePt;
    const QPointF p(qBound(0.0, pdfPt.x(), sz.width()), qBound(0.0, pdfPt.y(), sz.height()));
    // Tight probes first so a click without drag does not jump to the whole line.
    static constexpr qreal kProbeDx[] = { 1.0, 3.0, 10.0, 28.0, 80.0 };
    for (const qreal dx : kProbeDx) {
        const QPointF q(qMin(p.x() + dx, sz.width()), p.y());
        const QPdfSelection s = m_document->getSelection(page, p, q);
        if (s.startIndex() < s.endIndex()) {
            const int endExclusive = s.endIndex();
            return qMax(s.startIndex(), endExclusive - 1);
        }
    }
    const qreal y0 = p.y();
    const qreal y1 = qMin(y0 + 6.0, sz.height());
    const QPdfSelection line =
        m_document->getSelection(page, QPointF(0.0, y0), QPointF(sz.width(), y1));
    if (line.startIndex() < line.endIndex()) {
        const int endExclusive = line.endIndex();
        return qMax(line.startIndex(), endExclusive - 1);
    }
    return -1;
}

QPdfSelection PdfGraphicsView::pdfRangeForPageInSelection(int page,
                                                          bool isStartPage,
                                                          bool isEndPage,
                                                          const QPointF &startPoint,
                                                          const QPointF &endPoint) const
{
    Q_ASSERT(m_document);
    if (page < 0 || page >= m_pages.size()) {
        return m_document->getSelection(0, QPointF(0.0, 0.0), QPointF(0.0, 0.0));
    }
    // Index-based range: QPdfDocument::getSelection often returns nothing when both corners
    // resolve to the same character (Qt source: endIndex == startIndex). Character spans
    // from getSelectionAtIndex match PDFium and behave correctly across page boundaries.
    if (m_selDocFromPage >= 0 && m_selDocToPage >= 0 && m_selDocFromChar >= 0 && m_selDocToChar >= 0
        && page >= m_selDocFromPage && page <= m_selDocToPage) {
        const QPdfSelection all = m_document->getAllText(page);
        const int allExclusiveEnd = all.endIndex();
        if (allExclusiveEnd > 0) {
            if (m_selDocFromPage == m_selDocToPage) {
                const int a = qMin(m_selDocFromChar, m_selDocToChar);
                const int b = qMax(m_selDocFromChar, m_selDocToChar);
                return m_document->getSelectionAtIndex(page, a, b - a + 1);
            }
            if (page == m_selDocFromPage) {
                return m_document->getSelectionAtIndex(page, m_selDocFromChar, allExclusiveEnd - m_selDocFromChar);
            }
            if (page == m_selDocToPage) {
                return m_document->getSelectionAtIndex(page, 0, m_selDocToChar + 1);
            }
            return all;
        }
    }
    if (isStartPage && isEndPage) {
        const QSizeF pageSize = m_pages[page].pageSizePt;
        const qreal dx = qAbs(endPoint.x() - startPoint.x());
        const qreal dy = qAbs(endPoint.y() - startPoint.y());
        if (dy > qMax<qreal>(4.0, dx * 1.15)) {
            const qreal y1 = qMin(startPoint.y(), endPoint.y());
            const qreal y2 = qMax(startPoint.y(), endPoint.y());
            return m_document->getSelection(
                page,
                QPointF(0.0, y1),
                QPointF(pageSize.width(), y2));
        }
        return m_document->getSelection(page, startPoint, endPoint);
    }
    const QPdfSelection allText = m_document->getAllText(page);
    if (isStartPage) {
        const QSizeF pageSize = m_pages[page].pageSizePt;
        return m_document->getSelection(
            page, QPointF(0.0, startPoint.y()), QPointF(pageSize.width(), pageSize.height()));
    }
    if (isEndPage) {
        const QSizeF sz = m_pages[page].pageSizePt;
        auto rectSel = [&](const QPointF &a, const QPointF &b) -> QPdfSelection {
            const QPointF r1(qMin(a.x(), b.x()), qMin(a.y(), b.y()));
            const QPointF r2(qMax(a.x(), b.x()), qMax(a.y(), b.y()));
            return m_document->getSelection(page, r1, r2);
        };
        QPdfSelection sel = rectSel(QPointF(0.0, 0.0), QPointF(sz.width(), endPoint.y()));
        if (!sel.text().trimmed().isEmpty()) {
            return sel;
        }
        sel = rectSel(QPointF(0.0, 0.0), endPoint);
        if (!sel.text().trimmed().isEmpty()) {
            return sel;
        }
        sel = rectSel(QPointF(0.0, endPoint.y()), endPoint);
        if (!sel.text().trimmed().isEmpty()) {
            return sel;
        }
        return rectSel(QPointF(0.0, 0.0), endPoint);
    }
    return allText;
}

bool PdfGraphicsView::selectionAnchorComesBeforeCurrent() const
{
    if (m_selectionAnchorChar >= 0 && m_selectionCurrentChar >= 0) {
        return selectionAnchorComesBeforeCurrentCharOrder();
    }
    if (m_selectionAnchorPage != m_selectionCurrentPage) {
        return m_selectionAnchorPage < m_selectionCurrentPage;
    }
    if (!qFuzzyCompare(1.0 + m_selectionAnchorPdf.y(), 1.0 + m_selectionCurrentPdf.y())) {
        return m_selectionAnchorPdf.y() <= m_selectionCurrentPdf.y();
    }
    return m_selectionAnchorPdf.x() <= m_selectionCurrentPdf.x();
}

void PdfGraphicsView::clearTextSelection()
{
    m_hasTextSelection = false;
    m_selectionAnchorPage = -1;
    m_selectionCurrentPage = -1;
    m_selectionAnchorChar = -1;
    m_selectionCurrentChar = -1;
    m_selDocFromPage = -1;
    m_selDocFromChar = -1;
    m_selDocToPage = -1;
    m_selDocToChar = -1;
    m_selectingText = false;
    rebuildTextSelectionHighlights();
}

void PdfGraphicsView::updateTextSelection(int page, const QPointF &pdfPoint)
{
    const int candidateCurrentPage = page;
    const QPointF candidateCurrentPdf = clampPdfPointToPage(page, pdfPoint);
    // Always move the caret with the pointer while dragging. A previous early-return path
    // skipped updating these when a transient range had no text, which broke cross-page
    // highlights and copy (caret stayed on the previous page).
    m_selectionCurrentPage = candidateCurrentPage;
    m_selectionCurrentPdf = candidateCurrentPdf;
    m_selectionCurrentChar = pdfCharInclusiveEndIndexAtPoint(candidateCurrentPage, candidateCurrentPdf);
    syncDocSelectionCharRange();

    bool hasAnyText = false;
    const bool forward =
        (m_selectionAnchorPage < candidateCurrentPage)
        || (m_selectionAnchorPage == candidateCurrentPage
            && (m_selectionAnchorPdf.y() < candidateCurrentPdf.y()
                || (qFuzzyCompare(1.0 + m_selectionAnchorPdf.y(), 1.0 + candidateCurrentPdf.y())
                    && m_selectionAnchorPdf.x() <= candidateCurrentPdf.x())));
    const int startPage = forward ? m_selectionAnchorPage : candidateCurrentPage;
    const int endPage = forward ? candidateCurrentPage : m_selectionAnchorPage;
    const QPointF startPoint = forward ? m_selectionAnchorPdf : candidateCurrentPdf;
    const QPointF endPoint = forward ? candidateCurrentPdf : m_selectionAnchorPdf;

    for (int p = startPage; p <= endPage; ++p) {
        if (p < 0 || p >= m_pages.size()) {
            continue;
        }
        const QPdfSelection selection =
            pdfRangeForPageInSelection(p, p == startPage, p == endPage, startPoint, endPoint);
        if (!selection.text().trimmed().isEmpty()) {
            hasAnyText = true;
            break;
        }
    }
    // While dragging, QPdf can return empty for a frame (geometry hitches, etc.). Latch: once
    // the range had text, keep m_hasTextSelection true until release; then we finalize.
    if (m_selectingText) {
        m_hasTextSelection = m_hasTextSelection || hasAnyText;
    } else {
        m_hasTextSelection = hasAnyText;
    }
    rebuildTextSelectionHighlights();
}

void PdfGraphicsView::updateCurrentPageFromViewport()
{
    if (m_pages.isEmpty()) {
        if (m_currentPage != 0) {
            m_currentPage = 0;
            emit currentPageChanged(m_currentPage);
        }
        return;
    }
    const QPointF sceneCenter = mapToScene(viewport()->rect().center());
    int best = m_currentPage;
    qreal bestDist = std::numeric_limits<qreal>::max();
    for (int i = 0; i < m_pages.size(); ++i) {
        const qreal cy = m_pages[i].sceneRect.center().y();
        const qreal d = qAbs(sceneCenter.y() - cy);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    if (best != m_currentPage) {
        m_currentPage = best;
        emit currentPageChanged(m_currentPage);
    }
}

void PdfGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (!m_document || event->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    const QPointF scenePos =
        mapToScene(qRound(event->position().x()), qRound(event->position().y()));
    int page = -1;
    QPointF pdfPoint;
    if (!scenePointToPdfPoint(scenePos, &page, &pdfPoint)) {
        clearTextSelection();
        QGraphicsView::mousePressEvent(event);
        return;
    }
    m_selectingText = true;
    m_selectionAnchorPage = page;
    m_selectionAnchorPdf = clampPdfPointToPage(page, pdfPoint);
    m_selectionAnchorChar = pdfCharStartIndexAtPoint(page, m_selectionAnchorPdf);
    m_selectionCurrentChar = m_selectionAnchorChar;
    syncDocSelectionCharRange();
    // Start a fresh drag selection; avoid stale sticky state.
    m_hasTextSelection = false;
    m_selectionCurrentPage = page;
    m_selectionCurrentPdf = m_selectionAnchorPdf;
    rebuildTextSelectionHighlights();
    updateTextSelection(page, pdfPoint);
    setFocus(Qt::MouseFocusReason);
    event->accept();
}

void PdfGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_selectingText || !m_document || m_selectionAnchorPage < 0) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    const QPointF pos = event->position();
    constexpr int kAutoScrollMarginPx = 24;
    constexpr int kAutoScrollStepPx = 20;
    if (pos.y() >= qreal(viewport()->height() - kAutoScrollMarginPx)) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() + kAutoScrollStepPx);
    } else if (pos.y() <= qreal(kAutoScrollMarginPx)) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - kAutoScrollStepPx);
    }
    const QPointF scenePos = mapToScene(qRound(pos.x()), qRound(pos.y()));
    int page = -1;
    QPointF pdfPoint;
    if (!scenePointToPdfPoint(scenePos, &page, &pdfPoint)) {
        event->accept();
        return;
    }
    updateTextSelection(page, pdfPoint);
    event->accept();
}

void PdfGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selectingText) {
        m_selectingText = false;
        m_hasTextSelection = selectionRangeHasText();
        rebuildTextSelectionHighlights();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
