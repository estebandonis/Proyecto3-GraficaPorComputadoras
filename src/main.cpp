#include <SDL2/SDL.h>
#include <SDL_events.h>
#include <SDL_render.h>
#include <cstdlib>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/geometric.hpp>
#include <string>
#include <glm/glm.hpp>
#include <vector>
#include <print.h>

#include "color.h"
#include "intersect.h"
#include "object.h"
#include "sphere.h"
#include "cube.h"
#include "light.h"
#include "camera.h"
#include "skybox.h"

const int SCREEN_WIDTH = 500;
const int SCREEN_HEIGHT = 300;
const float ASPECT_RATIO = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
const int MAX_RECURSION = 4;
const float BIAS = 0.0001f;

Skybox skybox("assets/textures");
std::mutex pointMutex;

SDL_Renderer* renderer;
std::vector<Object*> objects;
Light light(glm::vec3(0, 5, 6), 6.0f, Color(255, 255, 255));
Camera camera(glm::vec3(0.0, 5.0, 6.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 4.0f, 0.0f), 10.0f);


void point(glm::vec2 position, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawPoint(renderer, position.x, position.y);
}

float castShadow(const glm::vec3& point, const glm::vec3& lightDir, const Object* hitObject) {
  float tNearShadow = INFINITY;
  for (const auto& object : objects) {
    if (object != hitObject) {
        float t = object->rayIntersect(point + lightDir * BIAS, lightDir).dist;
        if (t < tNearShadow) {
            tNearShadow = t;
        }
    }
  }
  return tNearShadow < 1 ? 0.5f : 1.0f;
}

Color sampleSkybox(const glm::vec3& direction) {
    // Convert direction vector to spherical coordinates
    float theta = atan2(direction.z, direction.x);
    float phi = acos(direction.y);
    
    // Convert spherical coordinates to texture coordinates
    float u = theta / (2 * M_PI) + 0.5f;
    float v = phi / M_PI;

    // Sample the skybox texture at the calculated coordinates
    return skybox.sample(u, v);
}

Color castRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const short recursion = 0) {
    float zBuffer = 99999;
    Object* hitObject = nullptr;
    Intersect intersect;

    for (const auto& object : objects) {
        Intersect i = object->rayIntersect(rayOrigin, rayDirection);
        if (i.isIntersecting && i.dist < zBuffer) {
            zBuffer = i.dist;
            hitObject = object;
            intersect = i;
        }
    }

    if (!intersect.isIntersecting || recursion == MAX_RECURSION) {
        return sampleSkybox(rayDirection);

    }

    glm::vec3 lightDir = glm::normalize(light.position - intersect.point);
    glm::vec3 viewDir = glm::normalize(rayOrigin - intersect.point);
    glm::vec3 reflectDir = glm::reflect(-lightDir, intersect.normal); 

    // Add a small bias to the origin of the shadow ray
    float shadowIntensity = castShadow(intersect.point + intersect.normal * BIAS, lightDir, hitObject);

    float diffuseLightIntensity = std::max(0.0f, glm::dot(intersect.normal, lightDir));
    float specReflection = glm::dot(viewDir, reflectDir);
    
    Material mat = hitObject->material;

    float specLightIntensity = std::pow(std::max(0.0f, glm::dot(viewDir, reflectDir)), mat.specularCoefficient);


    Color reflectedColor(0.0f, 0.0f, 0.0f);
    if (mat.reflectivity > 0) {
        glm::vec3 origin = intersect.point + intersect.normal * BIAS;
        reflectedColor = castRay(origin, reflectDir, recursion + 1); 
    }

    Color refractedColor(0.0f, 0.0f, 0.0f);
    if (mat.transparency > 0) {
        glm::vec3 normal = intersect.normal;
        float refractionIndex = mat.refractionIndex;
        if (glm::dot(rayDirection, normal) > 0) {
            normal = -normal;
            refractionIndex = 1 / refractionIndex;
        }
        glm::vec3 refractDir = glm::refract(rayDirection, normal, refractionIndex);
        refractedColor = castRay(intersect.point - normal * BIAS, refractDir, recursion + 1);
    }

    // Sample the color from the texture
    int x = intersect.uv.x * (mat.texture->w - 1);
    int y = intersect.uv.y * (mat.texture->h - 1);
    SDL_Color colorTexture;
    int bpp = mat.texture->format->BytesPerPixel;
    Uint8 *p = (Uint8 *)mat.texture->pixels + y * mat.texture->pitch + x * bpp;
    Uint32 pixel;
    switch (bpp) {
            case 1:
                pixel = *p;
                break;
            case 2:
                pixel = *(Uint16 *)p;
                break;
            case 3:
                if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                    pixel = p[0] << 16 | p[1] << 8 | p[2];
                } else {
                    pixel = p[0] | p[1] << 8 | p[2] << 16;
                }
                break;
            case 4:
                pixel = *(Uint32 *)p;
                break;
            default:
                throw std::runtime_error("Unknown format!");
        }
    SDL_GetRGB(pixel, mat.texture->format, &colorTexture.r, &colorTexture.g, &colorTexture.b);
    Color textureColor(colorTexture.r, colorTexture.g, colorTexture.b);

    Color diffuseLight = textureColor * light.intensity * diffuseLightIntensity * mat.albedo * shadowIntensity;
    Color specularLight = light.color * light.intensity * specLightIntensity * mat.specularAlbedo * shadowIntensity;
    Color color = (diffuseLight + specularLight) * (1.0f - mat.reflectivity - mat.transparency) + reflectedColor * mat.reflectivity + refractedColor * mat.transparency;
    return color;
} 

