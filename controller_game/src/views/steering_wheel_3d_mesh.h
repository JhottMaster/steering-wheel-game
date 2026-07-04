#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "raylib.h"

namespace steering_wheel_3d_mesh {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

struct MeshBuilder {
  std::vector<Vector3> vertices;
  std::vector<Vector3> normals;
  std::vector<Vector2> texcoords;
  std::vector<unsigned short> indices;
};

inline unsigned short AddVertex(MeshBuilder* builder, Vector3 vertex, Vector3 normal,
                                Vector2 texcoord) {
  builder->vertices.push_back(vertex);
  builder->normals.push_back(normal);
  builder->texcoords.push_back(texcoord);
  return static_cast<unsigned short>(builder->vertices.size() - 1);
}

inline void AddQuad(MeshBuilder* builder, Vector3 a, Vector3 b, Vector3 c, Vector3 d,
                    Vector3 normal) {
  const unsigned short base = static_cast<unsigned short>(builder->vertices.size());
  AddVertex(builder, a, normal, Vector2{0.0f, 0.0f});
  AddVertex(builder, b, normal, Vector2{1.0f, 0.0f});
  AddVertex(builder, c, normal, Vector2{1.0f, 1.0f});
  AddVertex(builder, d, normal, Vector2{0.0f, 1.0f});
  builder->indices.push_back(base);
  builder->indices.push_back(static_cast<unsigned short>(base + 1));
  builder->indices.push_back(static_cast<unsigned short>(base + 2));
  builder->indices.push_back(base);
  builder->indices.push_back(static_cast<unsigned short>(base + 2));
  builder->indices.push_back(static_cast<unsigned short>(base + 3));
}

inline Mesh UploadBuiltMesh(const MeshBuilder& builder) {
  Mesh mesh = {};
  mesh.vertexCount = static_cast<int>(builder.vertices.size());
  mesh.triangleCount = static_cast<int>(builder.indices.size() / 3);
  mesh.vertices = static_cast<float*>(MemAlloc(sizeof(float) * 3 * builder.vertices.size()));
  mesh.normals = static_cast<float*>(MemAlloc(sizeof(float) * 3 * builder.normals.size()));
  mesh.texcoords = static_cast<float*>(MemAlloc(sizeof(float) * 2 * builder.texcoords.size()));
  mesh.indices =
      static_cast<unsigned short*>(MemAlloc(sizeof(unsigned short) * builder.indices.size()));

  for (size_t i = 0; i < builder.vertices.size(); ++i) {
    mesh.vertices[i * 3 + 0] = builder.vertices[i].x;
    mesh.vertices[i * 3 + 1] = builder.vertices[i].y;
    mesh.vertices[i * 3 + 2] = builder.vertices[i].z;
    mesh.normals[i * 3 + 0] = builder.normals[i].x;
    mesh.normals[i * 3 + 1] = builder.normals[i].y;
    mesh.normals[i * 3 + 2] = builder.normals[i].z;
    mesh.texcoords[i * 2 + 0] = builder.texcoords[i].x;
    mesh.texcoords[i * 2 + 1] = builder.texcoords[i].y;
  }

  std::copy(builder.indices.begin(), builder.indices.end(), mesh.indices);
  UploadMesh(&mesh, false);
  return mesh;
}

inline Mesh GenerateTorusMesh(float majorRadius, float tubeRadius, int ringSegments,
                              int tubeSegments) {
  MeshBuilder builder;
  for (int ring = 0; ring < ringSegments; ++ring) {
    const float u = static_cast<float>(ring) / static_cast<float>(ringSegments) * kTwoPi;
    const float cosU = std::cos(u);
    const float sinU = std::sin(u);
    for (int tube = 0; tube < tubeSegments; ++tube) {
      const float v = static_cast<float>(tube) / static_cast<float>(tubeSegments) * kTwoPi;
      const float cosV = std::cos(v);
      const float sinV = std::sin(v);
      const Vector3 normal = Vector3{cosU * cosV, sinU * cosV, sinV};
      const Vector3 vertex = Vector3{(majorRadius + tubeRadius * cosV) * cosU,
                                     (majorRadius + tubeRadius * cosV) * sinU,
                                     tubeRadius * sinV};
      AddVertex(&builder, vertex, normal,
                Vector2{static_cast<float>(ring) / ringSegments,
                        static_cast<float>(tube) / tubeSegments});
    }
  }

  for (int ring = 0; ring < ringSegments; ++ring) {
    const int nextRing = (ring + 1) % ringSegments;
    for (int tube = 0; tube < tubeSegments; ++tube) {
      const int nextTube = (tube + 1) % tubeSegments;
      const unsigned short a = static_cast<unsigned short>(ring * tubeSegments + tube);
      const unsigned short b = static_cast<unsigned short>(nextRing * tubeSegments + tube);
      const unsigned short c = static_cast<unsigned short>(nextRing * tubeSegments + nextTube);
      const unsigned short d = static_cast<unsigned short>(ring * tubeSegments + nextTube);
      builder.indices.push_back(a);
      builder.indices.push_back(b);
      builder.indices.push_back(c);
      builder.indices.push_back(a);
      builder.indices.push_back(c);
      builder.indices.push_back(d);
    }
  }

  return UploadBuiltMesh(builder);
}

inline Mesh GenerateCylinderMesh(float radius, float depth, int segments) {
  MeshBuilder builder;
  const float halfDepth = depth * 0.5f;

  for (int i = 0; i < segments; ++i) {
    const float angle = static_cast<float>(i) / static_cast<float>(segments) * kTwoPi;
    const float cosA = std::cos(angle);
    const float sinA = std::sin(angle);
    const Vector3 normal = Vector3{cosA, sinA, 0.0f};
    AddVertex(&builder, Vector3{radius * cosA, radius * sinA, -halfDepth}, normal,
              Vector2{static_cast<float>(i) / segments, 0.0f});
    AddVertex(&builder, Vector3{radius * cosA, radius * sinA, halfDepth}, normal,
              Vector2{static_cast<float>(i) / segments, 1.0f});
  }

  for (int i = 0; i < segments; ++i) {
    const int next = (i + 1) % segments;
    const unsigned short a = static_cast<unsigned short>(i * 2);
    const unsigned short b = static_cast<unsigned short>(next * 2);
    const unsigned short c = static_cast<unsigned short>(next * 2 + 1);
    const unsigned short d = static_cast<unsigned short>(i * 2 + 1);
    builder.indices.push_back(a);
    builder.indices.push_back(b);
    builder.indices.push_back(c);
    builder.indices.push_back(a);
    builder.indices.push_back(c);
    builder.indices.push_back(d);
  }

  const unsigned short frontCenter =
      AddVertex(&builder, Vector3{0.0f, 0.0f, halfDepth}, Vector3{0.0f, 0.0f, 1.0f},
                Vector2{0.5f, 0.5f});
  const unsigned short backCenter =
      AddVertex(&builder, Vector3{0.0f, 0.0f, -halfDepth}, Vector3{0.0f, 0.0f, -1.0f},
                Vector2{0.5f, 0.5f});
  for (int i = 0; i < segments; ++i) {
    const int next = (i + 1) % segments;
    builder.indices.push_back(frontCenter);
    builder.indices.push_back(static_cast<unsigned short>(i * 2 + 1));
    builder.indices.push_back(static_cast<unsigned short>(next * 2 + 1));
    builder.indices.push_back(backCenter);
    builder.indices.push_back(static_cast<unsigned short>(next * 2));
    builder.indices.push_back(static_cast<unsigned short>(i * 2));
  }

  return UploadBuiltMesh(builder);
}

inline Vector3 Add(Vector3 a, Vector3 b) {
  return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vector3 Scale(Vector3 vector, float scale) {
  return Vector3{vector.x * scale, vector.y * scale, vector.z * scale};
}

inline void AddSpokePrism(MeshBuilder* builder, float angleDeg, float innerRadius,
                          float outerRadius, float width, float depth) {
  const float angleRad = angleDeg * kPi / 180.0f;
  const Vector3 forward = Vector3{std::cos(angleRad), std::sin(angleRad), 0.0f};
  const Vector3 side = Vector3{-forward.y, forward.x, 0.0f};
  const Vector3 front = Vector3{0.0f, 0.0f, 1.0f};
  const Vector3 start = Scale(forward, innerRadius);
  const Vector3 end = Scale(forward, outerRadius);
  const float halfWidth = width * 0.5f;
  const float halfDepth = depth * 0.5f;

  const Vector3 s0 = Add(Add(start, Scale(side, -halfWidth)), Scale(front, -halfDepth));
  const Vector3 s1 = Add(Add(start, Scale(side, halfWidth)), Scale(front, -halfDepth));
  const Vector3 s2 = Add(Add(start, Scale(side, halfWidth)), Scale(front, halfDepth));
  const Vector3 s3 = Add(Add(start, Scale(side, -halfWidth)), Scale(front, halfDepth));
  const Vector3 e0 = Add(Add(end, Scale(side, -halfWidth)), Scale(front, -halfDepth));
  const Vector3 e1 = Add(Add(end, Scale(side, halfWidth)), Scale(front, -halfDepth));
  const Vector3 e2 = Add(Add(end, Scale(side, halfWidth)), Scale(front, halfDepth));
  const Vector3 e3 = Add(Add(end, Scale(side, -halfWidth)), Scale(front, halfDepth));

  AddQuad(builder, s3, e3, e2, s2, Vector3{0.0f, 0.0f, 1.0f});
  AddQuad(builder, s0, s1, e1, e0, Vector3{0.0f, 0.0f, -1.0f});
  AddQuad(builder, s1, s2, e2, e1, side);
  AddQuad(builder, s0, e0, e3, s3, Scale(side, -1.0f));
  AddQuad(builder, e0, e1, e2, e3, forward);
  AddQuad(builder, s0, s3, s2, s1, Scale(forward, -1.0f));
}

inline Mesh GenerateThreeSpokeMesh(float innerRadius, float outerRadius, float width,
                                   float depth) {
  MeshBuilder builder;
  AddSpokePrism(&builder, -90.0f, innerRadius, outerRadius, width, depth);
  AddSpokePrism(&builder, 30.0f, innerRadius, outerRadius, width, depth);
  AddSpokePrism(&builder, 150.0f, innerRadius, outerRadius, width, depth);
  return UploadBuiltMesh(builder);
}
}  // namespace steering_wheel_3d_mesh
