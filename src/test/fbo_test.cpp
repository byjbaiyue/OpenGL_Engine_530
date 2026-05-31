#include "fbo_test.h"

#include "CameraController.h"
#include "chunkMash.h"
#include "ControllerManager.h"
#include "GLFWCallbackManager.h"
#include "imgui/imgui.h"
#include "InputManager.h"
#include "l_mesh.h"
#include "load_blockData.h"
#include "m_model.h"
#include "register_block.h"
#include "Renderer.h"
#include "texture_manager.h"
#include "block_best.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <iostream>


Test::FBOTest::FBOTest(GLFWwindow* window) : clearColor(0.2f, 0.4f, 0.4f), deltatime(-1.0f), window(window)

{

	Renderer& renderer = Renderer::Instance();

	renderer.SetCullFace(true);// 开启面剔除

	glDepthFunc(GL_GREATER);
	glClearDepth(0.0f);


	LoadBlockData::loadBlockDataFromFile("asset/block/block_1.json");

	{
		std::vector<LineVertex> Z_v = {
			{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)},
			{glm::vec3(1.0f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)},

			{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)},
			{glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)},

			{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)},
			{glm::vec3(0.0f, 0.0f, 1.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)},
		};

		std::vector<unsigned int> Z_i = {
			0, 1,
			2, 3,
			4, 5,
		};

		mesh_line = new L_Mesh(Z_v, Z_i);
	}



	camera = new Camera(glm::mat4(0.0f), 60.0f, (float)InputManager::GetInstance().GetWindowSize().x / InputManager::GetInstance().GetWindowSize().y, 10000.0f,0.02f );

	baseFBO = new FrameBuffer(InputManager::GetInstance().GetWindowSize().x, InputManager::GetInstance().GetWindowSize().y);
	opaqueColor = baseFBO->AddColorAttachment(GL_RGBA16F, GL_RGBA, GL_FLOAT);
	Texture* opaqueDepth = baseFBO->AddDepthTexture();

	baseFBO->SetDrawBuffer({ GL_COLOR_ATTACHMENT0 });



	transparentFBO = new FrameBuffer(InputManager::GetInstance().GetWindowSize().x, InputManager::GetInstance().GetWindowSize().y);
	accumTex = transparentFBO->AddColorAttachment(GL_RGBA16F, GL_RGBA, GL_FLOAT);
	revealTex = transparentFBO->AddColorAttachment(GL_RGBA16F, GL_RGBA, GL_FLOAT);

	// 共享深度纹理（让透明物体能被不透明物体正确遮挡）
	transparentFBO->AttachExternalDepthTexture(opaqueDepth);
	transparentFBO->SetDrawBuffer({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });


	setupQuad();


	lineShader = new Shader("src/shader/Line/V_lineShader.glsl", "src/shader/Line/F_lineShader.glsl", "");

	chunkShader = new Shader("src/shader/chunk/VertexShader.glsl", "src/shader/chunk/FragmentShader.glsl", "");

	transparentShader_a = new Shader("src/shader/wboit/wboit_accum.vert.glsl", "src/shader/wboit/wboit_accum.frag.glsl", "");
	transparentShader_r = new Shader("src/shader/wboit/wboit_accum.vert.glsl", "src/shader/wboit/wboit_Revealage.frag.glsl", "");

	compositeShader = new Shader("src/shader/wboit/wboit_composite.vert.glsl", "src/shader/wboit/wboit_composite.frag.glsl", "");



	transparentShader_a->use();
	transparentShader_a->uniformsetInt("texture_base", 0);

	transparentShader_r->use();
	transparentShader_r->uniformsetInt("texture_base", 0);

	{
		std::vector<Texture*> textures = { TextureManager::getInstance()->loadTexture("asset/image/t_t_1.png", TextureType::translucent) };
		std::vector<Vertex> vertex = blockVertex;
		std::vector<unsigned int> indices = blockIndices;

		triangleMesh = new M_Mesh(vertex, indices, textures);
	}

	framebuffer_size_callback(window, InputManager::GetInstance().GetWindowSize().x, InputManager::GetInstance().GetWindowSize().y);


	world = new World(0, camera->getPos());


	//控制器管理器
	ControllerManager& controller_manager = ControllerManager::GetInstance();
	//摄像机控制器
	camera_controller = std::make_shared<CameraController>(camera, 10);

	controller_manager.AddController(camera_controller);

	GLFWCallbackManager::GetInstance().SetFramebufferCallback(
		[this](GLFWwindow* window, int w, int h) {
			this->framebuffer_size_callback(window, w, h); // 成员函数
		});

}


