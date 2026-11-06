#include <unity.h>

#include <cstring>
#include <vector>

#include "jazzlights/network/tlv.h"

namespace jazzlights {
namespace {

std::vector<uint8_t> MakeTlv(uint64_t type, const std::vector<uint8_t>& value) {
  std::vector<uint8_t> tlv(2 * kMaxVarintSize + value.size());
  size_t size = WriteVarint(type, tlv.data());
  size += WriteVarint(value.size(), tlv.data() + size);
  if (!value.empty()) { memcpy(tlv.data() + size, value.data(), value.size()); }
  tlv.resize(size + value.size());
  return tlv;
}

std::vector<uint8_t> Concat(const std::vector<std::vector<uint8_t>>& parts) {
  std::vector<uint8_t> result;
  for (const std::vector<uint8_t>& part : parts) { result.insert(result.end(), part.begin(), part.end()); }
  return result;
}

}  // namespace

void test_varint_sizes() {
  TEST_ASSERT_EQUAL(1, VarintSize(0));
  TEST_ASSERT_EQUAL(1, VarintSize(63));
  TEST_ASSERT_EQUAL(2, VarintSize(64));
  TEST_ASSERT_EQUAL(2, VarintSize(16383));
  TEST_ASSERT_EQUAL(4, VarintSize(16384));
  TEST_ASSERT_EQUAL(4, VarintSize(1073741823));
  TEST_ASSERT_EQUAL(8, VarintSize(1073741824));
  TEST_ASSERT_EQUAL(8, VarintSize(kMaxVarint));
}

void test_varint_round_trip() {
  for (uint64_t value : {uint64_t{0}, uint64_t{63}, uint64_t{64}, uint64_t{16383}, uint64_t{16384},
                         uint64_t{1073741823}, uint64_t{1073741824}, kMaxVarint}) {
    uint8_t buffer[kMaxVarintSize];
    const size_t size = WriteVarint(value, buffer);
    uint64_t decoded = 0;
    TEST_ASSERT_EQUAL(size, ReadVarint(buffer, size, &decoded));
    TEST_ASSERT_TRUE(decoded == value);
    for (size_t cut = 0; cut < size; cut++) { TEST_ASSERT_EQUAL(0, ReadVarint(buffer, cut, &decoded)); }
  }
}

void test_varint_rfc_examples() {
  // These are the examples from RFC 9000 appendix A.1.
  const uint8_t eightBytes[] = {0xc2, 0x19, 0x7c, 0x5e, 0xff, 0x14, 0xe8, 0x8c};
  const uint8_t fourBytes[] = {0x9d, 0x7f, 0x3e, 0x7d};
  const uint8_t twoBytes[] = {0x7b, 0xbd};
  const uint8_t oneByte[] = {0x25};
  // This one is longer than needed, which is accepted.
  const uint8_t twoBytesFor37[] = {0x40, 0x25};
  uint64_t value = 0;
  TEST_ASSERT_EQUAL(8, ReadVarint(eightBytes, sizeof(eightBytes), &value));
  TEST_ASSERT_TRUE(value == 151288809941952652ull);
  TEST_ASSERT_EQUAL(4, ReadVarint(fourBytes, sizeof(fourBytes), &value));
  TEST_ASSERT_TRUE(value == 494878333ull);
  TEST_ASSERT_EQUAL(2, ReadVarint(twoBytes, sizeof(twoBytes), &value));
  TEST_ASSERT_TRUE(value == 15293ull);
  TEST_ASSERT_EQUAL(1, ReadVarint(oneByte, sizeof(oneByte), &value));
  TEST_ASSERT_TRUE(value == 37ull);
  TEST_ASSERT_EQUAL(2, ReadVarint(twoBytesFor37, sizeof(twoBytesFor37), &value));
  TEST_ASSERT_TRUE(value == 37ull);
}

void test_header_status() {
  TlvHeader header;
  const uint8_t complete[] = {0x05, 0x02, 0xaa};
  TEST_ASSERT_TRUE(ParseTlvHeader(complete, sizeof(complete), &header) == TlvHeaderStatus::kComplete);
  TEST_ASSERT_TRUE(header.type == 5);
  TEST_ASSERT_EQUAL(2, header.length);
  TEST_ASSERT_EQUAL(2, header.headerSize);
  TEST_ASSERT_TRUE(ParseTlvHeader(complete, 0, &header) == TlvHeaderStatus::kNeedMoreData);
  TEST_ASSERT_TRUE(ParseTlvHeader(complete, 1, &header) == TlvHeaderStatus::kNeedMoreData);
  const uint8_t cutOffLength[] = {0x05, 0x40};
  TEST_ASSERT_TRUE(ParseTlvHeader(cutOffLength, sizeof(cutOffLength), &header) == TlvHeaderStatus::kNeedMoreData);
  // Exactly the maximum Length, then one over. Both take a four-byte varint because 16384 doesn't fit in two.
  const uint8_t maximum[] = {0x05, 0x80, 0x00, 0x40, 0x00};
  TEST_ASSERT_TRUE(ParseTlvHeader(maximum, sizeof(maximum), &header) == TlvHeaderStatus::kComplete);
  TEST_ASSERT_EQUAL(kMaxTlvLength, header.length);
  const uint8_t overMaximum[] = {0x05, 0x80, 0x00, 0x40, 0x01};
  TEST_ASSERT_TRUE(ParseTlvHeader(overMaximum, sizeof(overMaximum), &header) == TlvHeaderStatus::kInvalid);
  // The largest Length that a varint can hold must not wrap around anything when narrowed.
  const uint8_t huge[] = {0x05, 0xc0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_TRUE(ParseTlvHeader(huge, sizeof(huge), &header) == TlvHeaderStatus::kInvalid);
}

void test_reader_iterates() {
  const std::vector<uint8_t> data = Concat({MakeTlv(1, {0xaa, 0xbb}), MakeTlv(200, {}), MakeTlv(3, {0xcc})});
  TlvReader reader(data.data(), data.size());
  TEST_ASSERT_TRUE(reader.Next());
  TEST_ASSERT_TRUE(reader.type() == 1);
  TEST_ASSERT_EQUAL(2, reader.length());
  TEST_ASSERT_EQUAL(0xbb, reader.value()[1]);
  TEST_ASSERT_EQUAL(4, reader.tlvLength());
  TEST_ASSERT_TRUE(reader.tlv() == data.data());
  TEST_ASSERT_TRUE(reader.Next());
  TEST_ASSERT_TRUE(reader.type() == 200);
  TEST_ASSERT_EQUAL(0, reader.length());
  TEST_ASSERT_TRUE(reader.Next());
  TEST_ASSERT_TRUE(reader.type() == 3);
  TEST_ASSERT_FALSE(reader.Next());
  TEST_ASSERT_FALSE(reader.failed());
}

void test_reader_empty_container() {
  TlvReader reader(nullptr, 0);
  TEST_ASSERT_FALSE(reader.Next());
  TEST_ASSERT_FALSE(reader.failed());
}

void test_reader_errors() {
  // A Length past the end of its container.
  const uint8_t pastEnd[] = {0x01, 0x03, 0xaa, 0xbb};
  TlvReader pastEndReader(pastEnd, sizeof(pastEnd));
  TEST_ASSERT_FALSE(pastEndReader.Next());
  TEST_ASSERT_TRUE(pastEndReader.failed());
  // A Type or Length that's cut off.
  const uint8_t cutOffType[] = {0x40};
  TlvReader cutOffTypeReader(cutOffType, sizeof(cutOffType));
  TEST_ASSERT_FALSE(cutOffTypeReader.Next());
  TEST_ASSERT_TRUE(cutOffTypeReader.failed());
  const uint8_t cutOffLength[] = {0x01};
  TlvReader cutOffLengthReader(cutOffLength, sizeof(cutOffLength));
  TEST_ASSERT_FALSE(cutOffLengthReader.Next());
  TEST_ASSERT_TRUE(cutOffLengthReader.failed());
  // A good TLV followed by a bad one, and nothing more comes out of it afterwards.
  const uint8_t goodThenBad[] = {0x01, 0x00, 0x02, 0x05};
  TlvReader goodThenBadReader(goodThenBad, sizeof(goodThenBad));
  TEST_ASSERT_TRUE(goodThenBadReader.Next());
  TEST_ASSERT_FALSE(goodThenBadReader.Next());
  TEST_ASSERT_TRUE(goodThenBadReader.failed());
  TEST_ASSERT_FALSE(goodThenBadReader.Next());
}

void test_reader_maximum_length() {
  std::vector<uint8_t> value(kMaxTlvLength, 0x11);
  std::vector<uint8_t> tlv = MakeTlv(7, value);
  TlvReader reader(tlv.data(), tlv.size());
  TEST_ASSERT_TRUE(reader.Next());
  TEST_ASSERT_EQUAL(kMaxTlvLength, reader.length());
  value.push_back(0x11);
  tlv = MakeTlv(7, value);
  TlvReader overReader(tlv.data(), tlv.size());
  TEST_ASSERT_FALSE(overReader.Next());
  TEST_ASSERT_TRUE(overReader.failed());
}

void test_integer() {
  uint64_t integer = 0;
  const uint8_t oneByte[] = {0x25};
  TEST_ASSERT_TRUE(ParseTlvInteger(oneByte, sizeof(oneByte), &integer));
  TEST_ASSERT_TRUE(integer == 37);
  const uint8_t longer[] = {0x40, 0x25};
  TEST_ASSERT_TRUE(ParseTlvInteger(longer, sizeof(longer), &integer));
  TEST_ASSERT_TRUE(integer == 37);
  // It must fill the Value exactly.
  const uint8_t trailing[] = {0x25, 0x00};
  TEST_ASSERT_FALSE(ParseTlvInteger(trailing, sizeof(trailing), &integer));
  const uint8_t cutOff[] = {0x40};
  TEST_ASSERT_FALSE(ParseTlvInteger(cutOff, sizeof(cutOff), &integer));
  TEST_ASSERT_FALSE(ParseTlvInteger(nullptr, 0, &integer));
}

void test_fields_success() {
  // In a different order than they're described, with unknown fields around them.
  const std::vector<uint8_t> data =
      Concat({MakeTlv(50, {0x01}), MakeTlv(2, {}), MakeTlv(1, {0x25}), MakeTlv(51, {}), MakeTlv(0, {0xaa, 0xbb})});
  TlvField fields[] = {
      {0, TlvField::Kind::kBytes, /*required=*/true, /*exactLength=*/2},
      {1, TlvField::Kind::kInteger, /*required=*/true},
      {2, TlvField::Kind::kFlag, /*required=*/false},
      {3, TlvField::Kind::kFlag, /*required=*/false},
  };
  TEST_ASSERT_TRUE(ParseTlvFields(data.data(), data.size(), fields, 4));
  TEST_ASSERT_TRUE(fields[0].present);
  TEST_ASSERT_EQUAL(2, fields[0].length);
  TEST_ASSERT_EQUAL(0xbb, fields[0].value[1]);
  TEST_ASSERT_EQUAL(4, fields[0].tlvLength);
  TEST_ASSERT_TRUE(fields[0].tlv == data.data() + data.size() - 4);
  TEST_ASSERT_TRUE(fields[1].present);
  TEST_ASSERT_TRUE(fields[1].integer == 37);
  TEST_ASSERT_TRUE(fields[2].present);
  TEST_ASSERT_FALSE(fields[3].present);
}

bool ParseOneField(const std::vector<uint8_t>& data, TlvField::Kind kind, bool required,
                   size_t exactLength = TlvField::kAnyLength) {
  TlvField fields[] = {
      {0, kind, required, exactLength}
  };
  return ParseTlvFields(data.data(), data.size(), fields, 1);
}

void test_fields_errors() {
  const std::vector<uint8_t> flagField = MakeTlv(0, {});
  // Missing.
  TEST_ASSERT_FALSE(ParseOneField({}, TlvField::Kind::kFlag, /*required=*/true));
  TEST_ASSERT_TRUE(ParseOneField({}, TlvField::Kind::kFlag, /*required=*/false));
  // Repeated, whether or not it's required.
  TEST_ASSERT_FALSE(ParseOneField(Concat({flagField, flagField}), TlvField::Kind::kFlag, /*required=*/false));
  // Wrong size.
  TEST_ASSERT_FALSE(ParseOneField(MakeTlv(0, {0x01}), TlvField::Kind::kBytes, /*required=*/true, /*exactLength=*/2));
  TEST_ASSERT_TRUE(ParseOneField(MakeTlv(0, {0x01}), TlvField::Kind::kBytes, /*required=*/true, /*exactLength=*/1));
  TEST_ASSERT_FALSE(ParseOneField(MakeTlv(0, {0x01}), TlvField::Kind::kFlag, /*required=*/true));
  TEST_ASSERT_FALSE(ParseOneField(MakeTlv(0, {0x01, 0x02}), TlvField::Kind::kInteger, /*required=*/true));
  TEST_ASSERT_FALSE(ParseOneField(MakeTlv(0, {}), TlvField::Kind::kInteger, /*required=*/true));
  // Past the end of the container, even if everything before it was fine.
  std::vector<uint8_t> cutOff = flagField;
  cutOff.push_back(0x05);
  TEST_ASSERT_FALSE(ParseOneField(cutOff, TlvField::Kind::kFlag, /*required=*/true));
  // An unknown field that's malformed isn't merely skipped.
  TEST_ASSERT_FALSE(ParseOneField(Concat({
                                      flagField, {0x09, 0x01}
  }),
                                  TlvField::Kind::kFlag, /*required=*/true));
}

void test_fields_compare_decoded_type() {
  // Type 0 in a longer varint is still type 0, so this is a repeated field.
  const std::vector<uint8_t> data = {0x00, 0x00, 0x40, 0x00, 0x00};
  TEST_ASSERT_FALSE(ParseOneField(data, TlvField::Kind::kFlag, /*required=*/true));
  // A type that only matches once it's truncated to fewer bits is unknown.
  const std::vector<uint8_t> truncatedType = {0xc0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_FALSE(ParseOneField(truncatedType, TlvField::Kind::kFlag, /*required=*/true));
  TEST_ASSERT_TRUE(ParseOneField(truncatedType, TlvField::Kind::kFlag, /*required=*/false));
}

void test_writer() {
  uint8_t buffer[16];
  TlvWriter writer(buffer, sizeof(buffer));
  const uint8_t value[] = {0xaa, 0xbb};
  writer.WriteBytes(1, value, sizeof(value));
  writer.WriteInteger(2, 16384);
  writer.WriteFlag(3);
  TEST_ASSERT_FALSE(writer.failed());
  const uint8_t expected[] = {0x01, 0x02, 0xaa, 0xbb, 0x02, 0x04, 0x80, 0x00, 0x40, 0x00, 0x03, 0x00};
  TEST_ASSERT_EQUAL(sizeof(expected), writer.size());
  TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(expected));
}

void test_writer_overflow() {
  uint8_t buffer[3];
  TlvWriter writer(buffer, sizeof(buffer));
  const uint8_t value[] = {0xaa, 0xbb};
  writer.WriteBytes(1, value, sizeof(value));
  TEST_ASSERT_TRUE(writer.failed());
  TEST_ASSERT_EQUAL(0, writer.size());
  writer.WriteFlag(3);
  TEST_ASSERT_TRUE(writer.failed());
  TEST_ASSERT_EQUAL(0, writer.size());
}

void test_writer_rejects_oversized_type_and_length() {
  uint8_t buffer[32];
  TlvWriter typeWriter(buffer, sizeof(buffer));
  typeWriter.WriteFlag(kMaxVarint + 1);
  TEST_ASSERT_TRUE(typeWriter.failed());
  TlvWriter lengthWriter(buffer, sizeof(buffer));
  lengthWriter.WriteBytes(1, buffer, kMaxTlvLength + 1);
  TEST_ASSERT_TRUE(lengthWriter.failed());
  TlvWriter maxTypeWriter(buffer, sizeof(buffer));
  maxTypeWriter.WriteFlag(kMaxVarint);
  TEST_ASSERT_FALSE(maxTypeWriter.failed());
  TEST_ASSERT_EQUAL(9, maxTypeWriter.size());
}

}  // namespace jazzlights

void setUp() {}
void tearDown() {}

int main(int /*argc*/, char** /*argv*/) {
  UNITY_BEGIN();
  RUN_TEST(jazzlights::test_varint_sizes);
  RUN_TEST(jazzlights::test_varint_round_trip);
  RUN_TEST(jazzlights::test_varint_rfc_examples);
  RUN_TEST(jazzlights::test_header_status);
  RUN_TEST(jazzlights::test_reader_iterates);
  RUN_TEST(jazzlights::test_reader_empty_container);
  RUN_TEST(jazzlights::test_reader_errors);
  RUN_TEST(jazzlights::test_reader_maximum_length);
  RUN_TEST(jazzlights::test_integer);
  RUN_TEST(jazzlights::test_fields_success);
  RUN_TEST(jazzlights::test_fields_errors);
  RUN_TEST(jazzlights::test_fields_compare_decoded_type);
  RUN_TEST(jazzlights::test_writer);
  RUN_TEST(jazzlights::test_writer_overflow);
  RUN_TEST(jazzlights::test_writer_rejects_oversized_type_and_length);
  return UNITY_END();
}
