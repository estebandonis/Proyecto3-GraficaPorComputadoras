#include <SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <array>
#include "color.h"

class Skybox {
public:
    Skybox(const std::string& directory);
    Color loadTexture(const std::string& filename, int x, int y);
    Color sample(float u, float v);

private:
    std::array<SDL_Surface*, 6> textures; // The six textures for the skybox
};

Skybox::Skybox(const std::string& directory) {
    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        throw std::runtime_error("Failed to initialize SDL_image: " + std::string(IMG_GetError()));
    }

    // Load the six textures for the skybox
    textures[0] = IMG_Load((directory + "/right.png").c_str());
    textures[1] = IMG_Load((directory + "/back.png").c_str());
    textures[2] = IMG_Load((directory + "/top.png").c_str());
    textures[3] = IMG_Load((directory + "/bottom.png").c_str());
    textures[4] = IMG_Load((directory + "/front.png").c_str());
    textures[5] = IMG_Load((directory + "/left.png").c_str());

    // Check if textures loaded successfully
    for (auto& texture : textures) {
        if (!texture) {
            throw std::runtime_error("Failed to load texture: " + std::string(IMG_GetError()));
        }
    }
}

Color Skybox::sample(float u, float v) {
    // Convert spherical coordinates to cube map coordinates
    float phi = v * M_PI;
    float theta = u * 2 * M_PI;
    glm::vec3 direction(
        std::cos(theta) * std::sin(phi),
        std::cos(phi),
        std::sin(theta) * std::sin(phi)
    );

    // Determine which face of the skybox to sample
    int faceIndex;
    float uFace, vFace;
    float absX = std::abs(direction.x);
    float absY = std::abs(direction.y);
    float absZ = std::abs(direction.z);
    if (absX >= absY && absX >= absZ) {
        faceIndex = direction.x > 0 ? 0 : 1; // right or left
        uFace = -direction.z / absX;
        vFace = -direction.y / absX;
    } else if (absY >= absX && absY >= absZ) {
        faceIndex = direction.y > 0 ? 2 : 3; // top or bottom
        uFace = direction.x / absY;
        vFace = direction.z / absY;
    } else {
        faceIndex = direction.z > 0 ? 4 : 5; // front or back
        uFace = direction.x / absZ;
        vFace = -direction.y / absZ;
    }

    // Convert cube map coordinates to texture coordinates
    uFace = uFace * 0.5f + 0.5f;
    vFace = vFace * 0.5f + 0.5f;

    // Convert texture coordinates to pixel coordinates
    int x = static_cast<int>(uFace * textures[faceIndex]->w);
    int y = static_cast<int>(vFace * textures[faceIndex]->h);

    // Make sure the coordinates are within the texture's bounds
    if (x < 0 || y < 0 || x >= textures[faceIndex]->w || y >= textures[faceIndex]->h) {
        throw std::runtime_error("Texture coordinates out of bounds");
    }

    // Get the color of the pixel at the given coordinates
    int bpp = textures[faceIndex]->format->BytesPerPixel;
    Uint8 *p = (Uint8 *)textures[faceIndex]->pixels + y * textures[faceIndex]->pitch + x * bpp;
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
    SDL_Color colorTexture;
    SDL_GetRGB(pixel, textures[faceIndex]->format, &colorTexture.r, &colorTexture.g, &colorTexture.b);
    return Color(colorTexture.r, colorTexture.g, colorTexture.b);
}