/*
  Selected code excerpt from my private C++ OpenGL RTS game engine.

  This file is not intended to compile standalone.
  It is included to demonstrate implementation style and technical approach
  without publishing the full engine source code.
*/

#include <vector>
#include <cfloat>
#include <algorithm>
#include <cmath>

#include <GLEW/glew.h>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "GLDebug.h"
#include "RenderPipeline.h"
#include "Environment.h"
#include "Building.h"

/*
  Notes:
  - This is a trimmed excerpt, not the full RenderPipeline.cpp file.
  - Constructor/destructor and some UI-only/debug methods are intentionally omitted.
  - Project-specific classes such as EntityManager, ModelManager, Renderer, Shader,
    FrameBuffer, Model and Building are defined in the private source tree.
*/

static std::vector<glm::vec4> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
{
	const glm::mat4 inv = glm::inverse(proj * view);

	std::vector<glm::vec4> corners;
	corners.reserve(8);

	for (int x = 0; x < 2; x++)
	{
		for (int y = 0; y < 2; y++)
		{
			for (int z = 0; z < 2; z++)
			{
				glm::vec4 pt = inv * glm::vec4(
					2.0f * x - 1.0f,
					2.0f * y - 1.0f,
					2.0f * z - 1.0f,
					1.0f
				);

				corners.push_back(pt / pt.w);
			}
		}
	}

	return corners;
}

void RenderPipeline::InitGL()
{
	GLCall(glEnable(GL_BLEND));
	GLCall(glEnable(GL_DEPTH_TEST));
	glEnable(GL_CULL_FACE);
	GLCall(glCullFace(GL_BACK));
	GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
}

void RenderPipeline::OnRender(EntityManager& ENT_MNGR, ModelManager* MODEL_MNGR, float clearColor[4])
{
	RenderShadowPass(ENT_MNGR, MODEL_MNGR);
	RenderMainScenePass(ENT_MNGR, MODEL_MNGR, clearColor);

	const GLuint sharpTex = m_mainFBO->GetColorTextureID();
	const GLuint blurTex = RenderBlurPass(MODEL_MNGR);

	RenderCompositePass(MODEL_MNGR, sharpTex, blurTex);
	RenderToPlacePreviewPointLabels(ENT_MNGR);
}

void RenderPipeline::BuildLightSpaceMatrix()
{
	std::vector<glm::vec4> baseCorners = GetFrustumCornersWorldSpace(m_CameraProj, m_CameraView);

	std::vector<glm::vec4> fitCorners = baseCorners;
	const float casterExtrude = 1200.0f;

	for (const auto& c : baseCorners)
	{
		fitCorners.push_back(c + glm::vec4(m_LightDir * casterExtrude, 0.0f));
	}

	glm::vec3 center(0.0f);
	for (const auto& v : baseCorners)
	{
		center += glm::vec3(v);
	}
	center /= static_cast<float>(baseCorners.size());

	glm::vec3 lightPos = center + m_LightDir * 1500.0f;
	m_LightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 0.0f, 1.0f));

	float minX = FLT_MAX, maxX = -FLT_MAX;
	float minY = FLT_MAX, maxY = -FLT_MAX;
	float minZ = FLT_MAX, maxZ = -FLT_MAX;

	for (const auto& corner : fitCorners)
	{
		glm::vec4 trf = m_LightView * corner;

		minX = std::min(minX, trf.x);
		maxX = std::max(maxX, trf.x);
		minY = std::min(minY, trf.y);
		maxY = std::max(maxY, trf.y);
		minZ = std::min(minZ, trf.z);
		maxZ = std::max(maxZ, trf.z);
	}

	const float padXY = 200.0f;
	const float padZ = 1200.0f;

	minZ -= padZ;
	maxZ += padZ;

	float width = (maxX - minX) + padXY * 2.0f;
	float height = (maxY - minY) + padXY * 2.0f;
	float extent = std::max(width, height) * 0.5f;

	float centerX = (minX + maxX) * 0.5f;
	float centerY = (minY + maxY) * 0.5f;

	float texelSize = (extent * 2.0f) / static_cast<float>(m_ShadowWidth);
	centerX = std::floor(centerX / texelSize) * texelSize;
	centerY = std::floor(centerY / texelSize) * texelSize;

	minX = centerX - extent;
	maxX = centerX + extent;
	minY = centerY - extent;
	maxY = centerY + extent;

	m_LightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
	m_LightSpaceMatrix = m_LightProjection * m_LightView;
}

