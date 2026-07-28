#include "player/utf_convert.h"
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include "../system/opendir.h"
// #include "../system/libminiconv.h"
// #include "../system/mem64.h"
// taken from LightMP3 src
#include "player/genres.h"
#include "player/id3.h"
// Search for FF+D8+FF bytes (first bytes of a jpeg image)
// Returns file position:
int searchJPGstart(int fp, int delta) {
  int retValue = -1;
  int i = 0;

  unsigned char *tBuffer = malloc(sizeof(unsigned char) * (delta + 2));
  if (tBuffer == NULL)
    return -1;

  int startPos = sceIoLseek(fp, 0, PSP_SEEK_CUR);
  sceIoRead(fp, tBuffer, delta + 2);
  sceIoLseek(fp, startPos, PSP_SEEK_SET);

  unsigned char *buff = tBuffer;
  for (i = 0; i < delta; i++) {
    if (!memcmp(buff++, ID3_JPEG, 3)) {
      retValue = startPos + i;
      break;
    }
  }
  free(tBuffer);
  return retValue;
}

// Search for 89 50 4E 47 0D 0A 1A 0A 00 00 00 0D 49 48 44 52 bytes (first bytes
// of a PNG image) Returns file position:
int searchPNGstart(int fp, int delta) {
  int retValue = -1;
  int i = 0;

  unsigned char *tBuffer = malloc(sizeof(unsigned char) * (delta + 15));
  if (tBuffer == NULL)
    return -1;

  int startPos = sceIoLseek(fp, 0, PSP_SEEK_CUR);
  sceIoRead(fp, tBuffer, delta + 15);
  sceIoLseek(fp, startPos, PSP_SEEK_SET);

  unsigned char *buff = tBuffer;
  for (i = 0; i < delta; i++) {
    if (!memcmp(buff++, ID3_PNG, 16)) {
      retValue = startPos + i;
      break;
    }
  }
  free(tBuffer);
  return retValue;
}

// ID3v2 code taken from libID3 by Xart
// http://www.xart.co.uk
short int swapInt16BigToHost(short int arg) {
  short int i = 0;
  int checkEndian = 1;
  if (1 == *(char *)&checkEndian) {
    // Intel (little endian)
    i = arg;
    i = ((i & 0xFF00) >> 8) | ((i & 0x00FF) << 8);
  } else {
    // PPC (big endian)
    i = arg;
  }
  return i;
}

int swapInt32BigToHost(int arg) {
  int i = 0;
  int checkEndian = 1;
  if (1 == *(char *)&checkEndian) {
    // Intel (little endian)
    i = arg;
    i = ((i & 0xFF000000) >> 24) | ((i & 0x00FF0000) >> 8) |
        ((i & 0x0000FF00) << 8) | ((i & 0x000000FF) << 24);
  } else {
    // PPC (big endian)
    i = arg;
  }
  return i;
}

