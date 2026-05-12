#include "my_bezier_curve_surface.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
// Control point helpers
// --------------------------------------------------------------------------

void MyBezier::addControlPoint(float x, float y)
{
	m_vControlPoints.push_back(glm::vec2(x, y));
}

int MyBezier::numberOfControlPoints()
{
	return (int)m_vControlPoints.size();
}

void MyBezier::controlPoint(int index, glm::vec2& point)
{
	if (index >= 0 && index < (int)m_vControlPoints.size())
	{
		point = m_vControlPoints[index];
	}
}

// --------------------------------------------------------------------------
// Phase 1: Bezier Curve
// --------------------------------------------------------------------------

void MyBezier::createBezierCurve(int resolution)
{
	m_vCurve.clear();

	if (m_vControlPoints.size() < 2)
		return;

	// Step 1: Degree = number of control points - 1
	int degree = (int)m_vControlPoints.size() - 1;

	// Step 2: Loop through u in [0, 1] with the given resolution
	for (int i = 0; i <= resolution; i++)
	{
		float u = static_cast<float>(i) / static_cast<float>(resolution);

		// Step 3: Evaluate the point on the Bezier curve at u
		glm::vec2 point;
		_pointOnBezierCurve(degree, u, point);

		// Step 4: Pack into PointCurve and append
		MyModel::PointCurve cp;
		cp.position = glm::vec3(point.x, point.y, 0.0f);
		cp.color    = glm::vec3(1.0f, 1.0f, 1.0f);
		m_vCurve.push_back(cp);
	}
}

// --------------------------------------------------------------------------
// Phase 5 helper: HSV -> RGB (h in [0,360], s,v in [0,1])
// --------------------------------------------------------------------------

glm::vec3 MyBezier::_hsvToRgb(float h, float s, float v)
{
	h = std::fmod(h, 360.0f);
	if (h < 0.0f) h += 360.0f;

	float c  = v * s;
	float x  = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
	float m  = v - c;

	glm::vec3 rgb{};
	if      (h < 60.0f)  rgb = {c, x, 0};
	else if (h < 120.0f) rgb = {x, c, 0};
	else if (h < 180.0f) rgb = {0, c, x};
	else if (h < 240.0f) rgb = {0, x, c};
	else if (h < 300.0f) rgb = {x, 0, c};
	else                 rgb = {c, 0, x};

	return rgb + glm::vec3(m);
}

// --------------------------------------------------------------------------
// Phase 2 & 3: Revolution Surface with normals
// Phase 5:     Twist + color gradient
// --------------------------------------------------------------------------

void MyBezier::createRevolutionSurface(int xResolution, int rResolution)
{
	m_vSurface.clear();
	m_vIndices.clear();

	if (m_vControlPoints.size() < 2)
		return;

	int   degree     = (int)m_vControlPoints.size() - 1;
	float deltaTheta = glm::two_pi<float>() / static_cast<float>(rResolution);

	// Build vertex grid: (xResolution+1) rows x (rResolution+1) columns
	// Revolve the 2D profile curve around the X-axis.
	//
	// 2D profile:  point = (px, pr)  where px = axial, pr = radius
	// 3D vertex:   (px, pr*cos(theta), pr*sin(theta))
	//
	// Tangent in 2D: (tx, tr),  perp (outward normal) = (-tr, tx)
	// 3D normal:    (-tr_n, tx_n*cos(theta), tx_n*sin(theta))  -- already unit length
	//
	// Phase 5 twist: theta_actual = theta + u * m_fTwistAngle
	// Phase 5 color: rainbow hue mapped from u in [0,1] -> hue in [0,300] degrees

	for (int i = 0; i <= xResolution; i++)
	{
		float u = static_cast<float>(i) / static_cast<float>(xResolution);

		// Evaluate profile position and tangent at u
		glm::vec2 point, tangent;
		_pointOnBezierCurve(degree, u, point);
		_derivative(degree, u, tangent);

		// Guard against degenerate tangent (e.g. all control points collinear)
		float tangentLen = glm::length(tangent);
		if (tangentLen < 1e-6f)
			tangent = glm::vec2(1.0f, 0.0f);
		else
			tangent = tangent / tangentLen;

		// 2D outward normal: perp of (tx, tr) is (-tr, tx)
		glm::vec2 normal2D(-tangent.y, tangent.x);

		// Phase 5: compute vertex color for this ring (rainbow gradient)
		glm::vec3 ringColor = m_bColorGradient
			? _hsvToRgb(u * 300.0f, 0.85f, 0.95f)   // 0 -> 300 deg avoids red wraparound
			: glm::vec3(0.8f, 0.8f, 0.8f);

		for (int j = 0; j <= rResolution; j++)
		{
			// Phase 5 twist: rotate each ring by an extra angle proportional to u
			float theta    = static_cast<float>(j) * deltaTheta + u * m_fTwistAngle;
			float cosTheta = std::cos(theta);
			float sinTheta = std::sin(theta);

			MyModel::Vertex vertex;

			// Phase 2: 3D position (revolve around X-axis)
			vertex.position = glm::vec3(
				point.x,
				point.y * cosTheta,
				point.y * sinTheta);

			// Phase 2 & 3: outward surface normal (already unit length)
			vertex.normal = glm::normalize(glm::vec3(
				normal2D.x,
				normal2D.y * cosTheta,
				normal2D.y * sinTheta));

			// Phase 5: apply rainbow color gradient
			vertex.color = ringColor;

			// UV coordinates (useful for texture mapping)
			vertex.uv = glm::vec2(u, static_cast<float>(j) / static_cast<float>(rResolution));

			m_vSurface.push_back(vertex);

			// Build two triangles per quad (not on the last row)
			if (i < xResolution && j < rResolution)
			{
				uint32_t idx00 = i       * (rResolution + 1) + j;
				uint32_t idx10 = (i + 1) * (rResolution + 1) + j;
				uint32_t idx01 = i       * (rResolution + 1) + (j + 1);
				uint32_t idx11 = (i + 1) * (rResolution + 1) + (j + 1);

				// Triangle 1
				m_vIndices.push_back(idx00);
				m_vIndices.push_back(idx10);
				m_vIndices.push_back(idx11);

				// Triangle 2
				m_vIndices.push_back(idx00);
				m_vIndices.push_back(idx11);
				m_vIndices.push_back(idx01);
			}
		}
	}
}

