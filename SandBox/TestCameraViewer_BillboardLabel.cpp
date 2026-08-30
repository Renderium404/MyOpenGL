#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>
#include <QVector3D>
#include <QVector4D>
#include <cmath>
#include <vector>
#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Item/AxisAlignedBoundingBox.h"
#include "MyOpenGL/Item/RenderPointCloud.h"
#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Light/Light.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

class PointCloudTestViewer : public OpenGLViewerWidget {
public:
    explicit PointCloudTestViewer(QWidget* parent = 0)
        : OpenGLViewerWidget(parent), m_pointGeometry(0), m_pointMaterial(0), m_ambientLight(0), m_pointLight(0),
          m_renderPointCloud(0), m_pointCloudItemId(InvalidRenderItemId), m_precisionLevel(1), m_latitudeSegments(0),
          m_longitudeSegments(0), m_pointCount(0), m_buildMilliseconds(0), m_benchmarkEnabled(true),
          m_frameSampleCount(0), m_frameIntervalSumMs(0.0), m_drawTimeSumMs(0.0), m_drawTimeMaxMs(0.0),
          m_averageFrameMs(0.0), m_averageFps(0.0), m_averageDrawMs(0.0), m_maxDrawMs(0.0) {
        setWindowTitle("MyOpenGL RenderPointCloud Billboard Stress Test");
        resize(1100, 760);

        Camera* camera = cameraManager().activeCamera();
        if (camera != 0) {
            if (camera->projectionType() == ProjectionType::Parallel)
                camera->setParallel(camera->parallelHeight(), camera->nearPlane(), 50.0f);
            else
                camera->setPerspective(camera->perspectiveFieldOfView(), camera->nearPlane(), 50.0f);
        }
        
        if (!buildPointResources()) {
            qWarning() << "PointCloudTestViewer construction failed:" << "unable to build point resources.";
            return;
        }
        if (!buildLights()) {
            qWarning() << "PointCloudTestViewer construction failed:" << "unable to build lights.";
            return;
        }
        if (!buildPointCloudItem()) {
            qWarning() << "PointCloudTestViewer construction failed:" << "unable to build PointCloud Item.";
            return;
        }
        rebuildSpherePointCloud();
        resetView();
        resetFrameStatistics();
        m_renderTimer.setInterval(0);
        connect(&m_renderTimer, &QTimer::timeout, this, [this]() { if (m_benchmarkEnabled) update(); });
        m_renderTimer.start();
    }

protected:
    bool handleKeyPress(QKeyEvent* event) override {
        if (event == 0) return false;
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal || event->key() == Qt::Key_Up) {
            ++m_precisionLevel;
            rebuildSpherePointCloud();
            resetFrameStatistics();
            update();
            return true;
        }
        if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Down) {
            if (m_precisionLevel > 1) {
                --m_precisionLevel;
                rebuildSpherePointCloud();
                resetFrameStatistics();
                update();
            }
            return true;
        }
        if (event->key() == Qt::Key_Space) {
            m_benchmarkEnabled = !m_benchmarkEnabled;
            resetFrameStatistics();
            if (m_benchmarkEnabled) update();
            updateViewportOverlay();
            return true;
        }
        if (event->key() == Qt::Key_R) {
            resetView();
            resetFrameStatistics();
            return true;
        }
        return OpenGLViewerWidget::handleKeyPress(event);
    }

    void drawOpenGLFrame(Renderer& renderer, const RenderContext& context) override {
        const qint64 frameStartNs = m_clock.nsecsElapsed();
        if (m_lastFrameStartNs > 0) {
            const double frameIntervalMs = static_cast<double>(frameStartNs - m_lastFrameStartNs) / 1000000.0;
            m_frameIntervalSumMs += frameIntervalMs;
        }
        m_lastFrameStartNs = frameStartNs;
        QElapsedTimer drawTimer;
        drawTimer.start();
        OpenGLViewerWidget::drawOpenGLFrame(renderer, context);
        const double drawMs = static_cast<double>(drawTimer.nsecsElapsed()) / 1000000.0;
        m_drawTimeSumMs += drawMs;
        if (drawMs > m_drawTimeMaxMs) m_drawTimeMaxMs = drawMs;
        ++m_frameSampleCount;
        if (m_frameSampleCount >= 60) finishFrameStatistics();
    }

    void drawViewportOverlay(QPainter& painter) override {
        painter.setPen(Qt::black);
        painter.drawText(QPointF(16.0, 26.0), QString("Level=%1  Lat=%2  Lon=%3  Points=%4")
            .arg(m_precisionLevel).arg(m_latitudeSegments).arg(m_longitudeSegments).arg(m_pointCount));
        painter.drawText(QPointF(16.0, 48.0), QString("Build=%1 ms  Parts=1  DrawCalls~%2")
            .arg(m_buildMilliseconds).arg(m_pointCount));
        painter.drawText(QPointF(16.0, 70.0), QString("FPS=%1  Frame=%2 ms")
            .arg(m_averageFps, 0, 'f', 1).arg(m_averageFrameMs, 0, 'f', 3));
        painter.drawText(QPointF(16.0, 92.0), QString("DrawCPU Avg=%1 ms  Max=%2 ms")
            .arg(m_averageDrawMs, 0, 'f', 3).arg(m_maxDrawMs, 0, 'f', 3));
        painter.drawText(QPointF(16.0, 114.0), QString("Benchmark=%1").arg(m_benchmarkEnabled ? "ON" : "OFF"));
        painter.drawText(QPointF(16.0, 136.0), QStringLiteral("+ / Up：提高一级    - / Down：降低一级"));
        painter.drawText(QPointF(16.0, 158.0), QStringLiteral("Space：连续绘制开关    R：重置视图"));
        OpenGLViewerWidget::drawViewportOverlay(painter);
    }