void RenderPipeline::RenderShadowPass(EntityManager& ENT_MNGR, ModelManager* MODEL_MNGR)
{
	BuildLightSpaceMatrix();

	GLCall(glEnable(GL_DEPTH_TEST));
	GLCall(glDepthMask(GL_TRUE));

	GLCall(glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFBO));
	GLCall(glViewport(0, 0, m_ShadowWidth, m_ShadowHeight));
	GLCall(glClear(GL_DEPTH_BUFFER_BIT));

	GLCall(glEnable(GL_POLYGON_OFFSET_FILL));
	GLCall(glPolygonOffset(2.0f, 4.0f));
	GLCall(glCullFace(GL_FRONT));

	m_ShadowMapShader->Bind();
	m_ShadowMapShader->SetUniformMat4f("u_LightSpace", m_LightSpaceMatrix);

	for (auto E : ENT_MNGR.Render_VEC)
	{
		if (E->E_enum != EntityEnum::TREE_1 && E->E_enum != EntityEnum::GRASS_1_1)
		{
			m_ShadowMapShader->SetUniformMat4f("u_Model", E->E_model_matrix);

			renderer.Draw(
				*E->E_model->m_VAO,
				*E->E_model->TRI_IBO,
				*m_ShadowMapShader,
				GL_TRIANGLES
			);
		}
	}

	for (auto E : ENT_MNGR.Environments_VEC)
	{
		m_ShadowMapShader->SetUniformMat4f("u_Model", E->E_model_matrix);

		renderer.Draw(
			*E->E_model->m_VAO,
			*E->E_model->TRI_IBO,
			*m_ShadowMapShader,
			GL_TRIANGLES
		);
	}

	Model* treeModel = MODEL_MNGR->UseModel(TREE_1);
	if (treeModel && !treeModel->entity_positions.empty())
	{
		m_ShadowMapInstancedShader->Bind();
		m_ShadowMapInstancedShader->SetUniformMat4f("u_LightSpace", m_LightSpaceMatrix);

		renderer.DrawInstances(
			treeModel->entity_positions.size(),
			*treeModel->m_VAO,
			*treeModel->TRI_IBO,
			*m_ShadowMapInstancedShader,
			GL_TRIANGLES
		);
	}

	Model* grassModel = MODEL_MNGR->UseModel(GRASS_1_1);
	if (grassModel && !grassModel->entity_positions.empty())
	{
		m_ShadowMapInstancedShader->Bind();
		m_ShadowMapInstancedShader->SetUniformMat4f("u_LightSpace", m_LightSpaceMatrix);

		renderer.DrawInstances(
			grassModel->entity_positions.size(),
			*grassModel->m_VAO,
			*grassModel->TRI_IBO,
			*m_ShadowMapInstancedShader,
			GL_TRIANGLES
		);
	}

	GLCall(glDisable(GL_POLYGON_OFFSET_FILL));
	GLCall(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GLCall(glCullFace(GL_BACK));
}

