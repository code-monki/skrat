#include "PdfGraphicsView.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QPdfDocument>
#include <QPdfSearchModel>
#include <QResizeEvent>
#include <QScrollBar>
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
        return;
    }
    m_document = document;
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
        return;
    }
    m_zoomMode = mode;
    rebuildPages();
    rebuildSearchHighlights();
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

void PdfGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_zoomMode == ZoomMode::FitToWidth) {
        rebuildPages();
        rebuildSearchHighlights();
    }
}

void PdfGraphicsView::clearScene()
{
    m_scene->clear();
    m_pages.clear();
    m_searchHighlights.clear();
}

void PdfGraphicsView::rebuildPages()
{
    clearScene();
    if (!m_document || m_document->pageCount() <= 0) {
        return;
    }

    // Keep visible gutters around pages so fit-width mode doesn't look clipped.
    const qreal leftPad = 56.0;
    const qreal topPad = 24.0;
    const qreal fitWidthGutterPx = 56.0;
    const qreal pageMatPx = 14.0;
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
        if (m_zoomMode == ZoomMode::FitToWidth) {
            const qreal usableWidth = qMax<qreal>(80.0, viewport()->width() - 2.0 * (leftPad + fitWidthGutterPx));
            scale = qMax<qreal>(0.05, usableWidth / pagePt.width());
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

    m_scene->setSceneRect(0.0, 0.0, maxWidth + leftPad * 2.0, y + topPad);
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
