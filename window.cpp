#include "window.h"
#include "glwidget.h"
//#include "VecReader.h"
#include "BoxRenderable.h"
#include "ParticleReader.h"
#include "DTIVolumeReader.h"
#include "SphereRenderable.h"
#include "SQRenderable.h"
#include "LensRenderable.h"
#include "GridRenderable.h"
#include "Displace.h"

#include "VecReader.h"
#include "ArrowRenderable.h"
#include "DataMgr.h"
#include "VRWidget.h"
#include "VRGlyphRenderable.h"
#include "ModelGridRenderable.h"
#include <ModelGrid.h>
#include "GLMatrixManager.h"
#include "PolyRenderable.h"
#include "MeshReader.h"


#include <LeapListener.h>
#include <Leap.h>

enum DATA_TYPE
{
	TYPE_PARTICLE,
	TYPE_TENSOR,
	TYPE_VECTOR,
};

QSlider* CreateSlider()
{
	QSlider* slider = new QSlider(Qt::Horizontal);
	slider->setRange(0, 10);
	slider->setValue(5);
	return slider;
}

class GLTextureCube;
Window::Window()
{
    setWindowTitle(tr("Interactive Glyph Visualization"));
	QHBoxLayout *mainLayout = new QHBoxLayout;

	dataMgr = std::make_unique<DataMgr>();
	
	//QSizePolicy fixedPolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
	std::unique_ptr<Reader> reader;

	DATA_TYPE dataType = DATA_TYPE::TYPE_PARTICLE; //DATA_TYPE::TYPE_VECTOR;//DATA_TYPE::TYPE_TENSOR; //
	if ("TYPE_PARTICLE" == dataMgr->GetConfig("DATA_TYPE")){
		dataType = DATA_TYPE::TYPE_PARTICLE;
	}
	else if ("TYPE_VECTOR" == dataMgr->GetConfig("DATA_TYPE")){
		dataType = DATA_TYPE::TYPE_VECTOR;
	}
	else if ("TYPE_TENSOR" == dataMgr->GetConfig("DATA_TYPE")){
		dataType = DATA_TYPE::TYPE_TENSOR;
	}
	const std::string dataPath = dataMgr->GetConfig("DATA_PATH");

	if (DATA_TYPE::TYPE_PARTICLE == dataType) {
		reader = std::make_unique<ParticleReader>(dataPath.c_str());
		glyphRenderable = std::make_unique<SphereRenderable>(
			((ParticleReader*)reader.get())->GetPos(),
			((ParticleReader*)reader.get())->GetVal());
		std::cout << "number of rendered glyphs: " << (((ParticleReader*)reader.get())->GetVal()).size() << std::endl;
	}
	else if (DATA_TYPE::TYPE_TENSOR == dataType) {
		reader = std::make_unique<DTIVolumeReader>(dataPath.c_str());
		std::vector<float4> pos;
		std::vector<float> val;

		if (((DTIVolumeReader*)reader.get())->LoadFeature(dataMgr->GetConfig("FEATURE_PATH").c_str())){
			std::vector<char> feature;
			((DTIVolumeReader*)reader.get())->GetSamplesWithFeature(pos, val, feature);
			glyphRenderable = std::make_unique<SQRenderable>(pos, val);
			glyphRenderable->SetFeature(feature, ((DTIVolumeReader*)reader.get())->featureCenter);
		}
		else
		{
			((DTIVolumeReader*)reader.get())->GetSamples(pos, val);
			glyphRenderable = std::make_unique<SQRenderable>(pos, val);
		}
		std::cout << "number of rendered glyphs: " << pos.size() << std::endl;

	}
	else if (DATA_TYPE::TYPE_VECTOR == dataType) {
		reader = std::make_unique<VecReader>(dataPath.c_str());
		std::vector<float4> pos;
		std::vector<float3> vec;
		std::vector<float> val;
		((VecReader*)reader.get())->GetSamples(pos, vec, val);
		glyphRenderable = std::make_unique<ArrowRenderable>(pos, vec, val);

		std::cout << "number of rendered glyphs: " << pos.size() << std::endl;
	}
	std::cout << "number of rendered glyphs: " << glyphRenderable->GetNumOfGlyphs() << std::endl;

	/********GL widget******/
	matrixMgr = std::make_shared<GLMatrixManager>();
	openGL = std::make_unique<GLWidget>(matrixMgr);
	lensRenderable = std::make_unique<LensRenderable>();
	//lensRenderable->SetDrawScreenSpace(false);
	if ("ON" == dataMgr->GetConfig("VR_SUPPORT")){
		vrWidget = std::make_unique<VRWidget>(matrixMgr, openGL.get());
		vrWidget->setWindowFlags(Qt::Window);
		vrGlyphRenderable = std::make_unique<VRGlyphRenderable>(glyphRenderable.get());
		vrWidget->AddRenderable("glyph", vrGlyphRenderable.get());
		vrWidget->AddRenderable("lens", lensRenderable.get());
		openGL->SetVRWidget(vrWidget.get());
	}
	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(2, 0);
	format.setProfile(QSurfaceFormat::CoreProfile);
	openGL->setFormat(format); // must be called before the widget or its parent window gets shown


	float3 posMin, posMax;
	reader->GetPosRange(posMin, posMax);
	gridRenderable = std::make_unique<GridRenderable>(64);
	matrixMgr->SetVol(posMin, posMax);// cubemap->GetInnerDim());
	modelGrid = std::make_shared<ModelGrid>(&posMin.x, &posMax.x, 25);
	modelGridRenderable = std::make_unique<ModelGridRenderable>(modelGrid.get());
	glyphRenderable->SetModelGrid(modelGrid.get());
	//openGL->AddRenderable("bbox", bbox);
	openGL->AddRenderable("glyph", glyphRenderable.get());
	openGL->AddRenderable("lenses", lensRenderable.get());
	openGL->AddRenderable("grid", gridRenderable.get());
	openGL->AddRenderable("model", modelGridRenderable.get());
	if (DATA_TYPE::TYPE_TENSOR == dataType) {
		PolyRenderable* polyFeature;
		MeshReader *meshReader;
		meshReader = new MeshReader();
		meshReader->LoadPLY(dataMgr->GetConfig("ventricles").c_str());
		polyFeature = new PolyRenderable(meshReader);
		openGL->AddRenderable("ventricles", polyFeature);

		meshReader = new MeshReader();
		meshReader->LoadPLY(dataMgr->GetConfig("tumor1").c_str());
		polyFeature = new PolyRenderable(meshReader);
		openGL->AddRenderable("tumor1", polyFeature);

		meshReader = new MeshReader();
		meshReader->LoadPLY(dataMgr->GetConfig("tumor2").c_str());
		polyFeature = new PolyRenderable(meshReader);
		openGL->AddRenderable("tumor2", polyFeature);

	}

	///********controls******/
	addLensBtn = new QPushButton("Add circle lens");
	addLineLensBtn = new QPushButton("Add straight band lens");
	delLensBtn = std::make_unique<QPushButton>("Delete a lens");
	addCurveBLensBtn = new QPushButton("Add curved band lens");
	saveStateBtn = std::make_unique<QPushButton>("Save State");
	loadStateBtn = std::make_unique<QPushButton>("Load State");

	QCheckBox* gridCheck = new QCheckBox("Grid", this);
	QLabel* transSizeLabel = new QLabel("Transition region size:", this);
	QSlider* transSizeSlider = CreateSlider();
	listener = new LeapListener();
	controller = new Leap::Controller();
	controller->setPolicyFlags(Leap::Controller::PolicyFlag::POLICY_OPTIMIZE_HMD);
	controller->addListener(*listener);
	

	QGroupBox *groupBox = new QGroupBox(tr("Deformation Mode"));
	QHBoxLayout *deformModeLayout = new QHBoxLayout;
	radioDeformScreen = std::make_unique<QRadioButton>(tr("&screen space"));
	radioDeformObject = std::make_unique<QRadioButton>(tr("&object space"));
	radioDeformScreen->setChecked(true);
	deformModeLayout->addWidget(radioDeformScreen.get());
	deformModeLayout->addWidget(radioDeformObject.get());
	groupBox->setLayout(deformModeLayout);

	usingGlyphSnappingCheck = new QCheckBox("Snapping Glyph", this);
	usingGlyphPickingCheck = new QCheckBox("Picking Glyph", this);
	freezingFeatureCheck = new QCheckBox("Freezing Feature", this);
	usingFeatureSnappingCheck = new QCheckBox("Snapping Feature", this);
	usingFeaturePickingCheck = new QCheckBox("Picking Feature", this);

	connect(glyphRenderable.get(), SIGNAL(glyphPickingFinished()), this, SLOT(SlotToggleGlyphPickingFinished()));
	connect(glyphRenderable.get(), SIGNAL(featurePickingFinished()), this, SLOT(SlotToggleFeaturePickingFinished()));


	QVBoxLayout *controlLayout = new QVBoxLayout;
	controlLayout->addWidget(addLensBtn);
	controlLayout->addWidget(addLineLensBtn);

	controlLayout->addWidget(addCurveBLensBtn);
	controlLayout->addWidget(delLensBtn.get());
	controlLayout->addWidget(saveStateBtn.get());
	controlLayout->addWidget(loadStateBtn.get());
	controlLayout->addWidget(groupBox);
	controlLayout->addWidget(transSizeLabel);
	controlLayout->addWidget(transSizeSlider);
	controlLayout->addWidget(usingGlyphSnappingCheck);
	controlLayout->addWidget(usingGlyphPickingCheck);
	controlLayout->addWidget(freezingFeatureCheck);
	controlLayout->addWidget(usingFeatureSnappingCheck); 
	controlLayout->addWidget(usingFeaturePickingCheck);
	controlLayout->addWidget(gridCheck);
	controlLayout->addStretch();

	connect(addLensBtn, SIGNAL(clicked()), this, SLOT(AddLens()));
	connect(addLineLensBtn, SIGNAL(clicked()), this, SLOT(AddLineLens()));
	connect(addCurveBLensBtn, SIGNAL(clicked()), this, SLOT(AddCurveBLens()));
	connect(delLensBtn.get(), SIGNAL(clicked()), lensRenderable.get(), SLOT(SlotDelLens()));
	connect(saveStateBtn.get(), SIGNAL(clicked()), this, SLOT(SlotSaveState()));
	connect(loadStateBtn.get(), SIGNAL(clicked()), this, SLOT(SlotLoadState()));


	connect(gridCheck, SIGNAL(clicked(bool)), this, SLOT(SlotToggleGrid(bool)));
	connect(transSizeSlider, SIGNAL(valueChanged(int)), lensRenderable.get(), SLOT(SlotFocusSizeChanged(int)));
	connect(listener, SIGNAL(UpdateRightHand(QVector3D, QVector3D, QVector3D)),
		this, SLOT(UpdateRightHand(QVector3D, QVector3D, QVector3D)));
	connect(usingGlyphSnappingCheck, SIGNAL(clicked(bool)), this, SLOT(SlotToggleUsingGlyphSnapping(bool)));
	connect(usingGlyphPickingCheck, SIGNAL(clicked(bool)), this, SLOT(SlotTogglePickingGlyph(bool)));
	connect(freezingFeatureCheck, SIGNAL(clicked(bool)), this, SLOT(SlotToggleFreezingFeature(bool)));
	connect(usingFeatureSnappingCheck, SIGNAL(clicked(bool)), this, SLOT(SlotToggleUsingFeatureSnapping(bool)));
	connect(usingFeaturePickingCheck, SIGNAL(clicked(bool)), this, SLOT(SlotTogglePickingFeature(bool)));
	connect(radioDeformObject.get(), SIGNAL(clicked(bool)), this, SLOT(SlotDeformModeChanged(bool)));
	connect(radioDeformScreen.get(), SIGNAL(clicked(bool)), this, SLOT(SlotDeformModeChanged(bool)));

	
	mainLayout->addWidget(openGL.get(), 3);
	mainLayout->addLayout(controlLayout,1);
	setLayout(mainLayout);
}

