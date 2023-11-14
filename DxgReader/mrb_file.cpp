#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#pragma warning(disable : 4996)

#include "mrb_file.h"


static bool read_mrb_data(FILE *fp, struct MRBData *data);
static bool read_mrb_bone(uint32_t flag, const void *read_bufp, size_t len, struct MRBBoneData *bone_data);
static bool read_mrb_animation(uint32_t flag, const void *read_bufp, size_t len, struct MRBAnimationData *anim_data);

static uint32_t
to_ui32(const char *d)
{
    return ((uint32_t)(d[0]))
        | ((uint32_t)(d[1]) << 8)
        | ((uint32_t)(d[2]) << 16)
        | ((uint32_t)(d[3]) << 24);
}

int
read_mrb_file(const char *path, struct MRBFile *mrb)
{
    if ((path == NULL) || (mrb == NULL)) {
        return EINVAL;
    }
    FILE *fp = fopen(path, "rb");
    if (fp == nullptr) {
        fprintf(stderr, "Could not open file. %s\n", path);
        return errno;
    }

    char tmp[16];

    if (fread(tmp, 12, 1, fp) != 1) {
        fprintf(stderr, "Invalid file.\n");
        fclose(fp);
        return EIO;
    }
    if (memcmp(tmp, "MRB\0", 4) != 0) {
        fprintf(stderr, "Not MRB file.\n");
        fclose(fp);
        return ENOTSUP;
    }

    struct MRBFileHeader &header = mrb->header;
    mrb->data.clear();

    memcpy(header.magic, tmp + 0, 4);
    header.tmp = to_ui32(tmp + 4);
    header.data_count = to_ui32(tmp + 8);

    while (!feof(fp) && !ferror(fp)) {
        struct MRBData data;
        if (read_mrb_data(fp, &data)) {
            mrb->data.push_back(data);
        }
    }

    fclose(fp);
    return 0;
}

static bool
read_mrb_data(FILE *fp, struct MRBData *data)
{
    if (fread(&(data->header), sizeof(struct MRBDataHeader), 1, fp) != 1) {
        return false;
    }
    size_t len = data->header.length - 44;
    void *bufp = malloc(len);
    if (bufp == NULL) {
        return false;
    }
    if (fread(bufp, len, 1, fp) != 1) {
        free(bufp);
        return false;
    }
    printf("type = %u, size = %u\n", data->header.type, data->header.length);

    switch (data->header.type) {
    case MRB_TYPE_BONE:
        read_mrb_bone(data->header.flag, bufp, len, &(data->bone));
        break;
    case MRB_TYPE_ANIMATION:
        read_mrb_animation(data->header.flag, bufp, len, &(data->anim));
        break;
    default:
        break;
    }
    free(bufp);

    return true;
}

#define BONE_DATAF_BONENAME (1 << 0)
#define BONE_DATAF_BONE     (1 << 1)

static bool
read_mrb_bone(uint32_t flag, const void *read_bufp, size_t len, struct MRBBoneData *bone_data)
{
    const uint32_t *ptr = (const uint32_t*)(read_bufp);
    std::vector<const char*> bone_name_list;
    size_t left_size = len;

    // Bone name list
    if (flag & BONE_DATAF_BONENAME) {
        size_t bone_list_length;
        size_t bone_list_area_size;
        size_t pos;
        const char *namep;
        const char *begin;

        bone_list_length = ptr[0] * ptr[1];
        ptr += 2;

        bone_list_area_size = ((bone_list_length + 3) / 4) * 4;

        namep = (const char*)(ptr);
        pos = 0;
        while (pos < bone_list_area_size) {
            begin = namep;
            while (((*namep) != '\0') && (pos < bone_list_area_size)) {
                namep++;
                pos++;
            }
            bone_name_list.push_back(begin);
            namep++;
            pos++;
        }

        ptr += (bone_list_area_size / 4);

        left_size -= (4 * 2 - bone_list_area_size);
    }


    // Bone data count.
    if (flag & BONE_DATAF_BONE) {
        size_t bone_count = ptr[0];
        size_t bone_size = ptr[1];
        size_t i;

        ptr += 2;

        left_size -= (4 * 2);
        if (left_size < (bone_count * bone_size)) {
            // Too short left size.
            bone_count = left_size / bone_size;
        }

        for (i = 0; i < bone_count; i++) {
            struct MRBBone bone;
            bone.flag = ptr[0];
            bone.no = (uint8_t)((ptr[1] >> 0) & 0xff);
            bone.parent = (uint8_t)((ptr[1] >> 8) & 0xff);
            bone.child = (uint8_t)((ptr[1] >> 16) & 0xff);
            bone.sibling_no = (uint8_t)((ptr[1] >> 24) & 0xff);

            bone.move.x = *((float*)(&ptr[2]));
            bone.move.y = *((float*)(&ptr[3]));
            bone.move.z = *((float*)(&ptr[4]));

            if (bone_size >= 36) {
                bone.quaternion.a1 = *((float*)(&ptr[5]));
                bone.quaternion.a2 = *((float*)(&ptr[6]));
                bone.quaternion.a3 = *((float*)(&ptr[7]));
                bone.quaternion.a4 = *((float*)(&ptr[8]));
            }

            if (bone_size >= 48) {
                bone.scale.x = *((float*)(&ptr[9]));
                bone.scale.y = *((float*)(&ptr[10]));
                bone.scale.z = *((float*)(&ptr[11]));
            }
            if (i < bone_name_list.size()) {
                bone.name = bone_name_list.at(i);
            }

            bone_data->bones.push_back(bone);
            ptr += (bone_size / 4);
        }
    }

    return true;
}

