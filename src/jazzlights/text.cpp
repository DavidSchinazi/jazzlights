#include "jazzlights/text.h"

#if JL_IS_CONTROLLER(ATOM_MATRIX)

namespace jazzlights {

struct CharacterPixels {
  char character;
  uint8_t width;
  uint32_t rows[5];
};

constexpr CharacterPixels kCharacterMap[] = {
    {'\0', 5, {0b00000, 0b00000, 0b00000, 0b00000, 0b00000}},
    { ' ', 2,                {0b00, 0b00, 0b00, 0b00, 0b00}},
    { '!', 1,                     {0b1, 0b1, 0b1, 0b0, 0b1}},
    { '"', 3,           {0b101, 0b101, 0b000, 0b000, 0b000}},
    { '#', 5, {0b01010, 0b11111, 0b01010, 0b11111, 0b01010}},
    // Skipped '$'
    { '%', 5, {0b11001, 0b11010, 0b00100, 0b01011, 0b10011}},
    // Skipped '&'
    {'\'', 1,                     {0b1, 0b1, 0b0, 0b0, 0b0}},
    { '(', 2,                {0b01, 0b10, 0b10, 0b10, 0b01}},
    { ')', 2,                {0b10, 0b01, 0b01, 0b01, 0b10}},
    { '*', 3,           {0b000, 0b101, 0b010, 0b101, 0b000}},
    { '+', 5, {0b00100, 0b00100, 0b11111, 0b00100, 0b00100}},
    { ',', 2,                {0b00, 0b00, 0b00, 0b01, 0b10}},
    { '-', 3,           {0b000, 0b000, 0b111, 0b000, 0b000}},
    { '.', 1,                     {0b0, 0b0, 0b0, 0b0, 0b1}},
    { '/', 3,           {0b001, 0b001, 0b010, 0b100, 0b100}},
    { '0', 3,           {0b111, 0b101, 0b101, 0b101, 0b111}},
    { '1', 3,           {0b110, 0b010, 0b010, 0b010, 0b111}},
    { '2', 3,           {0b111, 0b001, 0b111, 0b100, 0b111}},
    { '3', 3,           {0b111, 0b001, 0b111, 0b001, 0b111}},
    { '4', 3,           {0b101, 0b101, 0b111, 0b001, 0b001}},
    { '5', 3,           {0b111, 0b100, 0b110, 0b001, 0b110}},
    { '6', 3,           {0b111, 0b100, 0b111, 0b101, 0b111}},
    { '7', 3,           {0b111, 0b001, 0b001, 0b001, 0b001}},
    { '8', 3,           {0b111, 0b101, 0b111, 0b101, 0b111}},
    { '9', 3,           {0b111, 0b101, 0b111, 0b001, 0b111}},
    { ':', 1,                     {0b0, 0b1, 0b0, 0b1, 0b0}},
    { ';', 2,                {0b00, 0b01, 0b00, 0b01, 0b10}},
    { '<', 2,                {0b00, 0b01, 0b10, 0b01, 0b00}},
    { '=', 3,           {0b000, 0b111, 0b000, 0b111, 0b000}},
    { '>', 2,                {0b00, 0b10, 0b01, 0b10, 0b00}},
    { '?', 4,      {0b1111, 0b1001, 0b0010, 0b0000, 0b0010}},
    // Skipped '@'
    { 'A', 3,           {0b010, 0b101, 0b111, 0b101, 0b101}},
    { 'B', 3,           {0b110, 0b101, 0b110, 0b101, 0b110}},
    { 'C', 3,           {0b111, 0b100, 0b100, 0b100, 0b111}},
    { 'D', 3,           {0b110, 0b101, 0b101, 0b101, 0b110}},
    { 'E', 3,           {0b111, 0b100, 0b111, 0b100, 0b111}},
    { 'F', 3,           {0b111, 0b100, 0b111, 0b100, 0b100}},
    { 'G', 4,      {0b1110, 0b1000, 0b1011, 0b1001, 0b1110}},
    { 'H', 3,           {0b101, 0b101, 0b111, 0b101, 0b101}},
    { 'I', 3,           {0b111, 0b010, 0b010, 0b010, 0b111}},
    { 'J', 4,      {0b0111, 0b0010, 0b0010, 0b1010, 0b0110}},
    { 'K', 4,      {0b1001, 0b1010, 0b1100, 0b1010, 0b1001}},
    { 'L', 3,           {0b100, 0b100, 0b100, 0b100, 0b111}},
    { 'M', 5, {0b10001, 0b11011, 0b10101, 0b10001, 0b10001}},
    { 'N', 5, {0b10001, 0b11001, 0b10101, 0b10011, 0b10001}},
    { 'O', 4,      {0b1111, 0b1001, 0b1001, 0b1001, 0b1111}},
    { 'P', 3,           {0b111, 0b101, 0b111, 0b100, 0b100}},
    { 'Q', 4,      {0b1111, 0b1001, 0b1001, 0b1111, 0b0010}},
    { 'R', 3,           {0b111, 0b101, 0b111, 0b110, 0b101}},
    { 'S', 3,           {0b111, 0b100, 0b111, 0b001, 0b111}},
    { 'T', 3,           {0b111, 0b010, 0b010, 0b010, 0b010}},
    { 'U', 3,           {0b101, 0b101, 0b101, 0b101, 0b111}},
    { 'V', 3,           {0b101, 0b101, 0b101, 0b101, 0b010}},
    { 'W', 5, {0b10101, 0b10101, 0b10101, 0b10101, 0b01010}},
    { 'X', 3,           {0b101, 0b101, 0b010, 0b101, 0b101}},
    { 'Y', 3,           {0b101, 0b101, 0b010, 0b010, 0b010}},
    { 'Z', 3,           {0b111, 0b001, 0b010, 0b100, 0b111}},
    { '[', 2,                {0b11, 0b10, 0b10, 0b10, 0b11}},
    {'\\', 3,           {0b100, 0b100, 0b010, 0b001, 0b001}},
    { ']', 2,                {0b11, 0b01, 0b01, 0b01, 0b11}},
    { '^', 3,           {0b010, 0b101, 0b000, 0b000, 0b000}},
    { '_', 3,           {0b000, 0b000, 0b000, 0b000, 0b111}},
    { '`', 2,                {0b10, 0b01, 0b00, 0b00, 0b00}},
    { '{', 2,                {0b01, 0b01, 0b10, 0b01, 0b01}},
    { '|', 1,                     {0b1, 0b1, 0b1, 0b1, 0b1}},
    { '}', 2,                {0b10, 0b10, 0b01, 0b10, 0b10}},
    { '~', 4,      {0b0000, 0b0101, 0b1010, 0b0000, 0b0000}},
};
constexpr size_t kCharacterMapSize = sizeof(kCharacterMap) / sizeof(kCharacterMap[0]);

// Useful debugging string:
// "A !B\"C#D%E'F(G)H*I+,J-K.L/M:N;O<P=Q>?R[S\\T]U^V_W`X{Y|Z}0~123456789"

CharacterPixels getCharacterPixels(char character) {
  if (character == '\t' || character == '\n') { return getCharacterPixels('\0'); }
  if ('a' <= character && character <= 'z') { return getCharacterPixels(character - 'a' + 'A'); }
  for (size_t i = 0; i < kCharacterMapSize; i++) {
    if (kCharacterMap[i].character == character) { return kCharacterMap[i]; }
  }
  return getCharacterPixels('?');
}

void writeColumn(const CharacterPixels& characterPixels, uint8_t characterColumnFromRight, uint8_t pixelColumnFromRight,
                 CRGB pixels[MATRIX_SIZE], CRGB textColor, CRGB backgroundColor) {
  for (uint8_t row = 0; row < 5; row++) {
    const size_t pixelIndex = row * 5 + 4 - pixelColumnFromRight;
    if ((characterPixels.rows[row] & 1 << characterColumnFromRight) != 0) {
      pixels[pixelIndex] = textColor;
    } else {
      pixels[pixelIndex] = backgroundColor;
    }
  }
}

bool displayText(const std::string& text, CRGB pixels[MATRIX_SIZE], CRGB textColor, CRGB backgroundColor,
                 Milliseconds offsetMillis) {
  constexpr Milliseconds quantum = 150;
  // Start at 4 to start displaying from right edge of screen.
  size_t columns = 4;
  const size_t col0 = offsetMillis / quantum;
  const size_t col4 = col0 + 4;
  size_t textIndex = 0;
  CRGB tmpPixels[MATRIX_SIZE];
  for (uint8_t i = 0; i < MATRIX_SIZE; i++) { tmpPixels[i] = backgroundColor; }
  while (true) {
    const size_t characterStartColumn = columns;
    if (characterStartColumn > col4) {
      // This entire character is right of screen, stop.
      break;
    }
    if (textIndex >= text.size()) { return false; }
    CharacterPixels cp = getCharacterPixels(text[textIndex]);
    textIndex++;
    const size_t characterEndColumn = columns + cp.width - 1;
    columns += cp.width + 1;  // +1 is for inter-character gap.
    if (characterEndColumn < col0) {
      // This entire character is left of screen, skip.
      continue;
    }
    // This character is (at least partially) on screen.
    for (size_t col = col0; col <= col4; col++) {
      if (characterStartColumn <= col && col <= characterEndColumn) {
        const uint8_t characterColumnFromRight = characterEndColumn - col;
        const uint8_t pixelColumnFromRight = col4 - col;
        writeColumn(cp, characterColumnFromRight, pixelColumnFromRight, tmpPixels, textColor, backgroundColor);
      }
    }
  }
  for (uint8_t i = 0; i < MATRIX_SIZE; i++) { pixels[i] = tmpPixels[i]; }
  return true;
}

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_MATRIX)
