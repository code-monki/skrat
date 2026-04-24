#pragma once

#include <QGraphicsView>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWheelEvent>

class QGraphicsPixmapItem;
class QGraphicsPolygonItem;
class QGraphicsRectItem;
class QPdfDocument;
class QPdfSearchModel;
class QPdfSelection;
class QResizeEvent;

class PdfGraphicsView final : public QGraphicsView
{
    Q_OBJECT

public:
    enum class ZoomMode {
        Custom,
        FitToWidth,
        FitInView,
    };

    enum class PageMode {
        SinglePage,
        MultiPage,
    };

    explicit PdfGraphicsView(QWidget *parent = nullptr);

    void setDocument(QPdfDocument *document);
    QPdfDocument *document() const { return m_document; }

    void setSearchModel(QPdfSearchModel *searchModel);
    QPdfSearchModel *searchModel() const { return m_searchModel; }

    void setPageMode(PageMode mode);
    PageMode pageMode() const { return m_pageMode; }

    void setZoomMode(ZoomMode mode);
    ZoomMode zoomMode() const { return m_zoomMode; }

    void setZoomFactor(qreal factor);
    qreal zoomFactor() const { return m_zoomFactor; }

    int pageSpacing() const { return m_pageSpacingPx; }
    int currentPage() const { return m_currentPage; }

    void jumpTo(int page, const QPointF &location = QPointF(0, 0), qreal zoom = 0.0);

    int currentSearchResultIndex() const { return m_currentSearchResultIndex; }
    void setCurrentSearchResultIndex(int index);
    bool copySelectionToClipboard();
    bool hasTextSelection() const;

signals:
    void currentPageChanged(int page);
    void currentSearchResultIndexChanged(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    struct PageItem {
        QGraphicsPixmapItem *pixmapItem = nullptr;
        QRectF sceneRect;
        QSizeF pageSizePt;
        qreal scale = 1.0;
    };

    void clearScene();
    void rebuildPages();
    void rebuildSearchHighlights();
    void rebuildTextSelectionHighlights();
    QRectF mapPdfRectToSceneRect(int page, const QRectF &pdfRect) const;
    QPointF mapPdfPointToScenePoint(int page, const QPointF &pdfPoint) const;
    QPointF mapScenePointToPdfPoint(int page, const QPointF &scenePoint) const;
    bool scenePointToPdfPoint(const QPointF &scenePoint, int *page, QPointF *pdfPoint) const;
    QPointF clampPdfPointToPage(int page, const QPointF &pdfPoint) const;
    bool selectionAnchorComesBeforeCurrent() const;
    void clearTextSelection();
    void updateTextSelection(int page, const QPointF &pdfPoint);
    /** True if any in-range page has non-empty trimmed text for the current anchor→caret. */
    bool selectionRangeHasText();
    void updateCurrentPageFromViewport();
    // QPdfSelection is per-page; Qt has no single document-spanning text range. A drag
    // is a logical selection: anchor and caret (each page + PDF-local point). We
    // materialize one QPdfSelection per affected page (same shape for copy, bounds,
    // and live highlight). See copySelectionToClipboard, rebuildTextSelectionHighlights.
    QPdfSelection pdfRangeForPageInSelection(int page,
                                             bool isStartPage,
                                             bool isEndPage,
                                             const QPointF &startPoint,
                                             const QPointF &endPoint) const;
    void syncDocSelectionCharRange();
    int pdfCharStartIndexAtPoint(int page, const QPointF &pdfPt) const;
    int pdfCharInclusiveEndIndexAtPoint(int page, const QPointF &pdfPt) const;
    bool selectionAnchorComesBeforeCurrentCharOrder() const;

    QPdfDocument *m_document = nullptr;
    QPdfSearchModel *m_searchModel = nullptr;
    PageMode m_pageMode = PageMode::MultiPage;
    ZoomMode m_zoomMode = ZoomMode::FitInView;
    qreal m_zoomFactor = 1.0;
    int m_pageSpacingPx = 12;
    int m_currentPage = 0;
    int m_currentSearchResultIndex = -1;

    QGraphicsScene *m_scene = nullptr;
    QVector<PageItem> m_pages;
    QList<QGraphicsRectItem *> m_searchHighlights;
    QList<QGraphicsPolygonItem *> m_textSelectionHighlights;
    int m_selectionAnchorPage = -1;
    int m_selectionCurrentPage = -1;
    QPointF m_selectionAnchorPdf;
    QPointF m_selectionCurrentPdf;
    int m_selectionAnchorChar = -1;
    int m_selectionCurrentChar = -1;
    int m_selDocFromPage = -1;
    int m_selDocFromChar = -1;
    int m_selDocToPage = -1;
    int m_selDocToChar = -1;
    bool m_selectingText = false;
    bool m_hasTextSelection = false;
};
