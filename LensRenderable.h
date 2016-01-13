#ifndef LENS_RENDERABLE_H
#define LENS_RENDERABLE_H
#include "Renderable.h"


class Lens;
class LensRenderable :public Renderable
{
	std::vector<Lens*> lenses;
	int pickedLens = -1;
	//bool workingOnLens = false;
	int2 lastPt = make_int2(0, 0);
public:
	void init() override;
	void draw(float modelview[16], float projection[16]) override;
	void UpdateData() override;
	LensRenderable(){}
	std::vector<Lens*> GetLenses() { return lenses; }
	//void AddSphereLens(int x, int y, int radius, float3 center);
	void AddCircleLens();
	//bool IsWorkingOnLens(){ return workingOnLens; }

	void mousePress(int x, int y, int modifier) override;
	void mouseRelease(int x, int y, int modifier) override;
	void mouseMove(int x, int y, int modifier) override;
	bool MouseWheel(int x, int y, int delta)  override;

};
#endif