// Reads tag data purging invalid characters:
void readTagData(int fp, int tagLength, int maxTagLength, int encoding,
                 char *tagValue, size_t tagValueSize)
{
  uint8_t raw[260];

  if (tagLength > maxTagLength)
    tagLength = maxTagLength;

  if (tagLength >= sizeof(raw))
    tagLength = sizeof(raw) - 1;

  sceIoRead(fp, raw, tagLength);
  raw[tagLength] = '\0';

  switch (encoding) {
  case 1: /* UTF-16 with BOM */
    if (tagLength >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {

      utf16_to_utf8(raw + 2, tagLength - 2, tagValue, tagValueSize, 0);
    } else if (tagLength >= 2 && raw[0] == 0xFE && raw[1] == 0xFF) {

      utf16_to_utf8(raw + 2, tagLength - 2, tagValue, tagValueSize, 1);
    } else {
      tagValue[0] = '\0';
    }
    break;

  case 2: /* UTF-16BE */
    utf16_to_utf8(raw, tagLength, tagValue, tagValueSize, 1);
    break;

  case 0: /* ISO-8859-1-ish */
  case 3: /* UTF-8 */
  default:
    snprintf(tagValue, tagValueSize, "%.*s", tagLength, (char *)raw);
    break;
  }
}

int ID3v2TagSize(const char *mp3path) {
  int fp = 0;
  int size;
  char sig[3];

  fp = sceIoOpen(mp3path, PSP_O_RDONLY, 0777);
  if (fp < 0)
    return 0;

  sceIoRead(fp, sig, sizeof(sig));
  if (strncmp("ID3", sig, 3) != 0) {
    sceIoClose(fp);
    return 0;
  }

  sceIoLseek(fp, 6, PSP_SEEK_SET);
  sceIoRead(fp, &size, sizeof(unsigned int));
  /*
   *  The ID3 tag size is encoded with four bytes where the first bit
   *  (bit 7) is set to zero in every byte, making a total of 28 bits. The
   * zeroed bits are ignored, so a 257 bytes long tag is represented as $00 00
   * 02 01.
   */

  size = (unsigned int)swapInt32BigToHost((int)size);
  size = (((size & 0x7f000000) >> 3) | ((size & 0x7f0000) >> 2) |
          ((size & 0x7f00) >> 1) | (size & 0x7f));
  sceIoClose(fp);
  return size;
}

int ID3v2(const char *mp3path) {
  char sig[3];
  unsigned short int version;

  int fp = sceIoOpen(mp3path, PSP_O_RDONLY, 0777);
  if (fp < 0)
    return 0;

  sceIoRead(fp, sig, sizeof(sig));
  if (!strncmp("ID3", sig, 3)) {
    sceIoRead(fp, &version, sizeof(unsigned short int));
    version = (unsigned short int)swapInt16BigToHost((short int)version);
    version /= 256;
  }
  sceIoClose(fp);

  return (int)version;
}

void ParseID3v2_2(const char *mp3path, struct ID3Tag *id3tag) {
  int fp = 0;

  int size;
  int tag_length;
  char tag[3];
  unsigned char encoding;
  char buffer[20];

  // if(ID3v2(mp3path) == 2) {
  size = ID3v2TagSize(mp3path);
  fp = sceIoOpen(mp3path, PSP_O_RDONLY, 0777);
  if (fp < 0)
    return;
  sceIoLseek(fp, 10, PSP_SEEK_SET);

  while (size != 0) {
    sceIoRead(fp, tag, 3);
    size -= 3;

    /* read 3 byte big endian tag length */
    sceIoRead(fp, &tag_length, sizeof(unsigned int));
    sceIoLseek(fp, -1, PSP_SEEK_CUR);

    tag_length = (unsigned int)swapInt32BigToHost((int)tag_length);
    tag_length = (tag_length / 256);
    size -= 3;

    /* Perform checks for end of tags and tag length overflow or zero */
    if (*tag == 0 || tag_length > size || tag_length == 0)
      break;

    if (!strncmp("TP1", tag, 3)) /* Artist */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Artist, sizeof(id3tag->ID3Artist));
    } else if (!strncmp("TT2", tag, 3)) /* Title */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Title, sizeof(id3tag->ID3Title));
    } else if (!strncmp("TAL", tag, 3)) /* Album */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Album, sizeof(id3tag->ID3Album));
    } else if (!strncmp("TRK", tag, 3)) /* Track No. */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 8, encoding, id3tag->ID3TrackText, sizeof(id3tag->ID3TrackText));
      id3tag->ID3Track = atoi(id3tag->ID3TrackText);
    } else if (!strncmp("TYE", tag, 3)) /* Year */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 12, encoding, id3tag->ID3Year, sizeof(id3tag->ID3Year));
    } else if (!strncmp("TLE", tag, 3)) /* Length */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 264, encoding, buffer, sizeof(buffer));
      id3tag->ID3Length = atoi(buffer);
    } else if (!strncmp("COM", tag, 3)) /* Comment */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Comment, sizeof(id3tag->ID3Comment));
    } else if (!strncmp("TCO", tag, 3)) /* Genre */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3GenreText, sizeof(id3tag->ID3GenreText));
      if (id3tag->ID3GenreText[0] == '(' &&
          id3tag->ID3GenreText[strlen(id3tag->ID3GenreText) - 1] == ')') {
        id3tag->ID3GenreText[0] = ' ';
        id3tag->ID3GenreText[strlen(id3tag->ID3GenreText) - 1] = '\0';
        int index = atoi(id3tag->ID3GenreText);
        if (index >= 0 && index < genreNumber)
          strcpy(id3tag->ID3GenreText, genreList[index].text);
      }
    } else if (!strncmp("PIC", tag, 3)) /* Picture */
    {
      sceIoLseek(fp, 1, PSP_SEEK_CUR);
      sceIoLseek(fp, 5, PSP_SEEK_CUR);
      id3tag->ID3EncapsulatedPictureType = JPEG_IMAGE;
      id3tag->ID3EncapsulatedPictureOffset = searchJPGstart(fp, 20);
      if (id3tag->ID3EncapsulatedPictureOffset < 0) {
        id3tag->ID3EncapsulatedPictureType = PNG_IMAGE;
        id3tag->ID3EncapsulatedPictureOffset = searchPNGstart(fp, 20);
      }
      tag_length = tag_length - (id3tag->ID3EncapsulatedPictureOffset -
                                 sceIoLseek(fp, 0, PSP_SEEK_CUR));
      id3tag->ID3EncapsulatedPictureLength = tag_length - 6;
      sceIoLseek(fp, tag_length - 6, PSP_SEEK_CUR);
      if (id3tag->ID3EncapsulatedPictureOffset < 0) {
        id3tag->ID3EncapsulatedPictureType = 0;
        id3tag->ID3EncapsulatedPictureOffset = 0;
        id3tag->ID3EncapsulatedPictureLength = 0;
      }
    } else {
      sceIoLseek(fp, tag_length, PSP_SEEK_CUR);
    }
    size -= tag_length;
  }
  strcpy(id3tag->versionfound, "2.2");
  sceIoClose(fp);
  //}
}

