#include "stdafx.h"
#include "RenderScene.h"
#include "YshMath.h"
#include <freeglut.h>

struct CubeMapFace
{
	CubeMapFace(GLenum iFace_, const fVec3& viewDir_, const fVec3& viewUp_) : iFace(iFace_), viewDir(viewDir_), viewUp(viewUp_) {}

	GLenum iFace;
	fVec3 viewDir;
	fVec3 viewUp;
};
static const CubeMapFace gCubeMapFaces[6] =
{
	CubeMapFace( GL_TEXTURE_CUBE_MAP_POSITIVE_X, fVec3( 1.0f,  0.0f,  0.0f), fVec3(0.0f, -1.0f,  0.0f) ),
	CubeMapFace( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, fVec3(-1.0f,  0.0f,  0.0f), fVec3(0.0f, -1.0f,  0.0f) ),
	CubeMapFace( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, fVec3( 0.0f,  1.0f,  0.0f), fVec3(0.0f,  0.0f,  1.0f) ),
	CubeMapFace( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, fVec3( 0.0f, -1.0f,  0.0f), fVec3(0.0f,  0.0f, -1.0f) ),
	CubeMapFace( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, fVec3( 0.0f,  0.0f,  1.0f), fVec3(0.0f, -1.0f,  0.0f) ),
	CubeMapFace( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, fVec3( 0.0f,  0.0f, -1.0f), fVec3(0.0f, -1.0f,  0.0f) )
};

RenderNode::RenderNode() : m_renderObject(nullptr), m_prev(nullptr), m_next(nullptr)
{
}
RenderNode::~RenderNode()
{
}
RenderObject* RenderNode::GetRenderObject() const
{
	return m_renderObject;
}
RenderNode* RenderNode::GetPrev() const
{
	return m_prev;
}
RenderNode* RenderNode::GetNext() const
{
	return m_next;
}
void RenderNode::BindRenderObject(RenderObject* renderObject)
{
	m_renderObject = renderObject;
	renderObject->m_node = this;
}
void RenderNode::Remove()
{
	if (m_prev)
	{
		m_prev->m_next = m_next;
		m_prev = nullptr;
	}
	if (m_next)
	{
		m_next->m_prev = m_prev;
		m_next = nullptr;
	}
}
void RenderNode::AppendTo(RenderNode* prev)
{
	if (m_prev)
	{
		m_prev->m_next = m_next;
	}
	if (m_next)
	{
		m_next->m_prev = m_prev;
	}
	if (prev)
	{
		if (RenderNode* next = prev->m_next)
		{
			next->m_prev = this;
			m_next = next;
		}
		prev->m_next = this;
		m_prev = prev;
	}
}
void RenderNode::PrependTo(RenderNode* next)
{
	if (m_prev)
	{
		m_prev->m_next = m_next;
	}
	if (m_next)
	{
		m_next->m_prev = m_prev;
	}
	if (next)
	{
		if (RenderNode* prev = next->m_prev)
		{
			prev->m_next = this;
			m_prev = prev;
		}
		next->m_prev = this;
		m_next = next;
	}
}

FreedRenderNode::FreedRenderNode() : m_node(nullptr), m_precedingNode(nullptr)
{
}
FreedRenderNode::~FreedRenderNode()
{
}

RenderScene::RenderScene() :
	m_firstNode(nullptr),
	m_ambient(0.25f, 0.25f, 0.25f)
{
	m_renderNodes = new RenderNode[MAX_RENDER_NODES];

	for (int i = 0; i < MAX_RENDER_NODES- 1; ++i)
	{
		m_renderNodes[i].PrependTo(&m_renderNodes[i + 1]);
	}

	for (int i = MAX_RENDER_NODES- 1; i > 0; --i)
	{
		FreedRenderNode freeNode;
		freeNode.m_node = &m_renderNodes[i];
		freeNode.m_precedingNode = &m_renderNodes[i - 1];
		m_freedNodeStack.push(freeNode);
	}
	FreedRenderNode freeNode;
	freeNode.m_node = &m_renderNodes[0];
	freeNode.m_precedingNode = nullptr;
	m_freedNodeStack.push(freeNode);

	m_depthMap.Init(1200, 900);
	m_forwardRender.Init(1200, 900);
	m_finalRender.Init(1200, 900);
}


RenderScene::~RenderScene()
{
	delete[] m_renderNodes;
}

DebugRenderer& RenderScene::DebugDrawSystem()
{
	return m_debugRenderer;
}

