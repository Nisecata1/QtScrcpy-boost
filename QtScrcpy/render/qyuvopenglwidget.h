#ifndef QYUVOPENGLWIDGET_H
#define QYUVOPENGLWIDGET_H
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QRect>

class QYUVOpenGLWidget
    : public QOpenGLWidget
    , protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit QYUVOpenGLWidget(QWidget *parent = nullptr);
    virtual ~QYUVOpenGLWidget() override;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void setStreamFrameSize(const QSize &frameSize);
    const QSize &frameSize() const;
    void setCanvasSize(const QSize &canvasSize);
    const QSize &canvasSize() const;
    void setContentRect(const QRect &contentRect);
    const QRect &contentRect() const;
    QSize framebufferPixelSize() const;
    void updateTextures(quint8 *dataY, quint8 *dataU, quint8 *dataV, quint32 linesizeY, quint32 linesizeU, quint32 linesizeV);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    QSize effectiveCanvasSize() const;
    QRect effectiveContentRect() const;
    void initShader();
    void initTextures();
    void deInitTextures();
    void updateTexture(GLuint texture, quint32 textureType, quint8 *pixels, quint32 stride);

private:
    QSize m_streamFrameSize = { -1, -1 };
    QSize m_canvasSize = { -1, -1 };
    QRect m_contentRect;
    QSize m_framebufferPixelSize = { -1, -1 };
    bool m_needUpdate = false;
    bool m_textureInited = false;

    QOpenGLBuffer m_vbo;
    QOpenGLShaderProgram m_shaderProgram;
    GLuint m_texture[3] = { 0 };
};

#endif // QYUVOPENGLWIDGET_H
