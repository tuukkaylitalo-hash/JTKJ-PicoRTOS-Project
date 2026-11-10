#include "morse.h"
#include <stddef.h>
#include <string.h>

const char* morsetable[] = {
    ".-", // A
    "-...", // B
    "-.-.", // C
    "-..", // D
    ".", // E
    "..-.", // F
    "--.", // G
    "....", // H
    "..", // I
    ".---", // J
    "-.-", // K
    ".-..", // L
    "--", // M
    "-.", // N
    "---", // O
    ".--.", // P
    "--.-", // Q
    ".-.", // R
    "...", // S
    "-", // T
    "..-", // U
    "...-", // V
    ".--", // W
    "-..-", // X
    "-.--", // Y
    "--.." // Z
};

size_t morsetable_size = sizeof(morsetable) / sizeof(morsetable[0]);

const char* letter_to_morse(const char* letter) {
    if (letter == NULL || *letter < 'A' || *letter > 'Z') {
        return NULL; // Invalid input
    }
    return morsetable[*letter - 'A'];
}

char morse_to_letter(const char* morse) {
    if (morse == NULL) {
        return '\0'; // Invalid input
    }
    for (int i = 0; i < morsetable_size; i++) {
        if (strcmp(morse, morsetable[i]) == 0) {
            return 'A' + i;
        }
    }
    return '\0'; // Not found
}
