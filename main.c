// This code ensures the waffle always fits inside the window and takes up 45% of the window's smallest dimension.
// The waffle will never overflow the window, regardless of window aspect or size.

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SCR_WIDTH 1000
#define SCR_HEIGHT 800

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double lastX = SCR_WIDTH / 2.0, lastY = SCR_HEIGHT / 2.0;
float yaw = 0.0f, pitch = 0.0f;
int rotating = 0;

int winWidth = SCR_WIDTH, winHeight = SCR_HEIGHT;

float modelMin[3] = {0}, modelMax[3] = {0}, modelCenter[3] = {0};
float modelSize = 1.0f;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            rotating = 1;
            glfwGetCursorPos(window, &lastX, &lastY);
        } else if (action == GLFW_RELEASE) {
            rotating = 0;
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!rotating) return;
    float sensitivity = 0.3f;
    float xoffset = (float)(xpos - lastX) * sensitivity * -1;
    float yoffset = (float)(ypos - lastY) * sensitivity * -1;
    lastX = xpos;
    lastY = ypos;
    yaw += xoffset;
    pitch += yoffset;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    winWidth = width;
    winHeight = height;
    glViewport(0, 0, width, height);
}

const char* vShader = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec2 aTexCoord;\n"
"layout (location = 2) in vec3 aNormal;\n"
"out vec2 TexCoord;\n"
"out vec3 FragPos;\n"
"out vec3 Normal;\n"
"uniform mat4 model, view, proj;\n"
"void main() {\n"
"    FragPos = vec3(model * vec4(aPos, 1.0));\n"
"    Normal = normalize(mat3(transpose(inverse(model))) * aNormal);\n"
"    TexCoord = aTexCoord;\n"
"    gl_Position = proj * view * model * vec4(aPos, 1.0);\n"
"}";

const char* fShader = "#version 330 core\n"
"in vec2 TexCoord;\n"
"in vec3 FragPos;\n"
"in vec3 Normal;\n"
"out vec4 FragColor;\n"
"uniform sampler2D tex_diffuse;\n"
"uniform sampler2D tex_normal;\n"
"uniform sampler2D tex_roughness;\n"
"uniform sampler2D tex_ao;\n"
"uniform vec3 lightPos;\n"
"uniform vec3 viewPos;\n"
"void main() {\n"
"    vec3 albedo = texture(tex_diffuse, TexCoord).rgb;\n"
"    vec3 N = normalize(Normal);\n"
"    vec3 nmap = texture(tex_normal, TexCoord).rgb;\n"
"    nmap = normalize(nmap * 2.0 - 1.0);\n"
"    N = normalize(N + nmap * 0.5);\n"
"    vec3 lightColor = vec3(1.0);\n"
"    vec3 L = normalize(lightPos - FragPos);\n"
"    float diff = max(dot(N, L), 0.0);\n"
"    vec3 V = normalize(viewPos - FragPos);\n"
"    vec3 R = reflect(-L, N);\n"
"    float roughness = texture(tex_roughness, TexCoord).r;\n"
"    float shininess = mix(2.0, 128.0, 1.0-roughness);\n"
"    float specAmount = pow(max(dot(V, R), 0.0), shininess);\n"
"    float ao = texture(tex_ao, TexCoord).r;\n"
"    vec3 color = (0.15 + 0.85*ao) * albedo * (0.4 + 0.6*diff) + 0.1 * specAmount * lightColor;\n"
"    FragColor = vec4(color, 1.0);\n"
"}";

