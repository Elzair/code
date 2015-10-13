/// @file management of texture slots as visible ingame.
/// each texture slot can have multiple textures.
/// additional textures can be used for various shaders.
/// It is organized in two structures: virtual Slots and normal Slots.
/// Each Slot can have a node-chain of numerous virtual Slots which contain differing shader stuff,
/// like scale, color, rotation.. So a bunch of textures (a Slot) can easily be varying ingame.

#ifndef INEXOR_TEX_SLOT_H
#define INEXOR_TEX_SLOT_H

class Slot;

enum
{
    VSLOT_SHPARAM = 0,
    VSLOT_SCALE,
    VSLOT_ROTATION,
    VSLOT_OFFSET,
    VSLOT_SCROLL,
    VSLOT_LAYER,
    VSLOT_ALPHA,
    VSLOT_COLOR,
    VSLOT_NUM
};

/// A virtual Slot
/// @see slot.h file description for more info
class VSlot
{
  public:
    /// The Slot this VSlot derived from.
    Slot *slot;
    /// The next VSlot in the variant chain of the Slot.
    VSlot *next;
    int index, changed;
    vector<SlotShaderParam> params;
    bool linked;

    // The actual params this VSlot provides:
    float scale;
    int rotation;
    ivec2 offset;
    vec2 scroll;
    int layer;
    float alphafront, alphaback;
    vec colorscale;
    vec glowcolor;

    VSlot(Slot *slot = NULL, int index = -1);

    void addvariant(Slot *slot);

    void reset()
    {
        params.shrink(0);
        linked = false;
        scale = 1;
        rotation = 0;
        offset = ivec2(0, 0);
        scroll = vec2(0, 0);
        layer = 0;
        alphafront = 0.5f;
        alphaback = 0;
        colorscale = vec(1, 1, 1);
        glowcolor = vec(1, 1, 1);
    }

    void cleanup()
    {
        linked = false;
    }
};

class Slot
{
public:
    /// bitmask containing which textures are already loaded.
    int loaded = 0;

    struct Tex
    {
        int type;
        Texture *t;
        string name;
        int combined;
    };
    int index;
    vector<Tex> sts;
    Shader *shader;
    vector<SlotShaderParam> params;

    // All virtual Slots deriving from this slot in a node chain.
    VSlot *variants;
    uint texmask;
    char *autograss;
    Texture *grasstex, *thumbnail;
    char *layermaskname;
    int layermaskmode;
    float layermaskscale;
    ImageData *layermask;

    Slot(int index = -1) : index(index), variants(NULL), autograss(NULL), layermaskname(NULL), layermask(NULL) { reset(); }

    void reset()
    {
        sts.shrink(0);
        shader = NULL;
        params.shrink(0);
        loaded = 0;
        texmask = 0;
        DELETEA(autograss);
        grasstex = NULL;
        thumbnail = NULL;
        DELETEA(layermaskname);
        layermaskmode = 0;
        layermaskscale = 1;
        if(layermask) DELETEP(layermask);
    }

    void cleanup()
    {
        loaded = 0;
        grasstex = NULL;
        thumbnail = NULL;
        loopv(sts)
        {
            Tex &t = sts[i];
            t.t = NULL;
            t.combined = -1;
        }
    }

    void addtexture(int type, const char *filename, const char *configdir);

    void addvariant(VSlot *vs);
    VSlot *setvariantchain(VSlot *vs);

    VSlot *findvariant(const VSlot &src, const VSlot &delta);

    void combinetextures(int index, Slot::Tex &t, bool msg = true, bool forceload = false);

    Slot &load(bool msg, bool forceload);
    Texture *loadthumbnail();
    void loadlayermask();

    /// Force values for all variant vslots:
    void setscroll(float scrollS, float scrollT);
    void setoffset(int xoffset, int yoffset);
    void setrotate(int rot);
    void setscale(float scale);
    void setalpha(float front, float back);
    void setcolor(float r, float g, float b);
    void setlayer(int *layer, char *name, int *mode, float *scale); // TODO, Layer uses vslots instead of references..
};

struct MSlot : Slot, VSlot
{
    MSlot() : VSlot(this) {}

    void reset()
    {
        Slot::reset();
        VSlot::reset();
    }

    void cleanup()
    {
        Slot::cleanup();
        VSlot::cleanup();
    }
};

extern void loadlayermasks();

extern void clearslots();
extern void cleanupslots();
extern void cleanupvslots();
extern void cleanupmaterialslots();

extern void propagatevslot(VSlot *root, int changed);
extern void texturereset(int first, int num = 0);

extern MSlot &lookupmaterialslot(int slot, bool load = true);
extern Slot &lookupslot(int slot, bool load = true);
extern VSlot &lookupvslot(int slot, bool load = true);
extern VSlot *emptyvslot(Slot &owner);

extern VSlot *editvslot(const VSlot &src, const VSlot &delta);
extern void mergevslot(VSlot &dst, const VSlot &src, const VSlot &delta);

extern void compactvslots(cube *c, int n = 8);
extern void compactvslot(int &index);
extern void compactvslot(VSlot &vs);
extern int compactvslots();

extern void packvslot(vector<uchar> &buf, const VSlot &src);
extern bool unpackvslot(ucharbuf &buf, VSlot &dst, bool delta);

extern vector<Slot *> slots;
extern vector<VSlot *> vslots;

#endif //INEXOR_TEX_SLOT_H

/// TODO make loaded correctly isloaded()
/// make emptyvslot obsolete by providing a better cleaning algorithm
