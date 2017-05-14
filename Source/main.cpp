#include "../Externals/Include/Include.h"


#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;
struct Shape {
	GLuint vao;
	GLuint vbo_position;
	GLuint vbo_normal;
	GLuint vbo_texcoord;
	GLuint ibo;
	int drawCount;
	int materialID;
};

struct Material {
	GLuint diffuse_tex;
};

using namespace glm;
using namespace std;

mat4 mv;
mat4 p;

Shape* shapes;
Material* materials;
const aiScene* scene;

GLuint program;
GLuint um4mv;
GLuint um4p;
GLuint tex;

//camera
GLfloat camera_yaw = -90.0f;
GLfloat camera_pitch = 0.0f;
GLfloat mouse_speed = 2.0f;
GLfloat mouse_sensitivty = 0.25f;
GLfloat camera_zoom = 45.0f;

vec3 camera_up = vec3(0.0f, 1.0f, 0.0f);
vec3 camera_position = vec3(0.0f, 0.0f, 3.0f);
vec3 camera_front = vec3(0.0f, 0.0f, -1.0f);
vec3 camera_right;
vec3 world_up = vec3(0.0f, 1.0f, 0.0f);

//mouse
GLfloat oldX;
GLfloat oldY;


char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char **srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}

// define a simple data structure for storing texture image raw data
typedef struct _TextureData
{
    _TextureData(void) :
        width(0),
        height(0),
        data(0)
    {
    }

    int width;
    int height;
    unsigned char* data;
} TextureData;

// load a png image and return a TextureData structure with raw data
// not limited to png format. works with any image format that is RGBA-32bit
TextureData loadPNG(const char* const pngFilepath)
{
    TextureData texture;
    int components;

    // load the texture with stb image, force RGBA (4 components required)
    stbi_uc *data = stbi_load(pngFilepath, &texture.width, &texture.height, &components, 4);

    // is the image successfully loaded?
    if (data != NULL)
    {
        // copy the raw data
        size_t dataSize = texture.width * texture.height * 4 * sizeof(unsigned char);
        texture.data = new unsigned char[dataSize];
        memcpy(texture.data, data, dataSize);

        // mirror the image vertically to comply with OpenGL convention
        for (size_t i = 0; i < texture.width; ++i)
        {
            for (size_t j = 0; j < texture.height / 2; ++j)
            {
                for (size_t k = 0; k < 4; ++k)
                {
                    size_t coord1 = (j * texture.width + i) * 4 + k;
                    size_t coord2 = ((texture.height - j - 1) * texture.width + i) * 4 + k;
                    std::swap(texture.data[coord1], texture.data[coord2]);
                }
            }
        }

        // release the loaded image
        stbi_image_free(data);
    }

    return texture;
}

