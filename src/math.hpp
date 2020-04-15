#pragma once

#include <glm/glm.hpp>

#include <bvh/vector.hpp>

bvh::Vector3<float> ToBvh(glm::vec3 vertex);
glm::vec3 ToGlm(bvh::Vector3<float> vertex);