#define ANIM_DATAF_UNKNOWN1NAME   (1 << 0)
#define ANIM_DATAF_KEYFRAME   (1 << 1)
#define ANIM_DATAF_UNKNOWN1   (1 << 2)
#define ANIM_DATAF_EFFECT     (1 << 3)
#define ANIM_DATAF_UNKNOWN    (1 << 4)
#define ANIM_DATAF_MOVE       (1 << 5)
#define ANIM_DATAF_QUATERNION (1 << 6)
#define ANIM_DATAF_SCALE      (1 << 7)
#define ANIM_DATAF_BONE        (1 << 8)

struct mrb_vector {
    float x;
    float y;
    float z;
};
struct mrb_quaternion {
    float x;
    float y;
    float z;
    float w;
};

static bool
read_mrb_animation(uint32_t flag, const void *read_bufp, size_t len, struct MRBAnimationData *anim_data)
{
    const uint32_t *ptr = (const uint32_t*)(read_bufp);

    std::vector<const char*> bone_name_list;
    size_t total = 0;
    size_t key_frame_count = 0;
    std::vector<struct mrb_vector> moves;
    std::vector<struct mrb_quaternion> rotations;
    std::vector<struct mrb_vector> scales;

    // Bone name list
    if (flag & ANIM_DATAF_UNKNOWN1NAME) {
        size_t bone_list_length = ptr[0] * ptr[1];
        size_t bone_list_area_size;
        const char *namep;
        size_t pos;
        const char *begin;

        bone_list_area_size = ((bone_list_length + 3) / 4) * 4;
        ptr += 2;
        namep = (const char*)(ptr);
        pos = 0;
        while (pos < bone_list_area_size) {
            begin = namep;
            while (((*namep) != '\0') && (pos < bone_list_area_size)) {
                namep++;
                pos++;
            }
            bone_name_list.push_back(begin);
            namep++;
            pos++;
        }

        ptr += (bone_list_area_size / 4);
        total += (4 * 2 + bone_list_area_size);
    }

    // Key frame 
    if (flag & ANIM_DATAF_KEYFRAME) {
        key_frame_count = ptr[0];
        size_t key_frame_dsize = ptr[1];
        ptr += 2;

		 for (int i = 0; i < key_frame_count; i++) {
            uint32_t timeKey = *(ptr + key_frame_dsize / 4 * i);
			anim_data->keys.push_back(timeKey);
        }

        ptr += ((key_frame_dsize / 4) * key_frame_count);
        total += (4 * 2 + key_frame_dsize * key_frame_count);
    }


    // Unknown
    if (flag & ANIM_DATAF_UNKNOWN1) {
        size_t unknown_count = ptr[0];
        size_t unknown_dsize = ptr[1];
        ptr += 2;
        ptr += ((unknown_dsize / 4) * unknown_count);
        total += (4 * 2 + unknown_dsize * unknown_dsize);
    }

    // effect
    if (flag & ANIM_DATAF_EFFECT) {
        size_t effect_count = ptr[0];
        size_t effect_dsize = ptr[1];
        ptr += 2;
        ptr += ((effect_dsize / 4 * effect_count));
        total += (4 * 2 + effect_dsize * effect_count);
    }

    // Unknown
    if (flag & ANIM_DATAF_UNKNOWN) {
        size_t un_count = ptr[0];
        size_t un_dsize = ptr[1];
        ptr += 2;
        ptr += ((un_dsize / 4) * un_count);
        total += (4 * 2 + un_dsize * un_count);
    }

    // Move key vector
    if (flag & ANIM_DATAF_MOVE) {
        size_t move_count = ptr[0];
        size_t move_dsize = ptr[1];
        size_t i;
        const struct mrb_vector *vector;
        ptr += 2;

        for (i = 0; i < move_count; i++) {
            vector = (const struct mrb_vector*)(ptr + move_dsize / 4 * i);
            moves.push_back(*vector);
        }
        ptr += ((move_dsize / 4) * move_count);
        total += (4 * 2 + move_dsize * move_count);
    }

    // Rotation key. quaternion
    if (flag & ANIM_DATAF_QUATERNION) {
        size_t quat_count = ptr[0];
        size_t quat_dsize = ptr[1];
        size_t i;
        const struct mrb_quaternion *quat;
        ptr += 2;
        for (i = 0; i < quat_count; i++) {
            quat = (const struct mrb_quaternion*)(ptr + quat_dsize / 4 * i);
            rotations.push_back(*quat);
        }

        ptr += ((quat_dsize / 4) * quat_count);
        total += (4 * 2 + quat_dsize * quat_count);
    }

    // Scale key
    if (flag & ANIM_DATAF_SCALE) {
        size_t scale_count = ptr[0];
        size_t scale_dsize = ptr[1];
        size_t i;
        const struct mrb_vector *vector;
        ptr += 2;

        for (i = 0; i < scale_count; i++) {
            vector = (const struct mrb_vector*)(ptr + scale_dsize / 4 * i);
            scales.push_back(*vector);
        }
        ptr += ((scale_dsize / 4) * scale_count);
        total += (4 * 2 + scale_dsize * scale_count);
    }

    // Key
    if (flag & ANIM_DATAF_BONE) {
        size_t bone_count = ptr[0];
        size_t bone_dsize = ptr[1];
        size_t key_area_size = ((bone_count * bone_dsize + 3) / 4) * 4;
        size_t bone_no;
        size_t frame;
        const uint16_t *bone_datap;
        ptr += 2;

        size_t key_count = bone_dsize / (sizeof(uint16_t) * 3);
        bone_datap = (const uint16_t*)(ptr);
        for (bone_no = 0; bone_no < bone_count; bone_no++) {
            struct MRBAnimationBone bone;
            if (bone_no < bone_name_list.size()) {
                bone.name = bone_name_list.at(bone_no);
            }

            for (frame = 0; frame < key_frame_count; frame++) {
                struct MRBAnimationBoneData data;
                uint16_t move_index = bone_datap[0];
                uint16_t rotation_index = bone_datap[1];
                uint16_t scale_index = bone_datap[2];
                if (move_index < moves.size()) {
                    const struct mrb_vector &v = moves.at(move_index);
                    data.move.x = v.x;
                    data.move.y = v.y;
                    data.move.z = v.z;
                }
                else {
                    data.move.x = 0;
                    data.move.y = 0;
                    data.move.z = 0;
                }
                if (rotation_index < rotations.size()) {
                    const struct mrb_quaternion &q = rotations.at(rotation_index);
                    data.rotation.x = q.x;
                    data.rotation.y = q.y;
                    data.rotation.z = q.z;
                    data.rotation.w = q.w;
                }
                else {
                    data.rotation.x = 0;
                    data.rotation.y = 0;
                    data.rotation.z = 0;
                    data.rotation.w = 0;
                }
                if (scale_index < scales.size()) {
                    const struct mrb_vector &v = scales.at(scale_index);
                    data.expansion.x = v.x;
                    data.expansion.y = v.y;
                    data.expansion.z = v.z;
                }
                else {
                    data.expansion.x = 0;
                    data.expansion.y = 0;
                    data.expansion.z = 0;
                }

                bone.frames.push_back(data);
                bone_datap += 3;
            }
            anim_data->data.push_back(bone);
        }

        ptr += (key_area_size / 4);
        total += (4 * 2 + key_area_size);
    }
    return true;
}