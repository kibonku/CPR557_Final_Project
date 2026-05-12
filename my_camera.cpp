#include "my_camera.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <limits>
#include <cmath>
#include <iostream>

// --------------------------------------------------------------------------
// Constructor
// --------------------------------------------------------------------------

MyCamera::MyCamera()
{
    setViewTarget(glm::vec3(0.f, 0.f, -2.5f), glm::vec3(0.f));
}

// --------------------------------------------------------------------------
// Projection matrices (identical to A4)
// --------------------------------------------------------------------------

void MyCamera::setOrthographicProjection(
    float left, float right, float top, float bottom, float near, float far)
{
    m_m4ProjectionMatrix = glm::mat4{1.0f};
    m_m4ProjectionMatrix[0][0] =  2.f / (right - left);
    m_m4ProjectionMatrix[1][1] =  2.f / (top - bottom);
    m_m4ProjectionMatrix[2][2] = -2.0f / (far - near);
    m_m4ProjectionMatrix[3][0] = -(right + left) / (right - left);
    m_m4ProjectionMatrix[3][1] = -(top + bottom) / (top - bottom);
    m_m4ProjectionMatrix[3][2] = -(far + near)   / (far - near);
}

void MyCamera::setPerspectiveProjection(float fovy, float aspect, float near, float far)
{
    assert(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);
    const float tanHalfFovy = std::tan(fovy / 2.f);
    const float right  =  near * tanHalfFovy * aspect;
    const float left   = -right;
    const float bottom =  near * tanHalfFovy;
    const float top    = -bottom;

    m_m4ProjectionMatrix = glm::mat4{0.0f};
    m_m4ProjectionMatrix[0][0] =  2.0f * near / (right - left);
    m_m4ProjectionMatrix[2][0] =  (right + left) / (right - left);
    m_m4ProjectionMatrix[1][1] =  2.0f * near / (top - bottom);
    m_m4ProjectionMatrix[2][1] =  (top + bottom) / (top - bottom);
    m_m4ProjectionMatrix[2][2] = -1.0f * (far + near) / (far - near);
    m_m4ProjectionMatrix[3][2] = -2.0f * far * near / (far - near);
    m_m4ProjectionMatrix[2][3] = -1.0f;
}

// --------------------------------------------------------------------------
// View matrix setters (identical to A4)
// --------------------------------------------------------------------------

void MyCamera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up)
{
    const glm::vec3 w{ glm::normalize(direction) };
    const glm::vec3 u{ glm::normalize(glm::cross(up, w)) };
    const glm::vec3 v{ glm::cross(w, u) };

    // Build inverse view (camera-to-world) first
    m_m4InverseViewMatrix = glm::mat4{1.f};
    m_m4InverseViewMatrix[0][0] = u.x; m_m4InverseViewMatrix[0][1] = u.y; m_m4InverseViewMatrix[0][2] = u.z;
    m_m4InverseViewMatrix[1][0] = v.x; m_m4InverseViewMatrix[1][1] = v.y; m_m4InverseViewMatrix[1][2] = v.z;
    m_m4InverseViewMatrix[2][0] = w.x; m_m4InverseViewMatrix[2][1] = w.y; m_m4InverseViewMatrix[2][2] = w.z;
    m_m4InverseViewMatrix[3][0] = position.x;
    m_m4InverseViewMatrix[3][1] = position.y;
    m_m4InverseViewMatrix[3][2] = position.z;

    m_m4ViewMatrix = glm::inverse(m_m4InverseViewMatrix);
}

void MyCamera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up)
{
    setViewDirection(position, position - target, up);
}

void MyCamera::setViewYXZ(glm::vec3 position, glm::vec3 rotation)
{
    const float c3 = glm::cos(rotation.z), s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x), s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y), s1 = glm::sin(rotation.y);

    const glm::vec3 u{(c1*c3+s1*s2*s3),(c2*s3),(c1*s2*s3-c3*s1)};
    const glm::vec3 v{(c3*s1*s2-c1*s3),(c2*c3),(c1*c3*s2+s1*s3)};
    const glm::vec3 w{(c2*s1),(-s2),(c1*c2)};

    m_m4ViewMatrix = glm::mat4{1.f};
    m_m4ViewMatrix[0][0]=u.x; m_m4ViewMatrix[1][0]=u.y; m_m4ViewMatrix[2][0]=u.z;
    m_m4ViewMatrix[0][1]=v.x; m_m4ViewMatrix[1][1]=v.y; m_m4ViewMatrix[2][1]=v.z;
    m_m4ViewMatrix[0][2]=w.x; m_m4ViewMatrix[1][2]=w.y; m_m4ViewMatrix[2][2]=w.z;
    m_m4ViewMatrix[3][0]=-glm::dot(u,position);
    m_m4ViewMatrix[3][1]=-glm::dot(v,position);
    m_m4ViewMatrix[3][2]=-glm::dot(w,position);

    m_m4InverseViewMatrix = glm::inverse(m_m4ViewMatrix);
}