void My_Init()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	// Create Shader Program
	program = glCreateProgram();

	// Create customize shader by tell openGL specify shader type 
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load shader file
	char** vertexShaderSource = loadShaderSource("vertex.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("fragment.fs.glsl");

	// Assign content of these shader files to those shaders we created before
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);

	// Compile these shaders
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);

	// Assign the program we created before with these shaders
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	// Get the id of inner variable 'um4mvp' in shader programs
	um4mv = glGetUniformLocation(program, "um4mv");
	um4p = glGetUniformLocation(program, "um4p");
	tex = glGetUniformLocation(program, "tex");

	// Tell OpenGL to use this shader program now
	glUseProgram(program);



	scene = aiImportFile("crytek-sponza/sponza.obj", aiProcessPreset_TargetRealtime_MaxQuality);
	shapes = new Shape[scene->mNumMeshes]();
	materials = new Material[scene->mNumMaterials]();
	cout << "shape number: " << scene->mNumMeshes << endl;
	cout << "material number: " << scene->mNumMaterials << endl;
	


	for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
		aiMaterial *aimaterial = scene->mMaterials[i];
		Material material;
		aiString texturePath;
		if (aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS) {
			
			string directory = "crytek-sponza/";
			directory.append(texturePath.data);
			cout << directory << endl;
			TextureData data = loadPNG(directory.c_str());
			glGenTextures(1, &material.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, material.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		else {
			material.diffuse_tex = NULL;
		}
		materials[i] = material;
	}
	


	for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
		aiMesh* mesh = scene->mMeshes[i];
		Shape shape;
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);

		glGenBuffers(1, &shape.vbo_position);
		glGenBuffers(1, &shape.vbo_normal);
		glGenBuffers(1, &shape.vbo_texcoord);
		//test
		/*glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 3 * mesh->mNumVertices,
			(const void *)&mesh->mVertices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 3 * mesh->mNumVertices,
			(const void *)&mesh->mNormals[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 2 * mesh->mNumVertices,
			(const void *)&mesh->mTextureCoords[0][0], GL_STATIC_DRAW);*/
		unsigned int* faces = new unsigned int[mesh->mNumFaces * 3]();
		float* positions = new float[mesh->mNumVertices * 3]();
		float* normals = new float[mesh->mNumVertices * 3]();
		float* tex_coords = new float[mesh->mNumVertices * 2]();

		for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
			
			/*glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
			glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 3, 
				(const void *) &mesh->mVertices[v], GL_STATIC_DRAW);
			
			glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
			glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 3, 
				(const void *)&mesh->mNormals[v], GL_STATIC_DRAW);
			
			glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
			if (mesh->mTextureCoords[0]) {
				glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 2,
					(const void *)&mesh->mTextureCoords[0][v], GL_STATIC_DRAW);
			}*/
			for (unsigned int j = 0; j < 3; ++j) {
				positions[v * 3 + j] = mesh->mVertices[v][j];
			}
			//cout << mesh->mVertices[v][0] << endl;
			for (unsigned int j = 0; j < 3; ++j) {
				normals[v * 3 + j] = mesh->mNormals[v][j];
			}
			for (unsigned int j = 0; j < 2; ++j) {
				if (mesh->mTextureCoords[0])
					tex_coords[v * 2 + j] = mesh->mTextureCoords[0][v][j];
				else
					tex_coords[v * 2 + j] = 0.0;
			}
		}
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 3 * mesh->mNumVertices,
			(const void *)positions, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3,
			GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 3 * mesh->mNumVertices,
			(const void *)normals, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3,
			GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GL_FLOAT) * 2 * mesh->mNumVertices,
			(const void *)tex_coords, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2,
			GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

		glGenBuffers(1, &shape.ibo);

		for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
			
			/*glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GL_FLOAT) * 3, 
				(const void *)&mesh->mFaces[f], GL_STATIC_DRAW);*/
			for (int j = 0; j < 3; ++j) {
				faces[f * 3 + j] = mesh->mFaces[f].mIndices[j];
			}
		}

		//test
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 3 * mesh->mNumFaces, 
			(const void *)faces, GL_STATIC_DRAW);


		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;

		shapes[i] = shape;
	}
	//aiReleaseImport(scene);
	cout << "init ok" << endl;
}

void My_Display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(program);
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(mv));
	glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(p));

	glActiveTexture(GL_TEXTURE0);
	glUniform1i(tex, 0);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (int i = 0; i < scene->mNumMeshes; ++i) {
		glBindVertexArray(shapes[i].vao);
		int materialID = shapes[i].materialID;
		glBindTexture(GL_TEXTURE_2D, materials[materialID].diffuse_tex);
		glDrawElements(GL_TRIANGLES, shapes[i].drawCount, GL_UNSIGNED_INT, 0);
	}

    glutSwapBuffers();
}

void CameraUpdate() {
	camera_front.x = cos(radians(camera_yaw)) * cos(radians(camera_pitch));
	camera_front.y = sin(radians(camera_pitch));
	camera_front.z = sin(radians(camera_yaw)) * cos(radians(camera_pitch));
	camera_front = normalize(camera_front);
	camera_right = normalize(cross(camera_front, world_up));
	camera_up = normalize(cross(camera_right, camera_front));
}

