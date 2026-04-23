#define GLM_ENABLE_EXPERIMENTAL
#pragma comment(lib, "winmm.lib")
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <vector>
#include <learnopengl/filesystem.h>
#include <learnopengl/model.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path);
unsigned int loadCubemap(vector<std::string> faces);


// Settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Camera
Camera camera(glm::vec3(0.0f, 100.0f, 500.0f)); 
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- Terrain Class ---
class HeightmapTerrain {
public:
    unsigned int terrainVAO, terrainVBO, terrainIBO;
    int width, height;
    int numStrips, numTrisPerStrip;

    // �ѧ��ѹ���´֧��Ҥ����٧ (y) �ҡ�����ŴԺ ��ШѴ��âͺ�Ҿ (Clamp)
    float getHeight(int x, int z, unsigned char* data, int w, int h, int chan) {
        if (x < 0) x = 0; if (x >= h) x = h - 1;
        if (z < 0) z = 0; if (z >= w) z = w - 1;
        return (float)data[(z + w * x) * chan];
    }

    HeightmapTerrain(const char* path) {
        int nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
        if (!data) return;

        std::vector<float> vertices;
        float yScale = 0.8f, yShift = 70.0f;
        float hScale = 8.0f;

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                // --- 1. Position ---
                float posY = getHeight(i, j, data, width, height, nrChannels) * yScale - yShift;
                float posX = (-height / 2.0f + i) * hScale;
                float posZ = (-width / 2.0f + j) * hScale;

                // --- 2. �ӹǳ Normal Ẻ����ԧ ---
                // �֧�����٧���͹��ҹ (���� ��� �� ��ҧ)
                float hL = getHeight(i - 1, j, data, width, height, nrChannels) * yScale;
                float hR = getHeight(i + 1, j, data, width, height, nrChannels) * yScale;
                float hD = getHeight(i, j - 1, data, width, height, nrChannels) * yScale;
                float hU = getHeight(i, j + 1, data, width, height, nrChannels) * yScale;

                // �ٵäӹǳ Normal �ҡ������ҧ�ͧ�����٧
                // hScale * 2.0 ���������ҧ�����ҧ�ش��š 3D
                glm::vec3 normal;
                normal.x = hL - hR;
                normal.y = 2.0f * hScale;
                normal.z = hD - hU;
                normal = glm::normalize(normal);

                // ��������� Array
                vertices.push_back(posX);
                vertices.push_back(posY);
                vertices.push_back(posZ);

                vertices.push_back(normal.x);
                vertices.push_back(normal.y);
                vertices.push_back(normal.z);

                vertices.push_back((float)i / height * 30.0f);
                vertices.push_back((float)j / width * 30.0f);
            }
        }
        stbi_image_free(data);

        // 2. ���ҧ Indices
        std::vector<unsigned int> indices;
        for (int i = 0; i < height - 1; i++) {
            for (int j = 0; j < width; j++) {
                for (int k = 0; k < 2; k++) {
                    indices.push_back(j + width * (i + k));
                }
            }
        }

        numStrips = height - 1;
        numTrisPerStrip = width * 2;

        glGenVertexArrays(1, &terrainVAO);
        glBindVertexArray(terrainVAO);

        glGenBuffers(1, &terrainVBO);
        glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glGenBuffers(1, &terrainIBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // Stride ��� 8 (3 Pos + 3 Norm + 2 Tex)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    }

    void Draw(Shader& shader) {
        glBindVertexArray(terrainVAO);
        for (int strip = 0; strip < numStrips; strip++) {
            glDrawElements(GL_TRIANGLE_STRIP, numTrisPerStrip, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * numTrisPerStrip * strip));
        }
    }
};

// -----------------------------------------------------------
// 1. �ѧ��ѹ���¤ӹǳ��ȷҧ˹������ͧ�Թ 
// -----------------------------------------------------------
glm::vec3 ForwardFromEuler(float yaw, float pitch) {
    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);
    float cy = cos(yawRad);
    float sy = sin(yawRad);
    float cp = cos(pitchRad);
    float sp = sin(pitchRad);
    return glm::vec3(-sy * cp, sp, -cy * cp);
}



