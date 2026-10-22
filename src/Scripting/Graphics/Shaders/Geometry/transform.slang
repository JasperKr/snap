module transform;

public struct Transform {
  public float4x4 ModelMatrix;
  public float4x4 NormalMatrix;
};

public uniform StructuredBuffer<Transform>.Handle ModelTransforms;

public Transform GetModelTransform(uint index) {
  return ModelTransforms[index];
}

public float4x4 GetModelMatrix(uint index) {
  return ModelTransforms[index].ModelMatrix;
}

public float4x4 GetNormalMatrix(uint index) {
  return ModelTransforms[index].NormalMatrix;
}
