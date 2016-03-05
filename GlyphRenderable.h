#ifndef GLYPH_RENDERABLE_H
#define GLYPH_RENDERABLE_H

#include "Renderable.h"
#include <memory>
class Displace;
class ShaderProgram;
class QOpenGLContext;

class GlyphRenderable: public Renderable
{
	Q_OBJECT
	bool frameBufferObjectInitialized = false;

public:
	std::vector<float4> pos;
	std::vector<float4> posOrig;
	std::vector<char> feature;
	bool isUsingFeature = true;
	bool isPicking = false;


protected:
	std::shared_ptr<Displace> displace;
	std::vector<float> glyphSizeScale;
	std::vector<float> glyphBright;
	float glyphSizeAdjust = 0.5;
	int snappedGlyphIdx = -1;
	ShaderProgram* glProg = nullptr;
	//bool displaceOn = true;
	void ComputeDisplace();
	void mouseMove(int x, int y, int modifier) override;
	void resize(int width, int height) override;
	GlyphRenderable(std::vector<float4>& _pos);

	//virtual void drawPicking(float modelview[16], float projection[16]) = 0;
	unsigned int vbo_vert_picking;
	int snappedGlyphId = -1;
	ShaderProgram *glPickingProg;
	int numVerticeOfGlyph = 0;
	void initPickingDrawingObjects(int nv, float* vertex);
	void drawPicking(float modelview[16], float projection[16]);
	unsigned int framebuffer, renderbuffer[2];

public:
	~GlyphRenderable();
	void SetFeature(std::vector<char> & _feature){ for (int i = 0; i < _feature.size();i++) feature[i] = _feature[i]; };
	void RecomputeTarget();
	void DisplacePoints(std::vector<float2>& pts);
	virtual void LoadShaders(ShaderProgram*& shaderProg) = 0;
	virtual void DrawWithoutProgram(float modelview[16], float projection[16], ShaderProgram* sp) = 0;
	//void SetDispalceOn(bool b) { displaceOn = b; }


	void mousePress(int x, int y, int modifier) override;
	float3 findClosetGlyph(float3 aim);
	int GetSnappedGlyphId(){ return snappedGlyphId; }
	void SetSnappedGlyphId(int s){ snappedGlyphId = s; }


public slots:
	void SlotGlyphSizeAdjustChanged(int v);
};
#endif