// -----------------------------------------------------------
// 2. ���� Plane 
// -----------------------------------------------------------
struct Plane {
    glm::vec3 startingPos{ 0.0f, 150.0f, 0.0f }; // ��Ѻ��������٧���Ҿ�� Terrain �ͧ Code 1
    glm::vec3 Position{ startingPos };
    float Yaw = 0.0f;
    float Pitch = 0.0f;
    float Roll = 0.0f;

    float Stamina = 100.0f;
    const float MAX_STAMINA = 100.0f;
    float ConsumptionRate = 0.005f; // �ѵ�ҡ��Ŵ��ѧ�ҹ

    const float MAX_SPEED = 200.0f;
    const float MIN_SPEED = 20.0f;
    const float ACCELERATION = 20.0f;

    const float REAL_ROLL_RATE = 0.5f;
    const float MAX_ROLL_SPEED = 10.0f;
    const float INCREMENTAL_ROLL_SPEED = 4.8f;
    float ROLL_DAMPING = 1.5f;

    float Speed = 60.0f;
    float TurnSpeed = 30.0f;
    float PitchSpeed = 60.0f;
    float RollSpeed = 0.0f;

    Plane() = default;
    Plane(const glm::vec3& pos) : Position(pos) {}

    void TurnLeft(float dt) { Yaw += TurnSpeed * dt; }
    void TurnRight(float dt) { Yaw -= TurnSpeed * dt; }
    void PitchUp(float dt) { Pitch -= PitchSpeed * dt; Pitch = glm::clamp(Pitch, -85.0f, 85.0f); }
    void PitchDown(float dt) { Pitch += PitchSpeed * dt; Pitch = glm::clamp(Pitch, -85.0f, 85.0f); }
    void RollLeft(float dt) { 
        RollSpeed += INCREMENTAL_ROLL_SPEED * dt; 
        RollSpeed = glm::clamp(RollSpeed, -MAX_ROLL_SPEED, MAX_ROLL_SPEED); 
    }
    void RollRight(float dt) { RollSpeed -= INCREMENTAL_ROLL_SPEED * dt; RollSpeed = glm::clamp(RollSpeed, -MAX_ROLL_SPEED, MAX_ROLL_SPEED); }
    void SpeedUp(float dt) { 
        Speed += ACCELERATION * dt; Speed = glm::clamp(Speed, MIN_SPEED, MAX_SPEED);
    }
    void SlowDown(float dt) { Speed -= ACCELERATION * dt; Speed = glm::clamp(Speed, MIN_SPEED, MAX_SPEED); }
    void Reset() { 
        Position = startingPos; 
        Yaw = 0.0f;
        Pitch = 0.0f;
        Roll = 0.0f;
        Stamina = 100.0f;
        Speed = 60.0f;
    }


