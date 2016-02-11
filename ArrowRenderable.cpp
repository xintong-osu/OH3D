#include "ArrowRenderable.h"
#include <teem/ten.h>
//http://www.sci.utah.edu/~gk/vissym04/index.html
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>

//removing the following lines will cause runtime error
#ifdef WIN32
#include "windows.h"
#endif
#define qgl	QOpenGLContext::currentContext()->functions()
#include "ShaderProgram.h"

#include <memory>
#include "glwidget.h"
#include "helper_math.h"
#include "GLArrow.h"
#include "ColorGradient.h"

using namespace std;

ArrowRenderable::ArrowRenderable(vector<float4> _pos, vector<float3> _vec, vector<float> _val) :
GlyphRenderable(_pos)
{
	vecs = _vec;
	//val = &_val; //consider about the color later
	/* input variables */
	lMax = -1, lMin = 999999;
	glyphMesh = std::make_unique<GLArrow>();
	float3 orientation = glyphMesh->orientation;

	for (int i = 0; i < pos.size(); i++) {

		float l = length(_vec[i]);
		val.push_back(l);
		if (l>lMax)
			lMax = l;
		if (l < lMin)
			lMin = l;

		float3 norVec = normalize(_vec[i]);
		float sinTheta = length(cross(orientation, norVec));
		float cosTheta = dot(orientation, norVec);

		if (sinTheta<0.00001)
		{
			rotations.push_back(QMatrix4x4(1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1));
		}
		else
		{
			float3 axis = normalize(cross(orientation, norVec));
			rotations.push_back(QMatrix4x4(
				cosTheta + axis.x*axis.x*(1 - cosTheta), axis.x*axis.y*(1 - cosTheta) - axis.z*sinTheta,
				axis.x*axis.z*(1 - cosTheta) + axis.y*sinTheta, 0,
				axis.y*axis.x*(1 - cosTheta) + axis.z*sinTheta, cosTheta + axis.y*axis.y*(1 - cosTheta),
				axis.y*axis.z*(1 - cosTheta) - axis.x*sinTheta, 0,
				axis.z*axis.x*(1 - cosTheta) - axis.y*sinTheta, axis.z*axis.y*(1 - cosTheta) + axis.x*sinTheta,
				cosTheta + axis.z*axis.z*(1 - cosTheta), 0,
				0, 0, 0, 1));
		}
	}

	ColorGradient cg;
	cols.resize(pos.size());
	for (int i = 0; i < pos.size(); i++) {
		float valRate = (val[i] - lMin) / (lMax - lMin);
		cg.getColorAtValue(valRate, cols[i].x, cols[i].y, cols[i].z);
	}

}


void ArrowRenderable::LoadShaders()
{

#define GLSL(shader) "#version 440\n" #shader
	//shader is from https://www.packtpub.com/books/content/basics-glsl-40-shaders


	const char* vertexVS =
		GLSL(
		in vec4 VertexPosition;
		in vec4 VertexColor;
		in vec3 VertexNormal;
		smooth out vec3 tnorm;
		out vec4 eyeCoords;
		out vec4 fragColor;

		uniform mat4 ModelViewMatrix;
		uniform mat3 NormalMatrix;
		uniform mat4 ProjectionMatrix;
		uniform mat4 SQRotMatrix;
		uniform float scale;

		uniform vec3 Transform;

		vec4 DivZ(vec4 v){
			return vec4(v.x / v.w, v.y / v.w, v.z / v.w, 1.0f);
		}

		void main()
		{
			mat4 MVP = ProjectionMatrix * ModelViewMatrix;
			eyeCoords = ModelViewMatrix * VertexPosition;
			tnorm = normalize(NormalMatrix * /*vec3(VertexPosition) + 0.001 * */VertexNormal);
			//gl_Position = MVP * (VertexPosition + vec4(Transform, 0.0));
			if (scale<1)
				gl_Position = MVP * vec4(vec3(DivZ(SQRotMatrix * (VertexPosition*vec4(scale, scale, scale, 1.0)))) + Transform, 1.0);
			else
				gl_Position = MVP * vec4(vec3(DivZ(SQRotMatrix * (VertexPosition*vec4(1.0, 1.0, scale, 1.0)))) + Transform, 1.0);
			fragColor = VertexColor;
		}
	);

	const char* vertexFS =
		GLSL(
		uniform vec4 LightPosition; // Light position in eye coords.
		uniform vec3 Ka; // Diffuse reflectivity
		uniform vec3 Kd; // Diffuse reflectivity
		uniform vec3 Ks; // Diffuse reflectivity
		uniform float Shininess;
		in vec4 eyeCoords;
		in vec4 fragColor;

		uniform float Bright;

		smooth in vec3 tnorm;
		layout(location = 0) out vec4 FragColor;

		vec3 phongModel(vec3 a, vec4 position, vec3 normal) {
			vec3 s = normalize(vec3(LightPosition - position));
			vec3 v = normalize(-position.xyz);
			vec3 r = reflect(-s, normal);
			vec3 ambient = a + vec3(fragColor)*0.00001;// Ka * 0.8;
			float sDotN = max(dot(s, normal), 0.0);
			vec3 diffuse = Kd * sDotN;
			vec3 spec = vec3(0.0);
			if (sDotN > 0.0)
				spec = Ks *
				pow(max(dot(r, v), 0.0), Shininess);
			return ambient + diffuse + spec;
		}

		void main() {

			FragColor = vec4(Bright * phongModel(Ka * 0.5, eyeCoords, tnorm), 1.0);
		}
	);

	glProg = std::make_unique<ShaderProgram>();
	glProg->initFromStrings(vertexVS, vertexFS);

	glProg->addAttribute("VertexPosition");
	glProg->addAttribute("VertexColor");
	glProg->addAttribute("VertexNormal");
	glProg->addUniform("LightPosition");
	glProg->addUniform("Ka");
	glProg->addUniform("Kd");
	glProg->addUniform("Ks");
	glProg->addUniform("Shininess");

	glProg->addUniform("ModelViewMatrix");
	glProg->addUniform("NormalMatrix");
	glProg->addUniform("ProjectionMatrix");
	glProg->addUniform("SQRotMatrix");
	glProg->addUniform("scale");

	glProg->addUniform("Bright");


	glProg->addUniform("Transform");

}



