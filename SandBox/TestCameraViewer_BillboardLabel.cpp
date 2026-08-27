#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QMatrix4x4>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QVector4D>
#include <QVector2D>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <vector>

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Item/AxisAlignedBoundingBox.h"
#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderLabel.h"
#include "MyOpenGL/Item/RenderPart.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

#include "MyOpenGL/Viewer/Measurement/Length2DMeasurement.h"
#include "MyOpenGL/Viewer/Measurement/Length3DMeasurement.h"
#include "MyOpenGL/Viewer/Measurement/Angle2DMeasurement.h"
#include "MyOpenGL/Viewer/Measurement/Angle3DMeasurement.h"
class CameraTestViewer : public OpenGLViewerWidget
{
public:
    explicit CameraTestViewer(QWidget* parent = 0)
        : OpenGLViewerWidget(parent)
        , m_cubeGeometry(0)
        , m_billboardSquareGeometry(0)
        , m_billboardMaterial(0)
        , m_vertexColorMaterial(0)
        , m_minorGridGeometry(0)
        , m_majorGridGeometry(0)
        , m_gridMaterial(0)
        , m_gridVisible(true)
        , m_rulerVisible(true)
    {
        setWindowTitle("MyOpenGL Camera Visual Test");
        resize(1100, 760);
        Camera* camera = cameraManager().activeCamera();

        if (camera != 0)
        {
            if (camera->projectionType() == ProjectionType::Parallel)
                camera->setParallel(camera->parallelHeight(), camera->nearPlane(), 20.0f);
            else
                camera->setPerspective(camera->perspectiveFieldOfView(), camera->nearPlane(), 20.0f);
        }
        buildTestResources();
        buildTestItems();
    }

protected:
    void drawViewportOverlay(QPainter& painter) override
    {
        painter.setRenderHint(QPainter::Antialiasing, false);

        if (m_rulerVisible)
        {
            QPointF originPixel;
            double worldUnitsPerPixel = 0.0;

            if (viewportGridScale(originPixel, worldUnitsPerPixel))
            {
                const double majorWorldStep = niceGridStep(worldUnitsPerPixel);
                const double minorWorldStep = majorWorldStep / 5.0;
                const double majorPixelStep = majorWorldStep / worldUnitsPerPixel;
                const double minorPixelStep = minorWorldStep / worldUnitsPerPixel;

                drawViewportRuler(painter, originPixel, majorWorldStep, majorPixelStep, minorPixelStep);
            }
        }

        OpenGLViewerWidget::drawViewportOverlay(painter);
    }

