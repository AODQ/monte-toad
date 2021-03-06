#include <monte-toad/core/scene.hpp>

#include <monte-toad/core/intersection.hpp>
#include <monte-toad/core/log.hpp>
#include <monte-toad/core/surfaceinfo.hpp>
#include <monte-toad/core/triangle.hpp>
#include <mt-plugin/plugin.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace {
////////////////////////////////////////////////////////////////////////////////
mt::core::TriangleMesh LoadAssetIntoScene(
  mt::core::Scene & model
, mt::PluginInfo const & plugin
, std::string const & filename
) {
  Assimp::Importer importer;

  spdlog::info("Loading scene '{}'", filename);

  aiScene const * asset =
    importer.ReadFile(
      filename
    , aiProcess_CalcTangentSpace
    | aiProcess_FindDegenerates
    | aiProcess_FixInfacingNormals
    /* | aiProcess_GenSmoothNormals */
    | aiProcess_GenNormals
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

  mt::core::TriangleMesh triangleMesh;

  for (size_t meshIt = 0; meshIt < asset->mNumMeshes; ++ meshIt) {
    auto const & mesh = *asset->mMeshes[meshIt];

    model.meshes.emplace_back(mt::core::Any(), meshIt);
    plugin.material.Allocate(model.meshes.back().material);

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
      triangleMesh.origins.emplace_back(glm::vec3{v0.x, v0.y, v0.z});
      triangleMesh.origins.emplace_back(glm::vec3{v1.x, v1.y, v1.z});
      triangleMesh.origins.emplace_back(glm::vec3{v2.x, v2.y, v2.z});
      triangleMesh.normals.emplace_back(glm::vec3{n0.x, n0.y, n0.z});
      triangleMesh.normals.emplace_back(glm::vec3{n1.x, n1.y, n1.z});
      triangleMesh.normals.emplace_back(glm::vec3{n2.x, n2.y, n2.z});
      triangleMesh.uvCoords.emplace_back(glm::abs(glm::vec2{uv0.x, uv0.y}));
      triangleMesh.uvCoords.emplace_back(glm::abs(glm::vec2{uv1.x, uv1.y}));
      triangleMesh.uvCoords.emplace_back(glm::abs(glm::vec2{uv2.x, uv2.y}));
      triangleMesh.meshIndices.emplace_back(meshIt);

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

  return triangleMesh;
}

/* //////////////////////////////////////////////////////////////////////////////// */
/* void GetProperty( */
/*   aiMaterial const & aiMaterial */
/* , float & member */
/* , std::string const & property, int type, int idx // generated from AI_MATKEY_* */
/* ) { */
/*   if ( */
/*     ai_real value; */
/*     aiMaterial.Get(property.c_str(), type, idx, value) == aiReturn_SUCCESS */
/*   ) { */
/*     member = value; */
/*   } */
/* } */

/* //////////////////////////////////////////////////////////////////////////////// */
/* void GetProperty( */
/*   aiMaterial const & aiMaterial */
/* , glm::vec3 & member */
/* , std::string const & property, int type, int idx // generated from AI_MATKEY_* */
/* ) { */
/*   if ( */
/*     aiColor3D value; */
/*     aiMaterial.Get(property.c_str(), type, idx, value) == aiReturn_SUCCESS */
/*   ) { */
/*     member = glm::vec3(value.r, value.g, value.b); */
/*   } */
/* } */

/* //////////////////////////////////////////////////////////////////////////////// */
/* void GetProperty( */
/*   aiMaterial const & aiMaterial */
/* , std::string & member */
/* , std::string const & property, int type, int idx // generated from AI_MATKEY_* */
/* ) { */
/*   if ( */
/*     aiString value; */
/*     aiMaterial.Get(property.c_str(), type, idx, value) == aiReturn_SUCCESS */
/*   ) { */
/*     member = std::string{value.C_Str()}; */
/*   } */
/* } */

} // -- end namespace

/* mt::Material mt::Material::Construct( */
/*   aiMaterial const & aiMaterial */
/* , mt::Scene & scene */
/* ) { */
/*   mt::Material self; */

/*   auto fixFilename = [&basePath=scene.basePath](aiString const & file) { */
/*     auto filename = std::string{file.C_Str()}; */
/*     // Have to fix incoming file paths as they may be encoded using */
/*     // windows file path "/", and std::filesystem will not handle this. */
/*     std::replace(filename.begin(), filename.end(), '\\', '/'); */
/*     return (basePath / filename).string(); */
/*   }; */

/*   GetProperty(aiMaterial, self.name, AI_MATKEY_NAME); */

/*   // -- attempt first to load global properties (ei for obj) */
/*   GetProperty(aiMaterial, self.colorDiffuse,      AI_MATKEY_COLOR_DIFFUSE); */
/*   GetProperty(aiMaterial, self.colorSpecular,     AI_MATKEY_COLOR_SPECULAR); */
/*   GetProperty(aiMaterial, self.colorAmbient,      AI_MATKEY_COLOR_AMBIENT); */
/*   GetProperty(aiMaterial, self.colorEmissive,     AI_MATKEY_COLOR_EMISSIVE); */
/*   GetProperty(aiMaterial, self.colorTransparent,  AI_MATKEY_COLOR_TRANSPARENT); */
/*   GetProperty(aiMaterial, self.transparency,      AI_MATKEY_OPACITY); */
/*   GetProperty(aiMaterial, self.shininess,         AI_MATKEY_SHININESS); */
/*   GetProperty(aiMaterial, self.shininessStrength, AI_MATKEY_SHININESS_STRENGTH); */
/*   GetProperty(aiMaterial, self.indexOfRefraction, AI_MATKEY_REFRACTI); */

/*   self.emissive = self.emissive | (glm::length(self.colorEmissive) > 0.0f); */
/*   self.specular = glm::length(self.colorSpecular) > 0.0f; */
/*   self.transparency = 1.0f - self.transparency; */

/*   // until multi-layer material support exists */
/*   if (self.transparency > 0.0f) self.specular = false; */

/*   // load baseColor texture */
/*   if ( */
/*     aiString textureFile; */
/*     aiMaterial.GetTexture( */
/*       /1* AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE *1/ */
/*       /1* AI_MATKEY_TEXTURE_DIFFUSE *1/ */
/*       aiTextureType_DIFFUSE, 0 */
/*     , &textureFile */
/*     ) == aiReturn_SUCCESS */
/*   ) { */
/*     auto filename = fixFilename(textureFile); */

/*     // try to find cached texture */
/*     for (size_t i = 0; i < scene.textures.size(); ++ i) { */
/*       if (scene.textures[i].filename == filename) { */
/*         self.baseColorTextureIdx = i; */
/*         break; */
/*       } */
/*     } */

/*     // if not cached, create texture */
/*     if (self.baseColorTextureIdx == static_cast<size_t>(-1)) { */
/*       // TODO support embedded textures */
/*       scene.textures.emplace_back(Texture::Construct(filename)); */
/*       self.baseColorTextureIdx = scene.textures.size() - 1; */
/*     } */
/*   } */

/*   spdlog::debug("Loaded material {}", self.name); */
/*   spdlog::debug("\t colorDiffuse:      {}", self.colorDiffuse     ); */
/*   spdlog::debug("\t colorSpecular:     {}", self.colorSpecular    ); */
/*   spdlog::debug("\t colorAmbient:      {}", self.colorAmbient     ); */
/*   spdlog::debug("\t colorEmissive:     {}", self.colorEmissive    ); */
/*   spdlog::debug("\t colorTransparent:  {}", self.colorTransparent ); */
/*   spdlog::debug("\t shininess:         {}", self.shininess        ); */
/*   spdlog::debug("\t shininessStrength: {}", self.shininessStrength); */
/*   spdlog::debug("\t emissive:          {}", self.emissive         ); */
/*   spdlog::debug("\t specular:          {}", self.specular         ); */
/*   spdlog::debug("\t transparency:      {}", self.transparency     ); */
/*   spdlog::debug("\t indexOfRefraction: {}", self.indexOfRefraction); */

/*   return self; */
/* } */

////////////////////////////////////////////////////////////////////////////////
void mt::core::Scene::Construct(
  mt::core::Scene & self
, mt::PluginInfo const & plugin
, std::string const & filename
) {
  self.bboxMin = glm::vec3(std::numeric_limits<float>::max());
  self.bboxMax = glm::vec3(std::numeric_limits<float>::min());

  // load models & parse into BVH tree
  self.meshes.clear();
  self.accelStructure =
    plugin
      .accelerationStructure
      .Construct(LoadAssetIntoScene(self, plugin, filename))
  ;
}

////////////////////////////////////////////////////////////////////////////////
mt::core::SurfaceInfo mt::core::Raycast(
  mt::core::Scene const & scene
, mt::PluginInfo const & plugin
, glm::vec3 ori, glm::vec3 dir
, size_t ignoredTriangle
) {
  auto const hit =
    plugin.accelerationStructure.IntersectClosest(
      scene.accelStructure, ori, dir, ignoredTriangle
    );

  if (!hit.has_value()) {
    return mt::core::SurfaceInfo::Construct(ori, dir);
  }

  return
    mt::core::SurfaceInfo::Construct(
      scene
    , plugin
        .accelerationStructure
        .GetTriangle(scene.accelStructure, hit->triangleIdx)
    , *hit
    , ori + dir*hit->length, dir
    );
}

#include <mt-plugin/plugin.hpp>

////////////////////////////////////////////////////////////////////////////////
std::tuple<mt::core::Triangle, glm::vec2>
mt::core::EmissionSourceTriangle(
  mt::core::Scene const & scene
, mt::PluginInfo const & plugin
) {
  if (scene.emissionSource.triangles.size() == 0)
    { return std::make_pair(mt::core::Triangle{}, glm::vec2()); }

  // this needs to take into account triangle surface area as that plays heavily
  // into which ones need to be sampled
  auto const tri =
    plugin
      .accelerationStructure
      .GetTriangle(
        scene.accelStructure
      , scene.emissionSource.triangles[
          plugin.random.SampleUniform1() * scene.emissionSource.triangles.size()
        ]
      );

  // generate random barycentric coords
  glm::vec2 u = plugin.random.SampleUniform2();
  u.y *= (1.0f - u.x);
  return std::make_pair(tri, u);
}
