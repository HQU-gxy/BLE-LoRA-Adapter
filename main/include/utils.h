//
// Created by Kurosu Chan on 2023/10/27.
//

#ifndef WIT_HUB_UTILS_H
#define WIT_HUB_UTILS_H

#include "string"

namespace utils {

/**
 * @brief put hex string to out
 * @param out the output buffer
 * @param outSize the size of output buffer, will NOT include the null terminator
 * @param bytes the input bytes
 * @param size the size of input bytes
 * @return the length of output string
 * @note length of output string will not include the null terminator
 *       and the user should add it manually to the buffer, if null terminator is needed
 */
size_t sprintHex(char *out, size_t outSize, const uint8_t *bytes, size_t size);

std::string toHex(const uint8_t *bytes, size_t size);

}

#endif // WIT_HUB_UTILS_H