void RenderScene::AddRenderObject(RenderObject* renderObject)
{
	if (!m_freedNodeStack.empty())
	{
		FreedRenderNode& freeNode = m_freedNodeStack.top();
		freeNode.m_node->BindRenderObject(renderObject);

		if (freeNode.m_precedingNode)
		{
			freeNode.m_node->AppendTo(freeNode.m_precedingNode);
		}
		else
		{
			freeNode.m_node->PrependTo(m_firstNode);
			m_firstNode = freeNode.m_node;
		}

		m_freedNodeStack.pop();
	}
}

void RenderScene::RemoveRenderObject(RenderObject* renderObject)
{
	if (RenderNode* node = renderObject->GetRenderNode())
	{
		if (node->GetPrev() == nullptr && node->GetNext() != nullptr)
		{
			m_firstNode = node->GetNext();
		}
		node->BindRenderObject(nullptr);
		FreedRenderNode freeNode;
		freeNode.m_precedingNode = node->GetPrev();
		freeNode.m_node = node;
		node->Remove();
		m_freedNodeStack.push(freeNode);
	}
}

void RenderScene::AddPointLight(const PointLight& pointLight)
{
	m_pointLights.push_back(pointLight);
}

void RenderScene::AttachCamera(Camera* camera)
{
	camera->SetViewport(&m_viewport);
}

void RenderScene::RenderDepthFromLights()
{
//	glCullFace(GL_FRONT);

	// Damn graphics library mumbo jumbo...
	// https://www.opengl.org/discussion_boards/showthread.php/189389-Shadow-Mapping-not-working-for-textures-not-size-of-screen
	// http://www.idevgames.com/forums/thread-2744.html

	GLuint shadowMapShaderProgram = m_shadowCubeMapShader.GetProgram();
	glUseProgram(shadowMapShaderProgram);

	const GLint projectionViewLoc = glGetUniformLocation(shadowMapShaderProgram, "projectionViewMatrices");
	const GLint modelLoc = glGetUniformLocation(shadowMapShaderProgram, "modelMatrix");
	const GLint pointLightPosLoc = glGetUniformLocation(shadowMapShaderProgram, "pointLightPos");
	const GLint uniLoc_near = glGetUniformLocation(shadowMapShaderProgram, "gNear");
	const GLint uniLoc_far = glGetUniformLocation(shadowMapShaderProgram, "gFar");

	for (PointLight& pointLight : m_pointLights)
	{
		const fMat44 projMat = pointLight.shadowCubeMap.CreateProjectionMatrix();

		float projViewMats[16 * 6];
		float* ptr = projViewMats;

		DepthCubeMap& cubeMap = pointLight.shadowCubeMap;
		cubeMap.BindForWriting();
		glViewport(0, 0, pointLight.shadowCubeMap.m_width, pointLight.shadowCubeMap.m_width);
		glClear(GL_DEPTH_BUFFER_BIT);

		for (unsigned int i = 0; i < 6; i++)
		{
			const fMat44 viewMat = fHomogeneousTransformation::CreateViewMatrix
			(
				pointLight.position,
				gCubeMapFaces[i].viewDir,
				gCubeMapFaces[i].viewUp
			);
			const fMat44 projViewMat = projMat*viewMat;
			projViewMat.Transpose().GetData(ptr);
			ptr += 16;
		}
		glUniformMatrix4fv(projectionViewLoc, 6, GL_FALSE, projViewMats);
		glUniform3f(pointLightPosLoc, pointLight.position.x, pointLight.position.y, pointLight.position.z);
		glUniform1f(uniLoc_near, pointLight.shadowCubeMap.m_near);
		glUniform1f(uniLoc_far, pointLight.shadowCubeMap.m_far);

		const RenderNode* node = m_firstNode;
		while (node)
		{
			if (RenderObject* obj = node->GetRenderObject())
			{
				RenderMesh* mesh = obj->GetRenderMesh();
				glBindVertexArray(mesh->GetVAO());
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->GetIBO());
				glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &(obj->CreateModelMatrix().Transpose()(0, 0)));
				glDrawElements(GL_TRIANGLES, 3 * mesh->GetNTriangles(), GL_UNSIGNED_INT, 0);
			}

			node = node->GetNext();
		}
	}
}