    glm::mat4 GetModelMatrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, Position);
        m = glm::rotate(m, glm::radians(Yaw), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(Pitch), glm::vec3(1, 0, 0));
        m = glm::rotate(m, glm::radians(Roll), glm::vec3(0, 0, 1));
        m = glm::rotate(m, glm::radians(-180.0f), glm::vec3(0, 1, 0));
        m = glm::scale(m, glm::vec3(0.5f));
        return m;
    }

    void Update(float dt) {
        if (dt <= 0.0f || dt > 0.1f) return;

        // --- ���� Logic Stamina ---
        // ��觺Թ���� Stamina ���Ŵ����
        if (Stamina > 0) {
            Stamina -= (Speed / MAX_SPEED) * ConsumptionRate;
        }
        else {
            Stamina = 0;
            Speed = glm::max(Speed - 10.0f * dt, MIN_SPEED); // ��Ҿ�ѧ�ҹ��� �������Ǩе�
        }

        float rollTurnRate = glm::sin(glm::radians(Roll)) * 0.8 * Speed * dt;
        Yaw += rollTurnRate;

        glm::vec3 forward = ForwardFromEuler(Yaw, Pitch);
        Position += forward * Speed * dt;
		// limit position ������Թ��ӡ��Ҿ��/�٧�Թ�
		if (Position.y < 50.0f) Position.y = 50.0f;
		if (Position.y > 600.0f) Position.y = 600.0f;

        if (std::abs(RollSpeed) > 0.0001f) {
            if (RollSpeed > 0.0f) {
                Roll += REAL_ROLL_RATE * RollSpeed;
                RollSpeed = std::max(RollSpeed - ROLL_DAMPING * dt, 0.0f);
            }
            else {
                Roll += REAL_ROLL_RATE * RollSpeed;
                RollSpeed = std::min(RollSpeed + ROLL_DAMPING * dt, 0.0f);
            }
        }
        else {
            //tiny reduce roll 
            if (Roll > 0.0f) {
                Roll = std::max(Roll - ROLL_DAMPING * dt * 10, 0.0f);
            }
            else {
                Roll = std::min(Roll + ROLL_DAMPING * dt * 10, 0.0f);
            }
        }
    }

    void RefillStamina(float amount) {
        Stamina = glm::min(Stamina + amount, MAX_STAMINA);
    }

};

// ���ҧ����� Global ����Ѻ������
Plane player(glm::vec3(0.0f, 150.0f, 0.0f));
bool followPlane = true; 
// ========== BOIDS SYSTEM ==========
#include <cmath>
#include <algorithm>

// ���������� Boids
const int   NUM_BOIDS = 500;
const float BOID_MAX_SPEED = 100.0f;
const float BOID_MAX_FORCE = 2.0f;
const float BOID_PERCEPTION = 600.0f;   // ������ͧ��繡ѹ
const float BOID_SEPARATION = 50.0f;    // ������¡���
const float BOID_PREDATOR_RAD = 250.0f;  // �����˹ռ�����

glm::vec3 LimitVec(glm::vec3 v, float max_val) {
    float mag = glm::length(v);
    if (mag > max_val && mag > 0.0f)
        return (v / mag) * max_val;
    return v;
}

struct BoidEntity {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;

    BoidEntity() {
        // �������˹觺���ͧ���
        position = glm::vec3(
            (rand() % 4000) - 2000.0f,
            150.0f + rand() % 200,
            (rand() % 4000) - 2000.0f
        );
        float angle = ((float)rand() / RAND_MAX) * glm::two_pi<float>();
        velocity = glm::vec3(cos(angle), 0.0f, sin(angle)) * BOID_MAX_SPEED * 0.5f;
        acceleration = glm::vec3(0.0f);
    }
    glm::vec3 StayInBounds() {
        glm::vec3 steer(0.0f);

        // �ӹǳ�ͺࢵ�ҡ (1024 * 8.0) / 2 = 4096
        float boundX = 4096.0f;
        float boundZ = 4096.0f;
        float margin = 400.0f; // ��������������� (������ 400 ���Щҡ���ҧ����ҡ)

        // ��Ǩ�ͺ�ͺ X
        if (position.x < -boundX + margin) steer.x = 1.0f;
        else if (position.x > boundX - margin) steer.x = -1.0f;

        // ��Ǩ�ͺ�ͺ Z
        if (position.z < -boundZ + margin) steer.z = 1.0f;
        else if (position.z > boundZ - margin) steer.z = -1.0f;

        // ��Ǩ�ͺ�����٧ (Y)
        if (position.y < 60.0f) steer.y = 1.0f;
        else if (position.y > 500.0f) steer.y = -1.0f;

        if (glm::length(steer) > 0.0f) {
            steer = glm::normalize(steer) * BOID_MAX_SPEED;
            glm::vec3 force = steer - velocity;
            return LimitVec(force, BOID_MAX_FORCE * 10.0f); // �����ç��ѡ����ç���������������Ƿѹ�ҡ���ҧ
        }
        return glm::vec3(0.0f);
    }
    // ---- �ç 3 �ç��ѡ ----
    glm::vec3 Separation(const std::vector<BoidEntity>& boids) {
        glm::vec3 steer(0.0f);
        int count = 0;
        for (auto& other : boids) {
            if (&other == this) continue;
            glm::vec3 diff = position - other.position;
            float dist = glm::length(diff);
            if (dist < BOID_SEPARATION && dist > 0.0f) {
                steer += glm::normalize(diff) / dist;
                count++;
            }
        }
        if (count > 0) {
            steer /= (float)count;
            if (glm::length(steer) > 0.0f)
                steer = glm::normalize(steer) * BOID_MAX_SPEED - velocity;
            steer = LimitVec(steer, BOID_MAX_FORCE);
        }
        return steer;
    }