void setIdentity(float* m) { for(int i=0;i<16;i++) m[i]=0; m[0]=m[5]=m[10]=m[15]=1; }
void setPerspective(float* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / (float)tan(fov/2.f);
    setIdentity(m);
    m[0]=f/aspect; m[5]=f; m[10]=(far+near)/(near-far); m[11]=-1.f; m[14]=(2*far*near)/(near-far);
}
void setView(float* m, float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
    float f[3] = {centerX-eyeX, centerY-eyeY, centerZ-eyeZ};
    float fmag = sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
    for (int i=0;i<3;i++) f[i] /= fmag;
    float up[3] = {upX,upY,upZ};
    float smag = sqrtf(up[0]*up[0]+up[1]*up[1]+up[2]*up[2]);
    for (int i=0;i<3;i++) up[i] /= smag;
    float s[3] = {
        f[1]*up[2] - f[2]*up[1],
        f[2]*up[0] - f[0]*up[2],
        f[0]*up[1] - f[1]*up[0]
    };
    float u[3] = {
        s[1]*f[2] - s[2]*f[1],
        s[2]*f[0] - s[0]*f[2],
        s[0]*f[1] - s[1]*f[0]
    };
    setIdentity(m);
    m[0]=s[0]; m[4]=s[1]; m[8]=s[2];
    m[1]=u[0]; m[5]=u[1]; m[9]=u[2];
    m[2]=-f[0]; m[6]=-f[1]; m[10]=-f[2];
    m[12]=-(s[0]*eyeX+s[1]*eyeY+s[2]*eyeZ);
    m[13]=-(u[0]*eyeX+u[1]*eyeY+u[2]*eyeZ);
    m[14]=f[0]*eyeX+f[1]*eyeY+f[2]*eyeZ;
}
void setScale(float* m, float x, float y, float z) {
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=x; m[5]=y; m[10]=z; m[15]=1;
}
void setTranslation(float* m, float x, float y, float z) {
    setIdentity(m); m[12]=x; m[13]=y; m[14]=z;
}
void setRotationXY(float* m, float angleX, float angleY) {
    float rx[16], ry[16];
    setIdentity(rx); setIdentity(ry);
    float cx = cosf(angleX), sx = sinf(angleX);
    float cy = cosf(angleY), sy = sinf(angleY);
    rx[5]=cx; rx[6]=-sx; rx[9]=sx; rx[10]=cx;
    ry[0]=cy; ry[2]=sy; ry[8]=-sy; ry[10]=cy;
    float temp[16];
    for (int i=0;i<16;i++) temp[i]=0;
    for (int row=0;row<4;row++)
        for (int col=0;col<4;col++)
            for (int k=0;k<4;k++)
                temp[row*4+col]+=ry[row*4+k]*rx[k*4+col];
    for (int i=0;i<16;i++) m[i]=temp[i];
}
void mul4x4(const float *a, const float *b, float *out) {
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++) {
            out[j+i*4]=0.0f;
            for(int k=0;k<4;k++)
                out[j+i*4]+=a[k+i*4]*b[j+k*4];
        }
}
void computeBoundingBox(const float *vertices, int count) {
    for (int i = 0; i < 3; ++i) modelMin[i] = modelMax[i] = vertices[i];
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < 3; ++j) {
            float v = vertices[i*3+j];
            if (v < modelMin[j]) modelMin[j] = v;
            if (v > modelMax[j]) modelMax[j] = v;
        }
    }
    for (int j = 0; j < 3; ++j)
        modelCenter[j] = (modelMin[j] + modelMax[j]) * 0.5f;
    float sizeX = modelMax[0] - modelMin[0];
    float sizeY = modelMax[1] - modelMin[1];
    float sizeZ = modelMax[2] - modelMin[2];
    modelSize = sizeX;
    if (sizeY > modelSize) modelSize = sizeY;
    if (sizeZ > modelSize) modelSize = sizeZ;
}