void ParseID3v2_3(const char *mp3path, struct ID3Tag *id3tag) {
  int fp = 0;

  int size;
  int tag_length;
  char tag[4];
  unsigned char encoding;
  char buffer[20];

  // if(ID3v2(mp3path) == 3) {
  size = ID3v2TagSize(mp3path);
  fp = sceIoOpen(mp3path, PSP_O_RDONLY, 0777);
  if (fp < 0)
    return;
  sceIoLseek(fp, 10, PSP_SEEK_SET);

  while (size != 0) {
    sceIoRead(fp, tag, 4);
    size -= 4;

    /* read 4 byte big endian tag length */
    sceIoRead(fp, &tag_length, sizeof(unsigned int));
    tag_length = (unsigned int)swapInt32BigToHost((int)tag_length);
    size -= 4;

    sceIoLseek(fp, 2, PSP_SEEK_CUR);
    size -= 2;

    /* Perform checks for end of tags and tag length overflow or zero */
    if (*tag == 0 || tag_length > size || tag_length == 0)
      break;

    if (!strncmp("TPE1", tag, 4)) /* Artist */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Artist, sizeof(id3tag->ID3Artist));
    } else if (!strncmp("TIT2", tag, 4)) /* Title */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Title, sizeof(id3tag->ID3Title));
    } else if (!strncmp("TALB", tag, 4)) /* Album */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Album, sizeof(id3tag->ID3Album));
    } else if (!strncmp("TRCK", tag, 4)) /* Track No. */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 8, encoding, id3tag->ID3TrackText, sizeof(id3tag->ID3TrackText));
      id3tag->ID3Track = atoi(id3tag->ID3TrackText);
    } else if (!strncmp("TYER", tag, 4)) /* Year */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 12, encoding, id3tag->ID3Year, sizeof(id3tag->ID3Year));
    } else if (!strncmp("TLEN", tag, 4)) /* Length in milliseconds */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 264, encoding, buffer, sizeof(buffer));
      id3tag->ID3Length = atol(buffer) / 1000;
    } else if (!strncmp("TCON", tag, 4)) /* Genre */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3GenreText, sizeof(id3tag->ID3GenreText));
      if (id3tag->ID3GenreText[0] == '(' &&
          id3tag->ID3GenreText[strlen(id3tag->ID3GenreText) - 1] == ')') {
        id3tag->ID3GenreText[0] = ' ';
        id3tag->ID3GenreText[strlen(id3tag->ID3GenreText) - 1] = '\0';
        int index = atoi(id3tag->ID3GenreText);
        if (index >= 0 && index < genreNumber)
          strcpy(id3tag->ID3GenreText, genreList[index].text);
      }
    } else if (!strncmp("COMM", tag, 4)) /* Comment */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Comment, sizeof(id3tag->ID3Comment));
    } else if (!strncmp("APIC", tag, 4)) /* Picture */
    {
      sceIoLseek(fp, 1, PSP_SEEK_CUR);
      sceIoLseek(fp, 12, PSP_SEEK_CUR);
      id3tag->ID3EncapsulatedPictureType = JPEG_IMAGE;
      id3tag->ID3EncapsulatedPictureOffset = searchJPGstart(fp, 20);
      if (id3tag->ID3EncapsulatedPictureOffset < 0) {
        id3tag->ID3EncapsulatedPictureType = PNG_IMAGE;
        id3tag->ID3EncapsulatedPictureOffset = searchPNGstart(fp, 20);
      }
      tag_length = tag_length - (id3tag->ID3EncapsulatedPictureOffset -
                                 sceIoLseek(fp, 0, PSP_SEEK_CUR));
      id3tag->ID3EncapsulatedPictureLength = tag_length - 13;
      sceIoLseek(fp, tag_length - 13, PSP_SEEK_CUR);
      if (id3tag->ID3EncapsulatedPictureOffset < 0) {
        id3tag->ID3EncapsulatedPictureType = 0;
        id3tag->ID3EncapsulatedPictureOffset = 0;
        id3tag->ID3EncapsulatedPictureLength = 0;
      }
    } else {
      sceIoLseek(fp, tag_length, PSP_SEEK_CUR);
    }
    size -= tag_length;
  }
  strcpy(id3tag->versionfound, "2.3");
  sceIoClose(fp);
  //}
}