void RenderPipeline::RenderMainScenePass(EntityManager& ENT_MNGR, ModelManager* MODEL_MNGR, float clearColor[4])
{
	m_mainFBO->Bind();

	GLCall(glEnable(GL_DEPTH_TEST));
	GLCall(glDepthMask(GL_TRUE));
	GLCall(glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]));
	GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	GLCall(glViewport(0, 0, m_ScreenW, m_ScreenH));

	m_Shader_first->Bind();
	m_Shader_first->SetUniformMat4f("u_LightSpace", m_LightSpaceMatrix);
	m_Shader_first->SetUniform1i("u_Texture", 0);
	m_Shader_first->SetUniform1i("u_ShadowMap", 1);

	// default = normal rendering, not ghosted
	m_Shader_first->SetUniform1i("u_UseGhostTint", 0);
	m_Shader_first->SetUniform4f("u_GhostTint", 1.0f, 1.0f, 1.0f, 1.0f);
	m_Shader_first->SetUniform1f("u_GhostBlend", 0.0f);

	GLCall(glActiveTexture(GL_TEXTURE1));
	GLCall(glBindTexture(GL_TEXTURE_2D, m_ShadowDepthTexture));

	for (auto E : ENT_MNGR.Render_VEC)
	{
		if (E->E_enum != EntityEnum::TREE_1 && E->E_enum != EntityEnum::GRASS_1_1)
		{
			E->E_model->m_Texture->Bind(0);

			m_Shader_first->SetUniformMat4f("u_MVP", E->E_mvp_matrix);
			m_Shader_first->SetUniformMat4f("u_Model", E->E_model_matrix);

			renderer.Draw(
				*E->E_model->m_VAO,
				*E->E_model->TRI_IBO,
				*m_Shader_first,
				GL_TRIANGLES
			);
		}

		if (E->E_selected)
		{
			Model* selectionModel = MODEL_MNGR->UseModel(SELECTION_SQUARE);

			if (selectionModel->m_Texture)
			{
				selectionModel->m_Texture->Bind(0);
			}

			m_Shader_first->SetUniformMat4f("u_MVP", E->E_mvp_matrix);
			m_Shader_first->SetUniformMat4f("u_Model", E->E_model_matrix);

			renderer.Draw(
				*selectionModel->m_VAO,
				*selectionModel->TRI_IBO,
				*m_Shader_first,
				GL_TRIANGLES
			);
		}
	}

	for (auto E : ENT_MNGR.Environments_VEC)
	{
		E->E_model->m_Texture->Bind(0);

		m_Shader_first->SetUniformMat4f("u_MVP", E->E_mvp_matrix);
		m_Shader_first->SetUniformMat4f("u_Model", E->E_model_matrix);

		renderer.Draw(
			*E->E_model->m_VAO,
			*E->E_model->TRI_IBO,
			*m_Shader_first,
			GL_TRIANGLES
		);
	}

	if (m_PlacementPreviewActive && !ENT_MNGR.To_Place_VEC.empty())
	{
		const glm::vec4 ghostTint = m_PlacementPreviewValid
			? glm::vec4(1.0f, 1.0f, 1.0f, 0.45f)
			: glm::vec4(1.0f, 0.18f, 0.18f, 0.45f);

		GLCall(glEnable(GL_BLEND));
		GLCall(glDepthMask(GL_FALSE));

		m_Shader_first->SetUniform1i("u_UseGhostTint", 1);
		m_Shader_first->SetUniformVec4f("u_GhostTint", ghostTint);
		m_Shader_first->SetUniform1f("u_GhostBlend", 0.70f);

		for (Building& preview : ENT_MNGR.To_Place_VEC)
		{
			preview.E_model->m_Texture->Bind(0);

			m_Shader_first->SetUniformMat4f("u_MVP", preview.E_mvp_matrix);
			m_Shader_first->SetUniformMat4f("u_Model", preview.E_model_matrix);

			renderer.Draw(
				*preview.E_model->m_VAO,
				*preview.E_model->TRI_IBO,
				*m_Shader_first,
				GL_TRIANGLES
			);
		}

		m_Shader_first->SetUniform1i("u_UseGhostTint", 0);
		m_Shader_first->SetUniform1f("u_GhostBlend", 0.0f);

		GLCall(glDepthMask(GL_TRUE));
	}

	Model* treeModel = MODEL_MNGR->UseModel(TREE_1);
	if (treeModel && !treeModel->entity_positions.empty())
	{
		treeModel->m_Texture->Bind(0);

		m_Shader_second->Bind();
		m_Shader_second->SetUniform1i("u_Texture", 0);
		m_Shader_second->SetUniform1i("u_ShadowMap", 1);
		m_Shader_second->SetUniformMat4f("u_ViewProj", m_CameraProj * m_CameraView);
		m_Shader_second->SetUniformMat4f("u_LightSpace", m_LightSpaceMatrix);

		renderer.DrawInstances(
			treeModel->entity_positions.size(),
			*treeModel->m_VAO,
			*treeModel->TRI_IBO,
			*m_Shader_second,
			GL_TRIANGLES
		);
	}

	Model* grassModel = MODEL_MNGR->UseModel(GRASS_1_1);
	if (grassModel && !grassModel->entity_positions.empty())
	{
		grassModel->m_Texture->Bind(0);

		m_Shader_second->Bind();
		m_Shader_second->SetUniform1i("u_Texture", 0);
		m_Shader_second->SetUniform1i("u_ShadowMap", 1);
		m_Shader_second->SetUniformMat4f("u_ViewProj", m_CameraProj * m_CameraView);
		m_Shader_second->SetUniformMat4f("u_LightSpace", m_LightSpaceMatrix);

		renderer.DrawInstances(
			grassModel->entity_positions.size(),
			*grassModel->m_VAO,
			*grassModel->TRI_IBO,
			*m_Shader_second,
			GL_TRIANGLES
		);
	}

	RenderPlacementRadiusSpheres(ENT_MNGR, MODEL_MNGR);
	m_mainFBO->Unbind();
}