private:
    int latitudeSegmentsForLevel(int level) const {
        const double baseSegments = 6.0;
        const double growth = std::sqrt(2.0);
        if (level < 1) level = 1;
        const double segments = baseSegments * std::pow(growth, static_cast<double>(level - 1));
        if (segments < 2.0) return 2;
        if (segments > 1000000000.0) return 1000000000;
        return static_cast<int>(std::floor(segments + 0.5));
    }

    bool buildPointResources() {
        m_pointGeometry = new BufferGeometry("SpherePointQuad", BufferUsage::Static, RenderType::Triangles);
        std::vector<GeometryVertexAttribute> attributes;
        GeometryVertexAttribute position;
        position.location = GeometryAttribute::Position;
        position.componentCount = 3;
        position.valueOffset = 0;
        attributes.push_back(position);
        GeometryVertexAttribute normal;
        normal.location = GeometryAttribute::Normal;
        normal.componentCount = 3;
        normal.valueOffset = 3;
        attributes.push_back(normal);
        m_pointGeometry->setVertexLayout(6, attributes);
        const float halfSize = 0.045f;
        const std::vector<GLfloat> vertices = {
            -halfSize, -halfSize, 0.0f,  0.0f, 0.0f, 1.0f,
             halfSize, -halfSize, 0.0f,  0.0f, 0.0f, 1.0f,
             halfSize,  halfSize, 0.0f,  0.0f, 0.0f, 1.0f,
            -halfSize,  halfSize, 0.0f,  0.0f, 0.0f, 1.0f
        };
        const std::vector<GLuint> indices = { 0, 1, 2,  0, 2, 3 };
        m_pointGeometry->setVertexData(vertices);
        m_pointGeometry->setIndexData(indices);
        if (resourceManager().adopt(m_pointGeometry) == InvalidResourceId) {
            delete m_pointGeometry;
            m_pointGeometry = 0;
            qWarning() << "PointCloudTestViewer buildPointResources failed:" << "unable to adopt Point Geometry.";
            return false;
        }
        m_pointMaterial = materialManager().createMaterial("SpherePointMaterial");
        if (m_pointMaterial == 0) {
            qWarning() << "PointCloudTestViewer buildPointResources failed:" << "unable to create Material.";
            return false;
        }
        if (!m_pointMaterial->setSurfaceMode(SurfaceMode::Color)) {
            qWarning() << "PointCloudTestViewer buildPointResources failed:" << "unable to configure Material.";
            return false;
        }
        m_pointMaterial->setLightingEnabled(true);
        m_pointMaterial->setColor(QVector4D(0.08f, 0.55f, 0.95f, 1.0f));
        return true;
    }

    bool buildLights() {
        m_ambientLight = lightManager().createLight("PointCloudAmbientLight");
        if (m_ambientLight == 0) return false;
        m_ambientLight->setAmbient();
        m_ambientLight->setColor(QVector3D(1.0f, 1.0f, 1.0f));
        m_ambientLight->setIntensity(0.18f);
        m_pointLight = lightManager().createLight("PointCloudPointLight");
        if (m_pointLight == 0) return false;
        if (!m_pointLight->setPoint(QVector3D(4.0f, 5.0f, 5.0f), 20.0f)) return false;
        m_pointLight->setColor(QVector3D(1.0f, 0.96f, 0.88f));
        m_pointLight->setIntensity(1.20f);
        return true;
    }

    bool buildPointCloudItem() {
        if (m_pointGeometry == 0 || m_pointMaterial == 0) {
            qWarning() << "PointCloudTestViewer buildPointCloudItem failed:" << "Point resources are incomplete.";
            return false;
        }
        RenderItem* item = itemManager().createItem("SpherePointCloud");
        if (item == 0) {
            qWarning() << "PointCloudTestViewer buildPointCloudItem failed:" << "unable to create RenderItem.";
            return false;
        }
        m_pointCloudItemId = item->id();
        item->setMaterial(m_pointMaterial);
        m_renderPointCloud = item->createRenderPointCloud();
        if (m_renderPointCloud == 0) {
            qWarning() << "PointCloudTestViewer buildPointCloudItem failed:" << "unable to create RenderPointCloud.";
            return false;
        }
        m_renderPointCloud->setGeometry(m_pointGeometry);
        m_renderPointCloud->setFollowCamera(true);
        m_renderPointCloud->setPixelScale(false);
        return true;
    }

    void rebuildSpherePointCloud() {
        if (m_renderPointCloud == 0) {
            qWarning() << "PointCloudTestViewer rebuildSpherePointCloud failed:" << "RenderPointCloud is null.";
            return;
        }
        QElapsedTimer timer;
        timer.start();
        const float radius = 2.0f;
        const float pi = 3.14159265358979323846f;
        m_latitudeSegments = latitudeSegmentsForLevel(m_precisionLevel);
        m_longitudeSegments = m_latitudeSegments * 2;
        const std::size_t expectedPointCount = static_cast<std::size_t>(2) +
            static_cast<std::size_t>(m_latitudeSegments - 1) * static_cast<std::size_t>(m_longitudeSegments);
        std::vector<QVector3D> points;
        points.reserve(expectedPointCount);
        points.push_back(QVector3D(0.0f, radius, 0.0f));
        points.push_back(QVector3D(0.0f, -radius, 0.0f));
        for (int latitude = 1; latitude < m_latitudeSegments; ++latitude) {
            const float theta = pi * static_cast<float>(latitude) / static_cast<float>(m_latitudeSegments);
            const float y = radius * std::cos(theta);
            const float ringRadius = radius * std::sin(theta);
            for (int longitude = 0; longitude < m_longitudeSegments; ++longitude) {
                const float phi = 2.0f * pi * static_cast<float>(longitude) / static_cast<float>(m_longitudeSegments);
                const float x = ringRadius * std::cos(phi);
                const float z = ringRadius * std::sin(phi);
                points.push_back(QVector3D(x, y, z));
            }
        }
        m_renderPointCloud->setPoints(points);
        m_pointCount = m_renderPointCloud->pointCount();
        m_buildMilliseconds = timer.elapsed();
        qDebug() << "Sphere RenderPointCloud" << "Level=" << m_precisionLevel
                 << "Latitude=" << m_latitudeSegments << "Longitude=" << m_longitudeSegments
                 << "Points=" << m_pointCount << "Parts=1" << "DrawCalls~" << m_pointCount
                 << "Build(ms)=" << m_buildMilliseconds;
        updateViewportOverlay();
    }

    void resetView() {
        if (width() <= 0 || height() <= 0) return;
        const AxisAlignedBoundingBox bounds(QVector3D(-2.4f, -2.4f, -2.4f), QVector3D(2.4f, 2.4f, 2.4f));
        if (!cameraManager().setViewDirection(bounds.center(), QVector3D(0.0f, 0.0f, -1.0f), QVector3D(0.0f, 1.0f, 0.0f)))
            return;
        cameraManager().fitBounds(bounds, width(), height(), 1.15f);
        update();
        updateViewportOverlay();
    }

    void resetFrameStatistics() {
        if (!m_clock.isValid()) m_clock.start();
        m_lastFrameStartNs = 0;
        m_frameSampleCount = 0;
        m_frameIntervalSumMs = 0.0;
        m_drawTimeSumMs = 0.0;
        m_drawTimeMaxMs = 0.0;
        m_averageFrameMs = 0.0;
        m_averageFps = 0.0;
        m_averageDrawMs = 0.0;
        m_maxDrawMs = 0.0;
        updateViewportOverlay();
    }

    void finishFrameStatistics() {
        const int intervalCount = (m_lastFrameStartNs > 0 && m_frameSampleCount > 1) ? m_frameSampleCount - 1 : 0;
        if (intervalCount > 0) {
            m_averageFrameMs = m_frameIntervalSumMs / static_cast<double>(intervalCount);
            if (m_averageFrameMs > 1.0e-8) m_averageFps = 1000.0 / m_averageFrameMs;
            else m_averageFps = 0.0;
        }
        if (m_frameSampleCount > 0) {
            m_averageDrawMs = m_drawTimeSumMs / static_cast<double>(m_frameSampleCount);
            m_maxDrawMs = m_drawTimeMaxMs;
        }
        qDebug() << "PointCloud Draw Benchmark" << "Level=" << m_precisionLevel
                 << "Points=" << m_pointCount << "Parts=1" << "DrawCalls~" << m_pointCount
                 << "FPS=" << m_averageFps << "Frame(ms)=" << m_averageFrameMs
                 << "DrawCPU Avg(ms)=" << m_averageDrawMs << "DrawCPU Max(ms)=" << m_maxDrawMs;
        m_lastFrameStartNs = 0;
        m_frameSampleCount = 0;
        m_frameIntervalSumMs = 0.0;
        m_drawTimeSumMs = 0.0;
        m_drawTimeMaxMs = 0.0;
        updateViewportOverlay();
    }

private:
    BufferGeometry* m_pointGeometry;
    Material* m_pointMaterial;
    Light* m_ambientLight;
    Light* m_pointLight;
    RenderPointCloud* m_renderPointCloud;
    RenderItemId m_pointCloudItemId;
    int m_precisionLevel;
    int m_latitudeSegments;
    int m_longitudeSegments;
    int m_pointCount;
    qint64 m_buildMilliseconds;
    QTimer m_renderTimer;
    bool m_benchmarkEnabled;
    QElapsedTimer m_clock;
    qint64 m_lastFrameStartNs = 0;
    int m_frameSampleCount;
    double m_frameIntervalSumMs;
    double m_drawTimeSumMs;
    double m_drawTimeMaxMs;
    double m_averageFrameMs;
    double m_averageFps;
    double m_averageDrawMs;
    double m_maxDrawMs;
};

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    PointCloudTestViewer viewer;
    viewer.show();
    return application.exec();
}