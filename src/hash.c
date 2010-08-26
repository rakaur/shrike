/*
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains misc routines.
 */

#include "../inc/shrike.h"

FILE *log_file;

/* hash_upper()
 *     Converts a string to upper case and hashes the specified bits.
 *
 * inputs     - string to hash, bits
 * outputs    - hashed value
 */
uint32_t hash_upper(const char *s, int bits)
{
  uint32_t h = FNV1_32_INIT;

  while (*s)
  {
    h ^= ToUpper(*s++);     /* XXX rfc case mapping */
    h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
  }

    h = (h >> bits) ^ (h & ((2 ^ bits) - 1));

    return h;
}

/* hash_upper_len()
 *     Converts a string to upper case and hashes the specified length.
 *
 * inputs     - string to hash, bits, len
 * outputs    - hashed value
 */
uint32_t hash_upper_len(const char *s, int bits, int len)
{
  uint32_t h = FNV1_32_INIT;
  const char *x = (s + len);

  while ((*s) && (s < x))
  {
    h ^= ToUpper(*s++);     /* XXX rfc case mapping */
    h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
  }

  h = (h >> bits) ^ (h & ((2 ^ bits) - 1));

  return h;
}

/* hash()
 *     Hashes the a specified string.
 *
 * inputs     - string to hash, bits
 * outputs    - hashed value
 */
uint32_t hash(const char *s, int bits)
{
  uint32_t h = FNV1_32_INIT;

  while (*s)
  {
    h ^= *s++;
    h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
  }

  h = (h >> bits) ^ (h & ((2 ^ bits) - 1));

  return h;
}

/* hash_len()
 *     Hashes the specified part of a string.
 *
 * inputs     - string to hash, bits, len
 * outputs    - hashed value
 */
uint32_t hash_len(const char *s, int bits, int len)
{
  uint32_t h = FNV1_32_INIT;
  const char *x = (s + len);

  while ((*s) && (s < x))
  {
    h ^= *s++;
    h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
  }

  h = (h >> bits) ^ (h & ((2 ^ bits) - 1));

  return h;
}

/* hash_server()
 *     Hashes a server name.
 *
 * inputs     - string to hash
 * outputs    - hashed value
 */
uint32_t hash_server(const char *s)
{
  return hash_upper(s, SERVER_HASH_BITS);
}

/* hash_nick()
 *     Hashes a nickname.
 *
 * inputs     - string to hash
 * outputs    - hashed value
 */
uint32_t hash_nick(const char *s)
{
  return hash_upper(s, USER_HASH_BITS);
}

/* hash_channel()
 *     Hashes a channel name.
 *
 * inputs     - string to hash
 * outputs    - hashed value
 */
uint32_t hash_channel(const char *s)
{
  return hash_upper_len(s, CHANNEL_HASH_BITS, 30);
}

/* hash_cmd()
 *     A horrible hashing function for command hash tables.
 *
 * inputs     - string to hash
 * outputs    - hashed value
 */
uint32_t hash_cmd(const char *s)
{
  long h = 0, g;

  while (*s)
  {
    h = (h << 4) + ToLower(*s++);   /* XXX rfc casemapping */

    if ((g = (h & 0xF0000000)))
      h ^= g >> 24;

    h &= ~g;
  }

  return (h % CMD_HASH_SIZE);
}
