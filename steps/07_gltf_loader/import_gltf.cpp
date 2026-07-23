// Copyright 2026 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "import_gltf.h"

#include <anari/frontend/type_utility.h>
#include <tiny_gltf_v3.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

bool setParameterArray1D(ANARIDevice device,
    ANARIObject object,
    const char *name,
    ANARIDataType type,
    const void *values,
    uint64_t count)
{
  const size_t srcStride = anari::sizeOf(type);
  if (srcStride == 0 || count == 0)
    return false;

  uint64_t dstStride = 0;
  auto *dst = static_cast<uint8_t *>(
      anariMapParameterArray1D(device, object, name, type, count, &dstStride));
  if (!dst) {
    std::fprintf(stderr, "Failed to map parameter array '%s'\n", name);
    return false;
  }

  if (dstStride == 0)
    dstStride = srcStride;

  const auto *src = static_cast<const uint8_t *>(values);
  for (uint64_t i = 0; i < count; ++i)
    std::memcpy(dst + i * dstStride, src + i * srcStride, srcStride);

  anariUnmapParameterArray(device, object, name);
  return true;
}

int32_t fileExists(const char *path, uint32_t pathLen, void *)
{
  const std::string filename(path, pathLen);
  FILE *file = std::fopen(filename.c_str(), "rb");
  if (!file)
    return 0;
  std::fclose(file);
  return 1;
}

int32_t readFile(uint8_t **outData,
    uint64_t *outSize,
    const char *path,
    uint32_t pathLen,
    void *)
{
  *outData = nullptr;
  *outSize = 0;

  const std::string filename(path, pathLen);
  FILE *file = std::fopen(filename.c_str(), "rb");
  if (!file)
    return 0;

  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return 0;
  }
  const long size = std::ftell(file);
  if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return 0;
  }

  auto *data = static_cast<uint8_t *>(std::malloc(size_t(size)));
  if (!data) {
    std::fclose(file);
    return 0;
  }

  const size_t readSize = std::fread(data, 1, size_t(size), file);
  std::fclose(file);
  if (readSize != size_t(size)) {
    std::free(data);
    return 0;
  }

  *outData = data;
  *outSize = uint64_t(size);
  return 1;
}

void freeFile(uint8_t *data, uint64_t, void *)
{
  std::free(data);
}

struct Mat4
{
  float m[16] = {1.f,
      0.f,
      0.f,
      0.f,
      0.f,
      1.f,
      0.f,
      0.f,
      0.f,
      0.f,
      1.f,
      0.f,
      0.f,
      0.f,
      0.f,
      1.f};
};

Mat4 multiply(const Mat4 &a, const Mat4 &b)
{
  Mat4 r;
  for (int c = 0; c < 4; ++c) {
    for (int row = 0; row < 4; ++row) {
      r.m[c * 4 + row] = a.m[0 * 4 + row] * b.m[c * 4 + 0]
          + a.m[1 * 4 + row] * b.m[c * 4 + 1]
          + a.m[2 * 4 + row] * b.m[c * 4 + 2]
          + a.m[3 * 4 + row] * b.m[c * 4 + 3];
    }
  }
  return r;
}

Mat4 nodeTransform(const tg3_node &node)
{
  Mat4 r;
  if (node.has_matrix) {
    for (int i = 0; i < 16; ++i)
      r.m[i] = float(node.matrix[i]);
    return r;
  }

  const float tx = float(node.translation[0]);
  const float ty = float(node.translation[1]);
  const float tz = float(node.translation[2]);
  const float sx = float(node.scale[0]);
  const float sy = float(node.scale[1]);
  const float sz = float(node.scale[2]);
  const float x = float(node.rotation[0]);
  const float y = float(node.rotation[1]);
  const float z = float(node.rotation[2]);
  const float w = float(node.rotation[3]);

  const float xx = x * x;
  const float yy = y * y;
  const float zz = z * z;
  const float xy = x * y;
  const float xz = x * z;
  const float yz = y * z;
  const float wx = w * x;
  const float wy = w * y;
  const float wz = w * z;

  r.m[0] = (1.f - 2.f * (yy + zz)) * sx;
  r.m[1] = (2.f * (xy + wz)) * sx;
  r.m[2] = (2.f * (xz - wy)) * sx;
  r.m[4] = (2.f * (xy - wz)) * sy;
  r.m[5] = (1.f - 2.f * (xx + zz)) * sy;
  r.m[6] = (2.f * (yz + wx)) * sy;
  r.m[8] = (2.f * (xz + wy)) * sz;
  r.m[9] = (2.f * (yz - wx)) * sz;
  r.m[10] = (1.f - 2.f * (xx + yy)) * sz;
  r.m[12] = tx;
  r.m[13] = ty;
  r.m[14] = tz;
  return r;
}

