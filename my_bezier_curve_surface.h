#ifndef __MY_BEZIER_CURVE_SURFACE_H__
#define __MY_BEZIER_CURVE_SURFACE_H__

#include <vector>
#include <glm/glm.hpp>
#include "my_model.h"

class MyBezier
{
public:
	void addControlPoint(float x, float y);
	int  numberOfControlPoints();
	void controlPoint(int index, glm::vec2& point);
	void createBezierCurve(int resolution);
	void createRevolutionSurface(int xResolution, int rResolution);
	
	// Phase 7: Coordinate conversion and sorting helpers
	static glm::vec3 convertScreenToWorld(float screenX, float screenY, int windowWidth, int windowHeight);
	void sortControlPointsByHeight();

	// Phase 5: Twist and color gradient controls
	void  setTwistAngle(float radians) { m_fTwistAngle = radians; }
	float getTwistAngle() const        { return m_fTwistAngle; }
	void  setColorGradient(bool on)    { m_bColorGradient = on; }
	bool  getColorGradient() const     { return m_bColorGradient; }

	std::vector<MyModel::PointCurve> m_vCurve;
	std::vector<MyModel::Vertex>     m_vSurface;
	std::vector<uint32_t>            m_vIndices;
	std::vector<glm::vec2>           m_vControlPoints;

protected:
	void      _allBernstein(int n, float u, float* B);
	void      _pointOnBezierCurve(int n, float u, glm::vec2 &point);
	void      _derivative(int degree, float u, glm::vec2& der);
	glm::vec3 _hsvToRgb(float h, float s, float v);

	float m_fTwistAngle    = 0.0f;  // Phase 5: twist in radians (2*PI = one full helical turn)
	bool  m_bColorGradient = true;  // Phase 5: rainbow gradient along the axial direction
};

#endif
