#include "window.h"
#include "glwidget.h"
#include "BoxRenderable.h"
#include "LensRenderable.h"
#include "GridRenderable.h"
#include "Displace.h"
#include <iostream>

#include "SphereRenderable.h"
#include "SolutionParticleReader.h"
#include "RawVolumeReader.h"
#include "Volume.h"

#include "DataMgr.h"
#include "ModelGridRenderable.h"
#include <ModelGrid.h>
#include "GLMatrixManager.h"
#include "PolyRenderable.h"
#include "MeshReader.h"
#include "VecReader.h"
#include "VolumeRenderableCUDA.h"
#include "ModelVolumeDeformer.h"
#include "Lens.h"

#ifdef USE_LEAP
#include <leap/LeapListener.h>
#include <Leap.h>
#endif

#ifdef USE_OSVR
#include "VRWidget.h"
#include "VRGlyphRenderable.h"
#endif

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

	dataMgr = std::make_shared<DataMgr>();
	


	



	std::shared_ptr<Reader> reader;

	const std::string dataPath = dataMgr->GetConfig("DATA_PATH");
	reader = std::make_shared<SolutionParticleReader>(dataPath.c_str());
	glyphRenderable = std::make_shared<SphereRenderable>(
		((ParticleReader*)reader.get())->GetPos(),
		((ParticleReader*)reader.get())->GetVal());
	std::cout << "number of rendered glyphs: " << (((ParticleReader*)reader.get())->GetVal()).size() << std::endl;
	std::cout << "number of rendered glyphs: " << glyphRenderable->GetNumOfGlyphs() << std::endl;


	const std::string dataPath2 = dataMgr->GetConfig("VOLUME_DATA_PATH");

	int3 dims;
	float3 spacing;
	if (string(dataPath2).find("MGHT2") != std::string::npos){
		dims = make_int3(320, 320, 256);
		spacing = make_float3(0.7, 0.7, 0.7);
	}
	else if (string(dataPath2).find("MGHT1") != std::string::npos){
		dims = make_int3(256, 256, 176);
		spacing = make_float3(1.0, 1.0, 1.0);
	}
	else if (string(dataPath2).find("nek128") != std::string::npos){
		dims = make_int3(128, 128, 128);
		spacing = make_float3(2, 2, 2); //to fit the streamline of nek256
	}
	inputVolume = std::make_shared<Volume>();

	if (string(dataPath2).find(".vec") != std::string::npos){
		std::shared_ptr<VecReader> reader2;
		reader2 = std::make_shared<VecReader>(dataPath2.c_str());
		reader2->OutputToVolumeByNormalizedVecMag(inputVolume);
		reader2.reset();
	}
	else{
		std::shared_ptr<RawVolumeReader> reader2;
		reader2 = std::make_shared<RawVolumeReader>(dataPath2.c_str(), dims);
		reader2->OutputToVolumeByNormalizedValue(inputVolume);
		reader2.reset();
	}
	inputVolume->spacing = spacing;
	inputVolume->initVolumeCuda(0);

	volumeRenderable = std::make_shared<VolumeRenderableCUDA>(inputVolume);

	if (string(dataPath2).find("nek128") != std::string::npos){
		volumeRenderable->useColor = true;
	}
	
	
	/********GL widget******/
#ifdef USE_OSVR
	matrixMgr = std::make_shared<GLMatrixManager>(true);
#else
	matrixMgr = std::make_shared<GLMatrixManager>(false);
#endif
	openGL = std::make_shared<GLWidget>(matrixMgr);
	lensRenderable = std::make_shared<LensRenderable>();
	//lensRenderable->SetDrawScreenSpace(false);
#ifdef USE_OSVR
		vrWidget = std::make_shared<VRWidget>(matrixMgr, openGL.get());
		vrWidget->setWindowFlags(Qt::Window);
		vrGlyphRenderable = std::make_shared<VRGlyphRenderable>(glyphRenderable.get());
		vrWidget->AddRenderable("glyph", vrGlyphRenderable.get());
		vrWidget->AddRenderable("lens", lensRenderable.get());
		openGL->SetVRWidget(vrWidget.get());
#endif
	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(2, 0);
	format.setProfile(QSurfaceFormat::CoreProfile);
	openGL->setFormat(format); // must be called before the widget or its parent window gets shown


	float3 posMin, posMax;
	inputVolume->GetPosRange(posMin, posMax);
	gridRenderable = std::make_shared<GridRenderable>(64);
	matrixMgr->SetVol(posMin, posMax);// cubemap->GetInnerDim());
	modelGrid = std::make_shared<ModelGrid>(&posMin.x, &posMax.x, 20, true);
	modelGridRenderable = std::make_shared<ModelGridRenderable>(modelGrid.get());
	glyphRenderable->SetModelGrid(modelGrid.get());
	glyphRenderable->SetVisibility(false);


	modelVolumeDeformer = std::make_shared<ModelVolumeDeformer>();
	modelVolumeDeformer->SetModelGrid(modelGrid.get());
	modelVolumeDeformer->Init(inputVolume.get());
	volumeRenderable->SetModelVolumeDeformer(modelVolumeDeformer);
	volumeRenderable->lenses = lensRenderable->GetLensesAddr();
	volumeRenderable->SetModelGrid(modelGrid.get());

	openGL->AddRenderable("glyph", glyphRenderable.get());
	openGL->AddRenderable("lenses", lensRenderable.get());
	//openGL->AddRenderable("grid", gridRenderable.get());
	openGL->AddRenderable("1volume", volumeRenderable.get()); //make sure the volume is rendered first since it does not use depth test

	openGL->AddRenderable("model", modelGridRenderable.get());
	///********controls******/
	addLensBtn = new QPushButton("Add circle lens");
	addLineLensBtn = new QPushButton("Add straight band lens");
	delLensBtn = std::make_shared<QPushButton>("Delete a lens");
	addCurveBLensBtn = new QPushButton("Add curved band lens");
	saveStateBtn = std::make_shared<QPushButton>("Save State");
	loadStateBtn = std::make_shared<QPushButton>("Load State");