void ArrowRenderable::init()
{
	LoadShaders();
	m_vao = std::make_unique<QOpenGLVertexArrayObject>();
	m_vao->create();

	m_vao->bind();


	qgl->glGenBuffers(1, &vbo_vert);
	qgl->glBindBuffer(GL_ARRAY_BUFFER, vbo_vert);
	qgl->glVertexAttribPointer(glProg->attribute("VertexPosition"), 4, GL_FLOAT, GL_FALSE, 0, NULL);
	qgl->glBufferData(GL_ARRAY_BUFFER, glyphMesh->GetNumVerts() * sizeof(float)* 4, glyphMesh->GetVerts(), GL_STATIC_DRAW);
	qgl->glBindBuffer(GL_ARRAY_BUFFER, 0);
	qgl->glEnableVertexAttribArray(glProg->attribute("VertexPosition"));

	qgl->glGenBuffers(1, &vbo_normals);
	qgl->glBindBuffer(GL_ARRAY_BUFFER, vbo_normals);
	qgl->glVertexAttribPointer(glProg->attribute("VertexNormal"), 3, GL_FLOAT, GL_FALSE, 0, NULL);
	qgl->glBufferData(GL_ARRAY_BUFFER, glyphMesh->GetNumVerts() * sizeof(float)* 3, glyphMesh->GetNormals(), GL_STATIC_DRAW);
	qgl->glBindBuffer(GL_ARRAY_BUFFER, 0);
	qgl->glEnableVertexAttribArray(glProg->attribute("VertexNormal"));


	qgl->glGenBuffers(1, &vbo_colors);
	qgl->glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
	qgl->glVertexAttribPointer(glProg->attribute("VertexColor"), 4, GL_FLOAT, GL_FALSE, 0, NULL);
	qgl->glBufferData(GL_ARRAY_BUFFER, glyphMesh->GetNumVerts() * sizeof(float)* 4, glyphMesh->GetColors(), GL_STATIC_DRAW);
	qgl->glBindBuffer(GL_ARRAY_BUFFER, 0);
	qgl->glEnableVertexAttribArray(glProg->attribute("VertexColor"));


	qgl->glGenBuffers(1, &vbo_indices);
	qgl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices);
	qgl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)* glyphMesh->GetNumIndices(), glyphMesh->GetIndices(), GL_STATIC_DRAW);
	qgl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	m_vao->release();
}

void ArrowRenderable::draw(float modelview[16], float projection[16])
{
	if (!visible)
		return;

	RecordMatrix(modelview, projection);
	ComputeDisplace();

	glMatrixMode(GL_MODELVIEW);

	int firstVertex = 0;
	int firstIndex = 0;

	for (int i = 0; i < pos.size(); i++) {
		
		float4 shift = pos[i];

		glProg->use();
		m_vao->bind();

		QMatrix4x4 q_modelview = QMatrix4x4(modelview);
		q_modelview = q_modelview.transposed();
		float3 cen = actor->DataCenter();
		qgl->glUniform4f(glProg->uniform("LightPosition"), 0, 0, std::max(std::max(cen.x, cen.y), cen.z) * 2, 1);

		//float3 vec = vecs[i];
		//float cosX = dot(vec, make_float3(1, 0, 0));
		//float cosY = dot(vec, make_float3(0, 1, 0));
		//qgl->glUniform3f(glProg->uniform("Ka"), (cosY + 1) / 2, (cosX + 1) / 2, 1 - (cosX + 1) / 2);
		//qgl->glUniform3f(glProg->uniform("Ka"), 0.8f, 0.8f, 0.8f);
		qgl->glUniform3f(glProg->uniform("Kd"), 0.3f, 0.3f, 0.3f);
		qgl->glUniform3f(glProg->uniform("Ks"), 0.2f, 0.2f, 0.2f);
		qgl->glUniform1f(glProg->uniform("Shininess"), 5);
		qgl->glUniform3fv(glProg->uniform("Transform"), 1, &shift.x);
		qgl->glUniformMatrix4fv(glProg->uniform("ModelViewMatrix"), 1, GL_FALSE, modelview);
		qgl->glUniformMatrix4fv(glProg->uniform("ProjectionMatrix"), 1, GL_FALSE, projection);
		//qgl->glUniformMatrix3fv(glProg->uniform("NormalMatrix"), 1, GL_FALSE, q_modelview.normalMatrix().data());
		qgl->glUniformMatrix3fv(glProg->uniform("NormalMatrix"), 1, GL_FALSE, (q_modelview * rotations[i]).normalMatrix().data());
		qgl->glUniformMatrix4fv(glProg->uniform("SQRotMatrix"), 1, GL_FALSE, rotations[i].data());
		
		float maxSize = 8;

		qgl->glUniform1f(glProg->uniform("Bright"), glyphBright[i]);
		qgl->glUniform1f(glProg->uniform("scale"), val[i] / lMax * maxSize);

		qgl->glUniform3fv(glProg->uniform("Ka"), 1, &cols[i].x);

		glDrawArrays(GL_TRIANGLES, 0, glyphMesh->GetNumVerts());
		m_vao->release();
		glProg->disable();
		glPopMatrix();
	}
}
void ArrowRenderable::UpdateData()
{

}