#include "cube.h"

Cube::Cube(const glm::vec3& center, float side, const Material& mat)
  : center(center), side(side), half(side / 2.0f), Object(mat) {}

Intersect Cube::rayIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const {
  // Define the six planes of the cube
  std::array<glm::vec4, 6> planes = {
    glm::vec4(1, 0, 0, center.x + half),  // right
    glm::vec4(-1, 0, 0, -(center.x - half)),  // left
    glm::vec4(0, 1, 0, center.y + half),  // top
    glm::vec4(0, -1, 0, -(center.y - half)),  // bottom
    glm::vec4(0, 0, 1, center.z + half),  // front
    glm::vec4(0, 0, -1, -(center.z - half))  // back
  };

  float tNear = INFINITY;
  glm::vec3 normal;

  // For each plane, calculate the intersection distance
  for (const auto& plane : planes) {
    float denom = glm::dot(glm::vec3(plane), rayDirection);
    if (std::abs(denom) > 1e-6) {  // avoid division by zero
      float t = (plane.w - glm::dot(glm::vec3(plane), rayOrigin)) / denom;
      if (t >= 0 && t < tNear) {
        glm::vec3 point = rayOrigin + t * rayDirection;
        // Check if the intersection point is within the cube
        float epsilon = 1e-6f;  // adjust as needed

        if (std::abs(point.x - center.x) <= half + epsilon && 
            std::abs(point.y - center.y) <= half + epsilon && 
            std::abs(point.z - center.z) <= half + epsilon) {
          tNear = t;
          normal = glm::vec3(plane);

          // Adjust the normal based on the direction of the incoming ray
          if (glm::dot(normal, rayDirection) > 0) {
            normal = -normal;
          }
        }
      }
    }
  }

  if (tNear == INFINITY) {
    return Intersect{false};
  } else {
    glm::vec3 point = rayOrigin + tNear * rayDirection;
    return Intersect{true, tNear, point, normal};
  }
}