GLuint RenderPipeline::RenderBlurPass(ModelManager* MODEL_MNGR)
{
	GLuint sharpTex = m_mainFBO->GetColorTextureID();

	if (!m_DOFEnabled)
	{
		return sharpTex;
	}

	Model* quad = MODEL_MNGR->UseModel(FRAMEBUFFER);
	if (!quad)
	{
		return sharpTex;
	}

	const unsigned int blurW = (m_ScreenW > 1) ? m_ScreenW / 2 : 1;
	const unsigned int blurH = (m_ScreenH > 1) ? m_ScreenH / 2 : 1;

	GLCall(glDisable(GL_DEPTH_TEST));

	m_BlurFBOA->Bind();
	GLCall(glViewport(0, 0, blurW, blurH));
	GLCall(glClear(GL_COLOR_BUFFER_BIT));

	m_BlurShader->Bind();
	m_BlurShader->SetUniform1i("u_Texture", 0);
	m_BlurShader->SetUniform2f("u_TexelSize", 1.0f / (float)m_ScreenW, 1.0f / (float)m_ScreenH);
	m_BlurShader->SetUniform2f("u_Direction", 1.0f, 0.0f);

	GLCall(glActiveTexture(GL_TEXTURE0));
	GLCall(glBindTexture(GL_TEXTURE_2D, sharpTex));

	renderer.Draw(
		*quad->m_VAO,
		*quad->TRI_IBO,
		*m_BlurShader,
		GL_TRIANGLES
	);

	m_BlurFBOB->Bind();
	GLCall(glViewport(0, 0, blurW, blurH));
	GLCall(glClear(GL_COLOR_BUFFER_BIT));

	m_BlurShader->Bind();
	m_BlurShader->SetUniform1i("u_Texture", 0);
	m_BlurShader->SetUniform2f("u_TexelSize", 1.0f / (float)blurW, 1.0f / (float)blurH);
	m_BlurShader->SetUniform2f("u_Direction", 0.0f, 1.0f);

	GLCall(glActiveTexture(GL_TEXTURE0));
	GLCall(glBindTexture(GL_TEXTURE_2D, m_BlurFBOA->GetColorTextureID()));

	renderer.Draw(
		*quad->m_VAO,
		*quad->TRI_IBO,
		*m_BlurShader,
		GL_TRIANGLES
	);

	m_BlurFBOB->Unbind();
	return m_BlurFBOB->GetColorTextureID();
}

void RenderPipeline::RenderCompositePass(ModelManager* MODEL_MNGR, GLuint sharpTex, GLuint blurTex)
{
	Model* quad = MODEL_MNGR->UseModel(FRAMEBUFFER);
	if (!quad)
	{
		return;
	}

	GLCall(glViewport(0, 0, m_ScreenW, m_ScreenH));
	GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	GLCall(glDisable(GL_DEPTH_TEST));

	m_FrameBufferShader->Bind();

	m_FrameBufferShader->SetUniform1f("u_Exposure", m_Exposure);
	m_FrameBufferShader->SetUniform1f("u_Contrast", m_Contrast);
	m_FrameBufferShader->SetUniform1f("u_Saturation", m_Saturation);
	m_FrameBufferShader->SetUniform1f("u_Lift", m_Lift);
	m_FrameBufferShader->SetUniform1f("u_Gamma", m_Gamma);
	m_FrameBufferShader->SetUniform1f("u_Gain", m_Gain);

	m_FrameBufferShader->SetUniform1i("u_Texture", 0);
	m_FrameBufferShader->SetUniform1i("u_BlurTexture", 1);
	m_FrameBufferShader->SetUniform1i("u_DepthTexture", 2);

	m_FrameBufferShader->SetUniform1i("u_DOFEnabled", m_DOFEnabled ? 1 : 0);
	m_FrameBufferShader->SetUniform1f("u_Near", m_NearPlane);
	m_FrameBufferShader->SetUniform1f("u_Far", m_FarPlane);
	m_FrameBufferShader->SetUniform1f("u_FocusDistance", m_FocusDistance);
	m_FrameBufferShader->SetUniform1f("u_FocusRange", m_FocusRange);
	m_FrameBufferShader->SetUniform1f("u_DOFStrength", m_DOFStrength);

	GLCall(glActiveTexture(GL_TEXTURE0));
	GLCall(glBindTexture(GL_TEXTURE_2D, sharpTex));

	GLCall(glActiveTexture(GL_TEXTURE1));
	GLCall(glBindTexture(GL_TEXTURE_2D, blurTex));

	GLCall(glActiveTexture(GL_TEXTURE2));
	GLCall(glBindTexture(GL_TEXTURE_2D, m_mainFBO->GetDepthTextureID()));

	renderer.Draw(
		*quad->m_VAO,
		*quad->TRI_IBO,
		*m_FrameBufferShader,
		GL_TRIANGLES
	);

	m_FrameBufferShader->Unbind();
}

