#ifndef VR_WIDGET_H
#define VR_WIDGET_H

#include <QtWidgets>
#include <QVector3D>
#include <QMatrix4x4>
#include <QOpenGLWidget>
#include <vector_types.h>
#include <vector_functions.h>
#include <memory>
//enum INTERACT_MODE{
//	//	DRAG_LENS_EDGE,
//	//	DRAG_LENS_TWO_ENDS,
//	LENS,
//	TRANSFORMATION,
//	MODIFYING_LENS,
//	//CUT_LINE,
//	//ADD_NODE,
//	MODIFY_LENS,
//	//DRAW_ELLIPSE,
//};


//class Trackball;
//class Rotation;
class StopWatchInterface;
class Renderable;
class GLWidget;
class GLMatrixManager;
namespace osvr{
	namespace clientkit{
		class ClientContext;
		class DisplayConfig;
	}
}

class VRWidget : public QOpenGLWidget, public QOpenGLFunctions
{
	Q_OBJECT
public:
	explicit VRWidget(std::shared_ptr<GLMatrixManager> _matrixMgr, GLWidget* _mainGLWidget, QWidget *parent = 0);
	~VRWidget();

	QSize minimumSizeHint() const Q_DECL_OVERRIDE;
	QSize sizeHint() const Q_DECL_OVERRIDE;

	void AddRenderable(const char* name, void* r);

	Renderable* GetRenderable(const char* name);

	void GetWindowSize(int &w, int &h) { w = width; h = height; }

	int2 GetWindowSize() { return make_int2(width, height); }

	//void SetVol(int3 dim);

	//void SetVol(float3 posMin, float3 posMax);
	//void GetVol(float3 &posMin, float3 &posMax){ posMin = dataMin; posMax = dataMax; }
	//float3 DataCenter();
	void UpdateGL();


	//INTERACT_MODE GetInteractMode(){ return interactMode; }

	//void SetInteractMode(INTERACT_MODE v) { interactMode = v; }


protected:
	virtual void initializeGL() Q_DECL_OVERRIDE;
	virtual void paintGL() Q_DECL_OVERRIDE;
	virtual void resizeGL(int width, int height) Q_DECL_OVERRIDE;
	//virtual void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	//virtual void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	//virtual void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
	//virtual void wheelEvent(QWheelEvent * event) Q_DECL_OVERRIDE;
	virtual void keyPressEvent(QKeyEvent * event) Q_DECL_OVERRIDE;
	//virtual bool event(QEvent *event) Q_DECL_OVERRIDE;

	uint width = 750, height = 900;


private:
	void computeFPS();

	void TimerStart();

	void TimerEnd();


	//QPointF pixelPosToViewPos(const QPointF& p);

	//QPoint pixelPosToGLPos(const QPoint& p);

	//bool gestureEvent(QGestureEvent *event);

	//void pinchTriggered(QPinchGesture *gesture);
	/*****view*****/
	//transformation states
	//QVector3D transVec = QVector3D(0.0f, 0.0f, -5.0f);//move it towards the front of the camera
	//QMatrix4x4 transRot;
	//float transScale = 1;
	//float currentTransScale = 1;

	//Trackball *trackball;
	//QPointF prevPos;//previous mouse position
	//Rotation *rot;

	//INTERACT_MODE interactMode = INTERACT_MODE::TRANSFORMATION;

	/****timing****/
	StopWatchInterface *timer = 0;
	int m_frame;
	int fpsCount = 0;        // FPS count for averaging
	int fpsLimit = 128;        // FPS limit for sampling
	int g_Index = 0;
	unsigned int frameCount = 0;

	std::map<std::string, Renderable*> renderers;

	bool initialized = false;
	GLWidget* mainGLWidget;
	std::shared_ptr<GLMatrixManager> matrixMgr;

	std::shared_ptr<osvr::clientkit::ClientContext> ctx;// ("com.osvr.example.SDLOpenGL");
	std::shared_ptr<osvr::clientkit::DisplayConfig> display;// (ctx);

	//bool pinching = false;
	////mark whether there is any pinching gesture in this sequence of gestures.
	//// in order to prevent rotation if pinching is finished while one finger is still on the touch screen.
	//bool pinched = false;

	//float3 dataMin = make_float3(0, 0, 0);
	//float3 dataMax = make_float3(10, 10, 10);

	//private slots:
	//void animate();

};

#endif //VR_WIDGET_H