void ParseID3v2_4(const char *mp3path, struct ID3Tag *id3tag) {
  int fp = 0;

  int size;
  int tag_length;
  char tag[4];
  unsigned char encoding;
  char buffer[20];

  // if(ID3v2(mp3path) == 4) {
  size = ID3v2TagSize(mp3path);
  fp = sceIoOpen(mp3path, PSP_O_RDONLY, 0777);
  if (fp < 0)
    return;
  sceIoLseek(fp, 10, PSP_SEEK_SET);

  while (size != 0) {
    sceIoRead(fp, tag, 4);
    size -= 4;

    /* read 4 byte big endian tag length */
    sceIoRead(fp, &tag_length, sizeof(unsigned int));
    tag_length = (unsigned int)swapInt32BigToHost((int)tag_length);
    size -= 4;

    sceIoLseek(fp, 2, PSP_SEEK_CUR);
    size -= 2;

    /* Perform checks for end of tags and tag length overflow or zero */
    if (*tag == 0 || tag_length > size || tag_length == 0)
      break;

    if (!strncmp("TPE1", tag, 4)) /* Artist */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Artist, sizeof(id3tag->ID3Artist));
    } else if (!strncmp("TIT2", tag, 4)) /* Title */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Title, sizeof(id3tag->ID3Title));
    } else if (!strncmp("TALB", tag, 4)) /* Album */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Album, sizeof(id3tag->ID3Album));
    } else if (!strncmp("TRCK", tag, 4)) /* Track No. */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 8, encoding, id3tag->ID3TrackText, sizeof(id3tag->ID3TrackText));
      id3tag->ID3Track = atoi(id3tag->ID3TrackText);
    } else if (!strncmp("TYER", tag, 4)) /* Year */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 12, encoding, id3tag->ID3Year, sizeof(id3tag->ID3Year));
    } else if (!strncmp("TLEN", tag, 4)) /* Length in milliseconds */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 264, encoding, buffer, sizeof(buffer));
      id3tag->ID3Length = atol(buffer) / 1000;
    } else if (!strncmp("TCON", tag, 4)) /* Genre */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3GenreText, sizeof(id3tag->ID3GenreText));
    } else if (!strncmp("COMM", tag, 4)) /* Comment */
    {
      sceIoRead(fp, &encoding, 1);
      readTagData(fp, tag_length - 1, 260, encoding, id3tag->ID3Comment, sizeof(id3tag->ID3Comment));
    } else if (!strncmp("APIC", tag, 4)) /* Picture */
    {
      sceIoLseek(fp, 1, PSP_SEEK_CUR);
      sceIoLseek(fp, 12, PSP_SEEK_CUR);
      id3tag->ID3EncapsulatedPictureType = JPEG_IMAGE;
      id3tag->ID3EncapsulatedPictureOffset = searchJPGstart(fp, 20);
      if (id3tag->ID3EncapsulatedPictureOffset < 0) {
        id3tag->ID3EncapsulatedPictureType = PNG_IMAGE;
        id3tag->ID3EncapsulatedPictureOffset = searchPNGstart(fp, 20);
      }
      tag_length = tag_length - (id3tag->ID3EncapsulatedPictureOffset -
                                 sceIoLseek(fp, 0, PSP_SEEK_CUR));
      id3tag->ID3EncapsulatedPictureLength = tag_length - 13;
      sceIoLseek(fp, tag_length - 13, PSP_SEEK_CUR);
      if (id3tag->ID3EncapsulatedPictureOffset < 0) {
        id3tag->ID3EncapsulatedPictureType = 0;
        id3tag->ID3EncapsulatedPictureOffset = 0;
        id3tag->ID3EncapsulatedPictureLength = 0;
      }
    } else {
      sceIoLseek(fp, tag_length, PSP_SEEK_CUR);
    }
    size -= tag_length;
  }
  strcpy(id3tag->versionfound, "2.4");
  sceIoClose(fp);
  //}
}