// --------------------------------------------------------------------------
// Bernstein basis & curve evaluation (provided by template)
// --------------------------------------------------------------------------

void MyBezier::_allBernstein(int n, float u, float* B)
{
	int   k     = 0;
	float u1    = 1.0f - u;
	float saved = 0.0f;
	float temp  = 0.0f;
	B[0] = 1.0f;

	for (int j = 1; j <= n; j++)
	{
		saved = 0.0f;
		for (k = 0; k < j; k++)
		{
			temp  = B[k];
			B[k]  = saved + u1 * temp;
			saved = u * temp;
		}
		B[j] = saved;
	}
}

void MyBezier::_pointOnBezierCurve(int degree, float u, glm::vec2& point)
{
	std::vector<float> vfB(degree + 1, 0.0f);
	_allBernstein(degree, u, vfB.data());
	point = glm::vec2(0.0f);
	for (int k = 0; k <= degree; k++)
	{
		point = point + m_vControlPoints[k] * vfB[k];
	}
}

void MyBezier::_derivative(int degree, float u, glm::vec2& der)
{
	std::vector<float> vfB(degree, 0.0f);
	_allBernstein(degree - 1, u, vfB.data());
	der = glm::vec2(0.0f);
	for (int k = 0; k <= degree - 1; k++)
	{
		der = der + (m_vControlPoints[k + 1] - m_vControlPoints[k]) * vfB[k];
	}
	der = der * static_cast<float>(degree);
}

// --------------------------------------------------------------------------
// Phase 7: Coordinate conversion and control point sorting
// --------------------------------------------------------------------------

glm::vec3 MyBezier::convertScreenToWorld(float screenX, float screenY, int windowWidth, int windowHeight)
{
	// Screen coordinates: already normalized to [0, 1]
	// screenX = 0 (left), screenX = 1 (right)
	// screenY = 0 (top), screenY = 1 (bottom)
	// 
	// World coordinates: centered at origin, +Y pointing up
	// worldX: [0, 1] → [-2, 2]
	// worldY: [0, 1] → [2, -2] (inverted, so top=2, bottom=-2)
	
	float worldX = 4.0f * screenX - 2.0f;
	float worldY = -4.0f * screenY + 2.0f;
	
	return glm::vec3(worldX, worldY, 0.0f);
}

void MyBezier::sortControlPointsByHeight()
{
	// Sort control points by Y coordinate (height) in ascending order
	// This ensures the Bezier curve is generated from bottom (low Y) to top (high Y)
	std::sort(m_vControlPoints.begin(), m_vControlPoints.end(),
		[](const glm::vec2& a, const glm::vec2& b) {
			return a.y < b.y;
		});
}