    void populateContextMenu(QMenu& menu) override
    {
        OpenGLViewerWidget::populateContextMenu(menu);

        if (!menu.isEmpty())
            menu.addSeparator();

        QAction* gridAction = menu.addAction(QStringLiteral("显示网格"));
        gridAction->setCheckable(true);
        gridAction->setChecked(m_gridVisible);

        connect(gridAction, &QAction::toggled, this, [this](bool visible)
        {
            m_gridVisible = visible;
            update();
        });

        QAction* rulerAction = menu.addAction(QStringLiteral("显示标尺"));
        rulerAction->setCheckable(true);
        rulerAction->setChecked(m_rulerVisible);

        connect(rulerAction, &QAction::toggled, this, [this](bool visible)
        {
            m_rulerVisible = visible;
            updateViewportOverlay();
        });
    }

private:
    /// 计算当前 Viewport 的网格原点和世界尺度。
    ///
    /// 网格原点固定为世界坐标 (0, 0, 0) 在当前 Viewport 中的投影位置。
    ///
    /// Parallel:
    ///     worldUnitsPerPixel = ParallelHeight / ViewportHeight
    ///
    /// Perspective:
    ///     使用经过世界原点、垂直于 Camera Forward 的参考平面，
    ///     计算该平面深度处每 Pixel 对应的世界长度。
    bool viewportGridScale(QPointF& originPixel, double& worldUnitsPerPixel) const
    {
        const Camera* camera = cameraManager().activeCamera();

        if (camera == 0 || width() <= 0 || height() <= 0)
            return false;

        const float aspect = static_cast<float>(width()) / static_cast<float>(height());

        const QMatrix4x4 view = camera->viewMatrix();
        const QMatrix4x4 projection = camera->projectionMatrix(aspect);

        const QVector3D worldOrigin(0.0f, 0.0f, 0.0f);
        const QVector4D clip = projection * view * QVector4D(worldOrigin, 1.0f);

        if (std::fabs(clip.w()) <= 1.0e-8f)
            return false;

        const double ndcX = static_cast<double>(clip.x() / clip.w());
        const double ndcY = static_cast<double>(clip.y() / clip.w());

        /// OpenGL NDC:
        ///     左下角为 (-1, -1)
        ///
        /// QPainter:
        ///     左上角为 (0, 0)
        ///
        /// 因此 Y 方向需要翻转。

        const double pixelX = (ndcX * 0.5 + 0.5) * width();
        const double pixelY = (0.5 - ndcY * 0.5) * height();

        originPixel = QPointF(pixelX, pixelY);

        if (camera->projectionType() == ProjectionType::Parallel)
        {
            worldUnitsPerPixel = static_cast<double>(camera->parallelHeight()) / static_cast<double>(height());
            return worldUnitsPerPixel > 0.0;
        }

        /// Perspective 没有全局唯一的世界单位 / Pixel。
        ///
        /// 这里以经过世界原点并垂直 Camera Forward 的平面作为
        /// Viewport 网格参考平面。

        const double depth = static_cast<double>(
            QVector3D::dotProduct(worldOrigin - camera->position(), camera->forward()));

        if (depth <= 1.0e-8)
            return false;

        const double pi = 3.14159265358979323846;
        const double fieldOfViewRadians = static_cast<double>(camera->perspectiveFieldOfView()) * pi / 180.0;
        const double worldHeight = 2.0 * depth * std::tan(fieldOfViewRadians * 0.5);

        worldUnitsPerPixel = worldHeight / static_cast<double>(height());

        return worldUnitsPerPixel > 0.0;
    }

    /// 根据当前世界单位 / Pixel 自动选择主刻度。
    ///
    /// 刻度序列：
    ///
    /// 0.001  0.002  0.005
    /// 0.01   0.02   0.05
    /// 0.1    0.2    0.5
    /// 1      2      5
    /// 10     20     50
    /// ...
    ///
    /// 主刻度视觉距离尽量保持在约 100 Pixel。
    double niceGridStep(double worldUnitsPerPixel) const
    {
        const double targetPixelSpacing = 100.0;
        const double targetWorldSpacing = worldUnitsPerPixel * targetPixelSpacing;

        if (targetWorldSpacing <= 0.0)
            return 1.0;

        const double power = std::pow(10.0, std::floor(std::log10(targetWorldSpacing)));
        const double normalized = targetWorldSpacing / power;

        double factor = 1.0;

        if (normalized < 1.5)
            factor = 1.0;
        else if (normalized < 3.5)
            factor = 2.0;
        else if (normalized < 7.5)
            factor = 5.0;
        else
            factor = 10.0;

        return factor * power;
    }

    QString formatRulerValue(double value, double step) const
    {
        if (std::fabs(value) < step * 1.0e-8)
            return QStringLiteral("0");

        if (step < 1.0e-4 || std::fabs(value) >= 1.0e7)
            return QString::number(value, 'g', 6);

        int decimals = 0;

        if (step < 1.0)
            decimals = static_cast<int>(std::ceil(-std::log10(step)));

        if (decimals < 0)
            decimals = 0;

        if (decimals > 6)
            decimals = 6;

        return QString::number(value, 'f', decimals);
    }
   
