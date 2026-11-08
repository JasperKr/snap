#include "matrix.hpp"
#include "Modules/Math/math.hpp"
#include <cassert>
#include <cmath>
#include <lua.hpp>

namespace Math {

Matrix4x4::Matrix4x4(Matrix3x3 init) {
  for (size_t row = 0; row < Matrix3x3::Rows; ++row) {
    for (size_t col = 0; col < Matrix3x3::Cols; ++col) {
      At(row, col) = init.At(row, col);
    }
  }
  At(3, 3) = 1.0F;
}

auto Matrix4x4::FromRows(std::initializer_list<float> init) -> Matrix4x4 { return {init}; }

auto Matrix4x4::At(size_t row, size_t col) -> float & {
#ifndef NDEBUG
  assert(row < 4 && col < 4);
#endif
  return elements.at((row * Cols) + col);
}

auto Matrix4x4::At(size_t row, size_t col) const -> float {
#ifndef NDEBUG
  assert(row < 4 && col < 4);
#endif
  return elements.at((row * Cols) + col);
}

auto Matrix4x4::At(size_t index) -> float & {
#ifndef NDEBUG
  assert(index < Size);
#endif
  return elements.at(index);
}

auto Matrix4x4::At(size_t index) const -> float {
#ifndef NDEBUG
  assert(index < Size);
#endif
  return elements.at(index);
}

Matrix4x4::Matrix4x4() {
  At(0) = 1.0F;
  At(1, 1) = 1.0F;
  At(2, 2) = 1.0F;
  At(3, 3) = 1.0F;
}

auto Matrix4x4::Determinant() const -> std::pair<float, Matrix4x4> {
  Matrix4x4 inv;

  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

  inv[0] = (At(5) * At(10) * At(15)) - (At(5) * At(11) * At(14)) - (At(9) * At(6) * At(15)) + (At(9) * At(7) * At(14)) + (At(13) * At(6) * At(11)) -
           (At(13) * At(7) * At(10));

  inv[4] = (-At(4) * At(10) * At(15)) + (At(4) * At(11) * At(14)) + (At(8) * At(6) * At(15)) - (At(8) * At(7) * At(14)) - (At(12) * At(6) * At(11)) +
           (At(12) * At(7) * At(10));

  inv[8] = (At(4) * At(9) * At(15)) - (At(4) * At(11) * At(13)) - (At(8) * At(5) * At(15)) + (At(8) * At(7) * At(13)) + (At(12) * At(5) * At(11)) -
           (At(12) * At(7) * At(9));

  inv[12] = (-At(4) * At(9) * At(14)) + (At(4) * At(10) * At(13)) + (At(8) * At(5) * At(14)) - (At(8) * At(6) * At(13)) - (At(12) * At(5) * At(10)) +
            (At(12) * At(6) * At(9));

  inv[1] = (-At(1) * At(10) * At(15)) + (At(1) * At(11) * At(14)) + (At(9) * At(2) * At(15)) - (At(9) * At(3) * At(14)) - (At(13) * At(2) * At(11)) +
           (At(13) * At(3) * At(10));

  inv[5] = (At(0) * At(10) * At(15)) - (At(0) * At(11) * At(14)) - (At(8) * At(2) * At(15)) + (At(8) * At(3) * At(14)) + (At(12) * At(2) * At(11)) -
           (At(12) * At(3) * At(10));

  inv[9] = (-At(0) * At(9) * At(15)) + (At(0) * At(11) * At(13)) + (At(8) * At(1) * At(15)) - (At(8) * At(3) * At(13)) - (At(12) * At(1) * At(11)) +
           (At(12) * At(3) * At(9));

  inv[13] = (At(0) * At(9) * At(14)) - (At(0) * At(10) * At(13)) - (At(8) * At(1) * At(14)) + (At(8) * At(2) * At(13)) + (At(12) * At(1) * At(10)) -
            (At(12) * At(2) * At(9));

  inv[2] = (At(1) * At(6) * At(15)) - (At(1) * At(7) * At(14)) - (At(5) * At(2) * At(15)) + (At(5) * At(3) * At(14)) + (At(13) * At(2) * At(7)) -
           (At(13) * At(3) * At(6));

  inv[6] = (-At(0) * At(6) * At(15)) + (At(0) * At(7) * At(14)) + (At(4) * At(2) * At(15)) - (At(4) * At(3) * At(14)) - (At(12) * At(2) * At(7)) +
           (At(12) * At(3) * At(6));

  inv[10] = (At(0) * At(5) * At(15)) - (At(0) * At(7) * At(13)) - (At(4) * At(1) * At(15)) + (At(4) * At(3) * At(13)) + (At(12) * At(1) * At(7)) -
            (At(12) * At(3) * At(5));

  inv[14] = (-At(0) * At(5) * At(14)) + (At(0) * At(6) * At(13)) + (At(4) * At(1) * At(14)) - (At(4) * At(2) * At(13)) - (At(12) * At(1) * At(6)) +
            (At(12) * At(2) * At(5));

  inv[3] = (-At(1) * At(6) * At(11)) + (At(1) * At(7) * At(10)) + (At(5) * At(2) * At(11)) - (At(5) * At(3) * At(10)) - (At(9) * At(2) * At(7)) +
           (At(9) * At(3) * At(6));

  inv[7] = (At(0) * At(6) * At(11)) - (At(0) * At(7) * At(10)) - (At(4) * At(2) * At(11)) + (At(4) * At(3) * At(10)) + (At(8) * At(2) * At(7)) -
           (At(8) * At(3) * At(6));

  inv[11] = (-At(0) * At(5) * At(11)) + (At(0) * At(7) * At(9)) + (At(4) * At(1) * At(11)) - (At(4) * At(3) * At(9)) - (At(8) * At(1) * At(7)) +
            (At(8) * At(3) * At(5));

  inv[15] = (At(0) * At(5) * At(10)) - (At(0) * At(6) * At(9)) - (At(4) * At(1) * At(10)) + (At(4) * At(2) * At(9)) + (At(8) * At(1) * At(6)) -
            ((At(8) * At(2) * At(5)));

  float det = (At(0) * inv[0]) + (At(1) * inv[4]) + (At(2) * inv[8]) + (At(3) * inv[12]);

  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

  return {det, inv};
}

auto Matrix4x4::Transpose() const -> Matrix4x4 {
  Matrix4x4 result{};
  for (size_t row = 0; row < Rows; ++row) {
    for (size_t col = 0; col < Cols; ++col) {
      // NOLINTNEXTLINE, clangd thinks row, col are swapped, but that's intentional
      result.At(col, row) = At(row, col);
    }
  }
  return result;
}

auto Matrix4x4::AsMatrix3x3() const -> Matrix4x4 {
  Matrix4x4 result = *this;
  result.At(0, 3) = 0.0F;
  result.At(1, 3) = 0.0F;
  result.At(2, 3) = 0.0F;
  result.At(3, 0) = 0.0F;
  result.At(3, 1) = 0.0F;
  result.At(3, 2) = 0.0F;
  result.At(3, 3) = 1.0F;

  /*

  [ m00 m01 m02 m03 ]   [ m00 m01 m02 0 ]
  [ m10 m11 m12 m13 ] = [ m10 m11 m12 0 ]
  [ m20 m21 m22 m23 ]   [ m20 m21 m22 0 ]
  [ m30 m31 m32 m33 ]   [ 0   0   0   1 ]

  */

  return result;
}

auto Matrix4x4::operator*(const Matrix4x4 &other) const -> Matrix4x4 {
  Matrix4x4 result{};
#pragma unroll
  for (size_t row = 0; row < Rows; ++row) {
#pragma unroll
    for (size_t col = 0; col < Cols; ++col) {
      float sum = 0.0F;
#pragma unroll
      for (size_t k = 0; k < Cols; ++k) {
        sum += At(row, k) * other.At(k, col);
      }
      result.At(row, col) = sum;
    }
  }
  return result;
}

// auto operator*(const Vec4 &vec) const -> Vec4;

auto Matrix4x4::operator*(const Vec4 &vec) const -> Vec4 {
  Vec4 result{};
#pragma unroll
  for (size_t row = 0; row < Rows; ++row) {
    float sum = 0.0F;
#pragma unroll
    for (size_t col = 0; col < Cols; ++col) {
      sum += At(row, col) * vec[col];
    }
    result[row] = sum;
  }
  return result;
}

auto Matrix4x4::operator==(const Matrix4x4 &other) const -> bool { return elements == other.elements; }

auto Matrix4x4::Inverse() const -> Matrix4x4 {
  auto detPair = Determinant();
  auto det = detPair.first;
  auto inv = detPair.second;

  if (det == 0.0F) {
    return Matrix4x4{NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  }

  auto invDet = 1.0F / det;

  Matrix4x4 result;

#pragma unroll
  for (size_t i = 0; i < Size; ++i) {
    result.At(i) = inv.At(i) * invDet;
  }

  return result;
}

auto Matrix4x4::InverseTranspose() const -> Matrix4x4 { return Inverse().Transpose(); }

auto Matrix4x4::GetTranslation() const -> Vec3 { return {At(3, 0), At(3, 1), At(3, 2)}; }

auto Matrix4x4::operator!=(const Matrix4x4 &other) const -> bool { return !(*this == other); }

auto Matrix4x4::operator[](size_t index) -> float & {
#ifndef NDEBUG
  assert(index < Size);
#endif
  return elements.at(index);
}

auto Matrix4x4::operator[](size_t index) const -> float {
#ifndef NDEBUG
  assert(index < Size);
#endif
  return elements.at(index);
}

auto Matrix4x4::Perspective(float left, float right, float bottom, float top, float near, float far) -> Matrix4x4 {

  auto rml = right - left;
  auto tmb = top - bottom;
  auto fmn = far - near;
  auto rpl = right + left;
  auto tpb = top + bottom;
  auto fpn = far + near;

  // NOLINTBEGIN
  // clang-format off
  Matrix4x4 result = Matrix4x4::FromRows(
    {
      (2.0F * near) / rml, 0.0F, rpl / rml, 0.0F,
      0.0F, (2.0F * near) / tmb, tpb / tmb, 0.0F,
      0.0F, 0.0F, far / (near - far), (near * far) / (near - far),
      0.0F, 0.0F, -1.0F, 0.0F
    });
  // NOLINTEND
  // clang-format on

  return result;
}

// NOLINTNEXTLINE
auto Matrix4x4::Perspective(float fov, float aspect, float near, float far) -> Matrix4x4 {
  float ithf = 1.0F / std::tan(fov / 2.0F); // NOLINT
  float invr = 1.0F / (near - far);
  auto npf = near + far;
  auto nmf = near * far;
  auto fmn = far - near;

  // NOLINTBEGIN
  // clang-format off
  Matrix4x4 result = Matrix4x4::FromRows(
    {
      ithf / aspect, 0.0F, 0.0F, 0.0F,
      0.0F, -ithf,    0.0F, 0.0F,
      0.0F, 0.0F, near / (far - near), (far * near) / (far - near),
      0.0F, 0.0F, 1.0F, 0.0F
    });
  // NOLINTEND
  // clang-format on

  return result;
}

auto Matrix4x4::Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) -> Matrix4x4 {

  auto rml = right - left;
  auto tmb = top - bottom;
  auto fmn = farPlane - nearPlane;
  auto tpb = top + bottom;
  auto rpl = right + left;
  auto fpn = farPlane + nearPlane;

  // NOLINTBEGIN
  // clang-format off
  Matrix4x4 result = Matrix4x4::FromRows(
      {
        2.0F / rml, 0.0F,       0.0F,       -rpl / rml,
        0.0F,       2.0F / tmb, 0.0F,       -tpb / tmb,
        0.0F,       0.0F,       1.0F / fmn, -farPlane / fmn,
        0.0F,       0.0F,       0.0F,       1.0F
      });
  // NOLINTEND
  // clang-format on

  return result;
}

// NOLINTBEGIN
auto Matrix4x4::Orthographic(float width, float height, float nearPlane, float farPlane) -> Matrix4x4 {
  float left = -width / 2.0F;
  float right = width / 2.0F;
  float bottom = -height / 2.0F;
  float top = height / 2.0F;

  return Matrix4x4::Orthographic(left, right, bottom, top, nearPlane, farPlane);
}
// NOLINTEND

auto Matrix4x4::TranslationMatrix(Vec3 translation) -> Matrix4x4 {
  Matrix4x4 result;
  result.At(3, 0) = translation.x;
  result.At(3, 1) = translation.y;
  result.At(3, 2) = translation.z;
  return result;
}

auto Matrix4x4::TranslationMatrix(float x_pos, float y_pos, float z_pos) -> Matrix4x4 {
  Matrix4x4 result;
  result.At(3, 0) = x_pos;
  result.At(3, 1) = y_pos;
  result.At(3, 2) = z_pos;
  return result;
}

auto Matrix4x4::ScaleMatrix(Vec3 scale) -> Matrix4x4 {
  Matrix4x4 result;
  result.At(0, 0) = scale.x;
  result.At(1, 1) = scale.y;
  result.At(2, 2) = scale.z;
  return result;
}

auto Matrix4x4::ScaleMatrix(float x_scale, float y_scale, float z_scale) -> Matrix4x4 {
  Matrix4x4 result;
  result.At(0, 0) = x_scale;
  result.At(1, 1) = y_scale;
  result.At(2, 2) = z_scale;
  return result;
}

// NOLINTNEXTLINE
auto Matrix4x4::TransformationMatrix(Vec3 translation, Vec3 scale, Quaternion rotation) -> Matrix4x4 {
  Matrix4x4 translationMatrix = Matrix4x4::TranslationMatrix(translation);
  Matrix4x4 scaleMatrix = Matrix4x4::ScaleMatrix(scale);
  Matrix4x4 rotationMatrix = Conversions::ToMatrix(rotation);
  return scaleMatrix * rotationMatrix * translationMatrix;
}

auto Matrix4x4::TransformationMatrix(float x_pos, float y_pos, float z_pos, float x_scale, float y_scale, float z_scale, float x_rot, float y_rot, float z_rot,
                                     float w_rot) -> Matrix4x4 {
  Matrix4x4 translationMatrix = Matrix4x4::TranslationMatrix(x_pos, y_pos, z_pos);
  Matrix4x4 scaleMatrix = Matrix4x4::ScaleMatrix(x_scale, y_scale, z_scale);
  Quaternion rotation{x_rot, y_rot, z_rot, w_rot};
  Matrix4x4 rotationMatrix = Conversions::ToMatrix(rotation);
  return scaleMatrix * rotationMatrix * translationMatrix;
}

auto Matrix4x4::ToString() const -> std::string {
  std::string result;
  for (size_t row = 0; row < Rows; ++row) {
    result += "| ";
    for (size_t col = 0; col < Cols; ++col) {
      result += std::to_string(At(row, col)) + " ";
    }
    result += "|";
    if (row < Rows - 1) {
      result += "\n";
    }
  }
  return result;
}

auto Matrix3x3::At(size_t row, size_t col) -> float & {
#ifndef NDEBUG
  assert(row < 3 && col < 3);
#endif
  return elements.at((row * Cols) + col);
}

auto Matrix3x3::At(size_t row, size_t col) const -> float {
#ifndef NDEBUG
  assert(row < 3 && col < 3);
#endif
  return elements.at((row * Cols) + col);
}

auto Matrix3x3::At(size_t index) -> float & {
#ifndef NDEBUG
  assert(index < Size);
#endif
  return elements.at(index);
}

auto Matrix3x3::At(size_t index) const -> float {
#ifndef NDEBUG
  assert(index < Size);
#endif
  return elements.at(index);
}

Matrix3x3::Matrix3x3() {
  At(0) = 1.0F;
  At(1, 1) = 1.0F;
  At(2, 2) = 1.0F;
}

auto Matrix3x3::Transpose() const -> Matrix3x3 {
  Matrix3x3 result{};
#pragma unroll
  for (size_t row = 0; row < Rows; ++row) {
#pragma unroll
    for (size_t col = 0; col < Cols; ++col) {
      // NOLINTNEXTLINE, clangd thinks row, col are swapped, but that's intentional
      result.At(col, row) = At(row, col);
    }
  }
  return result;
}

auto Matrix3x3::operator*(const Matrix3x3 &other) const -> Matrix3x3 {
  Matrix3x3 result{};
#pragma unroll
  for (size_t row = 0; row < Rows; ++row) {
#pragma unroll
    for (size_t col = 0; col < Cols; ++col) {
      float sum = 0.0F;
#pragma unroll
      for (size_t k = 0; k < Cols; ++k) {
        sum += At(row, k) * other.At(k, col);
      }
      result.At(row, col) = sum;
    }
  }
  return result;
}

auto Matrix3x3::operator*(const Vec3 &vec) const -> Vec3 {
  Vec3 result{};
  for (size_t row = 0; row < Rows; ++row) {
    float sum = 0.0F;
    for (size_t col = 0; col < Cols; ++col) {
      sum += At(row, col) * vec[col];
    }
    result[row] = sum;
  }
  return result;
}

auto Matrix3x3::operator==(const Matrix3x3 &other) const -> bool { return elements == other.elements; }

auto Matrix3x3::operator!=(const Matrix3x3 &other) const -> bool { return !(*this == other); }

auto Matrix3x3::Determinant() const -> std::pair<float, Matrix3x3> {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
  float det = (At(0) * (At(4) * At(8) - At(5) * At(7))) - (At(1) * (At(3) * At(8) - At(5) * At(6))) + (At(2) * (At(3) * At(7) - At(4) * At(6)));

  Matrix3x3 cofactorMatrix{};
  cofactorMatrix.At(0) = (At(4) * At(8)) - (At(5) * At(7));
  cofactorMatrix.At(1) = -((At(3) * At(8)) - (At(5) * At(6)));
  cofactorMatrix.At(2) = (At(3) * At(7)) - (At(4) * At(6));
  cofactorMatrix.At(3) = -((At(1) * At(8)) - (At(2) * At(7)));
  cofactorMatrix.At(4) = (At(0) * At(8)) - (At(2) * At(6));
  cofactorMatrix.At(5) = -((At(0) * At(7)) - (At(1) * At(6)));
  cofactorMatrix.At(6) = (At(1) * At(5)) - (At(2) * At(4));
  cofactorMatrix.At(7) = -((At(0) * At(5)) - (At(2) * At(3)));
  cofactorMatrix.At(8) = (At(0) * At(4)) - (At(1) * At(3));
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

  return {det, cofactorMatrix.Transpose()};
}

auto Matrix3x3::Inverse() const -> Matrix3x3 { return InverseTranspose().Transpose(); }

auto Matrix3x3::InverseTranspose() const -> Matrix3x3 {
  auto detPair = Determinant();
  auto det = detPair.first;
  auto cofactorMatrix = detPair.second;

  if (det == 0.0F) {
    return Matrix3x3{NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  }

  float invDet = 1.0F / det;

  Matrix3x3 result;

  for (size_t i = 0; i < Size; ++i) {
    result.At(i) = cofactorMatrix.At(i) * invDet;
  }

  return result;
}

auto Matrix3x3::ToString() const -> std::string {
  std::string result;
  for (size_t row = 0; row < Rows; ++row) {
    result += "| ";
    for (size_t col = 0; col < Cols; ++col) {
      result += std::to_string(At(row, col)) + " ";
    }
    result += "|";
    if (row < Rows - 1) {
      result += "\n";
    }
  }
  return result;
}

auto Matrix4x4::ToLua(lua_State *state) const -> int {
#pragma unroll
  for (int i = 0; i < Size; ++i) {
    lua_pushnumber(state, At(i));
  }

  return Size;
}

auto Matrix3x3::ToLua(lua_State *state) const -> int {
#pragma unroll
  for (int i = 0; i < Size; ++i) {
    lua_pushnumber(state, At(i));
  }

  return Size;
}

auto Matrix4x4::FromLua(lua_State *state, int index) -> Matrix4x4 {
  Matrix4x4 mat;
  for (int i = 0; i < Size; ++i) {
    lua_rawgeti(state, index, i + 1);
    mat.At(i) = static_cast<float>(lua_tonumber(state, -1));
  }
  lua_pop(state, Size);
  return mat;
}

auto Matrix3x3::FromLua(lua_State *state, int index) -> Matrix3x3 {
  Matrix3x3 mat;
  for (int i = 0; i < Size; ++i) {
    lua_rawgeti(state, index, i + 1);
    mat.At(i) = static_cast<float>(lua_tonumber(state, -1));
  }
  lua_pop(state, Size);
  return mat;
}

} // namespace Math