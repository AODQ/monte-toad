#include "scene.hpp"

#include <spdlog/spdlog.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

////////////////////////////////////////////////////////////////////////////////
Scene Scene::Construct(std::string const & filename) {
  // -- load file
  Assimp::Importer importer;
  aiScene const * scene = importer.ReadFile(filename, 0);

  if (!scene) {
    spdlog::error(
      "Could not load '{}': '{}'"
    , filename, importer.GetErrorString()
    );
    return Scene{};
  }

  // -- load model
  Scene model;
  model.bboxMin = glm::vec3(std::numeric_limits<float>::max());
  model.bboxMax = glm::vec3(std::numeric_limits<float>::min());

  for (size_t meshIt = 0; meshIt < scene->mNumMeshes; ++ meshIt) {
    auto const & mesh = *scene->mMeshes[meshIt];
    for (size_t face = 0; face < mesh.mNumFaces; ++ face)
    for (size_t idx  = 0; idx < mesh.mFaces[face].mNumIndices/3; ++ idx) {
      aiVector3t<float>
        v0 = mesh.mVertices[mesh.mFaces[face].mIndices[idx*3 + 0]]
      , v1 = mesh.mVertices[mesh.mFaces[face].mIndices[idx*3 + 1]]
      , v2 = mesh.mVertices[mesh.mFaces[face].mIndices[idx*3 + 2]]
      ;

      aiVector3t<float> n0, n1, n2;

      if (mesh.mNormals) {
        n0 = mesh.mNormals[mesh.mFaces[face].mIndices[idx*3 + 0]];
        n1 = mesh.mNormals[mesh.mFaces[face].mIndices[idx*3 + 1]];
        n2 = mesh.mNormals[mesh.mFaces[face].mIndices[idx*3 + 2]];
      }

      // add to scene
      model.scene.push_back(
        Triangle (
          glm::vec3{v0.x, v0.y, v0.z}
        , glm::vec3{v1.x, v1.y, v1.z}
        , glm::vec3{v2.x, v2.y, v2.z}
        , glm::vec3{n0.x, n0.y, n0.z}
        , glm::vec3{n1.x, n1.y, n1.z}
        , glm::vec3{n2.x, n2.y, n2.z}
        )
      );

      // assign bbox
      for (auto vert : {v0, v1, v2}) {
        model.bboxMin =
          glm::vec3(
            glm::min(model.bboxMin.x, vert.x),
            glm::min(model.bboxMin.y, vert.y),
            glm::min(model.bboxMin.z, vert.z)
          );
        model.bboxMax =
          glm::vec3(
            glm::max(model.bboxMax.x, vert.x),
            glm::max(model.bboxMax.y, vert.y),
            glm::max(model.bboxMax.z, vert.z)
          );
      }
    }
  }

  return model;
}

////////////////////////////////////////////////////////////////////////////////
enum class CullFace { None, Front, Back };

////////////////////////////////////////////////////////////////////////////////
bool RayTriangleIntersection(
  glm::vec3 ori
, glm::vec3 dir
, Triangle const & triangle
, float & distance
, glm::vec2 & uvCoords
, CullFace cullFace = CullFace::None
, float epsilon = 0.0000001f
) {
  glm::vec3 ab = triangle.v1 - triangle.v0;
  glm::vec3 ac = triangle.v2 - triangle.v0;
  glm::vec3 pvec = glm::cross(dir, ac);
  float determinant = glm::dot(ab, pvec);

  if ((cullFace == CullFace::None  && glm::abs(determinant) <  epsilon)
   || (cullFace == CullFace::Back  &&          determinant  <  epsilon)
   || (cullFace == CullFace::Front &&          determinant  > -epsilon)
  ) {
    return false;
  }

  float determinantInv = 1.0f / determinant;

  glm::vec3 tvec = ori - triangle.v0;
  uvCoords.x = determinantInv * glm::dot(tvec, pvec);
  if (uvCoords.x < 0.0f || uvCoords.x > 1.0f) { return false; }

  glm::vec3 qvec = glm::cross(tvec, ab);
  uvCoords.y = determinantInv * glm::dot(dir, qvec);
  if (uvCoords.y < 0.0f || uvCoords.x + uvCoords.y > 1.0f) { return false; }

  distance = determinantInv * glm::dot(ac, qvec);
  return distance > 0.0f;
}

////////////////////////////////////////////////////////////////////////////////
Triangle const * Raycast(
  Scene const & model
, glm::vec3 ori, glm::vec3 dir
, float & distance, glm::vec2 & uv
) {
  distance = std::numeric_limits<float>::max();

  Triangle const * nearestTriangle = nullptr;

  for (auto const & tri : model.scene) {
    float triDist; glm::vec2 triUv;
    auto intersects = RayTriangleIntersection(ori, dir, tri, triDist, triUv);
    if (intersects && triDist < distance) {
      /* auto intersects = RayTriangleIntersection(ori, dir, tri, triDist, triUv) */
    /* ; intersects && triDist < distance */
    /* ) { */
      distance = triDist;
      uv = triUv;
      nearestTriangle = &tri;
    }
  }

  return nearestTriangle;
}