    void drawSceneBackground(Renderer& renderer, const RenderContext& context) override
    {
        OpenGLViewerWidget::drawSceneBackground(renderer, context);

        if (!m_gridVisible || m_gridMaterial == 0 ||
            m_minorGridGeometry == 0 || m_majorGridGeometry == 0)
        {
            return;
        }

        QPointF originPixel;
        double worldUnitsPerPixel = 0.0;

        if (!viewportGridScale(originPixel, worldUnitsPerPixel))
            return;

        const double majorWorldStep = niceGridStep(worldUnitsPerPixel);
        const double minorWorldStep = majorWorldStep / 5.0;
        const double majorPixelStep = majorWorldStep / worldUnitsPerPixel;
        const double minorPixelStep = minorWorldStep / worldUnitsPerPixel;

        const double originX = originPixel.x();
        const double originY = context.viewportHeight - originPixel.y();

        const double minorOffsetX = positiveModulo(originX, minorPixelStep);
        const double minorOffsetY = positiveModulo(originY, minorPixelStep);
        const double majorOffsetX = positiveModulo(originX, majorPixelStep);
        const double majorOffsetY = positiveModulo(originY, majorPixelStep);

        const std::vector<const Light*> noLights;

        RenderState minorState;

        if (buildGridRenderState(
                context,
                minorOffsetX,
                minorOffsetY,
                minorPixelStep,
                minorState))
        {
            if (!renderer.drawGeometry(m_minorGridGeometry, m_gridMaterial, minorState, noLights))
                qWarning() << "CameraTestViewer drawSceneBackground failed: Minor Grid drawing failed.";
        }

        RenderState majorState;

        if (buildGridRenderState(
                context,
                majorOffsetX,
                majorOffsetY,
                majorPixelStep,
                majorState))
        {
            if (!renderer.drawGeometry(m_majorGridGeometry, m_gridMaterial, majorState, noLights))
                qWarning() << "CameraTestViewer drawSceneBackground failed: Major Grid drawing failed.";
        }
    }
    void drawViewportRuler(
        QPainter& painter,
        const QPointF& originPixel,
        double majorWorldStep,
        double majorPixelStep,
        double minorPixelStep)
    {
        if (majorWorldStep <= 0.0 || majorPixelStep <= 0.0 || minorPixelStep <= 0.0)
            return;

        const int topRulerHeight = 26;
        const int leftRulerWidth = 54;
        const int minorDivisionCount = 5;

        /// 标尺背景。

        painter.fillRect(0, 0, width(), topRulerHeight, QColor(30, 34, 38, 220));
        painter.fillRect(0, 0, leftRulerWidth, height(), QColor(30, 34, 38, 220));

        /// 标尺边界。

        QPen borderPen(QColor(180, 185, 190, 180));
        borderPen.setWidth(1);
        painter.setPen(borderPen);

        painter.drawLine(0, topRulerHeight, width(), topRulerHeight);
        painter.drawLine(leftRulerWidth, 0, leftRulerWidth, height());

        /// 次刻度。

        QPen minorPen(QColor(210, 215, 220, 130));
        minorPen.setWidth(1);
        painter.setPen(minorPen);

        const int firstMinorX = static_cast<int>(std::ceil((0.0 - originPixel.x()) / minorPixelStep));
        const int lastMinorX = static_cast<int>(std::floor((width() - originPixel.x()) / minorPixelStep));

        for (int index = firstMinorX; index <= lastMinorX; ++index)
        {
            if (index % minorDivisionCount == 0)
                continue;

            const double x = originPixel.x() + index * minorPixelStep;
            painter.drawLine(QPointF(x, topRulerHeight - 5), QPointF(x, topRulerHeight));
        }

        const int firstMinorY = static_cast<int>(std::ceil((originPixel.y() - height()) / minorPixelStep));
        const int lastMinorY = static_cast<int>(std::floor(originPixel.y() / minorPixelStep));

        for (int index = firstMinorY; index <= lastMinorY; ++index)
        {
            if (index % minorDivisionCount == 0)
                continue;

            const double y = originPixel.y() - index * minorPixelStep;
            painter.drawLine(QPointF(leftRulerWidth - 5, y), QPointF(leftRulerWidth, y));
        }

        /// 主刻度和数值。

        QPen majorPen(QColor(240, 240, 240, 230));
        majorPen.setWidth(1);
        painter.setPen(majorPen);

        const int firstMajorX = static_cast<int>(std::ceil((0.0 - originPixel.x()) / majorPixelStep));
        const int lastMajorX = static_cast<int>(std::floor((width() - originPixel.x()) / majorPixelStep));

        for (int index = firstMajorX; index <= lastMajorX; ++index)
        {
            const double x = originPixel.x() + index * majorPixelStep;
            const double value = index * majorWorldStep;

            painter.drawLine(QPointF(x, topRulerHeight - 10), QPointF(x, topRulerHeight));

            if (x >= leftRulerWidth && x <= width() - 20)
                painter.drawText(QPointF(x + 3.0, 16.0), formatRulerValue(value, majorWorldStep));
        }

        const int firstMajorY = static_cast<int>(std::ceil((originPixel.y() - height()) / majorPixelStep));
        const int lastMajorY = static_cast<int>(std::floor(originPixel.y() / majorPixelStep));

        for (int index = firstMajorY; index <= lastMajorY; ++index)
        {
            const double y = originPixel.y() - index * majorPixelStep;
            const double value = index * majorWorldStep;

            painter.drawLine(QPointF(leftRulerWidth - 10, y), QPointF(leftRulerWidth, y));

            if (y >= topRulerHeight + 8 && y <= height() - 8)
            {
                const QRectF textRect(2.0, y - 8.0, leftRulerWidth - 14.0, 16.0);
                painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, formatRulerValue(value, majorWorldStep));
            }
        }