    glm::vec3 Alignment(const std::vector<BoidEntity>& boids) {
        glm::vec3 avg(0.0f);
        int count = 0;
        for (auto& other : boids) {
            if (&other == this) continue;
            if (glm::distance(position, other.position) < BOID_PERCEPTION) {
                avg += other.velocity;
                count++;
            }
        }
        if (count > 0) {
            avg /= (float)count;
            if (glm::length(avg) > 0.0f)
                avg = glm::normalize(avg) * BOID_MAX_SPEED;
            glm::vec3 steer = avg - velocity;
            return LimitVec(steer, BOID_MAX_FORCE);
        }
        return glm::vec3(0.0f);
    }

    glm::vec3 Cohesion(const std::vector<BoidEntity>& boids) {
        glm::vec3 center(0.0f);
        int count = 0;
        for (auto& other : boids) {
            if (&other == this) continue;
            if (glm::distance(position, other.position) < BOID_PERCEPTION) {
                center += other.position;
                count++;
            }
        }
        if (count > 0) {
            center /= (float)count;
            glm::vec3 desired = center - position;
            if (glm::length(desired) > 0.0f)
                desired = glm::normalize(desired) * BOID_MAX_SPEED;
            glm::vec3 steer = desired - velocity;
            return LimitVec(steer, BOID_MAX_FORCE);
        }
        return glm::vec3(0.0f);
    }

    // ---- ˹ռ����� (Player) ----
    glm::vec3 FleePredator(const glm::vec3& predatorPos) {
        glm::vec3 diff = position - predatorPos;
        float dist = glm::length(diff);
        if (dist < BOID_PREDATOR_RAD && dist > 0.0f) {
            glm::vec3 desired = glm::normalize(diff) * BOID_MAX_SPEED;
            glm::vec3 steer = desired - velocity;
            return LimitVec(steer, BOID_MAX_FORCE * 3.0f); // ˹����ǡ��һ���
        }
        return glm::vec3(0.0f);
    }

    void Update(const std::vector<BoidEntity>& boids, const glm::vec3& predatorPos, float dt) {
        glm::vec3 sep = Separation(boids) * 1.0f;
        glm::vec3 ali = Alignment(boids) * 2.0f;
        glm::vec3 coh = Cohesion(boids) * 1.5f;
        glm::vec3 flee = FleePredator(predatorPos) * 2.5f;
        glm::vec3 bounds = StayInBounds() * 3.0f;

        acceleration = sep + ali + coh + flee + bounds; 

        velocity += acceleration * dt;
        velocity = LimitVec(velocity, BOID_MAX_SPEED);
        position += velocity * dt;

        // �ӡѴ������Թ��ӡ��Ҿ��
        if (position.y < 50.0f)  position.y = 50.0f;
        if (position.y > 600.0f) position.y = 600.0f;

        acceleration = glm::vec3(0.0f);
    }
};

// ========== GPU Instancing ����Ѻ Boids ==========
// �Ҵ Boid �繷ç���˹�� (tetrahedron) ��Ҵ���
struct BoidRenderer {
    unsigned int instanceVBO;
    Model* modelPtr; // �� pointer ��ѧ���� boid

