#pragma once

#include <QGraphicsView>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QVector>

class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QPdfDocument;
class QPdfSearchModel;
class QResizeEvent;

class PdfGraphicsView final : public QGraphicsView
{
    Q_OBJECT

public:
    enum class ZoomMode {
        Custom,
        FitToWidth,
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

signals:
    void currentPageChanged(int page);
    void currentSearchResultIndexChanged(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;

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
    QRectF mapPdfRectToSceneRect(int page, const QRectF &pdfRect) const;
    QPointF mapPdfPointToScenePoint(int page, const QPointF &pdfPoint) const;
    void updateCurrentPageFromViewport();

    QPdfDocument *m_document = nullptr;
    QPdfSearchModel *m_searchModel = nullptr;
    PageMode m_pageMode = PageMode::MultiPage;
    ZoomMode m_zoomMode = ZoomMode::FitToWidth;
    qreal m_zoomFactor = 1.0;
    int m_pageSpacingPx = 12;
    int m_currentPage = 0;
    int m_currentSearchResultIndex = -1;

    QGraphicsScene *m_scene = nullptr;
    QVector<PageItem> m_pages;
    QList<QGraphicsRectItem *> m_searchHighlights;
};