        /// 左上角遮盖区域。
        ///
        /// 顶部标尺和左侧标尺在这里重叠，
        /// 最后重新覆盖一次，避免数字和刻度互相穿插。

        painter.fillRect(0, 0, leftRulerWidth, topRulerHeight, QColor(30, 34, 38, 240));

        painter.setPen(borderPen);
        painter.drawLine(0, topRulerHeight, leftRulerWidth, topRulerHeight);
        painter.drawLine(leftRulerWidth, 0, leftRulerWidth, topRulerHeight);
    }

    bool buildGridRenderState(
        const RenderContext& context,
        double offsetX,
        double offsetY,
        double pixelStep,
        RenderState& state) const
    {
        if (!context.isValid() || pixelStep <= 0.0)
            return false;

        state = RenderState();

        state.model.setToIdentity();
        state.model.translate(
            static_cast<float>(offsetX),
            static_cast<float>(offsetY),
            0.0f);

        state.model.scale(
            static_cast<float>(pixelStep),
            static_cast<float>(pixelStep),
            1.0f);

        state.view.setToIdentity();

        state.projection.setToIdentity();
        state.projection.ortho(
            0.0f,
            static_cast<float>(context.viewportWidth),
            0.0f,
            static_cast<float>(context.viewportHeight),
            -1.0f,
            1.0f);

        state.viewport = RenderViewport(
            0,
            0,
            context.viewportWidth,
            context.viewportHeight);

        /// Grid 是背景。
        ///
        /// 不参与 Depth，也不写入 Depth。
        /// 后续正常 Scene Item 会直接覆盖 Grid。

        state.depthTestEnabled = false;
        state.depthWriteEnabled = false;

        return state.viewport.isValid();
    }
    bool buildGridGeometry(BufferGeometry*& geometry, const QString& name, const QVector3D& color)
    {
        const int gridLineCount = 1024;

        geometry = new BufferGeometry(name, BufferUsage::Static, RenderType::Lines);

        std::vector<GeometryVertexAttribute> attributes;

        GeometryVertexAttribute position;
        position.location = GeometryAttribute::Position;
        position.componentCount = 3;
        position.valueOffset = 0;
        attributes.push_back(position);

        GeometryVertexAttribute colorAttribute;
        colorAttribute.location = GeometryAttribute::Color;
        colorAttribute.componentCount = 3;
        colorAttribute.valueOffset = 3;
        attributes.push_back(colorAttribute);

        geometry->setVertexLayout(6, attributes);

        std::vector<GLfloat> vertices;      //存储所有顶点属性的原始数组（如位置、颜色）。
        std::vector<GLuint> indices;        //存储顶点索引的数组，告诉 GPU 按什么顺序使用 vertices 中的数据来绘制线段或三角形。

        const int lineCount = gridLineCount + 2;

        vertices.reserve(lineCount * 4 * 6);
        indices.reserve(lineCount * 4);

        GLuint vertexIndex = 0;

        for (int index = -1; index <= gridLineCount; ++index)
        {
            const float value = static_cast<float>(index);
            const float minExtent = -1.0f;
            const float maxExtent = static_cast<float>(gridLineCount);

            /// Vertical。

            vertices.push_back(value);      //x
            vertices.push_back(minExtent);  //y
            vertices.push_back(0.0f);       //z
            vertices.push_back(color.x());  
            vertices.push_back(color.y());
            vertices.push_back(color.z());

            vertices.push_back(value);
            vertices.push_back(maxExtent);
            vertices.push_back(0.0f);
            vertices.push_back(color.x());
            vertices.push_back(color.y());
            vertices.push_back(color.z());

            indices.push_back(vertexIndex++);
            indices.push_back(vertexIndex++);

            /// Horizontal。

            vertices.push_back(minExtent);
            vertices.push_back(value);
            vertices.push_back(0.0f);
            vertices.push_back(color.x());
            vertices.push_back(color.y());
            vertices.push_back(color.z());

            vertices.push_back(maxExtent);
            vertices.push_back(value);
            vertices.push_back(0.0f);
            vertices.push_back(color.x());
            vertices.push_back(color.y());
            vertices.push_back(color.z());

            indices.push_back(vertexIndex++);
            indices.push_back(vertexIndex++);
        }

        geometry->setVertexData(vertices);
        geometry->setIndexData(indices);

        if (resourceManager().adopt(geometry) == InvalidResourceId)
        {
            delete geometry;
            geometry = 0;
            return false;
        }

        return true;
    }
    bool buildGridResources()
    {
        m_gridMaterial = materialManager().createMaterial("ViewportGridMaterial");

        if (m_gridMaterial == 0)
        {
            qWarning() << "CameraTestViewer buildGridResources failed: unable to create Material.";
            return false;
        }

        if (!m_gridMaterial->setSurfaceMode(SurfaceMode::VertexColor))
        {
            qWarning() << "CameraTestViewer buildGridResources failed: unable to configure Material.";
            return false;
        }

        m_gridMaterial->setLightingEnabled(false);

        if (!buildGridGeometry(
                m_minorGridGeometry,
                "ViewportMinorGrid",
                QVector3D(0.68f, 0.68f, 0.68f)))
        {
            qWarning() << "CameraTestViewer buildGridResources failed: unable to build Minor Grid.";
            return false;
        }

        if (!buildGridGeometry(
                m_majorGridGeometry,
                "ViewportMajorGrid",
                QVector3D(0.56f, 0.56f, 0.56f)))
        {
            qWarning() << "CameraTestViewer buildGridResources failed: unable to build Major Grid.";
            return false;
        }

        return true;
    }
    
    /// Billboard Label Test

    bool buildBillboardLabelResources()
    {
        m_billboardSquareGeometry = new BufferGeometry("BillboardSquare", BufferUsage::Static, RenderType::Triangles);

        std::vector<GeometryVertexAttribute> attributes;

        GeometryVertexAttribute position;
        position.location = GeometryAttribute::Position;
        position.componentCount = 3;
        position.valueOffset = 0;
        attributes.push_back(position);

        m_billboardSquareGeometry->setVertexLayout(3, attributes);

        const float halfSize = 0.16f; // 正方形边长为 0.32 个世界单位。

        const std::vector<GLfloat> vertices =
        {
            -halfSize, -halfSize, 0.0f,
             halfSize, -halfSize, 0.0f,
             halfSize,  halfSize, 0.0f,
            -halfSize,  halfSize, 0.0f
        };

        const std::vector<GLuint> indices =
        {
            0, 1, 2,
            0, 2, 3
        };

        m_billboardSquareGeometry->setVertexData(vertices);
        m_billboardSquareGeometry->setIndexData(indices);

        if (resourceManager().adopt(m_billboardSquareGeometry) == InvalidResourceId)
        {
            delete m_billboardSquareGeometry;
            m_billboardSquareGeometry = 0;
            return false;
        }

        m_billboardMaterial = materialManager().createMaterial("BillboardSquareMaterial");

        if (m_billboardMaterial == 0)
            return false;

        if (!m_billboardMaterial->setSurfaceMode(SurfaceMode::Color))
            return false;

        m_billboardMaterial->setLightingEnabled(false);
        m_billboardMaterial->setColor(QVector4D(0.10f, 0.85f, 1.0f, 1.0f));

        return true;
    }

    void buildBillboardLabelItem()
    {
        if (m_billboardSquareGeometry == 0 || m_billboardMaterial == 0)
            return;

        RenderItem* item = itemManager().createItem("BillboardLabelTest");

        if (item == 0)
            return;

        const int rowCount = 9;
        const int columnCount = 9;
        const float spacing = 0.48f; // 相邻锚点之间的世界距离。

        for (int row = 0; row < rowCount; ++row)
        {
            for (int column = 0; column < columnCount; ++column)
            {
                const float x = (static_cast<float>(column) - static_cast<float>(columnCount - 1) * 0.5f) * spacing;
                const float y = (static_cast<float>(row) - static_cast<float>(rowCount - 1) * 0.5f) * spacing + 1.0f;
                const float z = 0.65f * std::sin(x * 1.35f) * std::cos(y * 1.15f); // 让锚点形成具有明显深度变化的波浪面。

                RenderLabel* label = item->createLabel();

                if (label == 0)
                    continue;

                label->setAnchorWorld(QVector3D(x, y, z));
                label->setAnchorSence(QVector2D(0.0f, 0.0f));
                label->setPixelOffset(QPointF(0.0, 0.0));
                label->setGeometry(m_billboardSquareGeometry);
                label->setMaterial(m_billboardMaterial);
                label->setVisible(true);
            }
        }
    }

    void buildTestResources()
    {
        if (!buildGridResources())
            qWarning() << "CameraTestViewer buildTestResources failed: Grid resources failed.";

        if (!buildBillboardLabelResources())
            qWarning() << "CameraTestViewer buildTestResources failed: Billboard Label resources failed.";

        buildCubeGeometry();

        /// Camera Test 只测试 Camera / Viewer 行为。
        /// 使用无光照 VertexColor，避免测试结果受到场景 Light 影响。

        m_vertexColorMaterial = materialManager().createMaterial("CameraTestVertexColor");

        if (m_vertexColorMaterial == 0)
        {
            qWarning() << "CameraTestViewer buildTestResources failed: unable to create Material.";
            return;
        }

        if (!m_vertexColorMaterial->setSurfaceMode(SurfaceMode::VertexColor))
        {
            qWarning() << "CameraTestViewer buildTestResources failed: unable to set VertexColor Material.";
            return;
        }

        m_vertexColorMaterial->setLightingEnabled(false);
    }

    void buildTestItems()
    {
        if (m_cubeGeometry == 0 || m_vertexColorMaterial == 0)
            return;

        const AxisAlignedBoundingBox unitBounds(
            QVector3D(-0.5f, -0.5f, -0.5f),
            QVector3D(0.5f, 0.5f, 0.5f));

        /// Main Box

        RenderItem* mainBox = itemManager().createItem("MainBox");

        if (mainBox != 0)
        {
            mainBox->setMaterial(m_vertexColorMaterial);
            mainBox->transform().setPosition(QVector3D(0.0f, 1.0f, 0.0f));
            mainBox->transform().setScale(QVector3D(4.0f, 2.0f, 2.0f));

            mainBox->setDisplayMode(DisplayMode::ShadedWithEdges);
            mainBox->setEdgeColor(QVector4D(0.05f, 0.05f, 0.05f, 1.0f));

            RenderPart* part = mainBox->createPart();

            if (part != 0)
            {
                part->setGeometry(m_cubeGeometry);
                part->setLocalBounds(unitBounds);
            }
        }

        /// +X Marker

        RenderItem* xMarker = itemManager().createItem("PositiveXMarker");

        if (xMarker != 0)
        {
            xMarker->setMaterial(m_vertexColorMaterial);
            xMarker->transform().setPosition(QVector3D(3.2f, 0.5f, 0.0f));
            xMarker->transform().setScale(QVector3D(1.2f, 1.0f, 1.0f));
            xMarker->setDisplayMode(DisplayMode::ShadedWithEdges);

            RenderPart* part = xMarker->createPart();

            if (part != 0)
            {
                part->setGeometry(m_cubeGeometry);
                part->setLocalBounds(unitBounds);
            }
        }

        /// +Y Marker

        RenderItem* yMarker = itemManager().createItem("PositiveYMarker");

        if (yMarker != 0)
        {
            yMarker->setMaterial(m_vertexColorMaterial);
            yMarker->transform().setPosition(QVector3D(-1.0f, 3.0f, 0.0f));
            yMarker->transform().setScale(QVector3D(1.0f, 2.0f, 1.0f));
            yMarker->setDisplayMode(DisplayMode::ShadedWithEdges);

            RenderPart* part = yMarker->createPart();

            if (part != 0)
            {
                part->setGeometry(m_cubeGeometry);
                part->setLocalBounds(unitBounds);
            }
        }

        /// +Z Marker

        RenderItem* zMarker = itemManager().createItem("PositiveZMarker");

        if (zMarker != 0)
        {
            zMarker->setMaterial(m_vertexColorMaterial);
            zMarker->transform().setPosition(QVector3D(-1.0f, 0.5f, 3.0f));
            zMarker->transform().setScale(QVector3D(1.0f, 1.0f, 2.0f));
            zMarker->setDisplayMode(DisplayMode::ShadedWithEdges);

            RenderPart* part = zMarker->createPart();

            if (part != 0)
            {
                part->setGeometry(m_cubeGeometry);
                part->setLocalBounds(unitBounds);
            }
        }

        buildBillboardLabelItem();
    }

    void buildCubeGeometry()
    {
        m_cubeGeometry = new BufferGeometry("CameraTestCube", BufferUsage::Static, RenderType::Triangles);

        std::vector<GeometryVertexAttribute> attributes;

        GeometryVertexAttribute position;
        position.location = GeometryAttribute::Position;
        position.componentCount = 3;
        position.valueOffset = 0;
        attributes.push_back(position);

        GeometryVertexAttribute color;
        color.location = GeometryAttribute::Color;
        color.componentCount = 3;
        color.valueOffset = 3;
        attributes.push_back(color);

        m_cubeGeometry->setVertexLayout(6, attributes);

        const GLfloat vertices[] =
        {
            // +X
             0.5f, -0.5f, -0.5f,    1.0f, 0.20f, 0.20f,
             0.5f,  0.5f, -0.5f,    1.0f, 0.20f, 0.20f,
             0.5f,  0.5f,  0.5f,    1.0f, 0.20f, 0.20f,
             0.5f, -0.5f,  0.5f,    1.0f, 0.20f, 0.20f,

            // -X
            -0.5f, -0.5f,  0.5f,    0.55f, 0.10f, 0.10f,
            -0.5f,  0.5f,  0.5f,    0.55f, 0.10f, 0.10f,
            -0.5f,  0.5f, -0.5f,    0.55f, 0.10f, 0.10f,
            -0.5f, -0.5f, -0.5f,    0.55f, 0.10f, 0.10f,

            // +Y
            -0.5f,  0.5f, -0.5f,    0.20f, 1.0f, 0.20f,
            -0.5f,  0.5f,  0.5f,    0.20f, 1.0f, 0.20f,
             0.5f,  0.5f,  0.5f,    0.20f, 1.0f, 0.20f,
             0.5f,  0.5f, -0.5f,    0.20f, 1.0f, 0.20f,

            // -Y
            -0.5f, -0.5f,  0.5f,    0.10f, 0.50f, 0.10f,
            -0.5f, -0.5f, -0.5f,    0.10f, 0.50f, 0.10f,
             0.5f, -0.5f, -0.5f,    0.10f, 0.50f, 0.10f,
             0.5f, -0.5f,  0.5f,    0.10f, 0.50f, 0.10f,

            // +Z
             0.5f, -0.5f,  0.5f,    0.20f, 0.35f, 1.0f,
             0.5f,  0.5f,  0.5f,    0.20f, 0.35f, 1.0f,
            -0.5f,  0.5f,  0.5f,    0.20f, 0.35f, 1.0f,
            -0.5f, -0.5f,  0.5f,    0.20f, 0.35f, 1.0f,

            // -Z
            -0.5f, -0.5f, -0.5f,    0.10f, 0.15f, 0.55f,
            -0.5f,  0.5f, -0.5f,    0.10f, 0.15f, 0.55f,
             0.5f,  0.5f, -0.5f,    0.10f, 0.15f, 0.55f,
             0.5f, -0.5f, -0.5f,    0.10f, 0.15f, 0.55f
        };

        const GLuint indices[] =
        {
             0,  1,  2,   0,  2,  3,
             4,  5,  6,   4,  6,  7,
             8,  9, 10,   8, 10, 11,
            12, 13, 14,  12, 14, 15,
            16, 17, 18,  16, 18, 19,
            20, 21, 22,  20, 22, 23
        };

        const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);
        const int indexCount = sizeof(indices) / sizeof(indices[0]);

        m_cubeGeometry->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
        m_cubeGeometry->setIndexData(std::vector<GLuint>(indices, indices + indexCount));

        if (resourceManager().adopt(m_cubeGeometry) == InvalidResourceId)
        {
            qWarning() << "CameraTestViewer buildCubeGeometry failed: unable to adopt Geometry.";

            delete m_cubeGeometry;
            m_cubeGeometry = 0;
        }
    }
    double positiveModulo(double value, double divisor) const
    {
        if (divisor <= 0.0)
            return 0.0;

        double result = std::fmod(value, divisor);

        if (result < 0.0)
            result += divisor;

        return result;
    }