// --------------------------------------------------------------------------
// Mode & scene bounds
// --------------------------------------------------------------------------

void MyCamera::setMode(MyCameraMode mode)
{
    m_eMode = mode;
    std::cout << "MyCamera::setMode = " << (int)mode << std::endl;
    if (m_eMode == MYCAMERA_FITALL)
    {
        std::cout << "Fit All" << std::endl;
        _fitAll();
    }
}

void MyCamera::setSceneMinMax(glm::vec3 min, glm::vec3 max)
{
    m_vSceneMin = min;
    m_vSceneMax = max;
}

// --------------------------------------------------------------------------
// Mouse button / motion  (A4 style, bug #1 fixed here)
// --------------------------------------------------------------------------

void MyCamera::setButton(bool buttonPress, float x, float y)
{
    m_m4TempTransform = glm::mat4{1.0f};

    if (buttonPress)
    {
        std::cout << "Mouse button pressed" << std::endl;
        m_vCurrPos = glm::vec2(x, y);
        m_vPrevPos = m_vCurrPos;
        m_bMoving  = true;
    }
    else
    {
        std::cout << "Mouse button released" << std::endl;
        m_bMoving = false;
    }
}

void MyCamera::setMotion(bool /*buttonPress*/, float x, float y)
{
    if (!m_bMoving) return;

    m_vCurrPos = glm::vec2(x, y);
    glm::vec2 delta = m_vCurrPos - m_vPrevPos;
    if (glm::length(delta) < 1.0e-6f) return;

    if      (m_eMode == MYCAMERA_ROTATE) { std::cout << "  Rotating..." << std::endl; _rotate(delta.x, delta.y); }
    else if (m_eMode == MYCAMERA_PAN)    { std::cout << "  Panning..."   << std::endl; _pan   (delta.x, delta.y); }
    else if (m_eMode == MYCAMERA_ZOOM)   { std::cout << "  Zooming..."   << std::endl; _zoom  (delta.x, delta.y); }
    else if (m_eMode == MYCAMERA_TWIST)  { std::cout << "  Twisting..."  << std::endl; _twist (delta.x, delta.y); }

    _applyTempTransform();

    m_vPrevPos = m_vCurrPos;
}

// --------------------------------------------------------------------------
// _applyTempTransform  — BUG #1 FIX
//
// The A4 submission used world-space scene center in centerMat but then
// left-multiplied onto the view matrix.  Left-multiplication operates in
// camera space, so the center must first be transformed into camera space.
//
// Correct formula (camera-space center c_cam):
//   new_V = T_cam(c_cam) * R * T_cam(-c_cam) * old_V
// which is identical to the A4 structure except c_cam replaces world center.
// --------------------------------------------------------------------------

void MyCamera::_applyTempTransform()
{
    // Decompose TempTransform into rotation-only and translation-only parts
    glm::mat4 rotateOnly    = m_m4TempTransform;
    rotateOnly[3][0] = rotateOnly[3][1] = rotateOnly[3][2] = 0.0f;

    glm::mat4 translateOnly = glm::mat4{1.0f};
    translateOnly[3][0] = m_m4TempTransform[3][0];
    translateOnly[3][1] = m_m4TempTransform[3][1];
    translateOnly[3][2] = m_m4TempTransform[3][2];

    // *** FIX: transform scene center to CAMERA space ***
    glm::vec3 worldCenter = (m_vSceneMin + m_vSceneMax) * 0.5f;
    glm::vec4 camCenter4  = m_m4ViewMatrix * glm::vec4(worldCenter, 1.0f);
    glm::vec3 c(camCenter4.x, camCenter4.y, camCenter4.z); // camera-space center

    glm::mat4 centerMat        = glm::mat4{1.0f};
    centerMat[3][0] =  c.x;
    centerMat[3][1] =  c.y;
    centerMat[3][2] =  c.z;

    glm::mat4 centerInverseMat = glm::mat4{1.0f};
    centerInverseMat[3][0] = -c.x;
    centerInverseMat[3][1] = -c.y;
    centerInverseMat[3][2] = -c.z;

    // Combine: translateOnly * centerMat * rotateOnly * centerInverseMat * oldView
    m_m4ViewMatrix = translateOnly * centerMat * rotateOnly * centerInverseMat * m_m4ViewMatrix;

    // Keep inverse in sync
    m_m4InverseViewMatrix = glm::inverse(m_m4ViewMatrix);
}