void Window::AddLens()
{
	lensRenderable->AddCircleLens();
}

void Window::AddLineLens()
{
	lensRenderable->AddLineBLens();
}


void Window::AddCurveBLens()
{
	lensRenderable->AddCurveBLens();
}

//void Window::animate()
//{
//	//int v = heightScaleSlider->value();
//	//v = (v + 1) % nHeightScale;
//	//heightScaleSlider->setValue(v);
//	openGL->animate();
//}
//

void Window::SlotToggleGrid(bool b)
{
	modelGridRenderable->SetVisibility(b);
}

Window::~Window() {
}

void Window::init()
{
	if ("ON" == dataMgr->GetConfig("VR_SUPPORT")){
		vrWidget->show();
	}
}


void Window::UpdateRightHand(QVector3D thumbTip, QVector3D indexTip, QVector3D indexDir)
{
	//std::cout << indexTip.x() << "," << indexTip.y() << "," << indexTip.z() << std::endl;
	lensRenderable->SlotLensCenterChanged(make_float3(indexTip.x(), indexTip.y(), indexTip.z()));
}

void Window::SlotToggleUsingGlyphSnapping(bool b)
{
	lensRenderable->isSnapToGlyph = b;
	if (!b){
		glyphRenderable->SetSnappedGlyphId(-1);
	}
	else{
		usingFeatureSnappingCheck->setChecked(false);
		SlotToggleUsingFeatureSnapping(false);
		usingFeaturePickingCheck->setChecked(false);
		SlotTogglePickingFeature(false);
	}
}

