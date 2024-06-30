///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ============
// manage the preparing and rendering of 3D scenes - textures, materials, lighting
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  EDITOR: Jacob Hasbrook - SNHU Student
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
//  EDITED June 15th 2024
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>
#include <GLFW/glfw3.h>

#define VERTEX_SHADER_PATH "path/to/vertex_shader.vs"
constexpr auto FRAGMENT_SHADER_PATH = "path/to/fragment_shader.fs";

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
	constexpr int TOTAL_LIGHTS = 6; // Updated to include new lights
	const char* g_ShadowMapName = "shadowMap";
}

/***********************************************************
 *  SceneManager()
 *
 *  Constructor - because objects don't construct themselves.
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager, unsigned int screenWidth, unsigned int screenHeight, GLuint shaderProgramID)
	: m_ScreenWidth(screenWidth), m_ScreenHeight(screenHeight), m_ShaderProgramID(shaderProgramID)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
	m_loadedTextures = 0;

	// Shadow map setup
	const GLuint SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
	glGenFramebuffers(1, &depthMapFBO);

	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/***********************************************************
 *  ~SceneManager()
 *
 *  Destructor - because memory leaks are bad.
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  Loads textures from files because plain colors are boring.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// Flip the image, because it was upside down. Thanks, stbi.
	stbi_set_flip_vertically_on_load(true);

	// Load the image, because blank textures are no fun.
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// Check if image loading was a success. If not, we'll just cry in a corner.
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// Because texture wrapping is a thing.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// Because blurry textures are just ugly.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Determine the format and load the image.
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			return false;
		}

		// Generate mipmaps, because they're the cool thing to do.
		glGenerateMipmap(GL_TEXTURE_2D);

		// Free up the image memory.
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Store the texture with a tag, like a librarian with a fetish for textures.
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  Binds loaded textures, so OpenGL knows what's up.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  Frees up texture memory, because we care about memory leaks.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glDeleteTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  Gets the ID of a texture by tag. It's like a treasure hunt.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  Finds the slot index for a texture by tag. Slots are important.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  DefineObjectMaterials()
 *
 *  Defines materials, because objects need to look fancy.
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
	OBJECT_MATERIAL defaultMaterial;
	defaultMaterial.tag = "default";
	defaultMaterial.ambientColor = glm::vec3(1.0f, 1.0f, 1.0f);
	defaultMaterial.ambientStrength = 0.1f;
	defaultMaterial.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
	defaultMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	defaultMaterial.shininess = 32.0f;
	defaultMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(defaultMaterial);

	OBJECT_MATERIAL glowingOrangeMaterial;
	glowingOrangeMaterial.tag = "glowing_orange";
	glowingOrangeMaterial.ambientColor = glm::vec3(1.0f, 0.5f, 0.0f);
	glowingOrangeMaterial.ambientStrength = 0.5f;
	glowingOrangeMaterial.diffuseColor = glm::vec3(1.0f, 0.5f, 0.0f);
	glowingOrangeMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	glowingOrangeMaterial.shininess = 32.0f;
	glowingOrangeMaterial.emissiveColor = glm::vec3(1.0f, 0.5f, 0.0f);

	m_objectMaterials.push_back(glowingOrangeMaterial);

	OBJECT_MATERIAL floorMaterial;
	floorMaterial.tag = "floor";
	floorMaterial.ambientColor = glm::vec3(117 / 255.0f, 64 / 255.0f, 28 / 255.0f);
	floorMaterial.ambientStrength = 0.1f;
	floorMaterial.diffuseColor = glm::vec3(117 / 255.0f, 64 / 255.0f, 28 / 255.0f);
	floorMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	floorMaterial.shininess = 32.0f;
	floorMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(floorMaterial);

	OBJECT_MATERIAL wallMaterial;
	wallMaterial.tag = "wall";
	wallMaterial.ambientColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	wallMaterial.ambientStrength = 0.1f;
	wallMaterial.diffuseColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	wallMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	wallMaterial.shininess = 32.0f;
	wallMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(wallMaterial);

	OBJECT_MATERIAL ceilingMaterial;
	ceilingMaterial.tag = "ceiling";
	ceilingMaterial.ambientColor = glm::vec3(90 / 255.0f, 90 / 255.0f, 90 / 255.0f);
	ceilingMaterial.ambientStrength = 0.1f;
	ceilingMaterial.diffuseColor = glm::vec3(90 / 255.0f, 90 / 255.0f, 90 / 255.0f);
	ceilingMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	ceilingMaterial.shininess = 32.0f;
	ceilingMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(ceilingMaterial);

	OBJECT_MATERIAL glowingBeamMaterial;
	glowingBeamMaterial.tag = "glowing_beam";
	glowingBeamMaterial.ambientColor = glm::vec3(1.0f, 0.5f, 0.0f);
	glowingBeamMaterial.ambientStrength = 0.1f;
	glowingBeamMaterial.diffuseColor = glm::vec3(1.0f, 0.5f, 0.0f);
	glowingBeamMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	glowingBeamMaterial.shininess = 32.0f;
	glowingBeamMaterial.emissiveColor = glm::vec3(1.0f, 0.5f, 0.0f);

	m_objectMaterials.push_back(glowingBeamMaterial);

	OBJECT_MATERIAL sofaMaterial;
	sofaMaterial.tag = "sofa";
	sofaMaterial.ambientColor = glm::vec3(90 / 255.0f, 90 / 255.0f, 90 / 255.0f);
	sofaMaterial.ambientStrength = 0.1f;
	sofaMaterial.diffuseColor = glm::vec3(90 / 255.0f, 90 / 255.0f, 90 / 255.0f);
	sofaMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	sofaMaterial.shininess = 32.0f;
	sofaMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(sofaMaterial);

	OBJECT_MATERIAL sofaFeetMaterial;
	sofaFeetMaterial.tag = "sofa_feet";
	sofaFeetMaterial.ambientColor = glm::vec3(60 / 255.0f, 60 / 255.0f, 60 / 255.0f);
	sofaFeetMaterial.ambientStrength = 0.1f;
	sofaFeetMaterial.diffuseColor = glm::vec3(60 / 255.0f, 60 / 255.0f, 60 / 255.0f);
	sofaFeetMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	sofaFeetMaterial.shininess = 32.0f;
	sofaFeetMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(sofaFeetMaterial);

	OBJECT_MATERIAL rugMaterial;
	rugMaterial.tag = "rug";
	rugMaterial.ambientColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	rugMaterial.ambientStrength = 0.1f;
	rugMaterial.diffuseColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	rugMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	rugMaterial.shininess = 32.0f;
	rugMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);
	rugMaterial.tint = glm::vec3(0.9f, 0.9f, 0.9f);

	m_objectMaterials.push_back(rugMaterial);

	OBJECT_MATERIAL drawerMaterial;
	drawerMaterial.tag = "drawer";
	drawerMaterial.ambientColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	drawerMaterial.ambientStrength = 0.1f;
	drawerMaterial.diffuseColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	drawerMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	drawerMaterial.shininess = 32.0f;
	drawerMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(drawerMaterial);

	OBJECT_MATERIAL spaceHeaterMaterial;
	spaceHeaterMaterial.tag = "space_heater";
	spaceHeaterMaterial.ambientColor = glm::vec3(35 / 255.0f, 35 / 255.0f, 35 / 255.0f);
	spaceHeaterMaterial.ambientStrength = 0.1f;
	spaceHeaterMaterial.diffuseColor = glm::vec3(35 / 255.0f, 35 / 255.0f, 35 / 255.0f);
	spaceHeaterMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	spaceHeaterMaterial.shininess = 32.0f;
	spaceHeaterMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(spaceHeaterMaterial);

	OBJECT_MATERIAL windowGlassMaterial;
	windowGlassMaterial.tag = "window_glass";
	windowGlassMaterial.ambientColor = glm::vec3(137 / 255.0f, 196 / 255.0f, 244 / 255.0f);
	windowGlassMaterial.ambientStrength = 0.1f;
	windowGlassMaterial.diffuseColor = glm::vec3(137 / 255.0f, 196 / 255.0f, 244 / 255.0f);
	windowGlassMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	windowGlassMaterial.shininess = 32.0f;
	windowGlassMaterial.emissiveColor = glm::vec3(0.7f, 0.7f, 0.7f);

	m_objectMaterials.push_back(windowGlassMaterial);


	OBJECT_MATERIAL windowMaterial;
	windowMaterial.tag = "window";
	windowMaterial.ambientColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	windowMaterial.ambientStrength = 0.1f;
	windowMaterial.diffuseColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	windowMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	windowMaterial.shininess = 0.0f;
	windowMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(windowMaterial);

	OBJECT_MATERIAL beamMaterial;
	beamMaterial.tag = "beam";
	beamMaterial.ambientColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	beamMaterial.ambientStrength = 0.1f;
	beamMaterial.diffuseColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	beamMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	beamMaterial.shininess = 32.0f;
	beamMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(beamMaterial);

	OBJECT_MATERIAL lampMaterial;
	lampMaterial.tag = "lamp";
	lampMaterial.ambientColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	lampMaterial.ambientStrength = 0.1f;
	lampMaterial.diffuseColor = glm::vec3(180 / 255.0f, 180 / 255.0f, 180 / 255.0f);
	lampMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	lampMaterial.shininess = 100.0f;
	lampMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(lampMaterial);

	OBJECT_MATERIAL lamplitMaterial;
	lamplitMaterial.tag = "lamp_light";
	lamplitMaterial.ambientColor = glm::vec3(1.0f, 0.6f, 0.4f); // Pinkish-orange ambient color
	lamplitMaterial.ambientStrength = 0.1f;
	lamplitMaterial.diffuseColor = glm::vec3(1.0f, 0.6f, 0.4f); // Pinkish-orange diffuse color
	lamplitMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	lamplitMaterial.shininess = 10.0f;
	lamplitMaterial.emissiveColor = glm::vec3(1.0f, 0.6f, 0.4f); // Pinkish-orange emissive color

	m_objectMaterials.push_back(lamplitMaterial);

	// Define frame material
	OBJECT_MATERIAL frameMaterial;
	frameMaterial.tag = "frame_material";
	frameMaterial.ambientColor = glm::vec3(30 / 255.0f, 30 / 255.0f, 30 / 255.0f); // Dark color for the frame
	frameMaterial.ambientStrength = 0.1f;
	frameMaterial.diffuseColor = glm::vec3(30 / 255.0f, 30 / 255.0f, 30 / 255.0f);
	frameMaterial.specularColor = glm::vec3(0.5f, 0.5f, 0.5f);
	frameMaterial.shininess = 0.0f;
	frameMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);
	m_objectMaterials.push_back(frameMaterial);

	// Define canvas material
	OBJECT_MATERIAL canvasMaterial;
	canvasMaterial.tag = "canvas_material";
	canvasMaterial.ambientColor = glm::vec3(1.0f, 1.0f, 1.0f); // White color for the canvas
	canvasMaterial.ambientStrength = 0.1f;
	canvasMaterial.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
	canvasMaterial.specularColor = glm::vec3(0.5f, 0.5f, 0.5f);
	canvasMaterial.shininess = 0.0f;
	canvasMaterial.emissiveColor = glm::vec3(0.01f, 0.01f, 0.01f);
	m_objectMaterials.push_back(canvasMaterial);

	OBJECT_MATERIAL potMaterial;
	potMaterial.tag = "pot_material";
	potMaterial.ambientColor = glm::vec3(0.5f, 0.5f, 0.5f); // Grey color
	potMaterial.ambientStrength = 0.1f;
	potMaterial.diffuseColor = glm::vec3(0.5f, 0.5f, 0.5f); // Grey color
	potMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f); // White specular for high shine
	potMaterial.shininess = 64.0f; // Higher shininess for more gloss
	potMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);


	m_objectMaterials.push_back(potMaterial);

	OBJECT_MATERIAL stemMaterial;
	stemMaterial.tag = "stem_material";
	stemMaterial.ambientColor = glm::vec3(0.13f, 0.55f, 0.13f);
	stemMaterial.ambientStrength = 0.1f;
	stemMaterial.diffuseColor = glm::vec3(0.13f, 0.55f, 0.13f);
	stemMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
	stemMaterial.shininess = 16.0f;
	stemMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(stemMaterial);

	OBJECT_MATERIAL leafMaterial;
	leafMaterial.tag = "leaf_material";
	leafMaterial.ambientColor = glm::vec3(0.0f, 1.0f, 0.0f);
	leafMaterial.ambientStrength = 0.1f;
	leafMaterial.diffuseColor = glm::vec3(0.0f, 1.0f, 0.0f);
	leafMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
	leafMaterial.shininess = 16.0f;
	leafMaterial.emissiveColor = glm::vec3(0.0f, 0.0f, 0.0f);

	m_objectMaterials.push_back(leafMaterial);

}

/***********************************************************
 *  FindMaterial()
 *
 *  Finds materials by tag. It's like a phonebook for objects.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	size_t index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.ambientColor = m_objectMaterials[index].ambientColor;
			material.ambientStrength = m_objectMaterials[index].ambientStrength;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
			material.emissiveColor = m_objectMaterials[index].emissiveColor; // Add emissive color to the return value
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  Applies transformation values. Because we don't live in a 2D world.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	scale = glm::scale(scaleXYZ);
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationZ * rotationY * rotationX * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  Sets the shader color because grayscale is so last century.
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  Tells the shader which texture to use. Because blank surfaces are boring.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  Sets the texture UV scale, because one size does not fit all.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  Passes material values into the shader. So shiny.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);

		if (bReturn)
		{
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
			m_pShaderManager->setVec3Value("material.emissiveColor", material.emissiveColor); // Set emissive color

			if (materialTag == "floor" || materialTag == "wall" || materialTag == "ceiling" || materialTag == "sofa" || materialTag == "rug")
			{
				m_pShaderManager->setIntValue(g_UseTextureName, true);
			}
			else
			{
				m_pShaderManager->setIntValue(g_UseTextureName, false);
			}
		}
	}
	else
	{
		std::cout << "No materials defined." << std::endl;
	}
}

/***********************************************************
 *  SetShaderLights()
 *
 *  Lights up the scene. Because darkness is not fun.
 ***********************************************************/
