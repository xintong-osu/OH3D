#include <Displace.h>
#include <TransformFunc.h>
#include <vector_functions.h>
#include <helper_math.h>
#include <Lens.h>
#include <math_constants.h>

void Displace::LoadOrig(float4* v, int num)
{
	posOrig.assign(v, v + num);// , posOrig.begin());
	d_vec_posScreenTarget.assign(num, make_float2(0, 0));
	d_vec_glyphSizeTarget.assign(num, 1);
	//d_vec_Dist2LensBtm.assign(num, 0);
}

struct functor_Object2Clip//: public thrust::unary_function<float,float>
{
	__device__ __host__ float4 operator() (float4 p)
	{
		return Object2Clip(p, &mv.v[0].x, &pj.v[0].x);
	}
	matrix4x4 mv, pj;
	functor_Object2Clip(matrix4x4 _mv, matrix4x4 _pj) : mv(_mv), pj(_pj){}
};

struct functor_Clip2Screen
{
	int w, h;
	__device__ float2 operator() (float4 p)
	{
		return Clip2ScreenGlobal(GetXY(p), w, h);
	}
	functor_Clip2Screen(int _w, int _h) :w(_w), h(_h){}
};

__device__ __host__ inline float G(float x, float r)
{
	return pow((r - 1), 2) / (-r * r * x + r) + 2 - 1 / r;
}

__device__ __host__ inline float G_Diff(float x, float r)
{
	return pow((r - 1)/ (r * x - 1), 2);
}

__device__ __host__ float2 DisplaceCircleLens(float x, float y, float r, float2 screenPos, float& glyphSize, float focusRatio, float rSide = 0)
{
	float2 ret = screenPos;
	float2 dir = screenPos - make_float2(x, y);
	float disOrig = length(dir);
	float rOut = (r + rSide) / focusRatio; //including the focus and transition region
	if (disOrig < rOut) {
		float disNew = G(disOrig / rOut, focusRatio) * rOut;
		ret = make_float2(x, y) + dir / disOrig * disNew;
		glyphSize = G_Diff(disOrig / rOut, focusRatio);
	}
	return ret;
}

struct functor_Displace
{
	int x, y, r;
	float d;
	float focusRatio;
	float sideSize;
	template<typename Tuple>
	__device__ __host__ void operator() (Tuple t){//float2 screenPos, float4 clipPos) {
		float2 screenPos = thrust::get<0>(t);
		float4 clipPos = thrust::get<1>(t);
		float2 ret = screenPos;
		if (clipPos.z < d) {
			float glyphSize = 1;
			ret = DisplaceCircleLens(x, y, r, screenPos, glyphSize, focusRatio, (d - clipPos.z) * r * 64 * sideSize);
			thrust::get<3>(t) = glyphSize;
		}
		thrust::get<2>(t) = ret;
	}
	functor_Displace(int _x, int _y, int _r, float _d, float _focusRatio, float _sideSize) 
		: x(_x), y(_y), r(_r), d(_d), focusRatio(_focusRatio), sideSize(_sideSize){}
};

struct functor_Displace_Line
{
	int x, y;
	float d;

	float lSemiMajorAxis, lSemiMinorAxis;
	float2 direction;

	__device__ float2 operator() (float2 screenPos, float4 clipPos) {
		float2 ret = screenPos;

		if (clipPos.z < d) {
			//sigmoid function: y=2*(1/(1+e^(-20*(x+1)))-0.5), x in [-1,0]
			//sigmoid function: y=2*(1/(1+e^(20*(x-1)))-0.5), x in [0,1]

			//dot product of (_x-x, _y-y) and direction

			float2 toPoint = screenPos - make_float2(x, y);
			float disMajor = toPoint.x*direction.x + toPoint.y*direction.y;
			if (abs(disMajor) < lSemiMajorAxis) {

				float2 minorDirection = make_float2(-direction.y, direction.x);
				//dot product of (_x-x, _y-y) and minorDirection
				float disMinor = toPoint.x*minorDirection.x + toPoint.y*minorDirection.y;


				float disMajorRatio = disMajor / lSemiMajorAxis;
				float disSigmoid; //always positive or 0
				if (disMajorRatio < 0){
					disSigmoid = 1 / (1 + exp(-40 * (disMajorRatio + 0.8)));
				}
				else {
					disSigmoid = 1 / (1 + exp(40 * (disMajorRatio - 0.8)));
				}

				float ratio = 0.5;
				if (abs(disMinor) < disSigmoid*lSemiMinorAxis / ratio){			
					float rOut = disSigmoid *lSemiMinorAxis / ratio; //including the focus and transition region

					float disMinorNewAbs = G(abs(disMinor) / rOut, ratio) * rOut;
					if (disMinor > 0){
						ret = make_float2(screenPos.x, screenPos.y) + minorDirection * (disMinorNewAbs - disMinor);
					}
					else {
						ret = make_float2(screenPos.x, screenPos.y) - minorDirection * (disMinorNewAbs + disMinor);
					}
				}
			}
		}
		return ret;
	}
	functor_Displace_Line(int _x, int _y, int _lSemiMajorAxis, int _lSemiMinorAxis, float2 _direction, float _d) :
		x(_x), y(_y), lSemiMajorAxis(_lSemiMajorAxis), lSemiMinorAxis(_lSemiMinorAxis), direction(_direction), d(_d){}
};