// --------------------------------------------------------------------------
// Keyboard step — directly synthesises dx/dy and calls the same helpers
// --------------------------------------------------------------------------

void MyCamera::keyStep(float rotH, float rotV, float zoomDelta, float panH, float panV)
{
    m_m4TempTransform = glm::mat4{1.0f};

    if (rotH != 0.f || rotV != 0.f)
    {
        _rotate(rotH, rotV);
        _applyTempTransform();
        m_m4TempTransform = glm::mat4{1.0f};
    }
    if (zoomDelta != 0.f)
    {
        _zoom(0.f, zoomDelta);
        _applyTempTransform();
        m_m4TempTransform = glm::mat4{1.0f};
    }
    if (panH != 0.f || panV != 0.f)
    {
        _pan(panH, panV);
        _applyTempTransform();
    }
}

// --------------------------------------------------------------------------
// _pan — translate camera in screen X / Y  (unchanged from A4)
// --------------------------------------------------------------------------

void MyCamera::_pan(float dx, float dy)
{
    m_m4TempTransform = glm::mat4{1.0f};

    float d = std::fabs(m_m4ViewMatrix[3][2]);
    if (d < 0.1f) d = 0.1f;
    float scale = d * 0.7f;

    m_m4TempTransform[3][0] = -dx * scale;
    m_m4TempTransform[3][1] = -dy * scale;
}

// --------------------------------------------------------------------------
// _zoom — dolly along view Z  (unchanged from A4)
// --------------------------------------------------------------------------

void MyCamera::_zoom(float /*dx*/, float dy)
{
    m_m4TempTransform = glm::mat4{1.0f};

    float d      = m_m4ViewMatrix[3][2];
    float m      = 1.0f + dy * 0.7f;
    float delta_d = d * m - d;

    m_m4TempTransform[3][2] = delta_d;
}

// --------------------------------------------------------------------------
// _rotate — trackball around scene center  (unchanged from A4)
// --------------------------------------------------------------------------

void MyCamera::_rotate(float dx, float dy)
{
    float mag      = sqrtf(dx*dx + dy*dy);
    float tb_angle = mag * 200.0f;

    glm::vec3 axis(dy, dx, 0.f); // vertical drag → X axis, horizontal → Y axis

    glm::vec3 sx, sy, sz;
    _getScreenXYZ(sx, sy, sz);
    glm::vec3 tb_axis = sx * axis.x + sy * axis.y + sz * axis.z;

    _atRotate(tb_axis.x, tb_axis.y, tb_axis.z, tb_angle);
}

// --------------------------------------------------------------------------
// _twist — rotate around view Z axis   BUG #2 FIX
//
// A4 used only dx: delta_theta = dx * 7.0f.
// Correct twist uses BOTH dx and dy by projecting the mouse delta onto the
// tangent of the circle at the current cursor position.
//
// If the cursor is at screen coords (cx, cy) relative to screen centre,
// the tangent direction (CCW) is (-cy, cx) / |(cx,cy)|.
// The signed twist angle is: dot((dx,dy), tangent) * sensitivity.
//
// This degenerates gracefully to pure-dx when the cursor is directly above
// (cx≈0, cy<0) and pure-dy when the cursor is directly to the right
// (cx>0, cy≈0), matching intuition for a screen-space roll tool.
// --------------------------------------------------------------------------