    void Init(Model* targetModel) {
        modelPtr = targetModel;

        // 1. ���ҧ Instance VBO ����Ѻ�� mat4
        glGenBuffers(1, &instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, NUM_BOIDS * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

        // 2. �١ Instance VBO ��ҡѺ�ء� Mesh � Model
        // ���� Model class �ͧ LearnOpenGL ���� vector<Mesh> meshes;
        for (unsigned int i = 0; i < modelPtr->meshes.size(); i++) {
            unsigned int VAO = modelPtr->meshes[i].VAO;
            glBindVertexArray(VAO);

            // ��駤�� attribute location 3, 4, 5, 6 (��ͨҡ Pos, Norm, Tex � Mesh ����)
            for (int j = 0; j < 4; j++) {
                glEnableVertexAttribArray(3 + j);
                glVertexAttribPointer(3 + j, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(j * sizeof(glm::vec4)));
                glVertexAttribDivisor(3 + j, 1); // �͡����繢�����Ẻ per-instance
            }
            glBindVertexArray(0);
        }
    }

    void Draw(const std::vector<BoidEntity>& boids, Shader& shader) {
        std::vector<glm::mat4> matrices(boids.size());
        for (int i = 0; i < (int)boids.size(); i++) {
            auto& b = boids[i];
            glm::mat4 m(1.0f);
            // 1. ����ҧ�����˹� Boid 
            m = glm::translate(m, b.position);
            // 2. ��ع�����ȷҧ�������Ǵ��� quatLookAt
            if (glm::length(b.velocity) > 0.01f) {
                glm::vec3 direction = glm::normalize(b.velocity);
                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                glm::quat lookAtQuat = glm::quatLookAt(direction, up);
                m = m * glm::mat4_cast(lookAtQuat);
            }
            // 3. ��ع���ͻ�Ѻ���ȷҧ�ͧ���� .obj
            m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            // 4. ��Ѻ��Ҵ Scale
            m = glm::scale(m, glm::vec3(2.5f));
            matrices[i] = m;
        }

        // �ѻവ������� Instance VBO
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, matrices.size() * sizeof(glm::mat4), matrices.data());

        shader.use();
        for (unsigned int i = 0; i < modelPtr->meshes.size(); i++) {
            // --- ��ǹ�������: Bind Textures �ͧ Mesh ���� ---
            // ���� Mesh � LearnOpenGL �����ǡ���� textures
            for (unsigned int j = 0; j < modelPtr->meshes[i].textures.size(); j++) {
                glActiveTexture(GL_TEXTURE0 + j);
                // �觪��� uniform "texture_diffuse1" (���� 2, 3) ��ѧ shader
                string name = modelPtr->meshes[i].textures[j].type;
                shader.setInt(name, j);
                glBindTexture(GL_TEXTURE_2D, modelPtr->meshes[i].textures[j].id);
            }

            glBindVertexArray(modelPtr->meshes[i].VAO);
            glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)modelPtr->meshes[i].indices.size(), GL_UNSIGNED_INT, 0, (GLsizei)boids.size());
            glBindVertexArray(0);

            // �׹��� Active Texture
            glActiveTexture(GL_TEXTURE0);
        }
    }
};