void transformPoint(const Mat4 &m, const float in[3], float out[3])
{
  out[0] = m.m[0] * in[0] + m.m[4] * in[1] + m.m[8] * in[2] + m.m[12];
  out[1] = m.m[1] * in[0] + m.m[5] * in[1] + m.m[9] * in[2] + m.m[13];
  out[2] = m.m[2] * in[0] + m.m[6] * in[1] + m.m[10] * in[2] + m.m[14];
}

void transformVector(const Mat4 &m, const float in[3], float out[3])
{
  out[0] = m.m[0] * in[0] + m.m[4] * in[1] + m.m[8] * in[2];
  out[1] = m.m[1] * in[0] + m.m[5] * in[1] + m.m[9] * in[2];
  out[2] = m.m[2] * in[0] + m.m[6] * in[1] + m.m[10] * in[2];

  const float length =
      std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
  if (length > 0.f) {
    out[0] /= length;
    out[1] /= length;
    out[2] /= length;
  }
}

void transformNormal(const Mat4 &m, const float in[3], float out[3])
{
  const float a00 = m.m[0];
  const float a01 = m.m[4];
  const float a02 = m.m[8];
  const float a10 = m.m[1];
  const float a11 = m.m[5];
  const float a12 = m.m[9];
  const float a20 = m.m[2];
  const float a21 = m.m[6];
  const float a22 = m.m[10];

  const float c00 = a11 * a22 - a12 * a21;
  const float c01 = a12 * a20 - a10 * a22;
  const float c02 = a10 * a21 - a11 * a20;
  const float c10 = a02 * a21 - a01 * a22;
  const float c11 = a00 * a22 - a02 * a20;
  const float c12 = a01 * a20 - a00 * a21;
  const float c20 = a01 * a12 - a02 * a11;
  const float c21 = a02 * a10 - a00 * a12;
  const float c22 = a00 * a11 - a01 * a10;
  const float det = a00 * c00 + a01 * c01 + a02 * c02;

  if (std::fabs(det) <= 1e-20f) {
    transformVector(m, in, out);
    return;
  }

  const float invDet = 1.f / det;
  out[0] = (c00 * in[0] + c01 * in[1] + c02 * in[2]) * invDet;
  out[1] = (c10 * in[0] + c11 * in[1] + c12 * in[2]) * invDet;
  out[2] = (c20 * in[0] + c21 * in[1] + c22 * in[2]) * invDet;

  const float length =
      std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
  if (length > 0.f) {
    out[0] /= length;
    out[1] /= length;
    out[2] /= length;
  }
}

int32_t findAttribute(const tg3_primitive &primitive, const char *name)
{
  for (uint32_t i = 0; i < primitive.attributes_count; ++i) {
    if (tg3_str_equals_cstr(primitive.attributes[i].key, name))
      return primitive.attributes[i].value;
  }
  return TG3_INDEX_NONE;
}

const uint8_t *accessorElement(
    const tg3_model &model, const tg3_accessor &accessor, uint64_t index)
{
  if (accessor.buffer_view < 0
      || uint32_t(accessor.buffer_view) >= model.buffer_views_count)
    return nullptr;

  const tg3_buffer_view &view = model.buffer_views[accessor.buffer_view];
  if (view.buffer < 0 || uint32_t(view.buffer) >= model.buffers_count)
    return nullptr;

  const tg3_buffer &buffer = model.buffers[view.buffer];
  const int32_t stride = tg3_accessor_byte_stride(&accessor, &view);
  if (stride <= 0)
    return nullptr;

  const uint64_t offset =
      view.byte_offset + accessor.byte_offset + index * uint64_t(stride);
  const int32_t componentSize = tg3_component_size(accessor.component_type);
  const int32_t components = tg3_num_components(accessor.type);
  if (componentSize <= 0 || components <= 0)
    return nullptr;
  const uint64_t elementSize = uint64_t(componentSize) * uint64_t(components);
  if (offset > buffer.data.count || elementSize > buffer.data.count - offset)
    return nullptr;

  return buffer.data.data + offset;
}