Test::FBOTest::~FBOTest() {

	delete mesh_line;

	delete world;
	GLFWCallbackManager::GetInstance().SetFramebufferCallback(nullptr);
}

void Test::FBOTest::update(double deltatime) {
	this->deltatime = deltatime;


	InputManager& input_manager = InputManager::GetInstance();

	if(camera->isUpdatePJ){
		updateShaderProjection(camera->getProjection());
		camera->isUpdatePJ = false;
	}

	{
		// 鼠标左键点击：设置方块为空气（挖除方块）
		if (is_set_air) {
			if (input_manager.IsMouseButtonPressed(MouseButton::RIGHT)) {
				glm::i64vec4 res = RaycastBlock(camera, world);

				glm::ivec3 chunkPos = worldToChunk(glm::dvec3(res.x, res.y, res.z));
				glm::ivec3 blockPos = worldToChunkLocal(glm::dvec3(res.x, res.y, res.z));

				Chunk* chunk = world->getChunkorCreate(chunkPos);


				if (chunk != nullptr) {
					{
						std::lock_guard<std::mutex> lock(chunk->chunkMutex);
						chunk->setBlock(blockPos.x, blockPos.y, blockPos.z, RegisterBlock::getInstance()->getBlockState_NAME("air").ID);
						chunk->buildMeshData();
						chunk->buildMeshVertex();
					}

				}
				else
				{
					std::cout << "\033[31m [error] \033[0m [mc_test2] : setair chunk is nullptr" << glm::to_string(chunkPos) << std::endl;
				}
			}
		}
	}

	{
		// 冷却计时
		if (block_cd <= 0.2f) {
			block_cd += deltatime;
		}
		else {
			// 冷却完成 + 允许放置 + 右键按下
			if (is_set_block && input_manager.IsMouseButtonPressed(MouseButton::RIGHT)) {
				glm::i64vec4 res = RaycastBlock(camera, world);
				if (res.w == -1) {
					return; // 未命中方块，直接退出
				}

				// 根据碰撞面偏移坐标（核心逻辑简化）
				switch (res.w) {
				case 0: res.z++; break;
				case 1: res.z--; break;
				case 2: res.y--; break;
				case 3: res.y++; break;
				case 4: res.x++; break;
				case 5: res.x--; break;
				}

				// 打印调试信息
				std::cout << "res:" << res.x << ";" << res.y << ";" << res.z << ";    " << res.w << std::endl;

				// 计算区块与方块位置
				glm::dvec3 worldPos(res.x, res.y, res.z);
				glm::ivec3 chunkPos = worldToChunk(worldPos);
				glm::ivec3 blockPos = worldToChunkLocal(worldPos);
				std::cout << "chunkPos: " << glm::to_string(chunkPos) << " blockPos: " << glm::to_string(blockPos) << std::endl;

				// 获取/创建区块
				Chunk* chunk = world->getChunkorCreate(chunkPos);

				if (chunk) {// 设置方块并重建网格
					{
						std::lock_guard<std::mutex> lock(chunk->chunkMutex);
						chunk->setBlock(blockPos.x, blockPos.y, blockPos.z, RegisterBlock::getInstance()->getBlockState_NAME("red").ID);
						chunk->buildMeshData();
						chunk->buildMeshVertex();
					}

					world->addRenderChunk(chunk);

					block_cd = 0.0f;

				}
				else {
					std::cout << "\033[31m [error] \033[0m [mc_test2] : setblock chunk is nullptr" << glm::to_string(chunkPos) << std::endl;
				}
			}
		}


	}

	world->update(deltatime, camera->getPos());

}

