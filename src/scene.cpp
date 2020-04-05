#include "scene.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>

#include <filesystem>

namespace {
  //////////////////////////////////////////////////////////////////////////////
  enum class CullFace { None, Front, Back };

  //////////////////////////////////////////////////////////////////////////////
  bool RayTriangleIntersection(
    glm::vec3 ori
  , glm::vec3 dir
  , Triangle const & triangle
  , float & distance
  , glm::vec2 & uv
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
    uv.x = determinantInv * glm::dot(tvec, pvec);
    if (uv.x < 0.0f || uv.x > 1.0f) { return false; }

    glm::vec3 qvec = glm::cross(tvec, ab);
    uv.y = determinantInv * glm::dot(dir, qvec);
    if (uv.y < 0.0f || uv.x + uv.y > 1.0f) { return false; }

    distance = determinantInv * glm::dot(ac, qvec);
    return distance > 0.0f;
  }
}
template<> struct fmt::formatter<glm::vec2> {
  constexpr auto parse(format_parse_context & ctx) { return ctx.begin(); }

  template <typename FmtCtx> auto format(glm::vec2 const & vec, FmtCtx & ctx) {
    return format_to(ctx.out(), "({}, {})", vec.x, vec.y);
  }
};

////////////////////////////////////////////////////////////////////////////////
Scene Scene::Construct(std::string const & filename) {
  // -- load file
  Assimp::Importer importer;
  aiScene const * scene =
    importer.ReadFile(
      filename
    , //aiProcess_CalcTangentSpace
      aiProcess_JoinIdenticalVertices
    | aiProcess_Triangulate
    /* | aiProcess_ImproveCacheLocality */ // might want to test this one
    /* | aiProcess_OptimizeMeshes */ // probably not useful right now
    /* | aiProcess_OptimizeGraph  */ // this is probably not useful either
    );

  auto basePath = std::filesystem::path{filename}.remove_filename();

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

    {
      Mesh modelMesh;

      aiMaterial * material = scene->mMaterials[mesh.mMaterialIndex];
      aiString textureFile;
      if (
        material->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), textureFile)
      == aiReturn_SUCCESS
      ) {
        auto filename = std::string{textureFile.C_Str()};
        // Have to fix incoming file paths as they may be encoded using windows
        // file path "/", and std::filesystem will not handle this.
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
      model.scene.push_back(
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
