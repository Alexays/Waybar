#pragma once

#include <string>

/**
 * Result of transforming 8-bit hex codes to rgba().
 */
struct TransformResult {
  std::string css;
  bool was_transformed;
};

/**
 * Reads a CSS file, inlines its local @import files, searches for 8-bit hex codes
 * (#RRGGBBAA), and transforms them into GTK-compatible rgba() syntax.
 * was_transformed is true only when at least one 8-bit code was converted.
 */
TransformResult transform_8bit_to_hex(const std::string& file_path);