void Test::FBOTest::Render() {
	Renderer* renderer = &Renderer::Instance();

	//===========================================================================================
	baseFBO->Bind();

	glEnable(GL_DEPTH_TEST); // 开启深度测试
	glDepthFunc(GL_GREATER); // 深度测试函数：更远的片元通过（因为我们把深度反转，所以更远的片元深度值更大）
	glDepthMask(GL_TRUE);	 // 允许写入深度缓冲
	glDisable(GL_BLEND);     // 关闭混合

	renderer->setClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
	renderer->setClearDepth(0.0f);
	renderer->Clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// 绘制线框
	lineShader->use();
	lineShader->uniformsetMat4("view", camera->getView());
	lineShader->uniformsetMat4("model", glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0)));
	glLineWidth(2.0f); // 注意：Core 模式失效
	mesh_line->Draw(*lineShader);

	// 绘制场景块
	chunkShader->use();
	chunkShader->uniformsetMat4("view", glm::mat4(glm::mat3(camera->getView())));
	world->Draw(*chunkShader, camera->getPos());

	////===========================================================================================
	// 透明 FBO 阶段
	transparentFBO->Bind();

	// 2. 同时启用 0、1 两个颜色附件
	GLenum allBufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, allBufs);

	// 清空两个颜色附件
	float color0[] = { 0,0,0,0 };
	glClearBufferfv(GL_COLOR, 0, color0);
	// 揭示度附件通常清为1
	float color1[] = { 1,1,1,1 };
	glClearBufferfv(GL_COLOR, 1, color1);

	glEnable(GL_DEPTH_TEST); // 开启深度测试
	glDepthFunc(GL_GREATER); // 深度测试函数：更远的片元通过
	glDepthMask(GL_FALSE);   // 关闭深度写入

	// ===================== 绘制 Accumulation 附件0 (允许混合) =====================
	GLenum drawBuf0 = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &drawBuf0);

	glEnable(GL_BLEND); // 开启混合
	glBlendFunc(GL_ONE, GL_ONE); // 累加模式：源颜色直接加到目标颜色上

	transparentShader_a->use();
	transparentShader_a->uniformsetMat4("view", glm::mat4(glm::mat3(camera->getView())));

	glm::vec3 chunkRelativePos = glm::vec3(0, 0, 0) - glm::vec3(camera->getPos());
	transparentShader_a->uniformsetVec3("chunkRelativePos", chunkRelativePos);
	triangleMesh->Draw(*transparentShader_a);

	chunkRelativePos = glm::vec3(1, 1, 1) - glm::vec3(camera->getPos());
	transparentShader_a->uniformsetVec3("chunkRelativePos", chunkRelativePos);
	triangleMesh->Draw(*transparentShader_a);

	chunkRelativePos = glm::vec3(2, 2, 2) - glm::vec3(camera->getPos());
	transparentShader_a->uniformsetVec3("chunkRelativePos", chunkRelativePos);
	triangleMesh->Draw(*transparentShader_a);


	// ===================== 绘制 Revealage 附件1 =====================

	GLenum drawBuf1 = GL_COLOR_ATTACHMENT1;
	glDrawBuffers(1, &drawBuf1);

	//glDisable(GL_BLEND);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR); // 揭示度混合：目标颜色乘以(1-源颜色)，源颜色不影响目标颜色

	transparentShader_r->use();
	transparentShader_r->uniformsetMat4("view", glm::mat4(glm::mat3(camera->getView())));

	chunkRelativePos = glm::vec3(0, 0, 0) - glm::vec3(camera->getPos());
	transparentShader_r->uniformsetVec3("chunkRelativePos", chunkRelativePos);
	triangleMesh->Draw(*transparentShader_r);

	chunkRelativePos = glm::vec3(1, 1, 1) - glm::vec3(camera->getPos());
	transparentShader_r->uniformsetVec3("chunkRelativePos", chunkRelativePos);
	triangleMesh->Draw(*transparentShader_r);

	chunkRelativePos = glm::vec3(2, 2, 2) - glm::vec3(camera->getPos());
	transparentShader_r->uniformsetVec3("chunkRelativePos", chunkRelativePos);
	triangleMesh->Draw(*transparentShader_r);
	//============================================================================================
	
	



	//glReadBuffer(GL_COLOR_ATTACHMENT1); // 读取累积颜色附件
	//float pixel[4];
	//glReadPixels(InputManager::GetInstance().GetWindowSize().x / 2, InputManager::GetInstance().GetWindowSize().y / 2, 1, 1, GL_RGBA, GL_FLOAT, pixel);

	//// 3. 控制台打印 ViewPos.z 精确值
	//std::cout << "Center Pixel RGBA (Accum): " << pixel[0] << ", " << pixel[1] << ", " << pixel[2] << ", " << pixel[3] << std::endl;

	////===========================================================================================

	transparentFBO->Unbind();

	// 切回默认帧缓冲：先改状态，再清屏（规范顺序）
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClearDepth(0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 最终合成
	compositeShader->use();
	compositeShader->uniformsetInt("opaqueTexture", 0);
	compositeShader->uniformsetInt("accumulationTexture", 1);
	compositeShader->uniformsetInt("revealageTexture", 2);

	opaqueColor->activeTex(0);
	opaqueColor->bind();
	accumTex->activeTex(1);
	accumTex->bind();
	revealTex->activeTex(2);
	revealTex->bind();

	quadMesh->Draw(*compositeShader);
}