struct functor_Displace_PolyLine
{
	int x, y;
	float d;
	int numCtrlPoints;
	//vector<float2> ctrlPoints;
	float2 *ctrlPoints;
	float lSemiMajor, lSemiMinor;
	int width;
	float2 direction;

	//precompute can increase speed?
	float2 *angleBisectDirs;
	float2 *dirs;

	__device__ float2 operator() (float2 screenPos, float4 clipPos) {
		float2 ret = screenPos;
		////old
		//if (clipPos.z < d) {
		//	float2 toPoint = screenPos - make_float2(x, y);
		//	float disMajor = toPoint.x*direction.x + toPoint.y*direction.y;
		//	if (abs(disMajor) < lSemiMajor/1.1) { //!!!may need to modify about 1.1
		//		float ratio = 0.5;
		//		float2 minorDirection = make_float2(-direction.y, direction.x);
		//		//dot product of (_x-x, _y-y) and minorDirection
		//		float disMinor = toPoint.x*minorDirection.x + toPoint.y*minorDirection.y;
		//		if (abs(disMinor) < lSemiMinor / ratio)	{
		//			int segmentID = -1;
		//			for (int ii = 0; ii < numCtrlPoints; ii++) {
		//				float2 kk = ctrlPoints[numCtrlPoints - 1];
		//			}
		//		}
		//	}
		//}
		
		if (clipPos.z < d) {
			int segmentId = -1;
			bool segmentIdNotFound = true;
			bool locatedAtSegmentPos;
			float2 center = make_float2(x, y);
			 
			for (int ii = 0; ii < numCtrlPoints - 1 && segmentIdNotFound; ii++) {
				float2 toPoint = screenPos - (center+ctrlPoints[ii]);
				
				//float2 dir = dirs[ii];
				float2 dir = normalize(ctrlPoints[ii + 1] - ctrlPoints[ii]);

				float disMajor = toPoint.x*dir.x + toPoint.y*dir.y;
				if (disMajor >= 0 && disMajor <= length(ctrlPoints[ii + 1] - ctrlPoints[ii])) {
					float2 minorDir = make_float2(-dir.y, dir.x);
					float disMinor = toPoint.x*minorDir.x + toPoint.y*minorDir.y;
					if (abs(disMinor) <= width) {
						segmentId = ii;
						segmentIdNotFound = false;
					}
				}
			}
			
			if (segmentId >= 0) {

				//float2 dir = dirs[segmentId];
				float2 dir = normalize(ctrlPoints[segmentId + 1] - ctrlPoints[segmentId]);

				float2 minorDir = make_float2(-dir.y, dir.x);
				float2 toPoint = screenPos - (center + ctrlPoints[segmentId]);
				float disMinor = toPoint.x*minorDir.x + toPoint.y*minorDir.y;

				//float2 dd = normalize(screenPos - center);
				//ret = screenPos + dd * 5;;


				/*if (disMinor>0)
					ret = make_float2(screenPos.x, screenPos.y) + (width - disMinor) * minorDir;
				else
					ret = make_float2(screenPos.x, screenPos.y) - (width + disMinor) * minorDir;
					*/
			}
			
		}
		
		return ret;
	}

//	functor_Displace_PolyLine(int _x, int _y, int _numCtrlPoints, vector<float2> _ctrlPoints, float2 _direction, float _lSemiMajor, float _lSemiMinor, float _d) :
	//	x(_x), y(_y), numCtrlPoints(_numCtrlPoints), ctrlPoints(_ctrlPoints), direction(_direction), lSemiMajor(_lSemiMajor), lSemiMinor(_lSemiMinor), d(_d){}