bool readFloatAttribute(const tg3_model &model,
    int32_t accessorIndex,
    int32_t expectedType,
    std::vector<float> &values)
{
  if (accessorIndex < 0 || uint32_t(accessorIndex) >= model.accessors_count)
    return false;

  const tg3_accessor &accessor = model.accessors[accessorIndex];
  if (accessor.component_type != TG3_COMPONENT_TYPE_FLOAT
      || accessor.type != expectedType || accessor.sparse.is_sparse) {
    return false;
  }

  const int32_t components = tg3_num_components(accessor.type);
  values.resize(size_t(accessor.count) * size_t(components));
  for (uint64_t i = 0; i < accessor.count; ++i) {
    const uint8_t *src = accessorElement(model, accessor, i);
    if (!src)
      return false;
    std::memcpy(values.data() + size_t(i) * size_t(components),
        src,
        size_t(components) * sizeof(float));
  }
  return true;
}

bool readColorAttribute(
    const tg3_model &model, int32_t accessorIndex, std::vector<float> &colors)
{
  if (accessorIndex < 0 || uint32_t(accessorIndex) >= model.accessors_count)
    return false;

  const tg3_accessor &accessor = model.accessors[accessorIndex];
  if ((accessor.type != TG3_TYPE_VEC3 && accessor.type != TG3_TYPE_VEC4)
      || accessor.sparse.is_sparse) {
    return false;
  }

  const int32_t srcComponents = tg3_num_components(accessor.type);
  colors.resize(size_t(accessor.count) * 4);
  for (uint64_t i = 0; i < accessor.count; ++i) {
    const uint8_t *srcBytes = accessorElement(model, accessor, i);
    if (!srcBytes)
      return false;

    auto *dst = colors.data() + size_t(i) * 4;
    dst[3] = 1.f;

    for (int32_t c = 0; c < srcComponents; ++c) {
      switch (accessor.component_type) {
      case TG3_COMPONENT_TYPE_FLOAT:
        std::memcpy(dst + c, srcBytes + c * sizeof(float), sizeof(float));
        break;
      case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
        dst[c] = accessor.normalized ? float(srcBytes[c]) / 255.f
                                     : float(srcBytes[c]);
        break;
      case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t value = 0;
        std::memcpy(&value, srcBytes + c * sizeof(uint16_t), sizeof(uint16_t));
        dst[c] = accessor.normalized ? float(value) / 65535.f : float(value);
        break;
      }
      default:
        return false;
      }
    }
  }
  return true;
}

bool readIndices(const tg3_model &model,
    int32_t accessorIndex,
    uint64_t vertexCount,
    std::vector<uint32_t> &indices)
{
  if (accessorIndex < 0) {
    indices.resize(size_t(vertexCount));
    for (uint64_t i = 0; i < vertexCount; ++i)
      indices[size_t(i)] = uint32_t(i);
    return true;
  }

  if (uint32_t(accessorIndex) >= model.accessors_count)
    return false;

  const tg3_accessor &accessor = model.accessors[accessorIndex];
  if (accessor.type != TG3_TYPE_SCALAR || accessor.sparse.is_sparse)
    return false;

  indices.resize(size_t(accessor.count));
  for (uint64_t i = 0; i < accessor.count; ++i) {
    const uint8_t *src = accessorElement(model, accessor, i);
    if (!src)
      return false;

    switch (accessor.component_type) {
    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE: {
      uint8_t value = 0;
      std::memcpy(&value, src, sizeof(value));
      indices[size_t(i)] = value;
      break;
    }
    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
      uint16_t value = 0;
      std::memcpy(&value, src, sizeof(value));
      indices[size_t(i)] = value;
      break;
    }
    case TG3_COMPONENT_TYPE_UNSIGNED_INT: {
      uint32_t value = 0;
      std::memcpy(&value, src, sizeof(value));
      indices[size_t(i)] = value;
      break;
    }
    default:
      return false;
    }
  }
  return true;
}

