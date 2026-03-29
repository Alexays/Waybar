#pragma once

#include <string>

/**
 * Reads a CSS file, searches for 8-bit hex codes (#RRGGBBAA),
 * and transforms them into GTK-compatible rgba() syntax.
 */

std::pair<std::string, bool> transform_8bit_to_hex(const std::string& file_path);