void RenderScene::RenderDepthFromEye(Window* window)
{
	glCullFace(GL_BACK);

	GLuint program = m_depthPerspectiveShader.GetProgram();
	glUseProgram(program);

	const GLint uniLoc_projection = glGetUniformLocation(program, "gProjection");
	const GLint uniLoc_view = glGetUniformLocation(program, "gView");
	const GLint uniLoc_model = glGetUniformLocation(program, "gModel");

	const GLint uniLoc_near = glGetUniformLocation(program, "gNear");
	const GLint uniLoc_far = glGetUniformLocation(program, "gFar");

	glUniformMatrix4fv(uniLoc_projection, 1, GL_FALSE, &(m_viewport.CreateProjectionMatrix().Transpose()(0, 0)));
	glUniformMatrix4fv(uniLoc_view, 1, GL_FALSE, &(m_viewport.CreateViewMatrix().Transpose()(0, 0)));
	glUniform1f(uniLoc_near, m_viewport.m_near);
	glUniform1f(uniLoc_far, m_viewport.m_far);

	m_depthMap.BindForWriting();
	glClear(GL_DEPTH_BUFFER_BIT);

	int w, h;
	window->GetDimensions(w, h);
	glViewport(0, 0, w, h);

	const fMat44 viewMatrix = m_viewport.CreateViewMatrix();
	const fMat44 projectionMatrix = m_viewport.CreateProjectionMatrix();

	const RenderNode* node = m_firstNode;
	while (node)
	{
		if (RenderObject* obj = node->GetRenderObject())
		{
			if (RenderMesh* mesh = obj->GetRenderMesh())
			{
				glBindVertexArray(mesh->GetVAO());
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->GetIBO());
				glUniformMatrix4fv(uniLoc_model, 1, GL_FALSE, &(obj->CreateModelMatrix().Transpose()(0, 0)));
				glDrawElements(GL_TRIANGLES, 3 * mesh->GetNTriangles(), GL_UNSIGNED_INT, 0);
			}
		}
		node = node->GetNext();
	}
}

void RenderScene::ForwardPass()
{
	m_forwardRender.BindForWriting();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glViewport(0, 0, m_forwardRender.m_width, m_forwardRender.m_height);

	const fMat44 viewMatrix = m_viewport.CreateViewMatrix();
	const fMat44 projectionMatrix = m_viewport.CreateProjectionMatrix();

	GLuint program = m_forwardPassShader.GetProgram();
	glUseProgram(program);
	const GLint uniLoc_projection = glGetUniformLocation(program, "gProjection");
	const GLint uniLoc_view = glGetUniformLocation(program, "gView");
	const GLint uniLoc_model = glGetUniformLocation(program, "gModel");
	glUniformMatrix4fv(uniLoc_projection, 1, GL_FALSE, &(m_viewport.CreateProjectionMatrix().Transpose()(0, 0)));
	glUniformMatrix4fv(uniLoc_view, 1, GL_FALSE, &(m_viewport.CreateViewMatrix().Transpose()(0, 0)));

	const RenderNode* node = m_firstNode;
	while (node)
	{
		if (RenderObject* obj = node->GetRenderObject())
		{
			if (RenderMesh* mesh = obj->GetRenderMesh())
			{
				glBindVertexArray(mesh->GetVAO());
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->GetIBO());
				glUniformMatrix4fv(uniLoc_model, 1, GL_FALSE, &(obj->CreateModelMatrix().Transpose()(0, 0)));
				glDrawElements(GL_TRIANGLES, 3 * mesh->GetNTriangles(), GL_UNSIGNED_INT, 0);
			}
		}
		node = node->GetNext();
	}

	m_debugRenderer.DrawObjectsToGBuffer(m_viewport);
	m_debugRenderer.EvictObjects();

	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RenderScene::InitFinalRender()
{
	glViewport(0, 0, m_finalRender.m_width, m_finalRender.m_height);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	m_finalRender.BindForModification(GL_TEXTURE0);
	glClear(GL_COLOR_BUFFER_BIT);

	m_finalRender.SwapReadWriteTextures();

	m_finalRender.BindForModification(GL_TEXTURE0);
	glClear(GL_COLOR_BUFFER_BIT);
}