void checkCollisions(Plane& player, std::vector<BoidEntity>& boids) {
    float collisionRadius = 15.0f; // ���з������Ҫ�

    // �� iterator �������ź�������͡�ҡ vector ���ʹ��¢��ǹ�ٻ
    for (auto it = boids.begin(); it != boids.end(); ) {
        float dist = glm::distance(player.Position, it->position);

        if (dist < collisionRadius) {
            // �Դ��ê�!
            player.RefillStamina(100.0f); // �����ѧ�ҹ 20 ˹���
            it = boids.erase(it);        // ź boid ��Ƿ�誹�͡
            std::cout << "Boid Captured! Remaining: " << boids.size() << std::endl;
            // SND_ASYNC = ������§Ẻ�����ش�� (������ҧ)
            // SND_FILENAME = �͡��ҵ���á��ͪ������
            PlaySound(TEXT(FileSystem::getPath("/resources/audio/pickup.wav").c_str()), NULL, SND_ASYNC | SND_FILENAME);
        }
        else {
            ++it;
        }
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Terrain System", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glEnable(GL_DEPTH_TEST);

    Shader terrainShader("terrain.vs", "terrain.fs");
    HeightmapTerrain terrain(FileSystem::getPath("resources/terrain/heightmaps/iceland_heightmap.png").c_str());
    // Load textures
    unsigned int texWater = loadTexture(FileSystem::getPath("resources/terrain/heightmaps/water.jpg").c_str());
    unsigned int texSand = loadTexture(FileSystem::getPath("resources/terrain/heightmaps/sand.jpg").c_str());
    unsigned int texGrass = loadTexture(FileSystem::getPath("resources/terrain/heightmaps/grass.jpg").c_str());
    unsigned int texRock = loadTexture(FileSystem::getPath("resources/terrain/heightmaps/rock.jpg").c_str());
    unsigned int texSnow = loadTexture(FileSystem::getPath("resources/terrain/heightmaps/snow.jpg").c_str());

    terrainShader.use();
    // --- ������ǹ Lighting �ç��� ---
    glm::vec3 lightPos(1200.0f, 1000.0f, 1200.0f); // ���˹觴ǧ�ҷԵ��
    terrainShader.setVec3("lightPos", lightPos);
    terrainShader.setVec3("lightColor", glm::vec3(1.0f, 0.9f, 0.8f)); // ���ⷹ���
    terrainShader.setVec3("viewPos", camera.Position);

    terrainShader.setInt("uTextureWater", 0);
    terrainShader.setInt("uTextureSand", 1);
    terrainShader.setInt("uTextureGrass", 2);
    terrainShader.setInt("uTextureRock", 3);
    terrainShader.setInt("uTextureSnow", 4);

    // build and compile shaders
    // -------------------------
    Shader skyboxShader("skybox.vs", "skybox.fs");

    // set up vertex data (and buffer(s)) and configure vertex attributes
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

    // skybox VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    vector<std::string> faces
    {
        FileSystem::getPath("resources/textures/skybox1/right.jpg"),
        FileSystem::getPath("resources/textures/skybox1/left.jpg"),
        FileSystem::getPath("resources/textures/skybox1/top.jpg"),
        FileSystem::getPath("resources/textures/skybox1/bottom.jpg"),
        FileSystem::getPath("resources/textures/skybox1/front.jpg"),
        FileSystem::getPath("resources/textures/skybox1/back.jpg")
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    // shader configuration
    // --------------------
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // -----------------------------------------------------------
    // 3. ��Ŵ Shader ��� Model �ͧ����ͧ�Թ
    // -----------------------------------------------------------
    Shader modelShader("model.vs", "model.fs"); // �س��ͧ�� Shader ��� (�֧�Ҩҡ Code 2 �����)
    Model planeModel(FileSystem::getPath("resources/objects/ship1/Untitled1.obj").c_str());

    // �����������ҧ���ͧ�ҡ����ͧ�Թ
    float camDistance = 30.0f;
    float camHeight = 10.0f;
    // ===== � main() ��ѧ load shader/model ���� =====

    // ���ҧ Boids
    std::vector<BoidEntity> boids(NUM_BOIDS);
    Model boidModel(FileSystem::getPath("resources/objects/ship1/Untitled.obj").c_str());
    BoidRenderer boidRenderer;
    boidRenderer.Init(&boidModel);

    Shader boidShader("boid.vs", "boid.fs");

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = (float) (currentFrame - lastFrame);
        lastFrame = currentFrame;

        processInput(window);

        // �ѻവ���˹�����ͧ�Թ��������ͧ��觵��
        player.Update(deltaTime);

        if (followPlane) {
            glm::vec3 forward = ForwardFromEuler(player.Yaw, player.Pitch);
            camera.Position = player.Position - forward * camDistance + glm::vec3(0.0f, camHeight, 0.0f);
            camera.Front = glm::normalize(player.Position - camera.Position);
        }


        // �ѻവ Boids �ء���
        for (auto& b : boids)
            b.Update(boids, player.Position, deltaTime);
        checkCollisions(player, boids);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // �Ҵ Terrain 
        terrainShader.use();
        glm::vec3 lightPos(2048.0f, 1500.0f, 2048.0f);
        terrainShader.setVec3("lightPos", lightPos);
        terrainShader.setVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.9f));
        glm::vec3 fogColor = glm::vec3(0.6f, 0.7f, 0.8f);
        float fogDensity = 0.0003f;
        terrainShader.setVec3("fogColor", fogColor);
        terrainShader.setFloat("fogDensity", fogDensity);
        terrainShader.setVec3("viewPos", camera.Position);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        terrainShader.setMat4("projection", projection);
        terrainShader.setMat4("view", view);
        terrainShader.setMat4("model", glm::mat4(1.0f));
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWater);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texSand);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, texGrass);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, texRock);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, texSnow);
        terrain.Draw(terrainShader);

        // �Ҵ Boids
        boidShader.use();
        boidShader.setMat4("view", view);
        boidShader.setMat4("projection", projection);
        boidShader.setVec3("lightPos", lightPos);
        boidShader.setVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.9f));
        boidRenderer.Draw(boids, boidShader);

        // �Ҵ��������ͧ�Թ
        modelShader.use();
        modelShader.setVec3("lightPos", lightPos);
        modelShader.setVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.9f));
        if (followPlane) {
            glm::mat4 rotationMatrix = glm::mat4(1.0f);
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(-player.Roll) / 2, glm::vec3(0.0f, 0.0f, 1.0f));
            view = rotationMatrix * view;
        }
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        modelShader.setMat4("model", player.GetModelMatrix());
        modelShader.setVec3("lightPos", lightPos);
        modelShader.setVec3("viewPos", camera.Position);
        modelShader.setVec3("lightColor", glm::vec3(1.0f, 0.95f, 0.9f));
        planeModel.Draw(modelShader);

        // �Ҵ Skybox 
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // 1. �������� ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. ���ҧ˹�ҵ�ҧ UI Ẻ�����
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin("Game Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::Text("Boids Remaining: %d", (int)boids.size());
        ImGui::Text("Speed: %.1f km/h", player.Speed);

        // ᶺ��ѧ�ҹ Stamina
        ImVec4 color = player.Stamina > 30 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
        ImGui::ProgressBar(player.Stamina / 100.0f, ImVec2(200, 20), "STAMINA");
        ImGui::PopStyleColor();
        ImGui::End();

        // 3. Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    // --- ��͹ glfwTerminate() ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

// Utility function (loadTexture, callbacks, processInput ����͹����ͧ�س)
unsigned int loadTexture(char const* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (nrComponents == 1) {
            GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        }

        stbi_image_free(data);
    }
    else {
        std::cout << "ERROR::TEXTURE::FAILED_TO_LOAD: " << path << std::endl;
    }
    return textureID;
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    // �ѧ�Ѻ����ͧ�Թ
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) player.PitchUp(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) player.PitchDown(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) player.TurnLeft(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) player.TurnRight(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) player.RollLeft(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) player.RollRight(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        if (player.Stamina > 0.0f) {
            player.SpeedUp(deltaTime);
        }
     };
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) player.SlowDown(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) player.Reset();
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    //camera.ProcessMouseMovement(xpos - lastX, lastY - ypos);
    //lastX = xpos; lastY = ypos;
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) { camera.ProcessMouseScroll(yoffset); }

unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    stbi_set_flip_vertically_on_load(false);
    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