std::cout << posMin.x << " " << posMin.y << " " << posMin.z << std::endl;
std::cout << posMax.x << " " << posMax.y << " " << posMax.z << std::endl;
	QCheckBox* gridCheck = new QCheckBox("Grid", this);
	QCheckBox* udbeCheck = new QCheckBox("Use Density Based Elasticity", this);
	udbeCheck->setChecked(modelGrid->useDensityBasedElasticity);

	QLabel* transSizeLabel = new QLabel("Transition region size:", this);
	QSlider* transSizeSlider = CreateSlider();
#ifdef USE_LEAP
	listener = new LeapListener();
	controller = new Leap::Controller();
	controller->setPolicyFlags(Leap::Controller::PolicyFlag::POLICY_OPTIMIZE_HMD);
	controller->addListener(*listener);
#endif

	QGroupBox *groupBox = new QGroupBox(tr("Deformation Mode"));
	QHBoxLayout *deformModeLayout = new QHBoxLayout;
	radioDeformScreen = std::make_shared<QRadioButton>(tr("&screen space"));
	radioDeformObject = std::make_shared<QRadioButton>(tr("&object space"));
	radioDeformScreen->setChecked(true);
	deformModeLayout->addWidget(radioDeformScreen.get());
	deformModeLayout->addWidget(radioDeformObject.get());
	groupBox->setLayout(deformModeLayout);

	usingGlyphSnappingCheck = new QCheckBox("Snapping Glyph", this);
	usingGlyphPickingCheck = new QCheckBox("Picking Glyph", this);

	connect(glyphRenderable.get(), SIGNAL(glyphPickingFinished()), this, SLOT(SlotToggleGlyphPickingFinished()));


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
	controlLayout->addWidget(gridCheck);
	controlLayout->addWidget(udbeCheck);
	controlLayout->addStretch();

	connect(addLensBtn, SIGNAL(clicked()), this, SLOT(AddLens()));
	connect(addLineLensBtn, SIGNAL(clicked()), this, SLOT(AddLineLens()));
	connect(addCurveBLensBtn, SIGNAL(clicked()), this, SLOT(AddCurveBLens()));
	connect(delLensBtn.get(), SIGNAL(clicked()), lensRenderable.get(), SLOT(SlotDelLens()));
	connect(saveStateBtn.get(), SIGNAL(clicked()), this, SLOT(SlotSaveState()));
	connect(loadStateBtn.get(), SIGNAL(clicked()), this, SLOT(SlotLoadState()));

	
	connect(gridCheck, SIGNAL(clicked(bool)), this, SLOT(SlotToggleGrid(bool)));
	connect(udbeCheck, SIGNAL(clicked(bool)), this, SLOT(SlotToggleUdbe(bool)));
	connect(transSizeSlider, SIGNAL(valueChanged(int)), lensRenderable.get(), SLOT(SlotFocusSizeChanged(int)));
#ifdef USE_LEAP
	connect(listener, SIGNAL(UpdateHands(QVector3D, QVector3D, int)),
		this, SLOT(SlotUpdateHands(QVector3D, QVector3D, int)));
#endif
	connect(usingGlyphSnappingCheck, SIGNAL(clicked(bool)), this, SLOT(SlotToggleUsingGlyphSnapping(bool)));
	connect(usingGlyphPickingCheck, SIGNAL(clicked(bool)), this, SLOT(SlotTogglePickingGlyph(bool)));
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


void Window::SlotTogglePickingGlyph(bool b)
{
	glyphRenderable->isPickingGlyph = b;
}


void Window::SlotToggleUsingGlyphSnapping(bool b)
{
	lensRenderable->isSnapToGlyph = b;
	if (!b){
		glyphRenderable->SetSnappedGlyphId(-1);
	}
}

void Window::SlotToggleGrid(bool b)
{
	modelGridRenderable->SetVisibility(b);
}

void Window::SlotToggleUdbe(bool b)
{
	modelGrid->useDensityBasedElasticity = b;
	modelGrid->setReinitiationNeed();
	std::vector<Lens*> *lenses = volumeRenderable->lenses;

	if (lenses->size() > 0){
		((*lenses)[lenses->size() - 1])->justChanged = true;
	}
}

Window::~Window() {
}

void Window::init()
{
#ifdef USE_OSVR
		vrWidget->show();
#endif
}

#ifdef USE_LEAP
void Window::SlotUpdateHands(QVector3D leftIndexTip, QVector3D rightIndexTip, int numHands)
{
	if (1 == numHands){
		lensRenderable->SlotOneHandChanged(make_float3(rightIndexTip.x(), rightIndexTip.y(), rightIndexTip.z()));
	}
	else if(2 == numHands){
		//
		lensRenderable->SlotTwoHandChanged(
			make_float3(leftIndexTip.x(), leftIndexTip.y(), leftIndexTip.z()),
			make_float3(rightIndexTip.x(), rightIndexTip.y(), rightIndexTip.z()));
		
	}
}
#endif

void Window::SlotSaveState()
{
	matrixMgr->SaveState("current.state");
}

void Window::SlotLoadState()
{
	matrixMgr->LoadState("current.state");
}


void Window::SlotToggleGlyphPickingFinished()
{
	usingGlyphPickingCheck->setChecked(false);
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

