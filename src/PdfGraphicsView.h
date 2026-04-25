/**
 * @file PdfGraphicsView.h
 * @brief QGraphicsView-based PDF preview with zoom modes, search highlights, and text selection.
 *
 * Renders QPdfDocument pages into a vertical scene, maps PDF coordinates for hit-testing,
 * and coordinates with QPdfSearchModel for find-in-document navigation.
 */

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
    /** @brief How page scale is derived from viewport size versus an explicit zoom factor. */
    enum class ZoomMode {
        Custom,
        FitToWidth,
        FitInView,
    };

    /**
     * @brief Page layout policy (single vs multi); scene currently always stacks pages vertically.
     */
    enum class PageMode {
        SinglePage,
        MultiPage,
    };

    /**
     * @brief Constructs the PDF graphics view, scene, and scroll-bar current-page wiring.
     * @param[in] parent Optional parent QWidget.
     */
    explicit PdfGraphicsView(QWidget *parent = nullptr);

    /**
     * @brief Attaches a PDF document, connects status signals, and rebuilds the scene.
     * @param[in] document Document to render, or @c nullptr to clear the view.
     */
    void setDocument(QPdfDocument *document);

    /**
     * @brief Returns the document currently bound to this view.
     * @return Pointer to QPdfDocument, or @c nullptr if none.
     */
    QPdfDocument *document() const { return m_document; }

    /**
     * @brief Sets the search model whose rows define highlight rectangles on pages.
     * @param[in] searchModel Model from QPdfSearchModel, or @c nullptr to clear highlights.
     */
    void setSearchModel(QPdfSearchModel *searchModel);

    /**
     * @brief Returns the active search model.
     * @return Pointer to QPdfSearchModel, or @c nullptr if none.
     */
    QPdfSearchModel *searchModel() const { return m_searchModel; }

    /**
     * @brief Stores the page layout mode for API compatibility (layout is vertical stack).
     * @param[in] mode Desired PageMode enum value.
     */
    void setPageMode(PageMode mode);

    /**
     * @brief Returns the stored page layout mode.
     * @return Current PageMode value.
     */
    PageMode pageMode() const { return m_pageMode; }

    /**
     * @brief Sets zoom mode, scrollbar policy, and rebuilds layout when fit sizing changes.
     * @param[in] mode Custom, fit-to-width, or fit-in-view zoom behavior.
     */
    void setZoomMode(ZoomMode mode);

    /**
     * @brief Returns the active zoom mode.
     * @return Current ZoomMode value.
     */
    ZoomMode zoomMode() const { return m_zoomMode; }

    /**
     * @brief Sets the custom zoom factor (only affects layout when mode is Custom).
     * @param[in] factor Scale factor; values below an internal minimum are clamped.
     */
    void setZoomFactor(qreal factor);

    /**
     * @brief Returns the current custom zoom factor.
     * @return Stored zoom factor used when mode is Custom.
     */
    qreal zoomFactor() const { return m_zoomFactor; }

    /**
     * @brief Vertical gap between stacked pages in scene coordinates (pixels).
     * @return Page spacing in pixels.
     */
    int pageSpacing() const { return m_pageSpacingPx; }

    /**
     * @brief 0-based index of the page whose vertical center is nearest the viewport center.
     * @return Current page index.
     */
    int currentPage() const { return m_currentPage; }

    /**
     * @brief Scrolls so @a page and @a location are visible; optionally applies zoom in Custom mode.
     * @param[in] page 0-based page index.
     * @param[in] location PDF-space point on that page (top-left if default).
     * @param[in] zoom If greater than 0 and zoom mode is Custom, passed to setZoomFactor(); otherwise ignored.
     */
    void jumpTo(int page, const QPointF &location = QPointF(0, 0), qreal zoom = 0.0);

    /**
     * @brief Index of the search result drawn with the “current” highlight color.
     * @return 0-based model row, or -1 if none / invalid model.
     */
    int currentSearchResultIndex() const { return m_currentSearchResultIndex; }

    /**
     * @brief Highlights result @a index and emits currentSearchResultIndexChanged when it changes.
     * @param[in] index 0-based QPdfSearchModel row, or -1 to clear the active highlight.
     */
    void setCurrentSearchResultIndex(int index);

    /**
     * @brief Copies trimmed text from the multi-page drag selection to the system clipboard.
     * @return @c true if non-empty text was placed on the clipboard; otherwise @c false.
     */
    bool copySelectionToClipboard();

    /**
     * @brief Whether the user has a latched text selection after a completed drag.
     * @return @c true if copySelectionToClipboard() would return text.
     */
    bool hasTextSelection() const;

signals:
    /**
     * @brief Emitted when the page nearest the viewport vertical center changes.
     * @param[in] page New 0-based current page index.
     */
    void currentPageChanged(int page);

    /**
     * @brief Emitted when the highlighted search-result index changes.
     * @param[in] index New 0-based search model row, or -1 when cleared.
     */
    void currentSearchResultIndexChanged(int index);

