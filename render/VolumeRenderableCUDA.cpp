#include <time.h>
#include "VolumeRenderableCUDA.h"

#include <vector>

#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions_1_2>
#include <QOpenGLFunctions_4_3_Core>
// removing the following lines will cause runtime error
#ifdef WIN32
#include "windows.h"
#endif
#define qgl	QOpenGLContext::currentContext()->functions()

#include <memory>
#include "DeformGLWidget.h"
#include "helper_math.h"
#include <LineSplitModelGrid.h>
#include <cuda_gl_interop.h>

#include "VolumeRenderableCUDAKernel.h"
#include "modelVolumeDeformer.h"
#include "Lens.h"
#include "ModelGrid.h"
#include <TransformFunc.h>



VolumeRenderableCUDA::VolumeRenderableCUDA(std::shared_ptr<Volume> _volume)
{
	volume = _volume;

	volumeCUDAGradient.VolumeCUDA_init(_volume->size, 0, 1, 4);

}

VolumeRenderableCUDA::~VolumeRenderableCUDA()
{
	
	VolumeRender_deinit();

	deinitTextureAndCudaArrayOfScreen();

	//cudaDeviceReset();
};

void VolumeRenderableCUDA::init()
{
	VolumeRender_init();

	initTextureAndCudaArrayOfScreen();
}



void VolumeRenderableCUDA::draw(float modelview[16], float projection[16])
{
	if (!visible)
		return;

	RecordMatrix(modelview, projection);

	int winWidth, winHeight;
	actor->GetWindowSize(winWidth, winHeight);

	QMatrix4x4 q_modelview = QMatrix4x4(modelview).transposed();
	QMatrix4x4 q_invMV = q_modelview.inverted();
	QVector4D q_eye4 = q_invMV.map(QVector4D(0, 0, 0, 1));
	float3 eyeInWorld = make_float3(q_eye4[0], q_eye4[1], q_eye4[2]);

	QMatrix4x4 q_projection = QMatrix4x4(projection).transposed();
	QMatrix4x4 q_mvp = q_projection*q_modelview;
	QMatrix4x4 q_invMVP = q_mvp.inverted();

	q_invMV.copyDataTo(invMVMatrix);
	q_invMVP.copyDataTo(invMVPMatrix); //copyDataTo() automatically copy in row-major order
	q_mvp.copyDataTo(MVPMatrix);
	q_modelview.copyDataTo(MVMatrix);
	q_modelview.normalMatrix().copyDataTo(NMatrix);
	bool isCutaway = vis_method == VIS_METHOD::CUTAWAY;
	VolumeRender_setConstants(MVMatrix, MVPMatrix, invMVMatrix, invMVPMatrix, NMatrix, &isCutaway, &transFuncP1, &transFuncP2, &la, &ld, &ls, &(volume->spacing));
	if (!isFixed){
		recordFixInfo(q_mvp, q_modelview);
	}

	//prepare the storage for output
	uint *d_output;
	checkCudaErrors(cudaGraphicsMapResources(1, &cuda_pbo_resource, 0));
	size_t num_bytes;
	checkCudaErrors(cudaGraphicsResourceGetMappedPointer((void **)&d_output, &num_bytes,
		cuda_pbo_resource));
	checkCudaErrors(cudaMemset(d_output, 0, winWidth*winHeight * 4));

	if (lenses != 0 && lenses->size() > 0 && !(lenses->back()->isConstructing) && modelVolumeDeformer != 0){
		ComputeDisplace(modelview, projection);
		VolumeRender_computeGradient(&(modelVolumeDeformer->volumeCUDADeformed), &volumeCUDAGradient);
		VolumeRender_setGradient(&volumeCUDAGradient);
		VolumeRender_setVolume(&(modelVolumeDeformer->volumeCUDADeformed));
	}
	else if(volume!=0){
		VolumeRender_computeGradient(&(volume->volumeCuda), &volumeCUDAGradient);
		VolumeRender_setGradient(&volumeCUDAGradient);
		VolumeRender_setVolume(&(volume->volumeCuda));
	}
	else {
		std::cout << "data not well set for volume renderable" << std::endl;
		exit(0);
	}


	//compute the dvr
	VolumeRender_render(d_output, winWidth, winHeight, density, brightness, eyeInWorld, volume->size, maxSteps, tstep, useColor);

	checkCudaErrors(cudaGraphicsUnmapResources(1, &cuda_pbo_resource, 0));

	// display results
	glClear(GL_COLOR_BUFFER_BIT);

	// draw image from PBO
	glDisable(GL_DEPTH_TEST);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// draw using texture
	// copy from pbo to texture
	qgl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	qgl->glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, volumeTex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, winWidth, winHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0);


	qgl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	// draw textured quad

	auto functions12 = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_1_2>();
	functions12->glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(-1, -1);
	glTexCoord2f(1, 0);
	glVertex2f(1, -1);
	glTexCoord2f(1, 1);
	glVertex2f(1, 1);
	glTexCoord2f(0, 1);
	glVertex2f(-1, 1);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	glEnable(GL_DEPTH_TEST);
}

