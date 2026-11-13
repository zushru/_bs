#include "ds3231_clock.h"

#include <stddef.h>

namespace att {
namespace {

const uint8_t kTimeRegister = 0x00;
const uint8_t kStatusRegister = 0x0F;
const uint8_t kOscillatorStopMask = 0x80;
const int64_t kUnix2000 = 946684800LL;
const int64_t kSecondsPerDay = 86400LL;

bool validOffset(int16_t offsetMin) {
  return offsetMin >= -720 && offsetMin <= 840;
}

bool leapYear(uint16_t year) {
  return (year % 4u == 0u) &&
         ((year % 100u != 0u) || (year % 400u == 0u));
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t kDays[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  if (month == 2 && leapYear(year)) return 29;
  return kDays[month - 1];
}

bool decodeBcd(uint8_t raw, uint8_t mask, uint8_t maximum,
               uint8_t& value) {
  const uint8_t bcd = static_cast<uint8_t>(raw & mask);
  const uint8_t ones = static_cast<uint8_t>(bcd & 0x0Fu);
  const uint8_t tens = static_cast<uint8_t>((bcd >> 4u) & 0x0Fu);
  if (ones > 9 || tens > 9) return false;
  const uint8_t decoded = static_cast<uint8_t>(tens * 10u + ones);
  if (decoded > maximum) return false;
  value = decoded;
  return true;
}

uint8_t toBcd(uint8_t value) {
  return static_cast<uint8_t>(((value / 10u) << 4u) | (value % 10u));
}

bool localDateTimeToUnix(uint16_t year, uint8_t month, uint8_t day,
                         uint8_t hour, uint8_t minute, uint8_t second,
                         int64_t& localEpoch) {
  if (year < 2000 || year > 2199 || month < 1 || month > 12 ||
      day < 1 || day > daysInMonth(year, month) || hour > 23 ||
      minute > 59 || second > 59) return false;

  int64_t days = 0;
  for (uint16_t y = 2000; y < year; ++y)
    days += leapYear(y) ? 366 : 365;
  for (uint8_t m = 1; m < month; ++m)
    days += daysInMonth(year, m);
  days += static_cast<int64_t>(day - 1u);

  localEpoch = kUnix2000 + days * kSecondsPerDay +
      static_cast<int64_t>(hour) * 3600LL +
      static_cast<int64_t>(minute) * 60LL + second;
  return true;
}

bool unixToLocalDateTime(int64_t localEpoch, uint16_t& year,
                         uint8_t& month, uint8_t& day, uint8_t& hour,
                         uint8_t& minute, uint8_t& second,
                         uint8_t& dayOfWeek) {
  if (localEpoch < kUnix2000) return false;

  int64_t days = (localEpoch - kUnix2000) / kSecondsPerDay;
  int64_t seconds = (localEpoch - kUnix2000) % kSecondsPerDay;
  uint16_t y = 2000;
  while (y <= 2199) {
    const int64_t inYear = leapYear(y) ? 366 : 365;
    if (days < inYear) break;
    days -= inYear;
    ++y;
  }
  if (y > 2199) return false;

  uint8_t m = 1;
  while (m <= 12) {
    const uint8_t inMonth = daysInMonth(y, m);
    if (days < inMonth) break;
    days -= inMonth;
    ++m;
  }
  if (m > 12) return false;

  year = y;
  month = m;
  day = static_cast<uint8_t>(days + 1);
  hour = static_cast<uint8_t>(seconds / 3600LL);
  seconds %= 3600LL;
  minute = static_cast<uint8_t>(seconds / 60LL);
  second = static_cast<uint8_t>(seconds % 60LL);

  // DS3231 day-of-week values are user-defined but must advance sequentially.
  // This adapter writes Sunday=1 ... Saturday=7; the field is not used for
  // epoch conversion or trusted date validation.
  const int64_t daysSince2000 = (localEpoch - kUnix2000) / kSecondsPerDay;
  dayOfWeek = static_cast<uint8_t>((daysSince2000 + 6LL) % 7LL + 1LL);
  return true;
}

bool decodeTimeRegisters(const uint8_t raw[7], uint16_t& year,
                         uint8_t& month, uint8_t& day, uint8_t& hour,
                         uint8_t& minute, uint8_t& second) {
  // Reject reserved bits instead of silently interpreting corrupt register data.
  if ((raw[0] & 0x80u) != 0 || (raw[1] & 0x80u) != 0 ||
      (raw[2] & 0x80u) != 0 || (raw[3] & 0xF8u) != 0 ||
      (raw[4] & 0xC0u) != 0 || (raw[5] & 0x60u) != 0) return false;

  if (!decodeBcd(raw[0], 0x7Fu, 59, second) ||
      !decodeBcd(raw[1], 0x7Fu, 59, minute)) return false;

  if ((raw[2] & 0x40u) != 0) {
    uint8_t hour12 = 0;
    if (!decodeBcd(raw[2], 0x1Fu, 12, hour12) ||
        hour12 < 1 || hour12 > 12) return false;
    const bool pm = (raw[2] & 0x20u) != 0;
    hour = static_cast<uint8_t>((hour12 % 12u) + (pm ? 12u : 0u));
  } else {
    if (!decodeBcd(raw[2], 0x3Fu, 23, hour)) return false;
  }

  uint8_t dayOfWeek = 0;
  uint8_t yearWithinCentury = 0;
  uint8_t monthValue = 0;
  if (!decodeBcd(raw[3], 0x07u, 7, dayOfWeek) || dayOfWeek < 1 ||
      !decodeBcd(raw[4], 0x3Fu, 31, day) || day < 1 ||
      !decodeBcd(raw[5], 0x1Fu, 12, monthValue) || monthValue < 1 ||
      !decodeBcd(raw[6], 0xFFu, 99, yearWithinCentury)) return false;

  year = static_cast<uint16_t>(2000u + yearWithinCentury +
      ((raw[5] & 0x80u) != 0 ? 100u : 0u));
  month = monthValue;
  return day <= daysInMonth(year, month);
}

}  // namespace

Ds3231Code Ds3231Clock::readUtcEpoch(int16_t utcOffsetMin,
                                     Ds3231Reading& out) const {
  if (!validOffset(utcOffsetMin)) return DS3231_INVALID_ARGUMENT;

  uint8_t raw[7] = {};
  uint8_t status = 0;
  if (!io_.readRegisters(kTimeRegister, raw, sizeof raw) ||
      !io_.readRegisters(kStatusRegister, &status, 1)) return DS3231_IO_ERROR;

  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (!decodeTimeRegisters(raw, year, month, day, hour, minute, second))
    return DS3231_INVALID_TIME;

  int64_t localEpoch = 0;
  if (!localDateTimeToUnix(year, month, day, hour, minute, second,
                           localEpoch)) return DS3231_INVALID_TIME;
  const int64_t utcEpoch = localEpoch - static_cast<int64_t>(utcOffsetMin) * 60LL;
  if (utcEpoch < 0 || utcEpoch > 0xFFFFFFFFLL) return DS3231_INVALID_TIME;

  const Ds3231Reading candidate = {
      static_cast<uint32_t>(utcEpoch),
      (status & kOscillatorStopMask) != 0};
  out = candidate;
  return DS3231_OK;
}

Ds3231Code Ds3231Clock::writeUtcEpoch(uint32_t epochUtc,
                                      int16_t utcOffsetMin) {
  if (!validOffset(utcOffsetMin)) return DS3231_INVALID_ARGUMENT;

  const int64_t localEpoch = static_cast<int64_t>(epochUtc) +
      static_cast<int64_t>(utcOffsetMin) * 60LL;
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0, dayOfWeek = 0;
  if (!unixToLocalDateTime(localEpoch, year, month, day, hour, minute,
                           second, dayOfWeek)) return DS3231_INVALID_TIME;

  uint8_t raw[7] = {
      toBcd(second),
      toBcd(minute),
      toBcd(hour),  // Always write 24-hour mode (bit 6 clear).
      toBcd(dayOfWeek),
      toBcd(day),
      static_cast<uint8_t>(toBcd(month) | (year >= 2100 ? 0x80u : 0u)),
      toBcd(static_cast<uint8_t>(year % 100u))};

  // A false transport result is indeterminate: a short/failed I2C transaction
  // may have changed some clock registers. The caller must re-read/reconcile.
  const Ds3231WriteResult result =
      io_.writeRegisters(kTimeRegister, raw, sizeof raw);
  if (result == DS3231_WRITE_OK) return DS3231_OK;
  return result == DS3231_WRITE_NOT_STARTED
      ? DS3231_IO_NOT_STARTED : DS3231_IO_UNCERTAIN;
}

Ds3231Code Ds3231Clock::clearOscillatorStopFlag() {
  uint8_t status = 0;
  if (!io_.readRegisters(kStatusRegister, &status, 1)) return DS3231_IO_ERROR;
  if ((status & kOscillatorStopMask) == 0) return DS3231_OK;

  // A1F/A2F are write-zero-to-clear. Write ones so an alarm latched between
  // this read and the status write is not accidentally discarded.
  const uint8_t alarmFlagsMask = 0x03u;
  status = static_cast<uint8_t>(
      (status | alarmFlagsMask) &
      static_cast<uint8_t>(~kOscillatorStopMask));
  const Ds3231WriteResult writeResult =
      io_.writeRegisters(kStatusRegister, &status, 1);
  if (writeResult == DS3231_WRITE_NOT_STARTED) return DS3231_IO_NOT_STARTED;
  if (writeResult != DS3231_WRITE_OK) return DS3231_IO_UNCERTAIN;

  // Confirm OSF is still clear. A failed read after the write leaves the
  // operation unverified; a newly asserted OSF means the clock is still bad.
  uint8_t verifiedStatus = 0;
  if (!io_.readRegisters(kStatusRegister, &verifiedStatus, 1))
    return DS3231_IO_UNCERTAIN;
  return (verifiedStatus & kOscillatorStopMask) == 0
      ? DS3231_OK : DS3231_OSF_STILL_SET;
}

}  // namespace att