void RenderScene::FinalizeLighting()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glViewport(0, 0, m_finalRender.m_width, m_finalRender.m_height);

	GLuint program = m_finalizeLightingShader.GetProgram();
	glUseProgram(program);

	const GLint uniLoc_colorTex = glGetUniformLocation(program, "gColorTex");
	const GLint uniLoc_diffuseTex = glGetUniformLocation(program, "gDiffuseTex");
	const GLint uniLoc_ambient = glGetUniformLocation(program, "gAmbient");
	const GLint uniLoc_lightingStencil = glGetUniformLocation(program, "gLightingStencil");

	glUniform1i(uniLoc_colorTex, 0);
	glUniform1i(uniLoc_lightingStencil, 1);
	glUniform1i(uniLoc_diffuseTex, 2);
	glUniform3f(uniLoc_ambient, m_ambient.x, m_ambient.y, m_ambient.z);

	m_forwardRender.BindDiffuseForReading(GL_TEXTURE2);
	m_forwardRender.BindStencilForReading(GL_TEXTURE1);
	m_finalRender.BindForModification(GL_TEXTURE0);
	glClear(GL_COLOR_BUFFER_BIT);

	GLuint dummyVAO;
	glGenVertexArrays(1, &dummyVAO);
	glBindVertexArray(dummyVAO);
	glDrawArrays(GL_POINTS, 0, 1);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &dummyVAO);

	m_finalRender.SwapReadWriteTextures();
}

void RenderScene::LightingPass()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glViewport(0, 0, m_finalRender.m_width, m_finalRender.m_height);

	GLuint program = m_deferredPointLightShader.GetProgram();
	glUseProgram(program);

	const GLint uniLoc_colorTex = glGetUniformLocation(program, "gColorTex");

	const GLint uniLoc_positionTex = glGetUniformLocation(program, "gPositionTex");
	const GLint uniLoc_normalTex = glGetUniformLocation(program, "gNormalTex");
	const GLint uniLoc_diffuseTex = glGetUniformLocation(program, "gDiffuseTex");
	const GLint uniLoc_specularTex = glGetUniformLocation(program, "gSpecularTex");
	const GLint uniLoc_lightingStencil = glGetUniformLocation(program, "gLightingStencil");

	const GLint uniLoc_eyePos = glGetUniformLocation(program, "gEyePos");

	glUniform3f(uniLoc_eyePos, m_viewport.m_pos.x, m_viewport.m_pos.y, m_viewport.m_pos.z);

	m_forwardRender.BindNormalForReading(GL_TEXTURE1);
	m_forwardRender.BindPositionForReading(GL_TEXTURE2);
	m_forwardRender.BindDiffuseForReading(GL_TEXTURE3);
	m_forwardRender.BindSpecularForReading(GL_TEXTURE4);
	m_forwardRender.BindStencilForReading(GL_TEXTURE5);

	glUniform1i(uniLoc_colorTex, 0);

	glUniform1i(uniLoc_normalTex, 1);
	glUniform1i(uniLoc_positionTex, 2);
	glUniform1i(uniLoc_diffuseTex, 3);
	glUniform1i(uniLoc_specularTex, 4);

	glUniform1i(uniLoc_lightingStencil, 5);

	for (PointLight pointLight : m_pointLights)
	{
		m_finalRender.BindForModification(GL_TEXTURE0);
		glClear(GL_COLOR_BUFFER_BIT);

		const GLint uniLoc_lightPos = glGetUniformLocation(program, "gLightPos");
		const GLint uniLoc_lightInt = glGetUniformLocation(program, "gLightInt");
		glUniform3f(uniLoc_lightPos, pointLight.position.x, pointLight.position.y, pointLight.position.z);
		glUniform3f(uniLoc_lightInt, pointLight.intensity.x, pointLight.intensity.y, pointLight.intensity.z);

		GLuint dummyVAO;
		glGenVertexArrays(1, &dummyVAO);
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_POINTS, 0, 1);
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &dummyVAO);

		m_finalRender.SwapReadWriteTextures();
	}
}

void RenderScene::ShadowPass()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glViewport(0, 0, m_finalRender.m_width, m_finalRender.m_height);

	GLuint program = m_deferredPointLightShadowShader.GetProgram();
	glUseProgram(program);
	const GLint uniLoc_positionTex = glGetUniformLocation(program, "gPositionTex");
	const GLint uniLoc_colorTex = glGetUniformLocation(program, "gColorTex");
	const GLint uniLoc_lightingStencil = glGetUniformLocation(program, "gLightingStencil");

	const GLint uniLoc_near_light = glGetUniformLocation(program, "gNear_light");
	const GLint uniLoc_far_light = glGetUniformLocation(program, "gFar_light");

	m_forwardRender.BindPositionForReading(GL_TEXTURE2);
	m_forwardRender.BindStencilForReading(GL_TEXTURE3);

	glUniform1i(uniLoc_positionTex, 2);
	glUniform1i(uniLoc_lightingStencil, 3);

	for (PointLight pointLight : m_pointLights)
	{
		m_finalRender.BindForModification(GL_TEXTURE0);
		glUniform1i(uniLoc_colorTex, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		const GLint uniLoc_lightPos = glGetUniformLocation(program, "gLightPos");
		glUniform3f(uniLoc_lightPos, pointLight.position.x, pointLight.position.y, pointLight.position.z);

		pointLight.shadowCubeMap.BindForReading(GL_TEXTURE1);
		const GLint uniLoc_shadowCubeMap = glGetUniformLocation(program, "gShadowCubeMap");
		glUniform1i(uniLoc_shadowCubeMap, 1);

		glUniform1f(uniLoc_near_light, pointLight.shadowCubeMap.m_near);
		glUniform1f(uniLoc_far_light, pointLight.shadowCubeMap.m_far);

		GLuint dummyVAO;
		glGenVertexArrays(1, &dummyVAO);
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_POINTS, 0, 1);
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &dummyVAO);

		m_finalRender.SwapReadWriteTextures();
	}
}

