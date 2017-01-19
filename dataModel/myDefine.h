#ifndef MODES_H
#define MODES_H

#include "vector_types.h"
//#include <stdio.h>
#include <string.h>

struct matrix4x4
{
	float4 v[4];

	matrix4x4(float* _v){
		memcpy(&v[0].x, _v, sizeof(float4) * 4);
	}

	matrix4x4(){}
};

typedef struct
{
	float3 m[3];
} float3x3;

typedef struct
{
	float4 m[4];
} float4x4;


struct Ray
{
	float3 o;    // origin
	float3 d;    // direction
};

struct RayCastingParameters
{
	//NEK
	//lighting
	float la = 1.0, ld = 0.2, ls = 0.1;
	////MGHT2
	//transfer function
	float transFuncP1 = 0.55;
	float transFuncP2 = 0.13;
	float density = 1;
	//ray casting
	int maxSteps = 768;
	float tstep = 0.25f;
	float brightness = 1.0;
	bool useColor = true;


	////////MGHT2
	////lighting
	//float la = 1.0, ld = 0.2, ls = 0.7;
	////transfer function
	//float transFuncP1 = 0.44;// 0.55;
	//float transFuncP2 = 0.29;// 0.13;
	//float density = 1.25;
	////ray casting
	//int maxSteps = 512;
	//float tstep = 0.25f;
	//float brightness = 1.3;
	//bool useColor = false;
	RayCastingParameters(){};
	RayCastingParameters(float a, float b, float c, float d, float e, float f, int g, float h, float i, bool j){
		la = a, ld = b, ls = c;
		transFuncP1 = d, transFuncP2 = e;
		density = f;
		maxSteps = g;
		tstep = h; brightness = i;
		useColor = j;
	}
};


#endif