void setUp() {
    Material wood = {
        nullptr, // Load the texture here
        0.5,
        0.04,
        50.0f,
        0.02f,
        0.0f,
        1.54
    };

    wood.texture = IMG_Load("assets/wood.png");
    if (!wood.texture) {
        print("Error loading texture");
    }

    Material stone = {
        nullptr, // Load the texture here
        0.6,
        0.1,
        10.0f,
        0.05f,
        0.0f,
        1.54
    };

    stone.texture = IMG_Load("assets/stone.png");
    if (!stone.texture) {
        print("Error loading texture");
    }

    Material gold = {
        nullptr, // Load the texture here
        1.5f,
        0.4f,
        200.0f,
        0.4f,
        0.0f,
        0.47f
    };

    gold.texture = IMG_Load("assets/gold.png");
    if (!gold.texture) {
        print("Error loading texture");
    }

    Material water = {
        nullptr, // Load the texture here
        0.9,
        0.95,
        1000.0f,
        0.1f,
        0.55f,
        1.0f
    };

    water.texture = IMG_Load("assets/water.png");
    if (!water.texture) {
        print("Error loading texture");
    }

    Material dirt = {
        nullptr, // Load the texture here
        0.5,
        0.05,
        10.0f,
        0.05f,
        0.0f,
        1.54f
    };

    dirt.texture = IMG_Load("assets/dirt.png");
    if (!dirt.texture) {
        print("Error loading texture");
    }

    objects.push_back(new Cube(glm::vec3(-4.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(-4.0f, 0.0f, -1.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-4.0f, 0.0f, -2.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-4.0f, 0.0f, -3.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-4.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(-3.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(-3.0f, 0.0f, -2.0f), 1.0f, water));
    objects.push_back(new Cube(glm::vec3(-3.0f, -1.0f, -2.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-3.0f, 0.0f, -1.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-3.0f, 0.0f, -3.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-3.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(-2.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(-2.0f, 0.0f, -1.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-2.0f, 0.0f, -2.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-2.0f, 0.0f, -3.0f), 1.0f, stone));
    objects.push_back(new Cube(glm::vec3(-2.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(-1.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(-1.0f, 0.0f, -3.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(-1.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(0.0f, 0.0f, -1.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(0.0f, 0.0f, -2.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(0.0f, 0.0f, -3.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(0.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(4.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(4.0f, 0.0f, -1.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(4.0f, 0.0f, -2.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(4.0f, 0.0f, -3.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(4.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(3.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(3.0f, 0.0f, -1.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(3.0f, 0.0f, -2.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(3.0f, 0.0f, -3.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(3.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(2.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(2.0f, 0.0f, -1.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(2.0f, 0.0f, -2.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(2.0f, 0.0f, -3.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(2.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(1.0f, 0.0f, -1.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(1.0f, 0.0f, -2.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(1.0f, 0.0f, -3.0f), 1.0f, dirt));
    objects.push_back(new Cube(glm::vec3(1.0f, 0.0f, -4.0f), 1.0f, dirt));

    objects.push_back(new Cube(glm::vec3(-2.0f, 1.0f, -4.0f), 1.0f, gold));
    objects.push_back(new Cube(glm::vec3(-2.0f, 2.0f, -4.0f), 1.0f, gold));
    objects.push_back(new Cube(glm::vec3(-1.0f, -1.0f, -1.0f), 1.0f, gold));
    objects.push_back(new Cube(glm::vec3(-1.0f, -1.0f, -2.0f), 1.0f, gold));

    objects.push_back(new Cube(glm::vec3(1.0f, 1.0f, -1.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(1.0f, 2.0f, -1.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(1.0f, 3.0f, -1.0f), 1.0f, wood));

    objects.push_back(new Cube(glm::vec3(1.0f, 3.0f, -2.0f), 1.0f, wood));

    objects.push_back(new Cube(glm::vec3(1.0f, 1.0f, -3.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(1.0f, 2.0f, -3.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(1.0f, 3.0f, -3.0f), 1.0f, wood));

    objects.push_back(new Cube(glm::vec3(2.0f, 3.0f, -1.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(2.0f, 3.0f, -2.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(2.0f, 3.0f, -3.0f), 1.0f, wood));

    objects.push_back(new Cube(glm::vec3(3.0f, 1.0f, -1.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(3.0f, 2.0f, -1.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(3.0f, 3.0f, -1.0f), 1.0f, wood));

    objects.push_back(new Cube(glm::vec3(3.0f, 3.0f, -2.0f), 1.0f, wood));

    objects.push_back(new Cube(glm::vec3(3.0f, 1.0f, -3.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(3.0f, 2.0f, -3.0f), 1.0f, wood));
    objects.push_back(new Cube(glm::vec3(3.0f, 3.0f, -3.0f), 1.0f, wood));
}

void render() {
    float fov = 3.1415/3;
    #pragma omp parallel for
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {

            float screenX = (2.0f * (x + 0.5f)) / SCREEN_WIDTH - 1.0f;
            float screenY = -(2.0f * (y + 0.5f)) / SCREEN_HEIGHT + 1.0f;
            screenX *= ASPECT_RATIO;
            screenX *= tan(fov/2.0f);
            screenY *= tan(fov/2.0f);


            glm::vec3 cameraDir = glm::normalize(camera.target - camera.position);

            glm::vec3 cameraX = glm::normalize(glm::cross(cameraDir, camera.up));
            glm::vec3 cameraY = glm::normalize(glm::cross(cameraX, cameraDir));
            glm::vec3 rayDirection = glm::normalize(
                cameraDir + cameraX * screenX + cameraY * screenY
            );
           
            Color pixelColor = castRay(camera.position, rayDirection);

            point(glm::vec2(x, y), pixelColor);
        }
    }
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow("Hello World - FPS: 0", 
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          SCREEN_WIDTH, SCREEN_HEIGHT, 
                                          SDL_WINDOW_SHOWN);

    if (!window) {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create a renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!renderer) {
        SDL_Log("Unable to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    int frameCount = 0;
    Uint32 startTime = SDL_GetTicks();
    Uint32 currentTime = startTime;
    
    setUp();


    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN) {
                switch(event.key.keysym.sym) {
                    case SDLK_UP:
                        camera.move(1.0f);
                        break;
                    case SDLK_DOWN:
                        camera.move(-1.0f);
                        break;
                    case SDLK_LEFT:
                        print("left");
                        camera.rotate(-1.0f, 0.0f);
                        break;
                    case SDLK_RIGHT:
                        print("right");
                        camera.rotate(1.0f, 0.0f);
                        break;
                 }
            }


        }

        // Clear the screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        render();

        // Present the renderer
        SDL_RenderPresent(renderer);

        frameCount++;

        // Calculate and display FPS
        if (SDL_GetTicks() - currentTime >= 1000) {
            currentTime = SDL_GetTicks();
            std::string title = "Hello World - FPS: " + std::to_string(frameCount);
            SDL_SetWindowTitle(window, title.c_str());
            frameCount = 0;
        }
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