bool makeTriangleIndices(int32_t mode,
    const std::vector<uint32_t> &src,
    std::vector<uint32_t> &triangles)
{
  mode = mode < 0 ? TG3_MODE_TRIANGLES : mode;
  triangles.clear();
  if (mode == TG3_MODE_TRIANGLES) {
    if (src.size() < 3 || src.size() % 3 != 0)
      return false;
    triangles = src;
    return true;
  }

  if (mode == TG3_MODE_TRIANGLE_STRIP) {
    if (src.size() < 3)
      return false;
    for (size_t i = 0; i + 2 < src.size(); ++i) {
      if (i % 2 == 0) {
        triangles.push_back(src[i]);
        triangles.push_back(src[i + 1]);
        triangles.push_back(src[i + 2]);
      } else {
        triangles.push_back(src[i + 1]);
        triangles.push_back(src[i]);
        triangles.push_back(src[i + 2]);
      }
    }
    return true;
  }

  if (mode == TG3_MODE_TRIANGLE_FAN) {
    if (src.size() < 3)
      return false;
    for (size_t i = 1; i + 1 < src.size(); ++i) {
      triangles.push_back(src[0]);
      triangles.push_back(src[i]);
      triangles.push_back(src[i + 1]);
    }
    return true;
  }

  return false;
}

bool importPrimitive(ANARIDevice device,
    const tg3_model &model,
    const tg3_primitive &primitive,
    const Mat4 &transform,
    std::vector<ANARISurface> &surfaces)
{
  const int32_t positionAccessor = findAttribute(primitive, "POSITION");
  std::vector<float> positions;
  if (!readFloatAttribute(model, positionAccessor, TG3_TYPE_VEC3, positions))
    return false;

  std::vector<float> normals;
  const bool hasNormals =
      readFloatAttribute(
          model, findAttribute(primitive, "NORMAL"), TG3_TYPE_VEC3, normals)
      && normals.size() == positions.size();

  std::vector<float> colors;
  const bool hasColors =
      readColorAttribute(model, findAttribute(primitive, "COLOR_0"), colors)
      && colors.size() / 4 == positions.size() / 3;

  for (size_t i = 0; i < positions.size(); i += 3) {
    float transformed[3] = {};
    transformPoint(transform, positions.data() + i, transformed);
    positions[i + 0] = transformed[0];
    positions[i + 1] = transformed[1];
    positions[i + 2] = transformed[2];
  }

  if (hasNormals) {
    for (size_t i = 0; i < normals.size(); i += 3) {
      float transformed[3] = {};
      transformNormal(transform, normals.data() + i, transformed);
      normals[i + 0] = transformed[0];
      normals[i + 1] = transformed[1];
      normals[i + 2] = transformed[2];
    }
  }

  std::vector<uint32_t> sourceIndices;
  std::vector<uint32_t> triangleIndices;
  if (!readIndices(
          model, primitive.indices, positions.size() / 3, sourceIndices)
      || !makeTriangleIndices(primitive.mode, sourceIndices, triangleIndices)) {
    return false;
  }

  ANARIGeometry geometry = anariNewGeometry(device, "triangle");
  if (!geometry)
    return false;

  bool ok = setParameterArray1D(device,
      geometry,
      "vertex.position",
      ANARI_FLOAT32_VEC3,
      positions.data(),
      positions.size() / 3);

  if (ok && hasNormals) {
    ok = setParameterArray1D(device,
        geometry,
        "vertex.normal",
        ANARI_FLOAT32_VEC3,
        normals.data(),
        normals.size() / 3);
  }

  if (ok && hasColors) {
    ok = setParameterArray1D(device,
        geometry,
        "vertex.color",
        ANARI_FLOAT32_VEC4,
        colors.data(),
        colors.size() / 4);
  }

  if (ok) {
    ok = setParameterArray1D(device,
        geometry,
        "primitive.index",
        ANARI_UINT32_VEC3,
        triangleIndices.data(),
        triangleIndices.size() / 3);
  }

  if (!ok) {
    anariRelease(device, geometry);
    return false;
  }
  anariCommitParameters(device, geometry);

  ANARIMaterial material = anariNewMaterial(device, "matte");
  if (!material) {
    anariRelease(device, geometry);
    return false;
  }

  if (hasColors) {
    anariSetParameter(device, material, "color", ANARI_STRING, "color");
  } else {
    float color[3] = {0.8f, 0.8f, 0.8f};
    if (primitive.material >= 0
        && uint32_t(primitive.material) < model.materials_count) {
      const tg3_material &gltfMaterial = model.materials[primitive.material];
      color[0] =
          float(gltfMaterial.pbr_metallic_roughness.base_color_factor[0]);
      color[1] =
          float(gltfMaterial.pbr_metallic_roughness.base_color_factor[1]);
      color[2] =
          float(gltfMaterial.pbr_metallic_roughness.base_color_factor[2]);
    }
    anariSetParameter(device, material, "color", ANARI_FLOAT32_VEC3, color);
  }
  anariCommitParameters(device, material);

  ANARISurface surface = anariNewSurface(device);
  if (!surface) {
    anariRelease(device, material);
    anariRelease(device, geometry);
    return false;
  }

  anariSetParameter(device, surface, "geometry", ANARI_GEOMETRY, &geometry);
  anariSetParameter(device, surface, "material", ANARI_MATERIAL, &material);
  anariCommitParameters(device, surface);
  anariRelease(device, material);
  anariRelease(device, geometry);

  surfaces.push_back(surface);
  return true;
}

