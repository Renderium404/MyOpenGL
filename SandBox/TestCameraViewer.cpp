#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>

#include <vector>

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Scene/AxisAlignedBoundingBox.h"
#include "MyOpenGL/Scene/RenderItem.h"
#include "MyOpenGL/Scene/RenderPart.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

class CameraTestViewer : public OpenGLViewerWidget
{
public:
    explicit CameraTestViewer(QWidget* parent = 0)
        : OpenGLViewerWidget(parent)
        , m_cubeGeometry(0)
        , m_vertexColorMaterial(0)
        , m_informationLabel(0)
    {
        setWindowTitle("MyOpenGL Camera Visual Test");
        resize(1100, 760);

        buildTestResources();
        buildTestItems();

        m_informationLabel = new QLabel(this);
        m_informationLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_informationLabel->setStyleSheet(
            "QLabel {"
            " color: rgb(235, 240, 245);"
            " background-color: rgba(20, 24, 28, 190);"
            " border: 1px solid rgba(120, 130, 140, 180);"
            " padding: 8px;"
            "}"
        );

        m_informationLabel->move(12, 12);
        m_informationLabel->show();

        updateInformation();
    }

protected:
    void initializeGL() override
    {
        OpenGLViewerWidget::initializeGL();
        updateInformation();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        OpenGLViewerWidget::mouseMoveEvent(event);

        if (event->buttons() & (Qt::LeftButton | Qt::MiddleButton))
            updateInformation();
    }

    void wheelEvent(QWheelEvent* event) override
    {
        OpenGLViewerWidget::wheelEvent(event);
        updateInformation();
    }

private:
    void buildTestResources()
    {
        buildCubeGeometry();

        m_vertexColorMaterial = new Material("CameraTestVertexColor");
        m_vertexColorMaterial->setVertexColor();

        materialManager().add(m_vertexColorMaterial);
    }

    void buildTestItems()
    {
        const AxisAlignedBoundingBox unitBounds(
            QVector3D(-0.5f, -0.5f, -0.5f),
            QVector3D(0.5f, 0.5f, 0.5f));

        RenderItem* mainBox = scene().createItem("MainBox");
        mainBox->setMaterial(m_vertexColorMaterial);
        mainBox->transform().setPosition(QVector3D(0.0f, 1.0f, 0.0f));
        mainBox->transform().setScale(QVector3D(4.0f, 2.0f, 2.0f));
        mainBox->setDisplayMode(RenderItemDisplayShadedWithEdges);
        mainBox->setEdgeColor(QVector4D(0.05f, 0.05f, 0.05f, 1.0f));

        RenderPart* mainBoxPart = mainBox->createPart(0);
        mainBoxPart->setGeometry(m_cubeGeometry);
        mainBoxPart->setLocalBounds(unitBounds);

        RenderItem* xMarker = scene().createItem("PositiveXMarker");
        xMarker->setMaterial(m_vertexColorMaterial);
        xMarker->transform().setPosition(QVector3D(3.2f, 0.5f, 0.0f));
        xMarker->transform().setScale(QVector3D(1.2f, 1.0f, 1.0f));
        xMarker->setDisplayMode(RenderItemDisplayShadedWithEdges);

        RenderPart* xMarkerPart = xMarker->createPart(0);
        xMarkerPart->setGeometry(m_cubeGeometry);
        xMarkerPart->setLocalBounds(unitBounds);

        RenderItem* yMarker = scene().createItem("PositiveYMarker");
        yMarker->setMaterial(m_vertexColorMaterial);
        yMarker->transform().setPosition(QVector3D(-1.0f, 3.0f, 0.0f));
        yMarker->transform().setScale(QVector3D(1.0f, 2.0f, 1.0f));
        yMarker->setDisplayMode(RenderItemDisplayShadedWithEdges);

        RenderPart* yMarkerPart = yMarker->createPart(0);
        yMarkerPart->setGeometry(m_cubeGeometry);
        yMarkerPart->setLocalBounds(unitBounds);

        RenderItem* zMarker = scene().createItem("PositiveZMarker");
        zMarker->setMaterial(m_vertexColorMaterial);
        zMarker->transform().setPosition(QVector3D(-1.0f, 0.5f, 3.0f));
        zMarker->transform().setScale(QVector3D(1.0f, 1.0f, 2.0f));
        zMarker->setDisplayMode(RenderItemDisplayShadedWithEdges);

        RenderPart* zMarkerPart = zMarker->createPart(0);
        zMarkerPart->setGeometry(m_cubeGeometry);
        zMarkerPart->setLocalBounds(unitBounds);
    }

