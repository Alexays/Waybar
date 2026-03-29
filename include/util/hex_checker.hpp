#pragma once

#include <string>

/**
 * Reads a CSS file, searches for 8-bit hex codes (#RRGGBBAA),
 * and returns true if found, or false if not found.
 */

bool has_8bit_hex(std::string file_path);

/**
 * Reads a CSS file, searches for 8-bit hex codes (#RRGGBBAA),
 * and transforms them into GTK-compatible rgba() syntax.
 */

std::string transform_8bit_to_hex(std::string file_path);