bool importNode(ANARIDevice device,
    const tg3_model &model,
    int32_t nodeIndex,
    const Mat4 &parentTransform,
    std::vector<ANARISurface> &surfaces)
{
  if (nodeIndex < 0 || uint32_t(nodeIndex) >= model.nodes_count)
    return false;

  const tg3_node &node = model.nodes[nodeIndex];
  const Mat4 transform = multiply(parentTransform, nodeTransform(node));

  if (node.mesh >= 0) {
    if (uint32_t(node.mesh) >= model.meshes_count)
      return false;

    const tg3_mesh &mesh = model.meshes[node.mesh];
    for (uint32_t i = 0; i < mesh.primitives_count; ++i) {
      if (!importPrimitive(
              device, model, mesh.primitives[i], transform, surfaces)) {
        std::fprintf(stderr,
            "Skipping unsupported glTF primitive on node %d, mesh %d\n",
            nodeIndex,
            node.mesh);
      }
    }
  }

  for (uint32_t i = 0; i < node.children_count; ++i) {
    if (!importNode(device, model, node.children[i], transform, surfaces))
      return false;
  }

  return true;
}

void printGltfErrors(const tinygltf3::ErrorStack &errors)
{
  for (uint32_t i = 0; i < errors.count(); ++i) {
    const tg3_error_entry *entry = errors.entry(i);
    if (!entry)
      continue;
    std::fprintf(stderr,
        "glTF: %s%s%s\n",
        entry->message ? entry->message : "unknown error",
        entry->json_path ? " at " : "",
        entry->json_path ? entry->json_path : "");
  }
}

} // namespace

ANARIWorld import_gltf(ANARIDevice device, const char *filename)
{
  tg3_parse_options options;
  tg3_parse_options_init(&options);
  options.images_as_is = 1;
  options.fs.file_exists = fileExists;
  options.fs.read_file = readFile;
  options.fs.free_file = freeFile;

  tinygltf3::Model model;
  tinygltf3::ErrorStack errors;
  const tg3_error_code parseResult =
      tinygltf3::parse_file(model, errors, filename, &options);
  if (parseResult != TG3_OK || errors.has_error()) {
    std::fprintf(stderr, "Failed to parse glTF file '%s'\n", filename);
    printGltfErrors(errors);
    return nullptr;
  }

  const int32_t sceneIndex =
      model->default_scene >= 0 ? model->default_scene : 0;
  if (sceneIndex < 0 || uint32_t(sceneIndex) >= model->scenes_count) {
    std::fprintf(stderr, "glTF file '%s' contains no scene\n", filename);
    return nullptr;
  }

  std::vector<ANARISurface> surfaces;
  const tg3_scene &scene = model->scenes[sceneIndex];
  const Mat4 identity;
  for (uint32_t i = 0; i < scene.nodes_count; ++i) {
    if (!importNode(device, *model.get(), scene.nodes[i], identity, surfaces)) {
      for (ANARISurface surface : surfaces)
        anariRelease(device, surface);
      return nullptr;
    }
  }

  if (surfaces.empty()) {
    std::fprintf(stderr,
        "glTF file '%s' did not produce any ANARI surfaces\n",
        filename);
    return nullptr;
  }

  ANARIWorld world = anariNewWorld(device);
  if (!world) {
    for (ANARISurface surface : surfaces)
      anariRelease(device, surface);
    return nullptr;
  }

  if (!setParameterArray1D(device,
          world,
          "surface",
          ANARI_SURFACE,
          surfaces.data(),
          surfaces.size())) {
    for (ANARISurface surface : surfaces)
      anariRelease(device, surface);
    anariRelease(device, world);
    return nullptr;
  }

  anariCommitParameters(device, world);
  for (ANARISurface surface : surfaces)
    anariRelease(device, surface);
  return world;
}