	functor_Displace_PolyLine(int _x, int _y, int _width, int _numCtrlPoints, float2* _ctrlPoints, float2 _direction, float _lSemiMajor, float _lSemiMinor, float _d) :
		x(_x), y(_y), width(_width), numCtrlPoints(_numCtrlPoints), ctrlPoints(_ctrlPoints), direction(_direction), lSemiMajor(_lSemiMajor), lSemiMinor(_lSemiMinor), d(_d)
	{/*
		//will the new operation cause memory problem???
		angleBisectDirs = new float2[numCtrlPoints];
		float2 dirFirst = normalize(ctrlPoints[1] - ctrlPoints[0]);
		angleBisectDirs[0] = make_float2(-dirFirst.y, dirFirst.x);
		for (int ii = 1; ii < numCtrlPoints - 1; ii++) {
			float2 dir1 = normalize(ctrlPoints[ii] - ctrlPoints[ii - 1]);
			float2 dir2 = normalize(ctrlPoints[ii + 1] - ctrlPoints[ii]);
			float2 perpenAngleBisectDir = normalize(dir2 + dir1);
			angleBisectDirs[ii] = make_float2(-perpenAngleBisectDir.y, perpenAngleBisectDir.x);
		}
		float2 dirLast = normalize(ctrlPoints[numCtrlPoints - 1] - ctrlPoints[numCtrlPoints - 2]);
		angleBisectDirs[numCtrlPoints - 1] = make_float2(-dirLast.y, dirLast.x);
		
		dirs = new float2[numCtrlPoints-1];
		for (int ii = 0; ii < numCtrlPoints - 1; ii++) {
			dirs[ii] = normalize(ctrlPoints[ii+1] - ctrlPoints[ii]);
		}
		*/
	}

};

struct functor_Displace_PolyLine_NotFinished
{
	__device__ float2 operator() (float2 screenPos, float4 clipPos) {
		float2 ret = screenPos;
		return ret;
	}

	functor_Displace_PolyLine_NotFinished(){}
};

//thrust::transform(d_vec_posScreen.begin(), d_vec_posScreen.end(),
//	d_vec_posScreenTarget.begin(), d_vec_posScreen.begin(),
//	functor_ApproachTarget());

struct functor_ApproachTarget
{
	template<typename Tuple>
	__device__ float2 operator() (Tuple t) {
		float2 screenPos = thrust::get<0>(t); 
		float2 screenTarget = thrust::get<1>(t);
		float2 dir = screenTarget - screenPos;
		float sizeDiff = thrust::get<3>(t) - thrust::get<2>(t);
		if (length(dir) < 0.5) {
			thrust::get<0>(t) = screenTarget;
			thrust::get<2>(t) = thrust::get<3>(t);
		}
		else{
			thrust::get<0>(t) = screenPos + dir * 0.1;
			thrust::get<2>(t) = thrust::get<2>(t) + sizeDiff * 0.1;
		}

	}
};

struct functor_Unproject
{
	matrix4x4 inv_mv, inv_pj;
	int w, h;
	__device__ float4 operator() (float4 pClip, float2 pScreen)
	{
		float2 clip = Screen2Clip(pScreen, w, h);
		float4 clip2 = make_float4(clip.x, clip.y, pClip.z, pClip.w);
		return Clip2ObjectGlobal(clip2, &inv_mv.v[0].x, &inv_pj.v[0].x);
	}
	functor_Unproject(matrix4x4 _inv_mv, matrix4x4 _inv_pj, int _w, int _h) :
		inv_mv(_inv_mv), inv_pj(_inv_pj), w(_w), h(_h){}
};

//struct func_CompDist2LensBtm{
//	float3 c;
//	matrix4x4 mv;
//	__device__ float operator() (float4 pObj){
//		float4 a = mat4mulvec4(&(mv.v[0].x), make_float4(c.x, c.y, c.z, 1.0f));
//		float4 b = mat4mulvec4(&(mv.v[0].x), pObj);
//		return abs(a.z / a.w - b.z / b.w);//projection * modelview * v;
//	}
//	func_CompDist2LensBtm(float3 _c, matrix4x4 _mv) : c(_c), mv(_mv){}
//};
//thrust::transform(posOrig.begin(), posOrig.end(), d_vec_Dist2LensBtm.begin(), (l->c, modelview));

void Displace::DisplacePoints(std::vector<float2>& pts, std::vector<Lens*> lenses)
{
	for (int i = 0; i < lenses.size(); i++) {
		CircleLens* l = (CircleLens*)lenses[i];
		for (auto& p : pts) {
			float tmp = 1;
			p = DisplaceCircleLens(l->x, l->y, l->radius, p, tmp, focusRatio);
		}
	}
}