//a better code design should place this part into the modelGrid?
void VolumeRenderableCUDA::ComputeDisplace(float _mv[16], float _pj[16])
{
	if (lenses!=0 && lenses->size() > 0){
		Lens *l = lenses->back();

		if (l->justChanged){

			switch (((DeformGLWidget*)actor)->GetDeformModel())
			{
			case DEFORM_MODEL::OBJECT_SPACE:
			{
				if (l->type == TYPE_LINE)
					modelGrid->setReinitiationNeed();
				l->justChanged = false;
				break;
			}
			}
			//this setting can only do deform based on the last lens
		}

		if (((DeformGLWidget*)actor)->GetDeformModel() == DEFORM_MODEL::OBJECT_SPACE && l->type == TYPE_LINE && l->isConstructing == false){

			int winWidth, winHeight;
			actor->GetWindowSize(winWidth, winHeight);

			float3 dmin, dmax;
			volume->GetPosRange(dmin, dmax);
			((LineLens3D*)l)->UpdateLineLensGlobalInfo(winWidth, winHeight, _mv, _pj, dmin, dmax);

			if (actor->GetInteractMode() == INTERACT_MODE::TRANSFORMATION){
				modelGrid->ReinitiateMeshForVolume((LineLens3D*)l, volume);
				modelGrid->UpdateMesh(&(((LineLens3D*)l)->c.x), &(((LineLens3D*)l)->lensDir.x), ((LineLens3D*)l)->lSemiMajorAxisGlobal, ((LineLens3D*)l)->lSemiMinorAxisGlobal, ((LineLens3D*)l)->focusRatio, ((LineLens3D*)l)->majorAxisGlobal);			
				modelVolumeDeformer->deformByModelGrid(modelGrid->GetLensSpaceOrigin(), ((LineLens3D*)l)->majorAxisGlobal, ((LineLens3D*)l)->lensDir, modelGrid->GetNumSteps(), modelGrid->GetStep());
			}
		}
	}

}


void VolumeRenderableCUDA::resetVolume()
{
	//VolumeCUDA_deinit(&volumeCUDACur);

	//int winWidth, winHeight;
	//actor->GetWindowSize(winWidth, winHeight);

	//cudaExtent volumeSize = make_cudaExtent(volume->size[0], volume->size[1], volume->size[2]);
	//VolumeCUDA_init(&volumeCUDACur, volumeSize, volume, 1);

	//isFixed = false;
	//curDeformDegree = 0;
}




void VolumeRenderableCUDA::mousePress(int x, int y, int modifier)
{
	lastPt = make_int2(x, y);
}

void VolumeRenderableCUDA::mouseRelease(int x, int y, int modifier)
{

}

void VolumeRenderableCUDA::mouseMove(int x, int y, int modifier)
{

}

bool VolumeRenderableCUDA::MouseWheel(int x, int y, int modifier, int delta)
{

	return false;
}


void VolumeRenderableCUDA::initTextureAndCudaArrayOfScreen()
{
	int winWidth, winHeight;
	actor->GetWindowSize(winWidth, winHeight);

	qgl->glGenBuffers(1, &pbo);
	qgl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	qgl->glBufferData(GL_PIXEL_UNPACK_BUFFER, winWidth*winHeight*sizeof(GLubyte)* 4, 0, GL_STREAM_DRAW);
	qgl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	// register this buffer object with CUDA
	checkCudaErrors(cudaGraphicsGLRegisterBuffer(&cuda_pbo_resource, pbo, cudaGraphicsMapFlagsWriteDiscard));

	// create texture for display
	qgl->glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &volumeTex);
	glBindTexture(GL_TEXTURE_2D, volumeTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, winWidth, winHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void VolumeRenderableCUDA::deinitTextureAndCudaArrayOfScreen()
{
	if (cuda_pbo_resource != 0)
		checkCudaErrors(cudaGraphicsUnregisterResource(cuda_pbo_resource));
	if (pbo != 0)
		qgl->glDeleteBuffers(1, &pbo);
	if (volumeTex != 0)
		glDeleteTextures(1, &volumeTex);


}

void VolumeRenderableCUDA::resize(int width, int height)
{
	visible = false;
	deinitTextureAndCudaArrayOfScreen();
	initTextureAndCudaArrayOfScreen();
	visible = true;
}