private:
    BufferGeometry* m_cubeGeometry;
    BufferGeometry* m_billboardSquareGeometry; // Billboard 测试共享正方形 Geometry。
    Material* m_billboardMaterial;              // Billboard 测试共享 Material，由 MaterialManager 拥有。
    Material* m_vertexColorMaterial;

    BufferGeometry* m_minorGridGeometry;
    BufferGeometry* m_majorGridGeometry;
    Material* m_gridMaterial;

    bool m_gridVisible;
    bool m_rulerVisible;
};

class MeasurementTestWindow : public QWidget
{
public:
    explicit MeasurementTestWindow(QWidget* parent = 0)
        : QWidget(parent)
        , m_viewer(0)
    {
        setWindowTitle("MyOpenGL Measurement Test");
        resize(1400, 850);

        QHBoxLayout* mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        QWidget* functionPanel = buildFunctionPanel();

        m_viewer = new CameraTestViewer(this);

        mainLayout->addWidget(functionPanel);
        mainLayout->addWidget(m_viewer, 1);
    }

private:
    QWidget* buildFunctionPanel()
    {
        QWidget* panel = new QWidget(this);
        panel->setFixedWidth(220);

        QVBoxLayout* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        QPushButton* navigationButton = new QPushButton(QStringLiteral("导航"), panel);
        QPushButton* length2DButton = new QPushButton(QStringLiteral("二维长度"), panel);
        QPushButton* length3DButton = new QPushButton(QStringLiteral("三维长度"), panel);
        QPushButton* angle2DButton = new QPushButton(QStringLiteral("二维角度"), panel);
        QPushButton* angle3DButton = new QPushButton(QStringLiteral("三维角度"), panel);

        navigationButton->setCheckable(true);
        length2DButton->setCheckable(true);
        length3DButton->setCheckable(true);
        angle2DButton->setCheckable(true);
        angle3DButton->setCheckable(true);

        QButtonGroup* group = new QButtonGroup(panel);
        group->setExclusive(true);

        group->addButton(navigationButton);
        group->addButton(length2DButton);
        group->addButton(length3DButton);
        group->addButton(angle2DButton);
        group->addButton(angle3DButton);

        navigationButton->setChecked(true);

        layout->addWidget(navigationButton);
        layout->addSpacing(8);
        layout->addWidget(length2DButton);
        layout->addWidget(length3DButton);
        layout->addWidget(angle2DButton);
        layout->addWidget(angle3DButton);
        layout->addStretch();

        connect(navigationButton, &QPushButton::clicked, this, [this]()
        {
            m_viewer->setMeasurementTool(0);
            m_viewer->setFocus();
        });

        connect(length2DButton, &QPushButton::clicked, this, [this]()
        {
            m_length2DMeasurement.reset();
            m_viewer->setMeasurementTool(&m_length2DMeasurement);
            m_viewer->setFocus();
        });

        connect(length3DButton, &QPushButton::clicked, this, [this]()
        {
            m_length3DMeasurement.reset();
            m_viewer->setMeasurementTool(&m_length3DMeasurement);
            m_viewer->setFocus();
        });

        connect(angle2DButton, &QPushButton::clicked, this, [this]()
        {
            m_angle2DMeasurement.reset();
            m_viewer->setMeasurementTool(&m_angle2DMeasurement);
            m_viewer->setFocus();
        });

        connect(angle3DButton, &QPushButton::clicked, this, [this]()
        {
            m_angle3DMeasurement.reset();
            m_viewer->setMeasurementTool(&m_angle3DMeasurement);
            m_viewer->setFocus();
        });

        return panel;
    }

private:
    CameraTestViewer* m_viewer;
    Length2DMeasurement m_length2DMeasurement;
    Length3DMeasurement m_length3DMeasurement;
    Angle2DMeasurement m_angle2DMeasurement;
    Angle3DMeasurement m_angle3DMeasurement;
};



int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    MeasurementTestWindow window;
    window.show();

    return application.exec();
}