void RenderPipeline::RenderPlacementRadiusSpheres(EntityManager& ENT_MNGR, ModelManager* MODEL_MNGR)
{
	if (!m_PlacementPreviewActive) return;
	if (ENT_MNGR.To_Place_VEC.empty()) return;


	Model* sphereModel = MODEL_MNGR->UseModel(UNIT_SPHERE);
	if (!sphereModel) return;

	m_PlacementSphereShader->Bind();

	GLCall(glEnable(GL_BLEND));
	GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
	GLCall(glEnable(GL_DEPTH_TEST));
	GLCall(glDepthMask(GL_FALSE));
	GLCall(glDisable(GL_CULL_FACE));

	for (Building& preview : ENT_MNGR.To_Place_VEC)
	{
		const float radius = preview.E_sphere_placement_points_radius;
		if (radius <= 0.0f) continue;

		glm::vec3 spherePos = glm::vec3(preview.E_model_matrix[3]);

		glm::mat4 sphereModelMatrix =
			glm::translate(glm::mat4(1.0f), spherePos) *
			glm::scale(glm::mat4(1.0f), glm::vec3(radius));

		glm::mat4 sphereMVP = m_CameraProj * m_CameraView * sphereModelMatrix;

		m_PlacementSphereShader->SetUniformMat4f("u_MVP", sphereMVP);


		if (m_PlacementPreviewValid)
		{
			m_PlacementSphereShader->SetUniform4f("u_Color", 0.20f, 0.45f, 1.00f, 0.16f);
		}
		else
		{
			m_PlacementSphereShader->SetUniform4f("u_Color", 1.00f, 0.20f, 0.20f, 0.16f);
		}

		renderer.Draw(
			*sphereModel->m_VAO,
			*sphereModel->TRI_IBO,
			*m_PlacementSphereShader,
			GL_TRIANGLES
		);
	}

	GLCall(glEnable(GL_CULL_FACE));
	GLCall(glDepthMask(GL_TRUE));
	GLCall(glDisable(GL_BLEND));
}

void RenderPipeline::Resize(unsigned int width, unsigned int height)
{
	if (width == 0 || height == 0) return;
	if (width == m_ScreenW && height == m_ScreenH) return;

	m_ScreenW = width;
	m_ScreenH = height;

	if (m_mainFBO)
		m_mainFBO->Resize(width, height);

	unsigned int blurW = (width > 1) ? width / 2 : 1;
	unsigned int blurH = (height > 1) ? height / 2 : 1;

	if (m_BlurFBOA)
		m_BlurFBOA->Resize(blurW, blurH);

	if (m_BlurFBOB)
		m_BlurFBOB->Resize(blurW, blurH);
}

void RenderPipeline::SetCameraMatrices(const glm::mat4& view,
	const glm::mat4& proj,
	const glm::mat4& shadowFitProj)
{
	m_CameraView = view;
	m_CameraProj = proj;
	m_ShadowFitProj = shadowFitProj;
}

void RenderPipeline::SetPlacementPreviewState(bool active, bool valid)
{
	m_PlacementPreviewActive = active;
	m_PlacementPreviewValid = valid;
}


/*
  Omitted from this excerpt:
  - full constructor/destructor resource setup
  - full ImGui label overlay method
  - post-process parameter setters
  - other project-specific debug helpers
*/
