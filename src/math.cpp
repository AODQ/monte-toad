#include "math.hpp"

////////////////////////////////////////////////////////////////////////////////
float glm::sqr(float i) { return i*i; }

////////////////////////////////////////////////////////////////////////////////
bvh::Vector3<float> ToBvh(glm::vec3 v) {
  return bvh::Vector3<float>(v.x, v.y, v.z);
}

////////////////////////////////////////////////////////////////////////////////
glm::vec3 ToGlm(bvh::Vector3<float> vertex) {
  return glm::vec3(vertex.values[0], vertex.values[1], vertex.values[2]);
}
