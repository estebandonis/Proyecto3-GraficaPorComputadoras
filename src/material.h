#pragma once

#include <SDL.h>
#include <SDL2/SDL_image.h>

#include "color.h"

struct Material {
  SDL_Surface* texture; // Add this line
  float albedo;
  float specularAlbedo;
  float specularCoefficient;
  float reflectivity;
  float transparency;
  float refractionIndex;
};