void Window::SlotTogglePickingGlyph(bool b)
{
	glyphRenderable->isPickingGlyph = b;
	if (b){
		usingFeatureSnappingCheck->setChecked(false);
		SlotToggleUsingFeatureSnapping(false);
		usingFeaturePickingCheck->setChecked(false);
		SlotTogglePickingFeature(false);
	}
}

void Window::SlotToggleFreezingFeature(bool b)
{
	glyphRenderable->isFreezingFeature = b;
	glyphRenderable->RecomputeTarget();
}

void Window::SlotToggleUsingFeatureSnapping(bool b)
{
	lensRenderable->isSnapToFeature = b;
	if (!b){
		glyphRenderable->SetSnappedFeatureId(-1);
	}
	else{
		usingGlyphSnappingCheck->setChecked(false);
		SlotToggleUsingGlyphSnapping(false);
		usingGlyphPickingCheck->setChecked(false);
		SlotTogglePickingGlyph(false);
	}
}

void Window::SlotTogglePickingFeature(bool b)
{
	glyphRenderable->isPickingFeature = b;
	if (b){
		usingGlyphSnappingCheck->setChecked(false);
		SlotToggleUsingGlyphSnapping(false);
		usingGlyphPickingCheck->setChecked(false);
		SlotTogglePickingGlyph(false);
	}
}

void Window::SlotSaveState()
{
	matrixMgr->SaveState("state.txt");
}

void Window::SlotLoadState()
{
	matrixMgr->LoadState("state.txt");
}


void Window::SlotToggleGlyphPickingFinished()
{
	usingGlyphPickingCheck->setChecked(false);
}

void Window::SlotToggleFeaturePickingFinished()
{
	usingFeaturePickingCheck->setChecked(false);
}

void Window::SlotDeformModeChanged(bool clicked)
{
	if (radioDeformScreen->isChecked()){
		openGL->SetDeformModel(DEFORM_MODEL::SCREEN_SPACE);
	}
	else if (radioDeformObject->isChecked()){
		openGL->SetDeformModel(DEFORM_MODEL::OBJECT_SPACE);
	}
}