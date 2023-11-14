#ifndef MRB_FILE_H
#define MRB_FILE_H

#include <stdint.h>
#include <vector>

/**
 * MRB file header.
 */
struct MRBFileHeader {
    char magic[4]; /* Magic number "MRB\0" */
    uint32_t tmp; 
    uint32_t data_count; /* Data count of MRB file. */
};

struct MRBDataHeader {
    uint32_t type;
#define MRB_TYPE_MODEL      1
#define MRB_TYPE_BONE       2
#define MRB_TYPE_ANIMATION  3
#define MRB_TYPE_4          4
#define MRB_TYPE_5          5
    char name[32];
    uint32_t length;          // 44 + data length.
    uint32_t flag;
};

struct MRBBone {
    std::string name;
    uint32_t flag;
    uint8_t no;
    uint8_t parent;
    uint8_t child;
    uint8_t sibling_no;
    struct {
        float x; 
        float y;
        float z;
    } move;
    struct {
        float a1;
        float a2;
        float a3;
        float a4;
    } quaternion;
    struct {
        float x;
        float y;
        float z;
    } scale;
};

struct MRBBoneData {
    std::vector<struct MRBBone> bones;
};

struct MRBAnimationArrayKey {
    struct {
        float x;
        float y;
        float z;
    } vector;
    struct {
        float x;
        float y;
        float z;
        float w;
    } quat;
};

struct MRBAnimationEffect {
    uint32_t frame;
    char data[64];
};



struct MRBAnimationBoneData {
    struct {
        float x;
        float y;
        float z;
    } move;
    struct {
        float x;
        float y;
        float z;
        float w;
    } rotation;
    struct {
        float x;
        float y;
        float z;
    } expansion;
};
struct MRBAnimationBone {
    std::string name;
    std::vector<MRBAnimationBoneData> frames;
};

struct MRBAnimationData {
    std::vector<uint32_t> keys;
    std::vector<struct MRBAnimationArrayKey> arrayKeys;
    std::vector<struct MRBAnimationEffect> effects;
    std::vector<MRBAnimationBone> data;
};

struct MRBData {
    struct MRBDataHeader header;
    struct MRBBoneData bone;
    struct MRBAnimationData anim;
};

struct MRBFile {
    MRBFileHeader header;
    std::vector<MRBData> data;
};

int read_mrb_file(const char *path, struct MRBFile *mrb);


#endif /* MRB_FILE_H */