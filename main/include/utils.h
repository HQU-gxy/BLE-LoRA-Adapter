//
// Created by Kurosu Chan on 2023/10/27.
//

#ifndef WIT_HUB_UTILS_H
#define WIT_HUB_UTILS_H

#include "string"

namespace utils {

size_t sprintHex(char *out, size_t outSize, const uint8_t *bytes, size_t size);

std::string toHex(const uint8_t *bytes, size_t size);

}

#endif // WIT_HUB_UTILS_H
