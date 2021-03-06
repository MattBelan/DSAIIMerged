//Final Project
//Matt Belanger
//Chase Slattery

//Sources/References
///Tanat's In-Class Example of the Cube, Cubular Engine, and Cylinder Example
///Midterm Practical
///Camera stuff borrowed from here:
///https://learnopengl.com/Getting-started/Camera
///Sources for SAT
///https://github.com/IGME-RIT/physics-SAT3DAABB
///Sources for KD-Tree
///https://github.com/IGME-RIT/OpenGL_KD_Tree
///Resources used for SLERPING
///http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-17-quaternions/
///https://glm.g-truc.net/0.9.2/api/a00246.html
///Lighting stuff, based on this but translated to fit Tanat's system
///https://learnopengl.com/Lighting/Basic-Lighting
///Skybox
///https://learnopengl.com/Advanced-OpenGL/Cubemaps
///DynamicShader is based on the shader built in the LearnOpengl.com tutorials, allows for easier dynamic objects
///https://learnopengl.com/Getting-started/OpenGL
///Audio
///https://learnopengl.com/In-Practice/2D-Game/Audio
///https://www.ambiera.com/irrklang/tutorial-helloworld.html
///Background music- https://www.bensound.com/royalty-free-music/track/relaxing
///Collision Sound- http://soundbible.com/1468-Depth-Charge-Shorter.html


//IF THE PROJECT DOES NOT BUILD
///Make sure you are building on x86
///If that fails, try changing the Windows SDK Version under Project->Properties->Configuration Properties under General
///We ran into some issues with the SDK version being an issue, and couldn't find a consitent solution other than changing the version

#include "stdafx.h"
#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"
#include "GameEntity.h"
#include "Material.h"
#include "Input.h"
#include "KDTree.h"
#include "stb_image.h"
#include "DynamicShader.h"

#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <glm/gtc/quaternion.hpp>

#include <irrKlang.h>
using namespace irrklang;

using namespace std;

//TODO - maybe make some #define macro for a print if debug?
//TODO - make an Engine class with a specific Init() and Run() function such that
//       our Main.cpp is kept clean and tidy

//variables for camera rotations
double xposCam;
double yposCam;

float tm;
float prevTime;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	xposCam = xpos;
	yposCam = ypos;
}
//function to load skybox, takes in vector of different image values
unsigned int loadSkybox(std::vector<std::string> faces) {

	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	//loads each face png of the cube map
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
			);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	//creates texture ID along with the image values
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}