void My_Reshape(int width, int height)
{
	glViewport(0, 0, width, height);
	float viewportAspect = (float)width / (float)height;
	//mvp = lookAt(vec3(500.0f, 400.0f, 500.0f), vec3(500.0f, 200.0f, 500.0f), vec3(0.0f, 1.0f, 0.0f));
	mv = lookAt(camera_position, camera_position + camera_front, camera_up);
	p = perspective(45.0f, (float)width / (float)height, 0.1f, 5000.0f);
	CameraUpdate();
}

void My_Timer(int val)
{
	glutPostRedisplay();
	glutTimerFunc(timer_speed, My_Timer, val);
}



void My_Mouse_Motion(int x, int y)
{
	GLfloat x_offset = x - oldX;
	GLfloat y_offset = y - oldY;
	oldX = x;
	oldY = y;

	GLfloat camera_x = x_offset * mouse_sensitivty;
	GLfloat camera_y = y_offset * mouse_sensitivty;
	camera_yaw += x_offset;
	camera_pitch += y_offset;
	if (camera_pitch > 89.0f) {
		camera_pitch = 89.0f;
	}
	if (camera_yaw < -89.0f) {
		camera_yaw = -89.0f;
	}
	CameraUpdate();
	mv = lookAt(camera_position, camera_position + camera_front, camera_up);
}



void My_Mouse(int button, int state, int x, int y)
{
	if(state == GLUT_DOWN)
	{
		oldX = x;
		oldY = y;
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
	}
	else if(state == GLUT_UP)
	{
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
	}
}

void My_Keyboard(unsigned char key, int x, int y)
{
	printf("Key %c is pressed at (%d, %d)\n", key, x, y);
}

void My_SpecialKeys(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		printf("F1 is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_PAGE_UP:
		printf("Page up is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_LEFT:
		printf("Left arrow is pressed at (%d, %d)\n", x, y);
		camera_position -= camera_right * mouse_speed;
		break;
	case GLUT_KEY_RIGHT:
		printf("Right arrow is pressed at (%d, %d)\n", x, y);
		camera_position += camera_right * mouse_speed;
		break;
	case GLUT_KEY_UP:
		printf("Up arrow is pressed at (%d, %d)\n", x, y);
		camera_position += camera_front * mouse_speed;
		break;
	case GLUT_KEY_DOWN:
		printf("Down arrow is pressed at (%d, %d)\n", x, y);
		camera_position -= camera_front * mouse_speed;
		break;
	default:
		printf("Other special key is pressed at (%d, %d)\n", x, y);
		break;
	}
	CameraUpdate();
	mv = lookAt(camera_position, camera_position + camera_front, camera_up);
}

void My_Menu(int id)
{
	switch(id)
	{
	case MENU_TIMER_START:
		if(!timer_enabled)
		{
			timer_enabled = true;
			glutTimerFunc(timer_speed, My_Timer, 0);
		}
		break;
	case MENU_TIMER_STOP:
		timer_enabled = false;
		break;
	case MENU_EXIT:
		exit(0);
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
#ifdef __APPLE__
    // Change working directory to source code path
    chdir(__FILEPATH__("/../Assets/"));
#endif
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(600, 600);
	glutCreateWindow("AS2_Framework"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
	glewInit();
#endif
    glPrintContextInfo();
	My_Init();

	// Create a menu and bind it to mouse right button.
	int menu_main = glutCreateMenu(My_Menu);
	int menu_timer = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	glutAddSubMenu("Timer", menu_timer);
	glutAddMenuEntry("Exit", MENU_EXIT);

	glutSetMenu(menu_timer);
	glutAddMenuEntry("Start", MENU_TIMER_START);
	glutAddMenuEntry("Stop", MENU_TIMER_STOP);

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// Register GLUT callback functions.
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutMotionFunc(My_Mouse_Motion);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0); 

	// Enter main event loop.
	glutMainLoop();

	return 0;
}