void RenderScene::FinalizeRender(Window* window)
{
	glCullFace(GL_BACK);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int w, h;
	window->GetDimensions(w, h);

	glViewport(0, 0, w, h);

	glUseProgram(m_fullScreenQuadShader.GetProgram());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_finalRender.GetLastWrittenColorTexture());

	GLuint dummyVAO;
	glGenVertexArrays(1, &dummyVAO);
	glBindVertexArray(dummyVAO);
	glDrawArrays(GL_POINTS, 0, 1);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &dummyVAO);
}

void RenderScene::RenderPass(Window* window)
{
	glCullFace(GL_BACK);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int w, h;
	window->GetDimensions(w, h);

	glViewport(0, 0, w, h);

//	glUseProgram(m_fullScreenQuadShader.GetProgram());
//	glActiveTexture(GL_TEXTURE0);
////	glBindTexture(GL_TEXTURE_2D, m_forwardRender.m_normal);
//	glBindTexture(GL_TEXTURE_2D, m_forwardRender.m_color);

//	GLuint dummyVAO;
//	glGenVertexArrays(1, &dummyVAO);
//	glBindVertexArray(dummyVAO);
//	glDrawArrays(GL_POINTS, 0, 1);
//	glBindVertexArray(0);
//	glDeleteVertexArrays(1, &dummyVAO);
//	return;

	const fMat44 viewMatrix = m_viewport.CreateViewMatrix();
	const fMat44 projectionMatrix = m_viewport.CreateProjectionMatrix();

	const RenderNode* node = m_firstNode;
	while (node)
	{
		if (RenderObject* obj = node->GetRenderObject())
		{
			RenderMesh* mesh = obj->GetRenderMesh();
			Shader* shader = obj->GetShader();

			for (PointLight pointLight : m_pointLights)
			{
				GLuint program = shader->GetProgram();
				glUseProgram(program);
				GLint shadowCubeMapTex = glGetUniformLocation(program, "shadowCubeMap");
				GLint pointLightPos = glGetUniformLocation(program, "pointLightPos");
				const GLint pointLightInt = glGetUniformLocation(program , "pointLightInt");
				const GLint uniLoc_near = glGetUniformLocation(program, "gNear");
				const GLint uniLoc_far = glGetUniformLocation(program, "gFar");
				glUniform1i(shadowCubeMapTex, 0);
				glUniform3f(pointLightPos, pointLight.position.x, pointLight.position.y, pointLight.position.z);
				glUniform3f(pointLightInt, pointLight.intensity.x, pointLight.intensity.y, pointLight.intensity.z);
				glUniform1f(uniLoc_near, pointLight.shadowCubeMap.m_near);
				glUniform1f(uniLoc_far, pointLight.shadowCubeMap.m_far);
				pointLight.shadowCubeMap.BindForReading(GL_TEXTURE0);
			}

			obj->Draw(projectionMatrix, viewMatrix);
		}

		node = node->GetNext();
	}

	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

}

void RenderScene::DrawScene(Window* window)
{
	for (PointLight pointLight : m_pointLights)
	{
		m_debugRenderer.DrawBox(0.25f, 0.25f, 0.25f, pointLight.position, fQuat::Identity(), fVec3(1.0f, 1.0f, 0.0f), false, false);
	}

#if 1
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	RenderDepthFromLights();
	RenderDepthFromEye(window);
	ForwardPass();
//	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	InitFinalRender();
	LightingPass();
	FinalizeLighting();
	ShadowPass();
	FinalizeRender(window);
#else
	RenderPass(window);
#endif
	
	m_debugRenderer.EvictObjects();
}