int main()
{
    {
        //init GLFW
        {
            if (glfwInit() == GLFW_FALSE)
            {
#ifdef _DEBUG
                std::cout << "GLFW failed to initialize" << std::endl;
                _CrtDumpMemoryLeaks();
                std::cin.get();
#endif
                return 1;
            }
        }
#ifdef _DEBUG
        std::cout << "GLFW successfully initialized!" << std::endl;
#endif // _DEBUG

        //create & init window, set viewport
        int width = 1600;
        int height = 1200;
        GLFWwindow* window = glfwCreateWindow(width, height, "FPS Camera", nullptr, nullptr);
        {
            if (window == nullptr)
            {
#ifdef _DEBUG
                std::cout << "GLFW failed to create window" << std::endl;
                _CrtDumpMemoryLeaks();
                std::cin.get();
#endif
                glfwTerminate();
                return 1;
            }

            //tells OpenGL to use this window for this thread
            //(this would be more important for multi-threaded apps)
            glfwMakeContextCurrent(window);

            //gets the width & height of the window and specify it to the viewport
            int windowWidth, windowHeight;
            glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
            glViewport(0, 0, windowWidth, windowHeight);

			//setting cursor properties
			
        }
#ifdef _DEBUG
        std::cout << "Window successfully initialized!" << std::endl;
#endif // _DEBUG

        //init GLEW
        {
            if (glewInit() != GLEW_OK)
            {
#ifdef _DEBUG
                std::cout << "GLEW failed to initialize" << std::endl;
                _CrtDumpMemoryLeaks();
                std::cin.get();
#endif
                glfwTerminate();
                return 1;
            }
        }
#ifdef _DEBUG
        std::cout << "GLEW successfully initialized!" << std::endl;
#endif // _DEBUG

        //init the shader program
        //TODO - this seems like a better job for a shader manager
        //       perhaps the Shader class can be refactored to fit a shader program
        //       rather than be a thing for vs and fs
        GLuint shaderProgram = glCreateProgram();
        {

            //create vS and attach to shader program
            Shader *vs = new Shader();
            vs->InitFromFile("assets/shaders/vertexShader.glsl", GL_VERTEX_SHADER);
            glAttachShader(shaderProgram, vs->GetShaderLoc());

            //create FS and attach to shader program
            Shader *fs = new Shader();
            fs->InitFromFile("assets/shaders/fragmentShader.glsl", GL_FRAGMENT_SHADER);
            glAttachShader(shaderProgram, fs->GetShaderLoc());

            //link everything that's attached together
            glLinkProgram(shaderProgram);

            GLint isLinked;
            glGetProgramiv(shaderProgram, GL_LINK_STATUS, &isLinked);
            if (!isLinked)
            {
                char infolog[1024];
                glGetProgramInfoLog(shaderProgram, 1024, NULL, infolog);
#ifdef _DEBUG
                std::cout << "Shader Program linking failed with error: " << infolog << std::endl;
                std::cin.get();
#endif

                // Delete the shader, and set the index to zero so that this object knows it doesn't have a shader.
                glDeleteProgram(shaderProgram);
                glfwTerminate();
                _CrtDumpMemoryLeaks();
                return 1;
            }

            //everything's in the program, we don't need this
            delete fs;
            delete vs;
        }

		GLuint lightShaderProgram = glCreateProgram();
		{

			//same vertex shader as the cube, but the fragment shader will be different to produce a light
			Shader *lightVertex = new Shader();
			lightVertex->InitFromFile("assets/shaders/vertexShader.glsl", GL_VERTEX_SHADER);
			glAttachShader(lightShaderProgram, lightVertex->GetShaderLoc());

			//Different shader from the regular box shader
			Shader *lightFragment = new Shader();
			lightFragment->InitFromFile("assets/shaders/lightShader.glsl", GL_FRAGMENT_SHADER);
			glAttachShader(lightShaderProgram, lightFragment->GetShaderLoc());

			//link everything that's attached together
			glLinkProgram(lightShaderProgram);

			GLint isLinked;
			glGetProgramiv(lightShaderProgram, GL_LINK_STATUS, &isLinked);
			if (!isLinked)
			{
				char infolog[1024];
				glGetProgramInfoLog(lightShaderProgram, 1024, NULL, infolog);
#ifdef _DEBUG
				std::cout << "Light Shader Program linking failed with error: " << infolog << std::endl;
				std::cin.get();
#endif

				// Delete the shader, and set the index to zero so that this object knows it doesn't have a shader.
				glDeleteProgram(lightShaderProgram);
				glfwTerminate();
				_CrtDumpMemoryLeaks();
				return 1;
			}

			//everything's in the program, we don't need this
			delete lightFragment;
			delete lightVertex;
		}

#ifdef _DEBUG
        std::cout << "Shaders compiled attached, and linked!" << std::endl;
#endif // _DEBUG
		//position of the light to use for angle calculations
		glm::vec3 lightPosition = glm::vec3(0.f, 25.f, -5.f);
        //init the mesh (the cubes)
        //TODO - replace this with model loading
		GLfloat vertices[] = {
			-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
			0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
			0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
			0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
			-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
			-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

			-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
			0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
			0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
			0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
			-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
			-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

			-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
			-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
			-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
			-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
			-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
			-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

			0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
			0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
			0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
			0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
			0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
			0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

			-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
			0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
			0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
			0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
			-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

			-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
			0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
			0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
			0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
			-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
			-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
		};
		//passes shaders to properly display the skybox
		DynamicShader skyboxShader("assets/shaders/skyboxVertex.glsl", "assets/shaders/skyboxFragment.glsl");
		//position of the skybox vertices, basically just a cube
		float skyboxVertices[] = {
			// positions          
			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			1.0f, -1.0f, -1.0f,
			1.0f, -1.0f,  1.0f,
			1.0f,  1.0f,  1.0f,
			1.0f,  1.0f,  1.0f,
			1.0f,  1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			1.0f,  1.0f,  1.0f,
			1.0f,  1.0f,  1.0f,
			1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			-1.0f,  1.0f, -1.0f,
			1.0f,  1.0f, -1.0f,
			1.0f,  1.0f,  1.0f,
			1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			1.0f, -1.0f,  1.0f
		};

		//creates a manual skybox vao and vbo, since we only use it once 
		unsigned int skyboxVAO, skyboxVBO;
		glGenVertexArrays(1, &skyboxVAO);
		glGenBuffers(1, &skyboxVBO);
		glBindVertexArray(skyboxVAO);
		glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		//face data for game
		std::vector<std::string> faces
		{
			"assets/Skymap/right.png",
			"assets/Skymap/left.png",
			"assets/Skymap/top.png",
			"assets/Skymap/bot.png",
			"assets/SkyMap/front.png",
			"assets/Skymap/back.png"
		};
		//face data for main menu
		std::vector<std::string> mainFaces
		{
			"assets/Skymap/right.png",
			"assets/Skymap/left.png",
			"assets/Skymap/top.png",
			"assets/Skymap/bot.png",
			"assets/Menus/mainMenu.png",
			"assets/Skymap/back.png"
		};
		//face data for main menu
		std::vector<std::string> creditsFaces
		{
			"assets/Skymap/right.png",
			"assets/Skymap/left.png",
			"assets/Skymap/top.png",
			"assets/Skymap/bot.png",
			"assets/Menus/credits.png",
			"assets/Skymap/back.png"
		};
		

		unsigned int cubemapTexture = loadSkybox(mainFaces);
		//cube1
		float cube1X = 0.0f;
		float cube1Y = 0.0f;
		GLfloat cube1Vertices[] = {
			-1.0f,-1.0f ,-1.0f, // triangle 1 : begin
			-1.0f,-1.0f , 1.0f,
			-1.0f, 1.0f , 1.0f, // triangle 1 : end
			1.0f, 1.0f ,-1.0f, // triangle 2 : begin
			-1.0f,-1.0f ,-1.0f,
			-1.0f, 1.0f ,-1.0f, // triangle 2 : end
			1.0f,-1.0f , 1.0f,
			-1.0f,-1.0f ,-1.0f,
			1.0f,-1.0f ,-1.0f,
			1.0f, 1.0f ,-1.0f,
			1.0f,-1.0f ,-1.0f,
			-1.0f,-1.0f ,-1.0f,
			-1.0f,-1.0f ,-1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f,-1.0f,
			1.0f,-1.0f, 1.0f,
			-1.0f,-1.0f, 1.0f,
			-1.0f,-1.0f,-1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f,-1.0f, 1.0f,
			1.0f,-1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f,-1.0f,-1.0f,
			1.0f, 1.0f,-1.0f,
			1.0f,-1.0f,-1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f,-1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			1.0f, 1.0f,-1.0f,
			-1.0f, 1.0f,-1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f,-1.0f,
			-1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			1.0f,-1.0f, 1.0f
		};


		//cube2
		float cube2X = 0.1f;
		float cube2Y = 0.1f;


		//cube3
		float cube3X = 1.0f;
		float cube3Y = 7.0f;


		//cube4
		float cube4X = -4.0f;
		float cube4Y = 6.0f;


		//cube5
		float cube5X = 3.0f;
		float cube5Y = -6.0f;


		//cube6
		float cube6X = -7.5f;
		float cube6Y = 8.0f;


		//cube7
		float cube7X = 0.0f;
		float cube7Y = -5.0f;


		//cube8
		float cube8X = -6.0f;
		float cube8Y = -9.0f;

		//TODO - maybe a CameraManager?
		Camera* myCamera = new Camera(
			glm::vec3(0.0f, 0.0f, -30.f),    //position of camera
			glm::vec3(0.0f, 0.0f, 1.f),     //the 'forward' of the camera
			glm::vec3(0.0f, 1.f, 0.0f),     //what 'up' is for the camera
			60.0f,                          //the field of view in radians
			(float)width,                   //the width of the window in float
			(float)height,                  //the height of the window in float
			0.01f,                          //the near Z-plane
			1000.f                           //the far Z-plane
		);
		myCamera->gameCam = true;

		Camera* menuCam = new Camera(
			glm::vec3(-300.0f, 0.0f, -30.f),    //position of camera
			glm::vec3(0.0f, 0.0f, 1.f),     //the 'forward' of the camera
			glm::vec3(0.0f, 1.f, 0.0f),     //what 'up' is for the camera
			60.0f,                          //the field of view in radians
			(float)width,                   //the width of the window in float
			(float)height,                  //the height of the window in float
			0.01f,                          //the near Z-plane
			1000.f                           //the far Z-plane
		);

		Camera* creditsCam = new Camera(
			glm::vec3(300.0f, 0.0f, -30.f),    //position of camera
			glm::vec3(0.0f, 0.0f, 1.f),     //the 'forward' of the camera
			glm::vec3(0.0f, 1.f, 0.0f),     //what 'up' is for the camera
			60.0f,                          //the field of view in radians
			(float)width,                   //the width of the window in float
			(float)height,                  //the height of the window in float
			0.01f,                          //the near Z-plane
			1000.f                           //the far Z-plane
		);

		Mesh* cube1Mesh = new Mesh();
		cube1Mesh->InitWithVertexArray(vertices, _countof(vertices), lightShaderProgram);
		//vec3's to pass to the lighted object shader, so that they can represent light correctly
		glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
		glm::vec3 objectColor = glm::vec3(1.0f, 0.5f, 0.31f);
		glm::vec3 ambientColor = glm::vec3(.5f, 0.5f, .8f);
		Material* myMaterial = new Material(lightShaderProgram, lightColor, objectColor, lightPosition, myCamera->position, ambientColor, glm::vec3(1.0f, 0.5f, .31f), glm::vec3(0.5f, 0.5f, 0.5f), 64.0f);
		Mesh* lightMesh = new Mesh();
		lightMesh->InitWithVertexArray(vertices, _countof(vertices), shaderProgram);
		Material* lightMaterial = new Material(shaderProgram, lightColor, objectColor);

        //TODO - maybe a GameEntityManager?
		//Initialize all the cubes
        GameEntity* cube1 = new GameEntity(
            lightMesh,
            lightMaterial,
            glm::vec3(0.1f, 0.1f, 0.1f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(1.f, 1.f, 1.f)
        );

		GameEntity* cube2 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(8.0f, 0.f, 0.f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* cube3 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(2.f, 0.f, 0.f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* cube4 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(16.f, 0.f, 0.f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* cube5 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(12.f, 1.f, 0.f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* cube6 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(20.f, -2.f, 0.f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* cube7 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(cube7X, cube7Y, -6.f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* cube8 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(cube8X, cube8Y, 8.f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* cube9 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(5.0f, 3.0 ,6.0f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* cube10 = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(-8.0f, 13.0f, -4.0f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(.5f, .5f, .5f)
		);

		GameEntity* menuBox = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(-300.0f, 0.0f, 0.0f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(1.f, 1.f, 1.f)
		);
		GameEntity* creditsBox = new GameEntity(
			cube1Mesh,
			myMaterial,
			glm::vec3(300.0f, 0.0f, 0.0f),
			glm::vec3(0.f, 0.f, 0.f),
			glm::vec3(1.f, 1.f, 1.f)
		);

        

		vector<GameEntity*> cubes;
		cubes.push_back(cube1);
		cubes.push_back(cube2);
		cubes.push_back(cube3);
		cubes.push_back(cube4);
		cubes.push_back(cube5);
		cubes.push_back(cube6);
		cubes.push_back(cube7);
		cubes.push_back(cube8);
		cubes.push_back(cube9);
		cubes.push_back(cube10);

		cubes[0]->orbital = false;
		cubes[0]->SetMass(10.f);

		cubes[1]->AddVelocity(glm::vec3(0.f, 1.0f, 4.0f));
		cubes[2]->AddVelocity(glm::vec3(0.f, 0.f, 2.0f));
		cubes[3]->AddVelocity(glm::vec3(0.f, 0.f, 8.0f));
		cubes[4]->AddVelocity(glm::vec3(0.f, 0.f, 6.0f));
		cubes[5]->AddVelocity(glm::vec3(0.f, 0.f, 10.0f));
		cubes[6]->AddVelocity(glm::vec3(0.f, 0.f, 5.0f));
		cubes[7]->AddVelocity(glm::vec3(0.f, 0.f, 4.0f));
		cubes[8]->AddVelocity(glm::vec3(0.f, 0.f, 5.0f));
		cubes[9]->AddVelocity(glm::vec3(0.f, 0.f, 4.0f));

		for (size_t i = 0; i < cubes.size(); i++)
		{
			cubes[i]->startVel = cubes[i]->GetVelocity();
		}

		KDTree* tree = new KDTree();
		tree->center = cubes[0];

        Input::GetInstance()->Init(window);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

		xposCam = 400;
		yposCam = 300;

		tm = 0.0f;
		glm::vec3 gamePos = glm::vec3(0.0f, 0.0f, -30.f);
		glm::vec3 menuPos = glm::vec3(-300.f, 0.f, -10.f);
		glm::vec3 creditsPos = glm::vec3(300.f, 0.f, -10.f);

		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, mouse_callback);
		bool firstLeftClick = true;
		bool firstRightClick = true;
		bool firstPPress = true;
		bool playing = true;
		bool menu = true;
		bool game = false;
		bool credits = false;

		float instantiateSpeed = 6.f;
		skyboxShader.use();
		skyboxShader.setInt("skybox", 0);

		//audio player
		ISoundEngine *music = createIrrKlangDevice();
		music->play2D("assets/Audio/bensound-relaxing.mp3", GL_TRUE);
		music->setSoundVolume(.3f);

        //main loop
        while (!glfwWindowShouldClose(window))
        {
			prevTime = tm;
			tm = glfwGetTime();

			float dt = tm - prevTime;
            /* INPUT */
            
                //checks events to see if there are pending input
                glfwPollEvents();

                //breaks out of the loop if user presses ESC
                if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                {
                    break;
                }
				if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) //switches to the menu
				{
					menu = true;
					game = false;
					credits = false;
					myCamera->position = menuPos;
					cubemapTexture = loadSkybox(mainFaces);
				}
				if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) //switches to the game
				{
					menu = false;
					game = true;
					credits = false;
					myCamera->position = gamePos;
					cubemapTexture = loadSkybox(faces);
					for (size_t i = 0; i < cubes.size(); i++)
					{
						if (i < 10) {
							cubes[i]->Reset();
						}
						else {
							cubes[i]->enabled = false;
						}
					}
					myCamera->Reset();
					playing = true;
				}
				if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) //switches to the credits
				{
					menu = false;
					game = false;
					credits = true;
					myCamera->position = menuPos;
					cubemapTexture = loadSkybox(creditsFaces);
				}
				
				if (game) 
				{
					if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) // pauses the game
					{
						if (firstPPress) {
							firstPPress = false;
							playing = !playing;
						}
					}
					else {
						firstPPress = true;
					}
					if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) //resets the game
					{
						for (size_t i = 0; i < cubes.size(); i++)
						{
							if (i < 10) {
								cubes[i]->Reset();
							}
							else {
								cubes[i]->enabled = false;
							}
						}
						myCamera->Reset();
					}

					if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) //creates an object with no gravity
					{
						if (firstLeftClick) {
							firstLeftClick = false;
							cubes.push_back(new GameEntity(cube1Mesh, myMaterial, myCamera->GetPos(), glm::vec3(0.f, 0.f, 0.f),
								glm::vec3(.5f, .5f, .5f)));
							cubes.back()->AddVelocity(myCamera->forward*instantiateSpeed);
						}
					}
					else {
						firstLeftClick = true;
					}

					if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) //creates an object with gravity
					{
						if (firstRightClick) {
							firstRightClick = false;
							cubes.push_back(new GameEntity(cube1Mesh, myMaterial, myCamera->GetPos(), glm::vec3(0.f, 0.f, 0.f),
								glm::vec3(1.f, 1.f, 1.f)));
							cubes.back()->AddVelocity(myCamera->forward*instantiateSpeed*2.f);
							cubes.back()->orbital = false;
							cubes.back()->SetMass(5.f);
						}
					}
					else {
						firstRightClick = true;
					}

					/* GAMEPLAY UPDATE */
					if (playing) {

						for (size_t i = 0; i < cubes.size(); i++)
						{
							cubes[i]->CalculateBox();
						}

						tree->UpdateTree(cubes, cubes.size());

						tree->CheckCollisions(cubes, cubes.size());

						for (size_t i = 0; i < cubes.size(); i++)
						{
							glm::vec3 acc = glm::vec3(0.f, 0.f, 0.f);
							for (size_t j = 0; j < cubes.size(); j++)
							{
								if (i != j) {
									if (!cubes[j]->orbital) {
										glm::vec3 dir = glm::normalize(cubes[j]->GetPos() - cubes[i]->GetPos());
										float vel = glm::length(cubes[i]->GetVelocity());
										if (vel == 0) {
											vel = .2f;
										}
										//float centAcc = vel / glm::distance(cubes[0]->GetPos(), cubes[1]->GetPos());
										acc += dir * vel;
									}
								}
							}
							cubes[i]->SetAcceleration(acc);
						}

						for (size_t i = 0; i < cubes.size(); i++)
						{
							cubes[i]->Update(dt);
						}


						myCamera->Update();
						myCamera->UpdateRotation(xposCam, yposCam);
					}
					glm::mat4 view = myCamera->viewMatrix;

					/* PRE-RENDER */
					{
						//start off with clearing the 'color buffer'
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

						//clear the window to have c o r n f l o w e r   b l u e
						glClearColor(0.392f, 0.584f, 0.929f, 1.0f);
					}

					/* RENDER */
					for (size_t i = 0; i < cubes.size(); i++)
					{
						cubes[i]->Render(myCamera);
					}
					glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
					skyboxShader.use();
					view = glm::mat4(glm::mat3(myCamera->viewMatrix)); // remove translation from the view matrix
					skyboxShader.setMat4("view", view);
					skyboxShader.setMat4("projection", myCamera->projectionMatrix);

					glBindVertexArray(skyboxVAO);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
					glDrawArrays(GL_TRIANGLES, 0, 36);
					glBindVertexArray(0);
					glDepthFunc(GL_LESS);
				}
			
				if (menu) {
					
					menuBox->SLERP(dt);
					//myCamera->Update();
					myCamera->Update();
					myCamera->UpdateRotation(xposCam, yposCam);
					glm::mat4 view = myCamera->viewMatrix;
					/* PRE-RENDER */
					{
						//start off with clearing the 'color buffer'
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

						//clear the window to have c o r n f l o w e r   b l u e
						glClearColor(0.392f, 0.584f, 0.929f, 1.0f);
					}

					/* RENDER */
					menuBox->Render(myCamera);
					glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
					skyboxShader.use();
					view = glm::mat4(glm::mat3(myCamera->viewMatrix)); // remove translation from the view matrix
					skyboxShader.setMat4("view", view);
					skyboxShader.setMat4("projection", myCamera->projectionMatrix);

					glBindVertexArray(skyboxVAO);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
					glDrawArrays(GL_TRIANGLES, 0, 36);
					glBindVertexArray(0);
					glDepthFunc(GL_LESS);
				}

				if (credits) {
					myCamera->Update();
					myCamera->UpdateRotation(xposCam, yposCam);
					glm::mat4 view = myCamera->viewMatrix;
					/* PRE-RENDER */
					{
						//start off with clearing the 'color buffer'
						glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

						//clear the window to have c o r n f l o w e r   b l u e
						glClearColor(0.392f, 0.584f, 0.929f, 1.0f);
					}

					/* RENDER */
					glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
					skyboxShader.use();
					view = glm::mat4(glm::mat3(myCamera->viewMatrix)); // remove translation from the view matrix
					skyboxShader.setMat4("view", view);
					skyboxShader.setMat4("projection", myCamera->projectionMatrix);

					glBindVertexArray(skyboxVAO);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
					glDrawArrays(GL_TRIANGLES, 0, 36);
					glBindVertexArray(0);
					glDepthFunc(GL_LESS);
				}

            /* POST-RENDER */
            {
                //'clear' for next draw call
                glBindVertexArray(0);
                glUseProgram(0);
                //swaps the front buffer with the back buffer
                glfwSwapBuffers(window);
            }
        }

        //de-allocate our mesh!
        delete cube1Mesh;

        delete myMaterial;

		for (size_t i = 0; i < cubes.size(); i++)
		{
			delete cubes[i];
		}

        delete myCamera;
		delete menuCam;
		delete creditsCam;
		delete tree;
		delete menuBox;
		delete creditsBox;
		music->drop();
        Input::Release();
    }

    //clean up
    glfwTerminate();
#ifdef _DEBUG
    _CrtDumpMemoryLeaks();
#endif // _DEBUG
    return 0;
}

