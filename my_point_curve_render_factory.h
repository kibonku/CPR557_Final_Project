#ifndef __MY_POINT_CURVE_RENDER_FACTORY_H__
#define __MY_POINT_CURVE_RENDER_FACTORY_H__

#include "my_device.h"
#include "my_game_object.h"
#include "my_pipeline.h"
#include "my_camera.h"
#include "my_frame_info.h"

// std
#include <memory>
#include <vector>


class MyPointCurveRenderFactory
{
public:
	MyPointCurveRenderFactory(MyDevice& device, VkRenderPass renderPass, VkPrimitiveTopology topology);
	~MyPointCurveRenderFactory();

	MyPointCurveRenderFactory(const MyPointCurveRenderFactory&) = delete;
	MyPointCurveRenderFactory& operator=(const MyPointCurveRenderFactory&) = delete;

	void renderControlPoints(MyFrameInfo& frameInfo, std::vector<MyGameObject>& gameObjects);
	void renderCenterLine(MyFrameInfo& frameInfo, std::vector<MyGameObject>& gameObjects);
	void renderBezierCurve(MyFrameInfo& frameInfo, std::vector<MyGameObject>& gameObjects);
	void renderNormals(MyFrameInfo& frameInfo, std::vector<MyGameObject>& gameObjects);

private:
	void _createPipelineLayout();
	void _createPipeline(VkRenderPass renderPass, VkPrimitiveTopology topology);
	void _renderPointsLines(std::string name, MyFrameInfo& frameInfo, std::vector<MyGameObject>& gameObjects);

	MyDevice&                   m_myDevice;

	std::unique_ptr<MyPipeline> m_pMyPipeline;
	VkPipelineLayout            m_vkPipelineLayout;
};

#endif

