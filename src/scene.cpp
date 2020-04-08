#include "scene.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "../ext/bvh.hpp"
#include <glm/ext.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>

namespace {
  //////////////////////////////////////////////////////////////////////////////
  std::vector<Triangle> LoadAssetIntoScene(
    Scene & model
  , std::string const & filename
  , bool upAxisZ
  ) {
    Assimp::Importer importer;
    aiScene const * asset =
      importer.ReadFile(
        filename
      , //aiProcess_CalcTangentSpace
        aiProcess_JoinIdenticalVertices
      | aiProcess_Triangulate
      /* | aiProcess_ImproveCacheLocality */ // might want to test this one
      /* | aiProcess_OptimizeMeshes */ // probably not useful right now
      /* | aiProcess_OptimizeGraph  */ // this is probably not useful either
      | aiProcess_PreTransformVertices
      );

    auto basePath = std::filesystem::path{filename}.remove_filename();

    if (!asset) {
      spdlog::error(
        "Could not load '{}': '{}'"
      , filename, importer.GetErrorString()
      );
      return {};
    }

    std::vector<Triangle> triangles;

    for (size_t meshIt = 0; meshIt < asset->mNumMeshes; ++ meshIt) {
      auto const & mesh = *asset->mMeshes[meshIt];

      { // -- process mesh
        Mesh modelMesh;

        aiMaterial * material = asset->mMaterials[mesh.mMaterialIndex];
        aiString textureFile;
        if (
          material->Get(
            AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0)
          , textureFile
          ) == aiReturn_SUCCESS
        ) {
          auto filename = std::string{textureFile.C_Str()};
          // Have to fix incoming file paths as they may be encoded using
          // windows file path "/", and std::filesystem will not handle this.
          std::replace(filename.begin(), filename.end(), '\\', '/');
          filename = (basePath / filename).string();

          // try to find cached texture
          modelMesh.diffuseTextureIdx = static_cast<size_t>(-1);

          for (size_t i = 0; i < model.textures.size(); ++ i) {
            if (model.textures[i].filename == filename) {
              modelMesh.diffuseTextureIdx = i;
              break;
            }
          }

          if (modelMesh.diffuseTextureIdx == static_cast<size_t>(-1)) {
            // TODO support embedded textures
            model.textures.emplace_back(Texture::Construct(filename));
            modelMesh.diffuseTextureIdx = model.textures.size() - 1;
          }
        }

        model.meshes.emplace_back(std::move(modelMesh));
      }
      uint16_t modelMeshIt = static_cast<uint16_t>(model.meshes.size()) - 1;

      for (size_t face = 0; face < mesh.mNumFaces; ++ face)
      for (size_t idx  = 0; idx < mesh.mFaces[face].mNumIndices/3; ++ idx) {

        size_t
          idx0 = idx*3 + 0,
          idx1 = idx*3 + 1,
          idx2 = idx*3 + 2
        ;

        aiVector3t<float>
          v0 = mesh.mVertices[mesh.mFaces[face].mIndices[idx0]]
        , v1 = mesh.mVertices[mesh.mFaces[face].mIndices[idx1]]
        , v2 = mesh.mVertices[mesh.mFaces[face].mIndices[idx2]]
        ;

        aiVector3t<float> n0, n1, n2;

        aiVector3t<float> uv0, uv1, uv2;

        if (mesh.HasNormals()) {
          n0 = mesh.mNormals[mesh.mFaces[face].mIndices[idx0]];
          n1 = mesh.mNormals[mesh.mFaces[face].mIndices[idx1]];
          n2 = mesh.mNormals[mesh.mFaces[face].mIndices[idx2]];
        }

        if (mesh.HasTextureCoords(0)) {
          uv0 = mesh.mTextureCoords[0][mesh.mFaces[face].mIndices[idx0]];
          uv1 = mesh.mTextureCoords[0][mesh.mFaces[face].mIndices[idx1]];
          uv2 = mesh.mTextureCoords[0][mesh.mFaces[face].mIndices[idx2]];
        }

        // add to scene
        triangles
          .push_back(
            Triangle (
              modelMeshIt
            , glm::vec3{v0.x, v0.y, v0.z}
            , glm::vec3{v1.x, v1.y, v1.z}
            , glm::vec3{v2.x, v2.y, v2.z}
            , glm::vec3{n0.x, n0.y, n0.z}
            , glm::vec3{n1.x, n1.y, n1.z}
            , glm::vec3{n2.x, n2.y, n2.z}
            , glm::abs(glm::vec2{uv0.x, uv0.y})
            , glm::abs(glm::vec2{uv1.x, uv1.y})
            , glm::abs(glm::vec2{uv2.x, uv2.y})
            )
          );

        // fix orientation of vertices if upAxisZ is set
        if (upAxisZ) {
          std::swap(triangles.back().v1, triangles.back().v2);
          triangles.back().v2 *= -1.0f;
        }

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

    return triangles;
  }
}

////////////////////////////////////////////////////////////////////////////////
Scene Scene::Construct(
  std::string const & filename
, std::string const & environmentMapFilename
, bool upAxisZ
, bool optimizeBvh
) {
  Scene scene;
  scene.bboxMin = glm::vec3(std::numeric_limits<float>::max());
  scene.bboxMax = glm::vec3(std::numeric_limits<float>::min());

  // load models & parse into BVH tree
  scene.accelStructure =
    AccelerationStructure::Construct(
      std::move(LoadAssetIntoScene(scene, filename, upAxisZ))
    , optimizeBvh
    );

  if(environmentMapFilename != "") {
    scene.environmentTexture = Texture::Construct(environmentMapFilename);
  }

  return scene;
}

////////////////////////////////////////////////////////////////////////////////
std::pair<Triangle const *, Intersection> Raycast(
  Scene const & scene
, glm::vec3 ori, glm::vec3 dir
, bool useBvh
) {
  if (!useBvh) {
    float dist = std::numeric_limits<float>::max();
    Triangle const * closestTri = nullptr;
    Intersection intersection;

    for (auto const & tri : scene.accelStructure.triangles) {
      auto i = RayTriangleIntersection(ori, dir, tri);
      if (i && i->distance < dist) {
        dist = i->distance;
        intersection = *i;
        closestTri = &tri;
      }
    }
    return { closestTri, intersection };
  }

  auto hit = IntersectClosest(scene.accelStructure, ori, dir);
  if (!hit.has_value()) { return { nullptr, {} }; }
  return { scene.accelStructure.triangles.data() + hit->first, hit->second };
}