    void buildCubeGeometry()
    {
        m_cubeGeometry = new BufferGeometry("CameraTestCube", ResourceUpdateStatic, Triangles);

        std::vector<GeometryVertexAttribute> attributes;

        GeometryVertexAttribute position;
        position.location = 0;
        position.componentCount = 3;
        position.valueOffset = 0;
        attributes.push_back(position);

        GeometryVertexAttribute color;
        color.location = 1;
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

        resourceManager().add(m_cubeGeometry);
    }

    void updateInformation()
    {
        if (m_informationLabel == 0)
            return;

        const Camera* camera = cameraManager().activeCamera();

        if (camera == 0)
        {
            m_informationLabel->setText("Camera does not exist.");
            m_informationLabel->adjustSize();
            return;
        }

        const QString projectionName =
            camera->projectionType() == ProjectionType::Perspective ? "Perspective" : "Parallel";

        QString text;
        text += QString("Projection: %1\n").arg(projectionName);

        text += QString("Position: (%1, %2, %3)\n")
            .arg(camera->position().x(), 0, 'f', 2)
            .arg(camera->position().y(), 0, 'f', 2)
            .arg(camera->position().z(), 0, 'f', 2);

        text += QString("Forward:  (%1, %2, %3)\n")
            .arg(camera->forward().x(), 0, 'f', 2)
            .arg(camera->forward().y(), 0, 'f', 2)
            .arg(camera->forward().z(), 0, 'f', 2);

        text += QString("Up:       (%1, %2, %3)\n")
            .arg(camera->up().x(), 0, 'f', 2)
            .arg(camera->up().y(), 0, 'f', 2)
            .arg(camera->up().z(), 0, 'f', 2);

        text += QString("Right:    (%1, %2, %3)\n")
            .arg(camera->right().x(), 0, 'f', 2)
            .arg(camera->right().y(), 0, 'f', 2)
            .arg(camera->right().z(), 0, 'f', 2);

        if (cameraManager().hasViewBounds())
        {
            const QVector3D center = cameraManager().viewBounds().center();
            const float distance = (camera->position() - center).length();

            text += QString("View Center: (%1, %2, %3)\n")
                .arg(center.x(), 0, 'f', 2)
                .arg(center.y(), 0, 'f', 2)
                .arg(center.z(), 0, 'f', 2);

            text += QString("Distance To View Center: %1\n").arg(distance, 0, 'f', 3);
        }
        else
        {
            text += "View Bounds: None\n";
        }

        if (camera->projectionType() == ProjectionType::Perspective)
            text += QString("FOV: %1\n").arg(camera->perspectiveFieldOfView(), 0, 'f', 2);
        else
            text += QString("Parallel Height: %1\n").arg(camera->parallelHeight(), 0, 'f', 3);

        text += QString("Near: %1\n").arg(camera->nearPlane(), 0, 'f', 3);
        text += QString("Far:  %1\n").arg(camera->farPlane(), 0, 'f', 3);

        text += "\nMouse:\n";
        text += "  Left Drag   : Orbit\n";
        text += "  Middle Drag : Pan\n";
        text += "  Wheel       : Zoom\n";

        text += "\nKeyboard:\n";
        text += "  P : Perspective / Parallel\n";
        text += "  F : Fit Scene";

        m_informationLabel->setText(text);
        m_informationLabel->adjustSize();
        m_informationLabel->raise();
    }

private:
    BufferGeometry* m_cubeGeometry;
    Material* m_vertexColorMaterial;
    QLabel* m_informationLabel;
};

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    CameraTestViewer viewer;
    viewer.show();

    return application.exec();
}