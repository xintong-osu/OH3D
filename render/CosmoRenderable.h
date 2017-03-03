#ifndef COSMO_RENDERABLE_H
#define COSMO_RENDERABLE_H

#include "GlyphRenderable.h"

class ShaderProgram;
class QOpenGLVertexArrayObject;
class GLSphere;

class CosmoRenderable :public GlyphRenderable
{
public:
	void init() override;
	virtual void DrawWithoutProgram(float modelview[16], float projection[16], ShaderProgram* sp) override;

	void DrawWithoutProgramold(float modelview[16], float projection[16], ShaderProgram* sp);

	void draw(float modelview[16], float projection[16]) override;
	CosmoRenderable(std::shared_ptr<Particle> _particle);

	virtual void setColorMap(COLOR_MAP cm, bool isReversed = false) override;

protected:
	void initPickingDrawingObjects();
	void drawPicking(float modelview[16], float projection[16], bool isForGlyph);

private:
	//std::vector<float> val;// = nullptr;
	std::vector<float3> sphereColor;
	void GenVertexBuffer(int nv, float* vertex);
	virtual void LoadShaders(ShaderProgram*& shaderProg) override;
	unsigned int vbo_vert;
	std::shared_ptr<GLSphere> glyphMesh;
	std::shared_ptr<QOpenGLVertexArrayObject> m_vao;
};
#endif //SPHERE_RENDERABLE_H