int ParseID3v2(const char *mp3path, struct ID3Tag *id3tag) {
  switch (ID3v2(mp3path)) {
  case 2:
    ParseID3v2_2(mp3path, id3tag);
    break;
  case 3:
    ParseID3v2_3(mp3path, id3tag);
    break;
  case 4:
    ParseID3v2_4(mp3path, id3tag);
    break;
  default:
    return -1;
  }

  /* If no Title is found, uses filename - extension for Title. */
  /*if(*id3tag->ID3Title == 0) {
     strcpy(id3tag->ID3Title,strrchr(mp3path,'/') + 1);
     if (*strrchr(id3tag->ID3Title,'.') != 0) *strrchr(id3tag->ID3Title,'.') =
  0;
  }*/
  return 0;
}

int ParseID3v1(const char *mp3path, struct ID3Tag *id3tag) {
  int id3fd; // our local file descriptor
  char id3buffer[512];
  id3fd = sceIoOpen(mp3path, PSP_O_RDONLY, 0777);
  if (id3fd < 0)
    return -1;
  sceIoLseek(id3fd, -128, SEEK_END);
  sceIoRead(id3fd, id3buffer, 128);

  if (strstr(id3buffer, "TAG") != NULL) {
    sceIoLseek(id3fd, -125, SEEK_END);
    sceIoRead(id3fd, id3tag->ID3Title, 30);
    id3tag->ID3Title[30] = '\0';

    sceIoLseek(id3fd, -95, SEEK_END);
    sceIoRead(id3fd, id3tag->ID3Artist, 30);
    id3tag->ID3Artist[30] = '\0';

    sceIoLseek(id3fd, -65, SEEK_END);
    sceIoRead(id3fd, id3tag->ID3Album, 30);
    id3tag->ID3Album[30] = '\0';

    sceIoLseek(id3fd, -35, SEEK_END);
    sceIoRead(id3fd, id3tag->ID3Year, 4);
    id3tag->ID3Year[4] = '\0';

    sceIoLseek(id3fd, -31, SEEK_END);
    sceIoRead(id3fd, id3tag->ID3Comment, 30);
    id3tag->ID3Comment[30] = '\0';

    sceIoLseek(id3fd, -1, SEEK_END);
    sceIoRead(id3fd, id3tag->ID3GenreCode, 1);
    id3tag->ID3GenreCode[1] = '\0';

    /* Track */
    if (*(id3tag->ID3Comment + 28) == 0 && *(id3tag->ID3Comment + 29) > 0) {
      id3tag->ID3Track = (int)*(id3tag->ID3Comment + 29);
      strcpy(id3tag->versionfound, "1.1");
    } else {
      id3tag->ID3Track = 1;
      strcpy(id3tag->versionfound, "1.0");
    }

    if (((int)id3tag->ID3GenreCode[0] >= 0) &
        ((int)id3tag->ID3GenreCode[0] < genreNumber)) {
      strcpy(id3tag->ID3GenreText,
             genreList[(int)id3tag->ID3GenreCode[0]].text);
    } else {
      strcpy(id3tag->ID3GenreText, "");
    }
    id3tag->ID3GenreText[30] = '\0';
  } else {
    sceIoClose(id3fd);
    return -1;
  }
  sceIoClose(id3fd);
  return 0;
}

// Main function:
unsigned char *extract_album_art(const char *mp3path, const struct ID3Tag *tag, size_t *out_len) {
    if (tag->ID3EncapsulatedPictureLength <= 0 || tag->ID3EncapsulatedPictureOffset <= 0) {
        *out_len = 0;
        return NULL;
    }
    int fp = sceIoOpen(mp3path, PSP_O_RDONLY, 0777);
    if (fp < 0) { *out_len = 0; return NULL; }
    sceIoLseek(fp, tag->ID3EncapsulatedPictureOffset, PSP_SEEK_SET);
    unsigned char *data = malloc(tag->ID3EncapsulatedPictureLength);
    if (!data) { sceIoClose(fp); *out_len = 0; return NULL; }
    int n = sceIoRead(fp, data, tag->ID3EncapsulatedPictureLength);
    sceIoClose(fp);
    if (n != tag->ID3EncapsulatedPictureLength) { free(data); *out_len = 0; return NULL; }
    *out_len = n;
    return data;
}

int ParseID3(char *mp3path, struct ID3Tag *target) {
  memset(target, 0, sizeof(struct ID3Tag));

  ParseID3v1(mp3path, target);
  ParseID3v2(mp3path, target);
  if (!strlen(target->ID3Title)) {
    strncpy(target->ID3Title, mp3path, sizeof(target->ID3Title) - 1);
    target->ID3Title[sizeof(target->ID3Title) - 1] = '\0';
  }
  return 0;
}