void loadFBX(
    const char* path,
    float** vertices, float** texcoords, float** normals, unsigned int** indices,
    int* vertex_count, int* index_count
) {
    const struct aiScene* scene = aiImportFile(path, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);
    if (!scene || scene->mNumMeshes == 0) {
        fprintf(stderr, "Failed to load FBX: %s\n", path);
        *vertices = NULL; *indices = NULL; *texcoords = NULL; *normals = NULL; *vertex_count = 0; *index_count = 0;
        return;
    }
    const struct aiMesh* mesh = scene->mMeshes[0];
    *vertex_count = mesh->mNumVertices;
    *index_count = mesh->mNumFaces * 3;
    *vertices = (float*)malloc(sizeof(float) * 3 * (*vertex_count));
    *texcoords = (float*)malloc(sizeof(float) * 2 * (*vertex_count));
    *normals = (float*)malloc(sizeof(float) * 3 * (*vertex_count));
    *indices = (unsigned int*)malloc(sizeof(unsigned int) * (*index_count));
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        (*vertices)[3*i+0] = mesh->mVertices[i].x;
        (*vertices)[3*i+1] = mesh->mVertices[i].y;
        (*vertices)[3*i+2] = mesh->mVertices[i].z;
        if (mesh->mTextureCoords[0]) {
            (*texcoords)[2*i+0] = mesh->mTextureCoords[0][i].x;
            (*texcoords)[2*i+1] = mesh->mTextureCoords[0][i].y;
        } else {
            (*texcoords)[2*i+0] = 0.0f;
            (*texcoords)[2*i+1] = 0.0f;
        }
        if (mesh->mNormals) {
            (*normals)[3*i+0] = mesh->mNormals[i].x;
            (*normals)[3*i+1] = mesh->mNormals[i].y;
            (*normals)[3*i+2] = mesh->mNormals[i].z;
        } else {
            (*normals)[3*i+0] = 0.0f;
            (*normals)[3*i+1] = 0.0f;
            (*normals)[3*i+2] = 1.0f;
        }
    }
    int idx = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const struct aiFace* f = &mesh->mFaces[i];
        if (f->mNumIndices == 3) {
            (*indices)[idx++] = f->mIndices[0];
            (*indices)[idx++] = f->mIndices[1];
            (*indices)[idx++] = f->mIndices[2];
        }
    }
    aiReleaseImport(scene);
}
GLuint loadTexture(const char* filename, int force_srgb) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    if (!data) {
        fprintf(stderr, "Failed to load texture: %s\n", filename);
        return 0;
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum internalFormat = force_srgb ? GL_SRGB_ALPHA : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return tex;
}
GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint success;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(s, 512, NULL, info);
        fprintf(stderr, "Shader compile error: %s\n", info);
    }
    return s;
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "ohh my waffle", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glEnable(GL_DEPTH_TEST);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Load FBX model
    float* vertices = NULL;
    float* texcoords = NULL;
    float* normals = NULL;
    unsigned int* indices = NULL;
    int vertex_count = 0, index_count = 0;
    loadFBX("waffle/waffle.fbx", &vertices, &texcoords, &normals, &indices, &vertex_count, &index_count);
    if (!vertices || !indices || !texcoords || !normals) { fprintf(stderr, "FBX loading failed.\n"); return -1; }

    // Compute bounding box, center, and size for scaling/centering
    computeBoundingBox(vertices, vertex_count);

    // OpenGL buffers
    GLuint VAO, VBO[3], EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(3, VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * 3 * sizeof(float), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * 2 * sizeof(float), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * 3 * sizeof(float), normals, GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    glBindVertexArray(0);

    // Compile and link shader
    GLuint vs = compileShader(GL_VERTEX_SHADER, vShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fShader);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    // Textures
    GLuint tex_diffuse = loadTexture("textures/waffle_diffuse.jpg", 1);
    GLuint tex_normal = loadTexture("textures/waffle_normal_ogl.jpg", 0);
    GLuint tex_roughness = loadTexture("textures/waffle_roughness.jpg", 0);
    GLuint tex_ao = loadTexture("textures/waffle_AO.png", 0);
    if (!tex_diffuse || !tex_normal || !tex_roughness || !tex_ao) { fprintf(stderr, "Texture loading failed.\n"); return -1; }

    // Uniform locations
    int modelLoc = glGetUniformLocation(prog, "model");
    int viewLoc = glGetUniformLocation(prog, "view");
    int projLoc = glGetUniformLocation(prog, "proj");
    int lightPosLoc = glGetUniformLocation(prog, "lightPos");
    int viewPosLoc = glGetUniformLocation(prog, "viewPos");

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "tex_diffuse"), 0);
    glUniform1i(glGetUniformLocation(prog, "tex_normal"), 1);
    glUniform1i(glGetUniformLocation(prog, "tex_roughness"), 2);
    glUniform1i(glGetUniformLocation(prog, "tex_ao"), 3);

    float scale[16], rot[16], trans[16], model[16], view[16], proj[16];
    float eyeX=0, eyeY=0.4, eyeZ=3.0;
    float upX=0, upY=1, upZ=0;
    float lightPos[3] = {2.0f, 4.0f, 3.0f};
    float viewPos[3] = {eyeX, eyeY, eyeZ};

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(0.19f,0.19f,0.225f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. Adaptive scale: model always 45% of min(winWidth, winHeight)
        float percent = 0.45f;
        float minWindow = (winWidth < winHeight ? winWidth : winHeight);
        float desiredScreenSize = percent * minWindow;
        // At camera at z=eyeZ, projected modelSize is: projected = s * modelSize * (winHeight/2) / (tan(fov/2)*eyeZ)
        // So: s = desiredScreenSize / (modelSize * (winHeight/2) / (tan(fov/2)*eyeZ))
        float fov = (float)M_PI/3.0f;
        float projFactor = (winHeight/2.0f) / (tanf(fov/2.0f) * eyeZ);
        float adaptiveScale = desiredScreenSize / (modelSize * projFactor);

        setScale(scale, adaptiveScale, adaptiveScale, adaptiveScale);
        setRotationXY(rot, pitch * M_PI/180.0f, yaw * M_PI/180.0f);
        setTranslation(trans, -modelCenter[0], -modelCenter[1], -modelCenter[2]);
        float tmp[16];
        mul4x4(rot, trans, tmp);
        mul4x4(scale, tmp, model);
        setView(view, eyeX, eyeY, eyeZ, 0, 0, 0, upX, upY, upZ);
        setPerspective(proj, fov, (float)winWidth/(float)winHeight, 0.1f, 100.0f);

        glUseProgram(prog);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj);
        glUniform3fv(lightPosLoc, 1, lightPos);
        glUniform3fv(viewPosLoc, 1, viewPos);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex_diffuse);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, tex_normal);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, tex_roughness);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, tex_ao);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);
        glfwSwapBuffers(window);
    }
    free(vertices); free(indices); free(texcoords); free(normals);
    glfwTerminate();
    return 0;
}