void Test::FBOTest::GuiRender()
{
	// ====================== 屏幕中间十字准星 ===================================
	// 让 ImGui 画一个全屏透明窗口，只用来显示准星
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoFocusOnAppearing;

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::Begin("Crosshair", nullptr, window_flags);

	// 获取屏幕中心
	ImVec2 center = ImVec2(InputManager::GetInstance().GetWindowSize().x * 0.5f, InputManager::GetInstance().GetWindowSize().y * 0.5f);

	// 准星颜色（白色）、粗细、长度
	ImU32 color = IM_COL32_WHITE;
	float thickness = 2.0f;
	float length = 10.0f;

	// 画横线 + 竖线
	ImDrawList* draw = ImGui::GetWindowDrawList();
	draw->AddLine(ImVec2(center.x - length, center.y),
		ImVec2(center.x + length, center.y),
		color, thickness);
	draw->AddLine(ImVec2(center.x, center.y - length),
		ImVec2(center.x, center.y + length),
		color, thickness);

	ImGui::End();
	//=========================================================================


	ImGui::Begin("setting");
	ImGui::DragFloat("speed", &camera_controller->getSpeed(), 0.1f);
	if (ImGui::Button("set this block"))
	{
		glm::ivec3 chunkPos = worldToChunk(camera->getPos());
		glm::ivec3 blockPos = worldToChunkLocal(camera->getPos());
		std::cout << "res:" << (long long)chunkPos.x * Chunk::ChunkSize + blockPos.x << ";" << (long long)chunkPos.y * Chunk::ChunkSize + blockPos.y << ";" << (long long)chunkPos.z * Chunk::ChunkSize + blockPos.z << std::endl;
		std::cout << "chunkPos: " << glm::to_string(chunkPos) << " blockPos: " << glm::to_string(blockPos) << std::endl;

		Chunk* chunk = world->getChunkorCreate(chunkPos);
		if (chunk != nullptr) {
			chunk->setBlock(blockPos.x, blockPos.y, blockPos.z, RegisterBlock::getInstance()->getBlockState_NAME("stone").ID);
			chunk->buildMeshData();
			chunk->buildMeshVertex();
		}
		else
		{
			std::cout << "\033[31m error: chunk is nullptr at \033[0m" << glm::to_string(chunkPos) << std::endl;
		}
	}
	if (ImGui::Button("set this air"))
	{
		glm::ivec3 chunkPos = worldToChunk(camera->getPos());
		glm::ivec3 blockPos = worldToChunkLocal(camera->getPos());


		Chunk* chunk = world->getChunkorCreate(chunkPos);
		if (chunk != nullptr) {
			chunk->setBlock(blockPos.x, blockPos.y, blockPos.z, RegisterBlock::getInstance()->getBlockState_NAME("air").ID);
			chunk->buildMeshData();
			chunk->buildMeshVertex();
		}
		else
		{
			std::cout << "\033[31m error: chunk is nullptr at \033[0m" << glm::to_string(chunkPos) << std::endl;
		}
	}

	ImGui::Checkbox("set block_dir", &is_set_block);
	// 2. 绘制勾选框
	ImGui::Checkbox("set air_dir", &is_set_air);

	ImGui::End();

	ImGui::Begin("Debug Window");  // 示例窗口，验证流程
	isback = ImGui::Button("<-");
	ImGui::Text("FPS: %.1f", 1.0f / deltatime);


	// 2. RGBA 颜色编辑框（带透明度）
	ImGui::ColorEdit3("选择颜色", (float*)&clearColor);  // 显示透明度条

	glm::vec3 fronttemp = camera->getFront();
	if (ImGui::DragFloat3("camera front", (float*)&fronttemp, 0.1f)) {
		camera->setFront(fronttemp);
	};

	ImGui::DragScalarN(
		"Camera Pos",          // 标签
		ImGuiDataType_Double,  // 数据类型：双精度
		&camera->getPosRef(),                   // 数据指针（double*）
		3,                     // 分量数：3
		0.1,                   // 拖拽速度（double，不要 f 后缀）
		nullptr, nullptr,      // 最小值、最大值（nullptr=无限制）
		"%.6f",                // 显示格式（保留6位小数）
		1.0                    // 功率曲线（默认1.0线性）
	);



	ImGui::End();


}

