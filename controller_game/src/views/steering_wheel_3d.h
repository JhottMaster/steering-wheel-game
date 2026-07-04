#pragma once

#include <cmath>

#include "../game/game_logic.h"
#include "raylib.h"
#if defined(near)
#undef near
#endif
#if defined(far)
#undef far
#endif
#include "raymath.h"
#include "steering_wheel_3d_mesh.h"

struct SteeringWheel3DModel {
  Mesh rim = {};
  Mesh spokes = {};
  Mesh hub = {};
  Material rimMaterial = {};
  Material spokeMaterial = {};
  Material hubMaterial = {};
  bool loaded = false;
};

namespace steering_wheel_3d {
constexpr float kDegToRad = 0.017453292519943295769f;

inline Material MakeMaterial(Color color) {
  Material material = LoadMaterialDefault();
  material.maps[MATERIAL_MAP_DIFFUSE].color = color;
  return material;
}

inline SteeringWheel3DModel LoadSteeringWheel3DModel() {
  SteeringWheel3DModel model;
  model.rim = steering_wheel_3d_mesh::GenerateTorusMesh(1.05f, 0.105f, 72, 14);
  model.spokes = steering_wheel_3d_mesh::GenerateThreeSpokeMesh(0.18f, 0.92f, 0.145f, 0.105f);
  model.hub = steering_wheel_3d_mesh::GenerateCylinderMesh(0.28f, 0.18f, 36);
  model.rimMaterial = MakeMaterial(Color{42, 68, 82, 255});
  model.spokeMaterial = MakeMaterial(Color{75, 92, 104, 255});
  model.hubMaterial = MakeMaterial(Color{225, 230, 226, 255});
  model.loaded = true;
  return model;
}

inline void UnloadSteeringWheel3DModel(SteeringWheel3DModel* model) {
  if (!model->loaded) {
    return;
  }

  UnloadMesh(model->rim);
  UnloadMesh(model->spokes);
  UnloadMesh(model->hub);
  UnloadMaterial(model->rimMaterial);
  UnloadMaterial(model->spokeMaterial);
  UnloadMaterial(model->hubMaterial);
  model->loaded = false;
}

inline Matrix MatrixFromSensorQuaternion(const SensorQuaternion& sensorQuaternion,
                                         float fallbackRotationDeg, bool hasAnyPacket) {
  if (!hasAnyPacket) {
    return MatrixRotateZ(fallbackRotationDeg * kDegToRad);
  }

  const SensorQuaternion normalized = NormalizeQuaternion(sensorQuaternion);
  // The controller sensor axes do not line up with the render axes. Hardware testing selected
  // render xyz <- sensor xzy, with the sensor pitch component flipped for wheel/ring rotation.
  const Quaternion raylibQuaternion =
      Quaternion{normalized.x, normalized.z, -normalized.y, normalized.w};
  return QuaternionToMatrix(raylibQuaternion);
}

inline void DrawMeshPart(const Mesh& mesh, const Material& material, Matrix orientation,
                         Matrix baseTransform) {
  DrawMesh(mesh, material, MatrixMultiply(baseTransform, orientation));
}

inline void DrawSteeringWheel3D(const SteeringWheel3DModel& model,
                                const SensorQuaternion& orientation, bool hasAnyPacket,
                                float fallbackRotationDeg, Rectangle bounds) {
  if (!model.loaded) {
    return;
  }

  BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                   static_cast<int>(bounds.width), static_cast<int>(bounds.height));

  const Camera3D camera = {
      Vector3{0.0f, -0.12f, 4.25f},
      Vector3{0.0f, 0.0f, 0.0f},
      Vector3{0.0f, 1.0f, 0.0f},
      34.0f,
      CAMERA_PERSPECTIVE,
  };

  BeginMode3D(camera);
  const Matrix orientationMatrix =
      MatrixFromSensorQuaternion(orientation, fallbackRotationDeg, hasAnyPacket);
  const Matrix baseTransform = MatrixScale(0.68f, 0.68f, 0.68f);

  DrawMeshPart(model.rim, model.rimMaterial, orientationMatrix, baseTransform);
  DrawMeshPart(model.spokes, model.spokeMaterial, orientationMatrix, baseTransform);
  DrawMeshPart(model.hub, model.hubMaterial, orientationMatrix, baseTransform);
  EndMode3D();

  EndScissorMode();
}
}  // namespace steering_wheel_3d