void SceneManager::SetShaderLights()
{
	m_pShaderManager->setBoolValue(g_UseLightingName, true);
	m_pShaderManager->setVec3Value("globalAmbientColor", glm::vec3(0.05f, 0.05f, 0.05f));

	if (m_pShaderManager != NULL)
	{
		// Set up a spotlight to simulate sunlight through the window
		m_pShaderManager->setVec3Value("spotLight.position", glm::vec3(0.0f, 14.0f, -9.85f));
		m_pShaderManager->setVec3Value("spotLight.direction", glm::vec3(0.0f, -0.5f, 0.0f));
		m_pShaderManager->setFloatValue("spotLight.cutOff", glm::cos(glm::radians(85.0f)));
		m_pShaderManager->setFloatValue("spotLight.outerCutOff", glm::cos(glm::radians(90.0f)));
		m_pShaderManager->setVec3Value("spotLight.ambient", glm::vec3(0.7f, 0.55f, 0.4f));
		m_pShaderManager->setVec3Value("spotLight.diffuse", glm::vec3(1.0f, 0.9f, 0.7f));
		m_pShaderManager->setVec3Value("spotLight.specular", glm::vec3(1.0f, 0.9f, 0.8f));
		m_pShaderManager->setFloatValue("spotLight.constant", 1.0f);
		m_pShaderManager->setFloatValue("spotLight.linear", 0.05f);
		m_pShaderManager->setFloatValue("spotLight.quadratic", 0.0007f);
		m_pShaderManager->setBoolValue("spotLight.bActive", true);

		// Set other lights (point lights)
		for (int i = 0; i < TOTAL_LIGHTS; ++i)
		{
			std::string lightBase = "pointLights[" + std::to_string(i) + "].";

			if (i == TOTAL_LIGHTS - 1)
			{
				glm::vec3 pos = glm::vec3(0.0f, 8.5f, -3.0f);
				glm::vec3 ambient = glm::vec3(0.1f, 0.05f, 0.0f);
				glm::vec3 diffuse = glm::vec3(1.0f, 0.5f, 0.0f);
				glm::vec3 specular = glm::vec3(1.0f, 0.5f, 0.0f);
				float constant = 1.0f;
				float linear = 0.07f;
				float quadratic = 0.017f;

				m_pShaderManager->setVec3Value((lightBase + "position").c_str(), pos);
				m_pShaderManager->setVec3Value((lightBase + "ambient").c_str(), ambient);
				m_pShaderManager->setVec3Value((lightBase + "diffuse").c_str(), diffuse);
				m_pShaderManager->setVec3Value((lightBase + "specular").c_str(), specular);
				m_pShaderManager->setFloatValue((lightBase + "constant").c_str(), constant);
				m_pShaderManager->setFloatValue((lightBase + "linear").c_str(), linear);
				m_pShaderManager->setFloatValue((lightBase + "quadratic").c_str(), quadratic);
				m_pShaderManager->setBoolValue((lightBase + "bActive").c_str(), true);
			}
			else if (i == TOTAL_LIGHTS - 2) 
			{
				// Set up a super bright point light at the origin with an orange glow
				m_pShaderManager->setVec3Value("pointLights[0].position", glm::vec3(0.0f, 0.0f, 0.0f));
				m_pShaderManager->setVec3Value("pointLights[0].ambient", glm::vec3(1.0f, 0.5f, 0.0f)); // Bright orange ambient light
				m_pShaderManager->setVec3Value("pointLights[0].diffuse", glm::vec3(1.0f, 0.5f, 0.0f)); // Bright orange diffuse light
				m_pShaderManager->setVec3Value("pointLights[0].specular", glm::vec3(1.0f, 0.5f, 0.0f)); // Bright orange specular light
				m_pShaderManager->setFloatValue("pointLights[0].constant", 1.0f);
				m_pShaderManager->setFloatValue("pointLights[0].linear", 0.09f); // Lower attenuation
				m_pShaderManager->setFloatValue("pointLights[0].quadratic", 0.032f); // Lower attenuation
				m_pShaderManager->setBoolValue("pointLights[0].bActive", true);
			}
			else if (i == TOTAL_LIGHTS - 3)
			{
				// Set up a super bright point light at the origin with an orange glow
				m_pShaderManager->setVec3Value("pointLights[2].position", glm::vec3(0.0f, 0.0f, 0.0f));
				m_pShaderManager->setVec3Value("pointLights[2].ambient", glm::vec3(1.0f, 0.5f, 0.0f)); // Bright orange ambient light
				m_pShaderManager->setVec3Value("pointLights[2].diffuse", glm::vec3(1.0f, 0.5f, 0.0f)); // Bright orange diffuse light
				m_pShaderManager->setVec3Value("pointLights[2].specular", glm::vec3(1.0f, 0.5f, 0.0f)); // Bright orange specular light
				m_pShaderManager->setFloatValue("pointLights[2].constant", 1.0f);
				m_pShaderManager->setFloatValue("pointLights[2].linear", 0.09f); // Lower attenuation
				m_pShaderManager->setFloatValue("pointLights[2].quadratic", 0.032f); // Lower attenuation
				m_pShaderManager->setBoolValue("pointLights[2].bActive", true);
			}
			else
			{
				m_pShaderManager->setVec3Value((lightBase + "position").c_str(), glm::vec3(0.0f, 0.0f, 0.0f));
				m_pShaderManager->setVec3Value((lightBase + "ambient").c_str(), glm::vec3(0.05f, 0.05f, 0.05f));
				m_pShaderManager->setVec3Value((lightBase + "diffuse").c_str(), glm::vec3(0.0f, 0.0f, 0.0f));
				m_pShaderManager->setVec3Value((lightBase + "specular").c_str(), glm::vec3(0.0f, 0.0f, 0.0f));
				m_pShaderManager->setFloatValue((lightBase + "constant").c_str(), 1.0f);
				m_pShaderManager->setFloatValue((lightBase + "linear").c_str(), 0.09f);
				m_pShaderManager->setFloatValue((lightBase + "quadratic").c_str(), 0.032f);
				m_pShaderManager->setBoolValue((lightBase + "bActive").c_str(), false);
			}
		}

		// Disable the directional light if it's not needed anymore
		m_pShaderManager->setBoolValue("directionalLight.bActive", false);
	}
}