void MyCamera::_twist(float dx, float dy)
{
    // Cursor position is in [0,1]; screen centre is at (0.5, 0.5)
    float cx = m_vCurrPos.x - 0.5f;
    float cy = m_vCurrPos.y - 0.5f;
    float dist = sqrtf(cx*cx + cy*cy);

    float delta_theta;
    if (dist < 1e-4f)
    {
        // Cursor is very close to centre: fall back to horizontal drag only
        delta_theta = dx * 7.0f;
    }
    else
    {
        // Tangent direction at (cx, cy): perpendicular to radius (CCW)
        float tx = -cy / dist;
        float ty =  cx / dist;
        delta_theta = (dx * tx + dy * ty) * 7.0f;
    }

    glm::vec3 sx, sy, sz;
    _getScreenXYZ(sx, sy, sz);
    _atRotate(sz.x, sz.y, sz.z, delta_theta);
}

// --------------------------------------------------------------------------
// _fitAll — frame entire scene  (unchanged from A4)
// --------------------------------------------------------------------------

void MyCamera::_fitAll()
{
    m_m4TempTransform = glm::mat4{1.0f};

    glm::vec3 center = (m_vSceneMin + m_vSceneMax) * 0.5f;
    glm::vec3 size   = m_vSceneMax - m_vSceneMin;
    float radius     = glm::length(size) * 0.5f;

    float dist = (radius / std::tan(glm::radians(25.0f))) + radius * 0.1f;
    if (dist < 0.1f) dist = 3.0f;

    m_m4TempTransform[3][2] = -dist;

    glm::mat4 rotateOnlyMatrix    = m_m4ViewMatrix;
    rotateOnlyMatrix[3][0] = rotateOnlyMatrix[3][1] = rotateOnlyMatrix[3][2] = 0.0f;

    glm::mat4 centerInverseMat    = glm::mat4{1.0f};
    centerInverseMat[3][0] = -center.x;
    centerInverseMat[3][1] = -center.y;
    centerInverseMat[3][2] = -center.z;

    m_m4ViewMatrix        = m_m4TempTransform * rotateOnlyMatrix * centerInverseMat;
    m_m4InverseViewMatrix = glm::inverse(m_m4ViewMatrix);
}

// --------------------------------------------------------------------------
// _atRotate — Rodrigues rotation matrix around arbitrary axis  (A4, unchanged)
// --------------------------------------------------------------------------

void MyCamera::_atRotate(float x, float y, float z, float angle)
{
    float rad    = angle * glm::pi<float>() / 180.0f;
    float cosAng = std::cos(rad);
    float sinAng = std::sin(rad);

    glm::vec3 axis = glm::normalize(glm::vec3(x, y, z));
    float ax = axis.x, ay = axis.y, az = axis.z;

    m_m4TempTransform = glm::mat4{1.0f};

    m_m4TempTransform[0][0] = ax*ax + cosAng*(1 - ax*ax);
    m_m4TempTransform[0][1] = ax*ay - cosAng*ax*ay + sinAng*az;
    m_m4TempTransform[0][2] = ax*az - cosAng*ax*az - sinAng*ay;

    m_m4TempTransform[1][0] = ax*ay - cosAng*ax*ay - sinAng*az;
    m_m4TempTransform[1][1] = ay*ay + cosAng*(1 - ay*ay);
    m_m4TempTransform[1][2] = ay*az - cosAng*ay*az + sinAng*ax;

    m_m4TempTransform[2][0] = ax*az - cosAng*ax*az + sinAng*ay;
    m_m4TempTransform[2][1] = ay*az - cosAng*ay*az - sinAng*ax;
    m_m4TempTransform[2][2] = az*az + cosAng*(1 - az*az);

    m_m4TempTransform[3][3] = 1.0f;
}

// --------------------------------------------------------------------------
// _getScreenXYZ — extract world-space screen axes from view matrix  (A4, unchanged)
// --------------------------------------------------------------------------

void MyCamera::_getScreenXYZ(glm::vec3& sx, glm::vec3& sy, glm::vec3& sz)
{
    sx = glm::vec3(m_m4ViewMatrix[0][0], m_m4ViewMatrix[1][0], m_m4ViewMatrix[2][0]);
    sy = glm::vec3(m_m4ViewMatrix[0][1], m_m4ViewMatrix[1][1], m_m4ViewMatrix[2][1]);
    sz = glm::vec3(m_m4ViewMatrix[0][2], m_m4ViewMatrix[1][2], m_m4ViewMatrix[2][2]);
}