void Test::FBOTest::framebuffer_size_callback(GLFWwindow* window, int width, int height) {


	camera->setProjection(camera->getFov(), (float)width / height, camera->getNearFar().x, camera->getNearFar().y);

	updateShaderProjection(camera->getProjection());

	baseFBO->Resize(width, height);
	transparentFBO->Resize(width, height);
}

glm::vec3 getCameraLookAtCenter(Camera* camera)

{
	// 屏幕中心点 (NDC 空间)
	glm::vec4 screenCenter(0.0f, 0.0f, -1.0f, 1.0f);

	// 逆投影矩阵
	glm::mat4 invProj = glm::inverse(camera->getProjection());

	// 逆视图矩阵
	glm::mat4 invView = glm::inverse(camera->getView());

	// 转换到世界空间
	glm::vec4 worldDir = invView * invProj * screenCenter;
	worldDir = glm::normalize(worldDir);

	return glm::vec3(worldDir.x, worldDir.y, worldDir.z);
}


void Test::FBOTest::setupQuad() {
	// 全屏四边形顶点数据
	static std::vector<Vertex2D> quadVertices = {
		{ {-1.0f, -1.0f}, {0.0f, 0.0f} },
		{ { 1.0f, -1.0f}, {1.0f, 0.0f} },
		{ {-1.0f,  1.0f}, {0.0f, 1.0f} },
		{ { 1.0f,  1.0f}, {1.0f, 1.0f} }
	};
	static std::vector<unsigned int> quadIndices = { 0, 1, 2,  1, 3, 2 };


	std::vector<Texture*> quadtextures = { opaqueColor, accumTex ,revealTex };
	quadMesh = new Mesh_2D(quadVertices, quadIndices, quadtextures);
}

void Test::FBOTest::updateShaderProjection(const glm::mat4& projection) {
	lineShader->use();
	lineShader->uniformsetMat4("projection", projection);

	chunkShader->use();
	chunkShader->uniformsetMat4("projection", projection);

	transparentShader_a->use();
	transparentShader_a->uniformsetMat4("projection", projection);

	transparentShader_r->use();
	transparentShader_r->uniformsetMat4("projection", projection);
}