/***********************************************************
 *  RenderSceneFromLightPerspective()
 *
 *  Renders the scene from the light's perspective. Lights need their own POV.
 ***********************************************************/
void SceneManager::RenderSceneFromLightPerspective()
{
	glm::mat4 lightProjection, lightView;
	glm::mat4 lightSpaceMatrix;
	float near_plane = 1.0f, far_plane = 50.0f;
	lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
	lightView = glm::lookAt(glm::vec3(0.0f, 14.0f, -9.85f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	lightSpaceMatrix = lightProjection * lightView;

	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT);

	m_pShaderManager->use();
	m_pShaderManager->setMat4Value("lightSpaceMatrix", lightSpaceMatrix);

	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/***********************************************************
 *  RenderSceneWithShadows()
 *
 *  Renders the scene with shadows, because flat lighting is dull.
 ***********************************************************/
void SceneManager::RenderSceneWithShadows()
{
	RenderSceneFromLightPerspective();

	glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_pShaderManager->use();

	SetShaderLights();

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	m_pShaderManager->setSampler2DValue(g_ShadowMapName, 1);

	BindGLTextures();

	RenderScene();
}

/***********************************************************
 *  SetShaderEmissive()
 *
 *  Sets the emissive color, because some things just need to glow.
 ***********************************************************/
void SceneManager::SetShaderEmissive(
	float redColorValue,
	float greenColorValue,
	float blueColorValue)
{
	glm::vec3 emissiveColor;

	emissiveColor.r = redColorValue;
	emissiveColor.g = greenColorValue;
	emissiveColor.b = blueColorValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec3Value("material.emissiveColor", emissiveColor);
	}
}

/***********************************************************
 *  SetUseLighting()
 *
 *  Turns lighting on or off. Because sometimes, even virtual worlds need a light switch.
 ***********************************************************/
void SceneManager::SetUseLighting(bool useLighting) {
	glUseProgram(m_ShaderProgramID);
	glUniform1i(glGetUniformLocation(m_ShaderProgramID, "bUseLighting"), useLighting ? GL_TRUE : GL_FALSE);
}

/***********************************************************
 *  SetTextureOffset()
 *
 *  Applies texture offsets. Because textures need to move around too.
 ***********************************************************/
void SceneManager::SetTextureOffset(float offsetX, float offsetY) {
	glUseProgram(m_ShaderProgramID);
	glUniform2f(glGetUniformLocation(m_ShaderProgramID, "textureOffset"), offsetX, offsetY);
}


/***********************************************************
 *  LoadSceneTextures()
 *
 *  Loads the textures for the 3D scene. Because 3D without textures is just sad.
 ***********************************************************/
void SceneManager::LoadSceneTextures()
{
	// Load textures here
	CreateGLTexture("textures/floor.jpg", "texture1");
	CreateGLTexture("textures/couchfabric.jpg", "texture2");
	CreateGLTexture("textures/sidewall.jpg", "texture3");
	CreateGLTexture("textures/roof.jpg", "texture4");
	CreateGLTexture("textures/painting1.png", "texture5");
	CreateGLTexture("textures/roof.jpg", "texture4");
	CreateGLTexture("textures/desktop.png", "texture6");
	CreateGLTexture("textures/keyboard.png", "texture7");
	CreateGLTexture("textures/monitor.png", "texture8");
	CreateGLTexture("textures/drawer.png", "texture9");


	BindGLTextures();
}

/***********************************************************
 *  PrepareScene()
 *
 *  Prepares the 3D scene by loading shapes and textures.
 ***********************************************************/
void SceneManager::PrepareScene()
{
	LoadSceneTextures();

	DefineObjectMaterials();

	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadTaperedCylinderMesh();
	m_basicMeshes->LoadTorusMesh();
	m_basicMeshes->LoadSphereMesh();
}


/***********************************************************
 *  RenderScene()
 *
 *  Renders the 3D scene. Because why else are we here?
 ***********************************************************/
void SceneManager::RenderScene()
{

	GLuint ShaderProgramID = m_ShaderProgramID;

	// Bind the depth map texture for shadow mapping
	//Ignore this, still working on bias
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	m_pShaderManager->setSampler2DValue("shadowMap", 1);

	// Bind the other textures
	BindGLTextures();

	// Set up the lights
	SetShaderLights();

	// Update the positions of the moving lights
	float timeValue = glfwGetTime();
	float t = (sin(timeValue) + 1.0f) / 2.0f; // Normalize sine wave to range [0, 1]

	glm::vec3 startPoint1 = glm::vec3(0.0f, 15.5f, -8.9f);
	glm::vec3 endPoint1 = glm::vec3(14.4f, 13.0f, -8.9f);

	glm::vec3 lightPos1 = glm::mix(startPoint1, endPoint1, t);
	glm::vec3 lightPos2 = glm::mix(startPoint1, endPoint1, 1.0f - t);

	m_pShaderManager->setVec3Value("pointLights[0].position", lightPos1);
	m_pShaderManager->setVec3Value("pointLights[1].position", lightPos2);

	glm::vec3 startPoint2 = glm::vec3(0.0f, 15.5f, -8.9f);
	glm::vec3 endPoint2 = glm::vec3(-14.4f, 13.0f, -8.9f);

	glm::vec3 lightPos2_1 = glm::mix(startPoint2, endPoint2, t);
	glm::vec3 lightPos2_2 = glm::mix(startPoint2, endPoint2, 1.0f - t);

	m_pShaderManager->setVec3Value("pointLights[2].position", lightPos2_1);
	m_pShaderManager->setVec3Value("pointLights[3].position", lightPos2_2);


	// Declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/*** Draw the Attic Planes ***/
	// Draw the floor plane
	SetShaderMaterial("floor");
	scaleXYZ = glm::vec3(15.0f, 0.1f, 12.0f);  // Make the floor thin
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.0f, 0.0f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(4.0f, 4.0f);
	m_basicMeshes->DrawPlaneMesh();

	// Draw the back wall
	SetShaderMaterial("wall");
	scaleXYZ = glm::vec3(15.0f, 10.0f, 15.0f);  // Make the wall thin
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.0f, 15.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture3");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(0.75f, 0.75f);
	m_basicMeshes->DrawPlaneMesh();

	// Draw the left wall
	SetShaderMaterial("wall");
	scaleXYZ = glm::vec3(12.0f, 10.0f, 3.5f);  // Make the wall thin
	XrotationDegrees = 0.0f;
	YrotationDegrees = 90.0f;
	ZrotationDegrees = 90.0f;
	positionXYZ = glm::vec3(-15.0f, 3.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture3");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawPlaneMesh();

	// Draw the right wall
	positionXYZ = glm::vec3(15.0f, 3.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture3");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawPlaneMesh();

	// Draw the slanted ceiling planes
	SetShaderMaterial("ceiling");
	scaleXYZ = glm::vec3(12.0f, 20.0f, 10.70f);  // Adjust depth to be thin
	XrotationDegrees = -45.0f;
	positionXYZ = glm::vec3(7.4f, 14.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture4");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawPlaneMesh();

	XrotationDegrees = 45.0f;
	positionXYZ = glm::vec3(-7.4f, 14.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture4");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawPlaneMesh();

	// Furthest beam - Glowing yellow-orange light
	SetShaderMaterial("glowing_beam");
	scaleXYZ = glm::vec3(21.0f, 0.5f, 0.5f);
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 135.0f;
	positionXYZ = glm::vec3(7.4f, 14.5f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = -90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -135.0f;
	positionXYZ = glm::vec3(-7.4, 14.5f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	// Draw the structure beams
	SetShaderMaterial("beam");
	scaleXYZ = glm::vec3(21.0f, 0.5f, 0.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 90.0f;
	ZrotationDegrees = 90.0f;
	positionXYZ = glm::vec3(0.0f, 21.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 135.0f;
	positionXYZ = glm::vec3(7.4f, 14.5f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 135.0f;
	positionXYZ = glm::vec3(7.4f, 14.5f, -6.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 135.0f;
	positionXYZ = glm::vec3(7.4f, 14.5f, -2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 135.0f;
	positionXYZ = glm::vec3(7.4f, 14.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 135.0f;
	positionXYZ = glm::vec3(7.4f, 14.5f, 6.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 135.0f;
	positionXYZ = glm::vec3(7.4f, 14.5f, 10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = -90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -135.0f;
	positionXYZ = glm::vec3(-7.4, 14.5f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = -90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -135.0f;
	positionXYZ = glm::vec3(-7.4, 14.5f, -6.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = -90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -135.0f;
	positionXYZ = glm::vec3(-7.4, 14.5f, -2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = -90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -135.0f;
	positionXYZ = glm::vec3(-7.4, 14.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = -90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -135.0f;
	positionXYZ = glm::vec3(-7.4, 14.5f, 6.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	XrotationDegrees = -90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -135.0f;
	positionXYZ = glm::vec3(-7.4, 14.5f, 10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture1");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(2.0f, 2.0f);
	m_basicMeshes->DrawBoxMesh();

	/*** Draw the Sofa ***/
	SetShaderMaterial("sofa");
	// Draw the main body of the sofa
	scaleXYZ = glm::vec3(5.0f, 2.0f, 0.9f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-12.25f, 1.25f, 5.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture2");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(0.75f, 0.75f);
	m_basicMeshes->DrawBoxMesh();

	// Draw the main body of the sofa
	scaleXYZ = glm::vec3(5.0f, 2.0f, 0.9f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-12.25f, 1.25f, -3.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture2");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(0.75f, 0.75f);
	m_basicMeshes->DrawBoxMesh();

	// Draw the main body of the sofa
	scaleXYZ = glm::vec3(4.95f, 0.5f, 8.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-12.25f, 0.5f, 1.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture2");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(0.75f, 0.75f);
	m_basicMeshes->DrawBoxMesh();

	// Draw the sofa cushions
	SetShaderMaterial("sofa");
	scaleXYZ = glm::vec3(3.0f, 0.5f, 3.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -55.0f;
	positionXYZ = glm::vec3(-13.75f, 2.0f, 2.75f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture2");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(0.75f, 0.75f);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.0f, 0.5f, 3.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -55.0f;
	positionXYZ = glm::vec3(-13.75f, 2.0f, -0.75f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture2");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(0.75f, 0.75f);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(5.0f, 0.75f, 6.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-12.25f, 1.25f, 1.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture2");
	// Set the UV scale to repeat the texture
	SetTextureUVScale(0.75f, 0.75f);
	m_basicMeshes->DrawBoxMesh();

	// Draw the sofa feet
	SetShaderMaterial("sofa_feet");
	scaleXYZ = glm::vec3(0.1f, 0.5f, 0.1f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-14.25f, 0.0f, 5.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// No texture
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.1f, 0.75f, 0.1f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-10.25f, 0.0f, 5.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// No texture
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.1f, 0.5f, 0.1f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-14.25f, 0.0f, -3.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// No texture
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.1f, 0.75f, 0.1f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-10.25f, 0.0f, -3.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// No texture
	m_basicMeshes->DrawCylinderMesh();

	/*** Draw the Rug ***/
	SetShaderMaterial("rug");
	scaleXYZ = glm::vec3(16.0f, 0.2f, 16.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-1.0f, 0.15f, 4.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetShaderTexture("texture2");
	SetTextureUVScale(0.75f, 0.75f);
	// Set the tint intensity for the rug
	glUniform1f(glGetUniformLocation(ShaderProgramID, "tintIntensity"), 0.8f); // Set intensity to 70%
	m_basicMeshes->DrawBoxMesh();

	// Reset the tint intensity to default (no tint) after drawing the rug
	glUniform1f(glGetUniformLocation(ShaderProgramID, "tintIntensity"), 0.0f);

	SetShaderColor(242 / 255.0, 243 / 255.0, 244 / 255.0, 1.0);
	/*** Draw the Drawer Set ***/
	SetShaderMaterial("drawer");
	// Draw the main body of drawers
	scaleXYZ = glm::vec3(3.5f, 5.0f, 9.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(13.0f, 2.525f, -0.5f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw the main body of drawers
	SetShaderColor(231 / 255.0, 232 / 255.0, 233 / 255.0, 1.0);
	scaleXYZ = glm::vec3(3.25f, 1.25f, 9.25f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(12.8f, 1.05f, -0.6f);
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderTexture("texture9");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw the main body of drawers
	SetShaderColor(231 / 255.0, 232 / 255.0, 233 / 255.0, 1.0);
	scaleXYZ = glm::vec3(3.25f, 1.25f, 9.25f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(12.8f, 4.05f, -0.6f);
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderTexture("texture9");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw the main body of drawers
	SetShaderColor(231 / 255.0, 232 / 255.0, 233 / 255.0, 1.0);
	scaleXYZ = glm::vec3(3.25f, 1.25f, 9.25f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(12.8f, 2.55f, -0.6f);
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderTexture("texture9");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw the main body of drawers

	SetShaderColor(242 / 255.0, 243 / 255.0, 244 / 255.0, 1.0);
	/*** Draw the Drawer Set ***/
	SetShaderMaterial("drawer");
	scaleXYZ = glm::vec3(3.5f, 5.0f, 0.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(13.0f, 2.525f, -9.5f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.45f, 0.5f, 6.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(13.0f, 4.75f, -6.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.45f, 0.05f, 6.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(13.0f, 0.05f, -6.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.45f, 0.05f, 6.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(13.0f, 3.0f, -6.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.45f, 0.05f, 6.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(13.0f, 1.5f, -6.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw computer shelf
	scaleXYZ = glm::vec3(2.75f, 0.75f, 2.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(7.5f, 0.40f, -8.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw computer shelf
	scaleXYZ = glm::vec3(2.75f, 0.5f, 2.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(7.5f, 7.27f, -8.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw computer shelf
	scaleXYZ = glm::vec3(2.75f, 0.15f, 2.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(7.5f, 5.27f, -8.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw computer shelf
	scaleXYZ = glm::vec3(2.75f, 0.15f, 2.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(7.5f, 3.0f, -8.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw computer shelf
	scaleXYZ = glm::vec3(0.25f, 7.5f, 2.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(6.25f, 3.77f, -8.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw computer shelf
	scaleXYZ = glm::vec3(0.25f, 7.5f, 2.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(9.0f, 3.77f, -8.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw computer shelf
	scaleXYZ = glm::vec3(0.25f, 7.5f, 2.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 90.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(7.6f, 3.77f, -8.25f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	/*** Draw Wall boards ***/
	scaleXYZ = glm::vec3(1.0f, 1.0f, 24.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(15.0f, 6.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	/*** Draw Wall boards ***/
	scaleXYZ = glm::vec3(1.0f, 1.0f, 24.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-15.0f, 6.5f, 2.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Set the tint intensity for the rug
	glUniform1f(glGetUniformLocation(ShaderProgramID, "tintIntensity"), 0.8f); // Set intensity to 70%
	m_basicMeshes->DrawBoxMesh();

	// Reset the tint intensity to default (no tint) after drawing the rug
	glUniform1f(glGetUniformLocation(ShaderProgramID, "tintIntensity"), 0.0f);
	// Render the canvas with unlit shader
	SetUseLighting(false);
	SetShaderColor(180 / 255.0, 180 / 255.0, 180 / 255.0, 1.0);
	SetShaderMaterial("window");
	scaleXYZ = glm::vec3(8.0f, 0.75f, 1.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.0f, 5.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(8.0f, 0.75f, 1.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.0f, 15.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(0.75f, 10.75f, 1.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-4.0f, 10.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(0.75f, 10.75f, 1.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(4.0f, 10.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(0.35f, 8.75f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.25f, 10.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(0.35f, 8.75f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-0.25f, 10.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(0.35f, 8.75f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-3.25f, 10.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(0.35f, 8.75f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(3.25f, 10.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.25f, 0.35f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(1.75f, 10.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.25f, 0.35f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-1.75f, 14.25f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.25f, 0.35f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(1.75f, 5.75f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.25f, 0.35f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-1.75f, 5.75f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.25f, 0.35f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(1.75f, 14.25f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(3.25f, 0.35f, 0.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-1.75f, 10.0f, -10.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw the window glass and set it to emit light
	SetShaderMaterial("window_glass");
	SetShaderColor(135 / 255.0, 206 / 255.0, 235 / 255.0, 1.0);
	scaleXYZ = glm::vec3(4.0f, 1.0f, 4.75f);
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.0f, 10.0f, -9.9f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawPlaneMesh();
	// Render the canvas with unlit shader
	SetUseLighting(true);

	// Draw Space Heater
	SetShaderMaterial("space_heater");
	SetShaderColor(30 / 255.0, 30 / 255.0, 30 / 255.0, 1.0);
	scaleXYZ = glm::vec3(2.0f, 4.0f, 1.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(13.75f, 2.01f, 6.1f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	SetShaderMaterial("glowing_orange");
	scaleXYZ = glm::vec3(0.25f, 2.0f, 0.25f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(13.75f, 1.5f, 6.8f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Lamp Feet
	SetShaderMaterial("lamp");
	SetShaderColor(160 / 255.0, 161 / 255.0, 161 / 255.0, 1.0);
	scaleXYZ = glm::vec3(0.1f, 8.0f, 0.1f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(10.0f, 0.0f, -7.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	SetShaderColor(160 / 255.0, 161 / 255.0, 161 / 255.0, 1.0);
	scaleXYZ = glm::vec3(0.9f, 0.1f, 0.9f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(10.0f, 0.0f, -7.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	SetShaderColor(160 / 255.0, 161 / 255.0, 161 / 255.0, 1.0);
	scaleXYZ = glm::vec3(0.1f, 1.0f, 0.1f);
	XrotationDegrees = 45.0f;
	YrotationDegrees = 90.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(10.0f, 8.0f, -7.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	SetShaderColor(160 / 255.0, 161 / 255.0, 161 / 255.0, 1.0);
	scaleXYZ = glm::vec3(0.1f, 1.0f, 0.1f);
	XrotationDegrees = 45.0f;
	YrotationDegrees = -90.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(10.0f, 8.0f, -7.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	SetShaderMaterial("lamp_light");
	SetShaderColor(255 / 255.0, 200 / 255.0, 124 / 255.0, 0.6);
	scaleXYZ = glm::vec3(1.5f, 1.5f, 1.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(10.0f, 8.5f, -7.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawTaperedCylinderMesh();

	SetShaderMaterial("lamp");
	SetShaderColor(160 / 255.0, 161 / 255.0, 161 / 255.0, 1.0);
	scaleXYZ = glm::vec3(0.1f, 7.0f, 0.1f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-12.0f, 0.0f, -7.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	SetShaderColor(160 / 255.0, 161 / 255.0, 161 / 255.0, 1.0);
	scaleXYZ = glm::vec3(0.9f, 0.1f, 0.9f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-12.0f, 0.0f, -7.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();


	SetShaderMaterial("lamp_light");
	SetShaderColor(255 / 255.0, 165 / 255.0, 0 / 255.0, 0.98);
	scaleXYZ = glm::vec3(1.5f, 2.5f, 1.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-12.0f, 6.0f, -7.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawTaperedCylinderMesh();

	// DESK OBJECT //
	SetShaderColor(242 / 255.0, 243 / 255.0, 244 / 255.0, 1.0);
	SetShaderMaterial("drawer");
	scaleXYZ = glm::vec3(0.1f, 4.0f, 0.1f);
	XrotationDegrees = -10.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -10.0f;
	positionXYZ = glm::vec3(-8.0f, 0.0f, -6.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.1f, 4.0f, 0.1f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -10.0f;
	positionXYZ = glm::vec3(-8.0f, 0.0f, -8.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.1f, 4.0f, 0.1f);
	XrotationDegrees = -10.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 10.0f;
	positionXYZ = glm::vec3(2.0f, 0.0f, -6.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.1f, 4.0f, 0.1f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 10.0f;
	positionXYZ = glm::vec3(2.0f, 0.0f, -8.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(14.0f, 0.4f, 4.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-2.0f, 4.0f, -7.9f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Render the canvas with unlit shader
	SetUseLighting(false);
	// DESKTOP OBJECT //
	SetShaderColor(0 / 255.0, 0 / 255.0, 0 / 255.0, 1.0);
	scaleXYZ = glm::vec3(2.0f, 3.5f, 3.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(4.0f, 1.8f, -7.9f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(1.9f, 3.4f, 3.5f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(4.0f, 1.8f, -7.8f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	// Use the texture tag for this object
	SetTextureUVScale(1.0f, 1.0f);
	SetTextureOffset(0.5f, 0.5f); // Move texture to see the effect
	SetShaderTexture("texture6");
	m_basicMeshes->DrawBoxMesh();

	// Frame - Bottom
	SetShaderColor(60 / 255.0, 60 / 255.0, 60 / 255.0, 1.0); // Dark color for the frame
	SetShaderMaterial("frame_material");
	scaleXYZ = glm::vec3(3.7f, 0.2f, 0.2f); // Bottom frame
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-8.0f, 5.5f, -9.8f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Frame - Top
	SetShaderColor(60 / 255.0, 60 / 255.0, 60 / 255.0, 1.0); // Dark color for the frame
	scaleXYZ = glm::vec3(3.7f, 0.2f, 0.2f); // Bottom frame
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-8.0f, 11.1f, -9.8f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Frame - Left
	SetShaderColor(60 / 255.0, 60 / 255.0, 60 / 255.0, 1.0); // Dark color for the frame
	scaleXYZ = glm::vec3(0.2f, 5.7f, 0.2f); // Right frame
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-9.75f, 8.3f, -9.80f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Frame - Right
	SetShaderColor(60 / 255.0, 60 / 255.0, 60 / 255.0, 1.0); // Dark color for the frame
	scaleXYZ = glm::vec3(0.2f, 5.7f, 0.2f); // Right frame
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-6.25f, 8.3f, -9.80f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Canvas
	SetShaderColor(1.0f, 1.0f, 1.0f, 1.0f); // White color for the canvas
	SetShaderMaterial("canvas_material");
	scaleXYZ = glm::vec3(3.5f, 5.5f, 0.1f); // Canvas
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-8.0f, 8.3f, -9.85);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderTexture("texture5");
	SetTextureUVScale(1.0f, 1.0f);
	SetTextureOffset(0.5f, 0.5f); // Move texture to see the effect
	m_basicMeshes->DrawBoxMesh();

	SetShaderColor(0 / 255.0, 0 / 255.0, 0 / 255.0, 1.0); // Dark color for the frame
	SetShaderMaterial("default");
	scaleXYZ = glm::vec3(9.5f, 3.75f, 0.2f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-0.5f, 7.3f, -8.85);
	// Use the texture tag for this object
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(9.45f, 3.72f, 0.1f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-0.5f, 7.3f, -8.78);
	// Use the texture tag for this object
	SetTextureUVScale(1.0f, 1.0f);
	SetTextureOffset(0.5f, 0.5f); // Move texture to see the effect
	SetShaderTexture("texture8");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	SetShaderMaterial("default");
	SetShaderColor(30 / 255.0, 30 / 255.0, 30 / 255.0, 1.0);
	scaleXYZ = glm::vec3(1.0f, 2.0f, 0.2f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-0.5f, 5.3f, -9.45);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(6.0f, 0.25f, 2.0f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-0.5f, 4.3f, -7.45);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(6.0f, 0.25f, 2.0f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-0.5f, 4.3f, -7.45);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	SetShaderMaterial("default");
	SetShaderColor(50 / 255.0, 50 / 255.0, 50 / 255.0, 1.0);
	scaleXYZ = glm::vec3(4.0f, 0.25f, 1.8f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-0.5f, 4.5f, -7.45);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetTextureUVScale(1.0, 1.0);
	m_basicMeshes->DrawBoxMesh();

	// Reset the lighting flag to true for other objects
	SetUseLighting(false);
	scaleXYZ = glm::vec3(3.90f, 0.25f, 1.7f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-0.5f, 4.51f, -7.45);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderTexture("texture7");
	SetTextureUVScale(1.0, 1.0);
	m_basicMeshes->DrawBoxMesh();
	// Reset the lighting flag to true for other objects
	SetUseLighting(true);

	// Drawing the pot
	SetShaderColor(200 / 255.0, 200 / 255.0, 200 / 255.0, 1.0);
	SetShaderMaterial("pot_material");
	scaleXYZ = glm::vec3(0.5f, 0.75f, 0.5f);
	positionXYZ = glm::vec3(12.0f, 5.0f, -7.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	SetShaderColor(0 / 255.0, 140 / 255.0, 30 / 255.0, 1.0);
	// Drawing the plant stem
	SetShaderMaterial("stem_material");
	scaleXYZ = glm::vec3(0.1f, 1.0f, 0.1f);
	positionXYZ = glm::vec3(12.0f, 5.25f, -7.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	// Drawing the leaves
	SetShaderMaterial("leaf_material");

	// Left leaf
	scaleXYZ = glm::vec3(0.3f, 0.3f, 0.3f);
	positionXYZ = glm::vec3(11.7f, 6.25f, -7.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Right leaf
	positionXYZ = glm::vec3(12.3f, 6.25f, -7.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Top leaf
	positionXYZ = glm::vec3(12.0f, 6.75f, -7.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	// Drawing the pot
	SetShaderColor(200 / 255.0, 200 / 255.0, 200 / 255.0, 1.0);
	SetShaderMaterial("pot_material");
	scaleXYZ = glm::vec3(0.5f, 0.75f, 0.5f);
	positionXYZ = glm::vec3(13.0f, 5.0f, -5.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	// Drawing the plant stem
	SetShaderColor(0 / 255.0, 140 / 255.0, 30 / 255.0, 1.0);
	SetShaderMaterial("stem_material");
	scaleXYZ = glm::vec3(0.05f, 2.0f, 0.05f);
	positionXYZ = glm::vec3(13.0f, 5.0f, -5.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	// Drawing the grassy leaves
	SetShaderMaterial("leaf_material");

	glm::vec3 stemPosition = glm::vec3(13.0f, 5.0f, -5.0f); // Position of the stem
	// Draw multiple grassy leaves
	for (int i = 0; i < 10; ++i) {
		float angle = glm::radians(i * 36.0f); // Spread leaves evenly around the stem
		float offsetX = 0.3f * cos(angle) + stemPosition.x;
		float offsetY = 1.2f + 0.4f * sin(angle) + stemPosition.y; // Random height for natural look
		float offsetZ = 0.3f * sin(angle) + stemPosition.z;

		scaleXYZ = glm::vec3(0.05f, 0.5f, 0.05f);
		XrotationDegrees = 45.0f * sin(angle); // Random tilt for natural look
		YrotationDegrees = glm::degrees(angle);
		ZrotationDegrees = 0.0f;
		positionXYZ = glm::vec3(offsetX, offsetY, offsetZ);
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		m_basicMeshes->DrawCylinderMesh();
	}


	// Drawing the pot
	SetShaderColor(200 / 255.0, 200 / 255.0, 200 / 255.0, 1.0);
	SetShaderMaterial("pot_material");
	scaleXYZ = glm::vec3(0.5f, 0.75f, 0.5f);
	positionXYZ = glm::vec3(12.4f, 5.0f, -3.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	// Drawing the plant stem
	SetShaderColor(0 / 255.0, 140 / 255.0, 30 / 255.0, 1.0);
	SetShaderMaterial("stem_material");
	scaleXYZ = glm::vec3(0.05f, 2.0f, 0.05f);
	positionXYZ = glm::vec3(12.4f, 5.0f, -3.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	// Drawing the grassy leaves
	SetShaderMaterial("leaf_material");

	stemPosition = glm::vec3(12.4f, 5.0f, -3.0f); // Position of the stem
	// Draw multiple grassy leaves
	for (int i = 0; i < 10; ++i) {
		float angle = glm::radians(i * 36.0f); // Spread leaves evenly around the stem
		float offsetX = 0.3f * cos(angle) + stemPosition.x;
		float offsetY = 1.2f + 0.4f * sin(angle) + stemPosition.y; // Random height for natural look
		float offsetZ = 0.3f * sin(angle) + stemPosition.z;

		scaleXYZ = glm::vec3(0.05f, 0.5f, 0.05f);
		XrotationDegrees = 45.0f * sin(angle); // Random tilt for natural look
		YrotationDegrees = glm::degrees(angle);
		ZrotationDegrees = 0.0f;
		positionXYZ = glm::vec3(offsetX, offsetY, offsetZ);
		SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
		m_basicMeshes->DrawCylinderMesh();
	}

	// Position the pot
	SetShaderColor(200 / 255.0, 200 / 255.0, 200 / 255.0, 1.0);
	SetShaderMaterial("pot_material");
	glm::vec3 potScale = glm::vec3(1.0f, 3.5f, 1.0f);
	glm::vec3 potPosition = glm::vec3(-13.0f, 3.55f, -5.0f); // Centered at origin
	XrotationDegrees = 180.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(potScale, XrotationDegrees, YrotationDegrees, ZrotationDegrees, potPosition);
	m_basicMeshes->DrawTaperedCylinderMesh();

	// Position the stems
	SetShaderColor(0 / 255.0, 140 / 255.0, 30 / 255.0, 1.0);
	SetShaderMaterial("stem_material");
	int numStems = 3;
	float stemOffsets[3][2] = { {0.0f, 0.0f}, {0.15f, 0.15f}, {-0.15f, -0.15f} };

	for (int s = 0; s < numStems; ++s) {
		float baseX = stemOffsets[s][0];
		float baseZ = stemOffsets[s][1];

		glm::vec3 stemScale = glm::vec3(0.1f, 2.0f, 0.1f); // Thin and tall
		glm::vec3 stemPosition = glm::vec3(13.0f + baseX, 6.0f, -5.0f + baseZ); // Adjusted position based on pot's position
		SetTransformations(stemScale, 0.0f, 0.0f, 0.0f, stemPosition);
		m_basicMeshes->DrawCylinderMesh();

		// Position the leaves around each stem
		SetShaderMaterial("leaf_material");
		int numLeaves = 20;
		for (int i = 0; i < numLeaves; ++i) {
			float angle = glm::radians(i * 18.0f); // Spread leaves evenly around the center
			float offsetX = 13.0f + baseX + 0.3f * cos(angle);
			float offsetY = 6.0f + (i % 10) * 0.2f; // Random height for natural look
			float offsetZ = -5.0f + baseZ + 0.3f * sin(angle);

			glm::vec3 leafPosition = glm::vec3(offsetX, offsetY, offsetZ);

			// Set transformations for the leaf
			glm::vec3 leafScale = glm::vec3(0.05f, 0.5f, 0.1f); // Adjust scale for leaf shape
			float XrotationDegrees = 0.0f;
			float YrotationDegrees = glm::degrees(angle); // Rotate leaf to face outward
			float ZrotationDegrees = 45.0f; // Slight tilt for a more natural look

			SetTransformations(leafScale, XrotationDegrees, YrotationDegrees, ZrotationDegrees, leafPosition);
			m_basicMeshes->DrawBoxMesh();
		}
	}

	stemPosition = glm::vec3(-13.0f, 3.55f, -5.0f);
	numStems = 6;
	for (int s = 0; s < numStems; ++s) {
		float baseX = stemOffsets[s][0];
		float baseZ = stemOffsets[s][1];

		glm::vec3 stemScale = glm::vec3(0.15f, 3.0f, 0.15f); // Scale up the stems
		float rotationAngle = s * -10.0f; // Different angles for each stem
		glm::vec3 stemPosition = glm::vec3(-13.0f + baseX, 3.25f, -5.0f + baseZ); // Adjusted position based on pot's position
		SetTransformations(stemScale, rotationAngle, rotationAngle, 0.0f, stemPosition);
		m_basicMeshes->DrawCylinderMesh();

		// Position the leaves around each stem with spiral effect
		SetShaderMaterial("leaf_material");
		int numLeaves = 20;
		for (int i = 0; i < numLeaves; ++i) {
			float angle = glm::radians(i * 18.0f + s * 60.0f); // Spread leaves evenly around the center and add spiral effect
			float offsetX = baseX + 0.5f * cos(angle);
			float offsetY = 4.25f + (i % 10) * 0.4f; // Random height for natural look
			float offsetZ = baseZ + 0.5f * sin(angle);

			glm::vec3 leafPosition = glm::vec3(-13.0f + offsetX, offsetY, -5.0f + offsetZ);

			// Set transformations for the leaf
			glm::vec3 leafScale = glm::vec3(0.1f, 0.8f, 0.2f); // Scale up the leaves
			float XrotationDegrees = 0.0f;
			float YrotationDegrees = glm::degrees(angle); // Rotate leaf to face outward
			float ZrotationDegrees = 45.0f; // Slight tilt for a more natural look

			SetTransformations(leafScale, XrotationDegrees, YrotationDegrees, ZrotationDegrees, leafPosition);
			m_basicMeshes->DrawBoxMesh();
		}
	}

	/*** Draw the Chair ***/
	SetShaderMaterial("default");
	SetShaderColor(30 / 255.0, 30 / 255.0, 30 / 255.0, 1.0);
	scaleXYZ = glm::vec3(1.9f, 0.75f, 0.75f);
	positionXYZ = glm::vec3(-2.0f, 6.0f, -3.0f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawTorusMesh();

	scaleXYZ = glm::vec3(01.25f, 0.65f, 0.75f);
	positionXYZ = glm::vec3(-2.0f, 7.0f, -3.0f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawTorusMesh();

	// Draw the cylinder inside the torus
	scaleXYZ = glm::vec3(1.25f, 0.15f, 0.55f);
	positionXYZ = glm::vec3(-2.0f, 7.0f, -3.0f);
	XrotationDegrees = 100.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(01.25f, 0.65f, 0.15f);
	positionXYZ = glm::vec3(-2.0f, 6.0f, -3.0f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(2.75f, 0.5f, 0.05f);
	positionXYZ = glm::vec3(-2.0f, 5.5f, -3.0f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(1.50f, 0.55f, 0.05f);
	positionXYZ = glm::vec3(-3.1f, 6.1f, -3.8f);
	XrotationDegrees = -15.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 45.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(1.50f, 0.55f, 0.05f);
	positionXYZ = glm::vec3(-1.0f, 6.1f, -2.4f);
	XrotationDegrees = 25.0f;
	YrotationDegrees = -15.0f;
	ZrotationDegrees = -45.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(1.95f, 0.95f, 0.75f);
	positionXYZ = glm::vec3(-2.0f, 4.2f, -3.2f);
	XrotationDegrees = 10.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawTorusMesh();

	// Draw the cylinder inside the torus
	scaleXYZ = glm::vec3(1.75f, 0.15f, 0.75f);
	positionXYZ = glm::vec3(-2.0f, 4.2f, -3.3f);
	XrotationDegrees = 90.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	// Draw the cylinder inside the torus
	scaleXYZ = glm::vec3(2.55f, 0.15f, 0.75f);
	positionXYZ = glm::vec3(-2.0f, 3.2f, -3.35f);
	XrotationDegrees = 100.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw the cylinder inside the torus
	scaleXYZ = glm::vec3(2.55f, 2.3f, 0.75f);
	positionXYZ = glm::vec3(-1.35f, 2.8f, -4.25f);
	XrotationDegrees = 100.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();
	
	// Draw the cylinder inside the torus
	scaleXYZ = glm::vec3(0.25f, 0.15f, 1.0f);
	positionXYZ = glm::vec3(-0.65f, 3.4f, -3.25f);
	XrotationDegrees = 100.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	// Draw the cylinder inside the torus
	scaleXYZ = glm::vec3(0.1f, 0.65f, 0.1f);
	positionXYZ = glm::vec3(-0.25f, 3.9f, -3.75f);
	XrotationDegrees = 100.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	// Draw the cylinder inside the torus
	scaleXYZ = glm::vec3(0.25f, 0.15f, 1.0f);
	positionXYZ = glm::vec3(-0.65f, 3.4f, -3.25f);
	XrotationDegrees = 100.0f;
	YrotationDegrees = -35.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawBoxMesh();

	scaleXYZ = glm::vec3(0.2f, 2.0f, 0.2f);
	positionXYZ = glm::vec3(-1.5f, 1.0f, -3.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.1f, 3.65f, 0.1f);
	positionXYZ = glm::vec3(0.5f, 1.0f, -3.75f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 90.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.1f, 3.65f, 0.1f);
	positionXYZ = glm::vec3(-1.5f, 1.0f, -5.65f);
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawCylinderMesh();

	scaleXYZ = glm::vec3(0.4f, 0.4f, 0.4f);
	positionXYZ = glm::vec3(-1.45f, 0.4f, -5.65f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	scaleXYZ = glm::vec3(0.4f, 0.4f, 0.4f);
	positionXYZ = glm::vec3(-3.15f, 0.7f, -3.65f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	scaleXYZ = glm::vec3(0.4f, 0.4f, 0.4f);
	positionXYZ = glm::vec3(-1.45f, 0.7f, -1.85f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawSphereMesh();

	scaleXYZ = glm::vec3(0.4f, 0.4f, 0.4f);
	positionXYZ = glm::vec3(0.60f, 0.7f, -3.65f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawSphereMesh();


	SetShaderMaterial("default");


}


