#include <string.h>
#include <stdlib.h>
#include <pspiofilemgr.h>
#include "player/metadata.h"
#include "player/id3.h"

// Sample rate table indexed by MPEG version from frame header
static const int sr_table[4][4] = {
    {11025, 12000, 8000, 0},   // MPEG2.5 (version=0)
    {0, 0, 0, 0},               // reserved (version=1)
    {22050, 24000, 16000, 0},   // MPEG2    (version=2)
    {44100, 48000, 32000, 0},   // MPEG1    (version=3)
};

static int parse_mp3_duration(const char *path) {
    int fp = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fp < 0) return 0;

    // Skip ID3v2 tag if present
    int id3_size = 0;
    char sig[3];
    if (sceIoRead(fp, sig, 3) == 3 && !strncmp(sig, "ID3", 3)) {
        unsigned char size_bytes[4];
        sceIoLseek(fp, 6, PSP_SEEK_SET);
        sceIoRead(fp, size_bytes, 4);
        int raw = (size_bytes[0] << 24) | (size_bytes[1] << 16) |
                  (size_bytes[2] << 8) | size_bytes[3];
        id3_size = ((raw & 0x7f000000) >> 3) | ((raw & 0x7f0000) >> 2) |
                   ((raw & 0x7f00) >> 1) | (raw & 0x7f);
        id3_size += 10;
        sceIoLseek(fp, id3_size, PSP_SEEK_SET);
    } else {
        sceIoLseek(fp, 0, PSP_SEEK_SET);
        id3_size = 0;
    }

    // Read first frame header (4 bytes)
    unsigned char hdr[4];
    if (sceIoRead(fp, hdr, 4) != 4) { sceIoClose(fp); return 0; }

    if (hdr[0] != 0xFF || (hdr[1] & 0xE0) != 0xE0) { sceIoClose(fp); return 0; }

    int version   = (hdr[1] >> 3) & 3;
    int layer     = (hdr[1] >> 1) & 3;
    int protect   = hdr[1] & 1;
    int sr_idx    = (hdr[2] >> 2) & 3;
    int chan_mode = (hdr[3] >> 6) & 3;

    if (version == 1 || sr_idx == 3) { sceIoClose(fp); return 0; }

    int samplerate = sr_table[version][sr_idx];
    if (samplerate <= 0) { sceIoClose(fp); return 0; }

    int is_mpeg1          = (version == 3);
    int is_single_channel = (chan_mode == 3);
    int is_layer3         = (layer == 1);

    // Xing offset: 4 (sync) + optional CRC + side info (varies by version/channel)
    int side;
    if (is_mpeg1)
        side = is_single_channel ? 17 : 32;
    else
        side = is_single_channel ? 9 : 17;

    int xing_off = id3_size + 4 + (protect ? 0 : 2) + side;

    // Try Xing/Info
    unsigned char buf[4];
    sceIoLseek(fp, xing_off, PSP_SEEK_SET);
    if (sceIoRead(fp, buf, 4) == 4) {
        if (!memcmp(buf, "Xing", 4) || !memcmp(buf, "Info", 4)) {
            unsigned char flagb[4];
            if (sceIoRead(fp, flagb, 4) == 4) {
                unsigned long flags = ((unsigned long)flagb[0] << 24) |
                                      ((unsigned long)flagb[1] << 16) |
                                      ((unsigned long)flagb[2] << 8) |
                                      (unsigned long)flagb[3];
                if (flags & 0x01) {
                    unsigned char frameb[4];
                    if (sceIoRead(fp, frameb, 4) == 4) {
                        unsigned long num = ((unsigned long)frameb[0] << 24) |
                                            ((unsigned long)frameb[1] << 16) |
                                            ((unsigned long)frameb[2] << 8) |
                                            (unsigned long)frameb[3];
                        unsigned long spf;
                        if (layer == 3)
                            spf = 384;
                        else if (is_layer3)
                            spf = is_mpeg1 ? 1152 : 576;
                        else
                            spf = 1152;
                        sceIoClose(fp);
                        return (int)((unsigned long long)num * spf / samplerate);
                    }
                }
            }
        }
    }

    // Try VBRI at fixed offset 36 from frame start
    int vbri_off = id3_size + 36;
    sceIoLseek(fp, vbri_off, PSP_SEEK_SET);
    if (sceIoRead(fp, buf, 4) == 4 && !memcmp(buf, "VBRI", 4)) {
        sceIoLseek(fp, vbri_off + 14, PSP_SEEK_SET);
        unsigned char frameb[4];
        if (sceIoRead(fp, frameb, 4) == 4) {
            unsigned long num = ((unsigned long)frameb[0] << 24) |
                                ((unsigned long)frameb[1] << 16) |
                                ((unsigned long)frameb[2] << 8) |
                                (unsigned long)frameb[3];
            unsigned long spf = is_mpeg1 ? 1152 : 576;
            sceIoClose(fp);
            return (int)((unsigned long long)num * spf / samplerate);
        }
    }

    sceIoClose(fp);
    return 0;
}

static bool mp3_load(metadata_loader_t *self, const char *path, metadata_t *out) {
    (void)self;
    memset(out, 0, sizeof(*out));

    struct ID3Tag tag;
    ParseID3((char *)path, &tag);

    strncpy(out->title,  tag.ID3Title,  sizeof(out->title) - 1);
    strncpy(out->artist, tag.ID3Artist, sizeof(out->artist) - 1);
    strncpy(out->album,  tag.ID3Album,  sizeof(out->album) - 1);
    strncpy(out->year,   tag.ID3Year,   META_YEAR_LEN - 1);
    strncpy(out->genre,  tag.ID3GenreText, META_GENRE_LEN - 1);
    out->track = tag.ID3Track;

    out->art_data = extract_album_art(path, &tag, &out->art_len);
    out->art_type = tag.ID3EncapsulatedPictureType;

    out->total_seconds = parse_mp3_duration(path);
    return true;
}

metadata_loader_t metadata_mp3 = {
    .load = mp3_load,
    .priv = NULL,
};


