#include "scene.hpp"

#include "log.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/pbrmaterial.h>
#include <glm/ext.hpp>

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
      , aiProcess_CalcTangentSpace
      | aiProcess_FindDegenerates
      | aiProcess_FixInfacingNormals
      | aiProcess_GenSmoothNormals
      | aiProcess_GlobalScale
      | aiProcess_ImproveCacheLocality
      | aiProcess_JoinIdenticalVertices
      | aiProcess_OptimizeGraph
      | aiProcess_OptimizeMeshes
      | aiProcess_PreTransformVertices
      | aiProcess_TransformUVCoords
      | aiProcess_Triangulate
      );

    model.basePath = std::filesystem::path{filename}.remove_filename();

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

        modelMesh.material =
          Material::Construct(*asset->mMaterials[mesh.mMaterialIndex], model);

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
          .emplace_back(
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

////////////////////////////////////////////////////////////////////////////////
void GetProperty(
  aiMaterial const & aiMaterial
, float & member
, std::string const & property, int type, int idx // generated from AI_MATKEY_*
) {
  if (
    ai_real value;
    aiMaterial.Get(property.c_str(), type, idx, value) == aiReturn_SUCCESS
  ) {
    member = value;
  }
}

////////////////////////////////////////////////////////////////////////////////
void GetProperty(
  aiMaterial const & aiMaterial
, glm::vec3 & member
, std::string const & property, int type, int idx // generated from AI_MATKEY_*
) {
  if (
    aiColor3D value;
    aiMaterial.Get(property.c_str(), type, idx, value) == aiReturn_SUCCESS
  ) {
    member = glm::vec3(value.r, value.g, value.b);
  }
}

////////////////////////////////////////////////////////////////////////////////
void GetProperty(
  aiMaterial const & aiMaterial
, std::string & member
, std::string const & property, int type, int idx // generated from AI_MATKEY_*
) {
  if (
    aiString value;
    aiMaterial.Get(property.c_str(), type, idx, value) == aiReturn_SUCCESS
  ) {
    member = std::string{value.C_Str()};
  }
}

} // -- end namespace


Material Material::Construct(aiMaterial const & aiMaterial, Scene & scene) {
  Material self;

  auto fixFilename = [&basePath=scene.basePath](aiString const & file) {
    auto filename = std::string{file.C_Str()};
    // Have to fix incoming file paths as they may be encoded using
    // windows file path "/", and std::filesystem will not handle this.
    std::replace(filename.begin(), filename.end(), '\\', '/');
    return (basePath / filename).string();
  };

  GetProperty(aiMaterial, self.name, AI_MATKEY_NAME);

  // -- attempt first to load global properties (ei for obj)
  GetProperty(aiMaterial, self.colorDiffuse,      AI_MATKEY_COLOR_DIFFUSE);
  GetProperty(aiMaterial, self.colorSpecular,     AI_MATKEY_COLOR_SPECULAR);
  GetProperty(aiMaterial, self.colorAmbient,      AI_MATKEY_COLOR_AMBIENT);
  GetProperty(aiMaterial, self.colorEmissive,     AI_MATKEY_COLOR_EMISSIVE);
  GetProperty(aiMaterial, self.colorTransparent,  AI_MATKEY_COLOR_TRANSPARENT);
  GetProperty(aiMaterial, self.opacity,           AI_MATKEY_OPACITY);
  GetProperty(aiMaterial, self.shininess,         AI_MATKEY_SHININESS);
  GetProperty(aiMaterial, self.shininessStrength, AI_MATKEY_SHININESS_STRENGTH);
  GetProperty(aiMaterial, self.indexOfRefraction, AI_MATKEY_REFRACTI);

  self.emissive = self.emissive | (glm::length(self.colorEmissive) > 0.001f);

  // load baseColor texture
  if (
    aiString textureFile;
    aiMaterial.GetTexture(
      AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE
    , &textureFile
    ) == aiReturn_SUCCESS
  ) {
    auto filename = fixFilename(textureFile);

    // try to find cached texture
    for (size_t i = 0; i < scene.textures.size(); ++ i) {
      if (scene.textures[i].filename == filename) {
        self.baseColorTextureIdx = i;
        break;
      }
    }

    // if not cached, create texture
    if (self.baseColorTextureIdx == static_cast<size_t>(-1)) {
      // TODO support embedded textures
      scene.textures.emplace_back(Texture::Construct(filename));
      self.baseColorTextureIdx = scene.textures.size() - 1;
    }
  }

  spdlog::debug("Loaded material {}", self.name);
  spdlog::debug("\t colorDiffuse:      {}", self.colorDiffuse     );
  spdlog::debug("\t colorSpecular:     {}", self.colorSpecular    );
  spdlog::debug("\t colorAmbient:      {}", self.colorAmbient     );
  spdlog::debug("\t colorEmissive:     {}", self.colorEmissive    );
  spdlog::debug("\t colorTransparent:  {}", self.colorTransparent );
  spdlog::debug("\t opacity:           {}", self.opacity          );
  spdlog::debug("\t shininess:         {}", self.shininess        );
  spdlog::debug("\t shininessStrength: {}", self.shininessStrength);
  spdlog::debug("\t emissive:          {}", self.emissive         );

  return self;
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

    for (auto const & tri : scene.accelStructure->triangles) {
      auto i = RayTriangleIntersection(ori, dir, tri);
      if (i && i->distance() < dist) {
        dist = i->distance();
        intersection = *i;
        closestTri = &tri;
      }
    }
    return { closestTri, intersection };
  }

  auto hit = IntersectClosest(*scene.accelStructure, ori, dir);
  if (!hit.has_value()) { return { nullptr, {} }; }
  return { scene.accelStructure->triangles.data() + hit->triangleIndex, *hit };
}