protected:
    /**
     * @brief Relayouts rasterized pages when fit modes depend on viewport size.
     * @param[in] event Resize event forwarded from Qt.
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief In fit modes, scrolls vertically only and pins horizontal offset; may clear selection.
     * @param[in] dx Requested horizontal scroll delta (often suppressed in fit modes).
     * @param[in] dy Requested vertical scroll delta.
     */
    void scrollContentsBy(int dx, int dy) override;

    /**
     * @brief Forwards the wheel event and clears stale selection when not actively dragging.
     * @param[in] event Wheel event from Qt.
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * @brief Starts a left-button text drag at the PDF coordinates under the cursor.
     * @param[in] event Mouse press event.
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Updates the selection end point and auto-scrolls near viewport edges while dragging.
     * @param[in] event Mouse move event.
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief Ends text selection on left-button release and recomputes highlight state.
     * @param[in] event Mouse release event.
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    /**
     * @brief Per-page scene bookkeeping: pixmap item, bounds, PDF size, and scale.
     */
    struct PageItem {
        QGraphicsPixmapItem *pixmapItem = nullptr; ///< Raster page item in the scene.
        QRectF sceneRect;                          ///< Bounding rect of the page pixmap in scene coords.
        QSizeF pageSizePt;                         ///< Original page size in PDF points.
        qreal scale = 1.0;                         ///< PDF points to scene pixels scale for this page.
    };

    /** @brief Clears the scene and internal page/search/selection item lists. */
    void clearScene();

    /** @brief Rasterizes every document page into the scene according to zoom mode. */
    void rebuildPages();

    /** @brief Rebuilds search-match rectangles from m_searchModel and m_currentSearchResultIndex. */
    void rebuildSearchHighlights();

    /** @brief Rebuilds blue selection polygons from anchor/caret PDF geometry. */
    void rebuildTextSelectionHighlights();

    /**
     * @brief Maps a PDF-space rectangle on one page into scene coordinates.
     * @param[in] page 0-based page index.
     * @param[in] pdfRect Rectangle in PDF point space for that page.
     * @return Corresponding scene rectangle, or null if @a page is out of range.
     */
    QRectF mapPdfRectToSceneRect(int page, const QRectF &pdfRect) const;

    /**
     * @brief Maps a PDF-space point on one page into scene coordinates.
     * @param[in] page 0-based page index.
     * @param[in] pdfPoint Point in PDF point space.
     * @return Scene coordinates, or (0,0) if @a page is out of range.
     */
    QPointF mapPdfPointToScenePoint(int page, const QPointF &pdfPoint) const;

    /**
     * @brief Maps a scene point into PDF-space for a known @a page.
     * @param[in] page 0-based page index.
     * @param[in] scenePoint Point in scene coordinates.
     * @return PDF-space point before clamping to the page box.
     */
    QPointF mapScenePointToPdfPoint(int page, const QPointF &scenePoint) const;

    /**
     * @brief Resolves a scene position to a page index and clamped PDF coordinates.
     * @param[in] scenePoint Point in scene coordinates (e.g. from mapToScene).
     * @param[out] page Receives 0-based page index on success.
     * @param[out] pdfPoint Receives PDF coordinates clamped to the page media box.
     * @return @c true if a page could be resolved; @c false if arguments are null or no pages exist.
     */
    bool scenePointToPdfPoint(const QPointF &scenePoint, int *page, QPointF *pdfPoint) const;

    /**
     * @brief Clamps a PDF-space point to the page’s width and height in points.
     * @param[in] page 0-based page index.
     * @param[in] pdfPoint Candidate PDF coordinates.
     * @return Clamped point, or (0,0) if @a page is out of range.
     */
    QPointF clampPdfPointToPage(int page, const QPointF &pdfPoint) const;

    /**
     * @brief Returns whether the selection anchor precedes the caret in document order.
     * @return @c true if anchor is before caret using character indices or geometric fallback.
     */
    bool selectionAnchorComesBeforeCurrent() const;

    /** @brief Clears drag/selection state and removes selection highlight items. */
    void clearTextSelection();

    /**
     * @brief Updates the caret endpoint during a drag and refreshes selection highlights.
     * @param[in] page 0-based page index under the pointer.
     * @param[in] pdfPoint PDF-space point on that page (clamped internally).
     */
    void updateTextSelection(int page, const QPointF &pdfPoint);

    /**
     * @brief Returns whether any page in the anchor–caret span has non-empty trimmed selection text.
     * @return @c true if at least one QPdfSelection in the span has visible text.
     */
    bool selectionRangeHasText();

    /** @brief Recomputes m_currentPage from viewport geometry and emits currentPageChanged if needed. */
    void updateCurrentPageFromViewport();

    /**
     * @brief Builds the QPdfSelection slice for one page within a multi-page drag range.
     *
     * QPdfSelection is per-page; this maps logical anchor/caret across pages to per-page ranges.
     *
     * @param[in] page 0-based page index to query.
     * @param[in] isStartPage @c true if this page contains the range start point.
     * @param[in] isEndPage @c true if this page contains the range end point.
     * @param[in] startPoint PDF-space start of the overall drag (on the start page).
     * @param[in] endPoint PDF-space end of the overall drag (on the end page).
     * @return QPdfSelection for @a page; may be empty if geometry does not intersect text.
     */
    QPdfSelection pdfRangeForPageInSelection(int page,
                                             bool isStartPage,
                                             bool isEndPage,
                                             const QPointF &startPoint,
                                             const QPointF &endPoint) const;

    /** @brief Fills m_selDoc* from anchor/caret character indices for index-based spans. */
    void syncDocSelectionCharRange();

    /**
     * @brief Probes QPdfDocument::getSelection to find a stable start character index near @a pdfPt.
     * @param[in] page 0-based page index.
     * @param[in] pdfPt PDF-space reference point.
     * @return Start character index, or -1 if none found.
     */
    int pdfCharStartIndexAtPoint(int page, const QPointF &pdfPt) const;

    /**
     * @brief Probes getSelection to find an inclusive end character index near @a pdfPt.
     * @param[in] page 0-based page index.
     * @param[in] pdfPt PDF-space reference point.
     * @return Inclusive end character index, or -1 if none found.
     */
    int pdfCharInclusiveEndIndexAtPoint(int page, const QPointF &pdfPt) const;

    /**
     * @brief Compares anchor and caret using stored character indices on the same or different pages.
     * @return @c true if anchor precedes caret in (page, character) order.
     */
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