void Displace::Compute(float* modelview, float* projection, int winW, int winH,
	std::vector<Lens*> lenses, float4* ret, float* glyphSizeScale)
{
	if (lenses.size() <= 0)
		return;
	int size = posOrig.size();

	//clip coordiates of streamlines
	matrix4x4 mv(modelview);
	matrix4x4 pj(projection);

	thrust::device_vector<float4> d_vec_posClip(size);
	thrust::device_vector<float2> d_vec_posScreen(size);
	//thrust::counting_iterator < int > first(0);

	if (recomputeTarget) {
		thrust::transform(posOrig.begin(), posOrig.end(), d_vec_posClip.begin(), functor_Object2Clip(mv, pj));

		thrust::transform(d_vec_posClip.begin(), d_vec_posClip.end(),
			d_vec_posScreen.begin(), functor_Clip2Screen(winW, winH));

		//reset to 1
		d_vec_glyphSizeTarget.assign(size, 1);


		for (int i = 0; i < lenses.size(); i++) {
			switch (lenses[i]->GetType()) {
				case LENS_TYPE::TYPE_CIRCLE:
				{
					CircleLens* l = (CircleLens*)lenses[i];
					thrust::for_each(
						thrust::make_zip_iterator(
						thrust::make_tuple(
						d_vec_posScreen.begin(),
						d_vec_posClip.begin(),
						d_vec_posScreenTarget.begin(),
						d_vec_glyphSizeTarget.begin()
						)),
						thrust::make_zip_iterator(
						thrust::make_tuple(
						d_vec_posScreen.end(),
						d_vec_posClip.end(),
						d_vec_posScreenTarget.end(),
						d_vec_glyphSizeTarget.end()
						)),
						functor_Displace(l->x, l->y, l->radius, l->GetClipDepth(modelview, projection), focusRatio, sideSize));
					break;

				}
				case LENS_TYPE::TYPE_LINE:
				{
					LineLens* l = (LineLens*)lenses[i];
					thrust::transform(d_vec_posScreen.begin(), d_vec_posScreen.end(),
						d_vec_posClip.begin(), d_vec_posScreenTarget.begin(),
						functor_Displace_Line(l->x, l->y, l->lSemiMajorAxis, l->lSemiMinorAxis, l->direction, l->GetClipDepth(modelview, projection)));
					break;
				}
				case LENS_TYPE::TYPE_POLYLINE:
				{
					PolyLineLens* l = (PolyLineLens*)lenses[i];
					
					if (!l->isConstructing) {
						thrust::transform(d_vec_posScreen.begin(), d_vec_posScreen.end(),
							d_vec_posClip.begin(), d_vec_posScreenTarget.begin(),
							functor_Displace_PolyLine(l->x, l->y, l->width, l->numCtrlPoints, l->ctrlPoints, l->direction, l->lSemiMajor, l->lSemiMinor, l->GetClipDepth(modelview, projection)));
					}
					else
					{
						thrust::transform(d_vec_posScreen.begin(), d_vec_posScreen.end(),
							d_vec_posClip.begin(), d_vec_posScreenTarget.begin(),
							functor_Displace_PolyLine_NotFinished());

					}
					break;
				}
			}
			//thrust::transform(posOrig.begin(), posOrig.end(), 
			//	d_vec_Dist2LensBtm.begin(), func_CompDist2LensBtm(l->c, mv));
		}
		recomputeTarget = false;
	}


	 

	thrust::device_vector<float4> d_vec_posCur(size);
	thrust::copy(ret, ret + size, d_vec_posCur.begin());
	thrust::device_vector<float> d_vec_glyphSizeScale(size);
	thrust::copy(glyphSizeScale, glyphSizeScale + size, d_vec_glyphSizeScale.begin());

	thrust::transform(d_vec_posCur.begin(), d_vec_posCur.end(), d_vec_posClip.begin(), functor_Object2Clip(mv, pj));
	thrust::transform(d_vec_posClip.begin(), d_vec_posClip.end(),
		d_vec_posScreen.begin(), functor_Clip2Screen(winW, winH));

	thrust::for_each(
		thrust::make_zip_iterator(
		thrust::make_tuple(
		d_vec_posScreen.begin(), 
		d_vec_posScreenTarget.begin(), 
		d_vec_glyphSizeScale.begin(),
		d_vec_glyphSizeTarget.begin()
		)),
		thrust::make_zip_iterator(
		thrust::make_tuple(
		d_vec_posScreen.end(),
		d_vec_posScreenTarget.end(),
		d_vec_glyphSizeScale.end(),
		d_vec_glyphSizeTarget.end()
		)),
		functor_ApproachTarget());

	//posScreenTarget = d_vec_posScreen;
	//}

	matrix4x4 invMV;
	matrix4x4 invPJ;
	invertMatrix(&mv.v[0].x, &invMV.v[0].x);
	invertMatrix(&pj.v[0].x, &invPJ.v[0].x);

	thrust::device_vector<float4> d_vec_ret(size);
	thrust::transform(d_vec_posClip.begin(), d_vec_posClip.end(), d_vec_posScreen.begin(), d_vec_ret.begin(),
		functor_Unproject(invMV, invPJ, winW, winH));
	thrust::copy(d_vec_ret.begin(), d_vec_ret.end(), ret);
	thrust::copy(d_vec_glyphSizeScale.begin(), d_vec_glyphSizeScale.end(), glyphSizeScale);
}
