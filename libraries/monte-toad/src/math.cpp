#include <monte-toad/math.hpp>

//------------------------------------------------------------------------------
bool glm::GreaterThan(glm::vec3 a, glm::vec3 b) {
  return (a.x > b.x) || (a.y > b.y) || (a.z > b.z);
}

//------------------------------------------------------------------------------
bvh::Vector3<float> ToBvh(glm::vec3 v) {
  return bvh::Vector3<float>(v.x, v.y, v.z);
}

//------------------------------------------------------------------------------
glm::vec3 ToGlm(bvh::Vector3<float> vertex) {
  return glm::vec3(vertex.values[0], vertex.values[1], vertex.values[2]);
}
