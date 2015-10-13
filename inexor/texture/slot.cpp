/// @file management of texture slots as visible ingame.
/// each texture slot can have multiple textures.
/// additional textures can be used for various shaders.

#include "inexor/engine/engine.h"
#include "inexor/texture/texture.h"
#include "inexor/texture/macros.h"
#include "inexor/texture/modifiers.h"
#include "inexor/texture/cubemap.h"
#include "inexor/texture/slot.h"
#include "inexor/shared/filesystem.h"

vector<VSlot *> vslots;
vector<Slot *> slots;
MSlot materialslots[(MATF_VOLUME | MATF_INDEX) + 1];
Slot dummyslot;
VSlot dummyvslot(&dummyslot);

/// Some textures get merged/"combined" internally before sending it to the gpu to save memory.
/// Those are currently the spec + depthmap and the normal
int inflictedtexture(int changedtex)
{

}

void Slot::addtexture(int type, const char *filename, const char *configdir)
{
    Slot::Tex &st = sts.add();
    st.type = type;
    st.combined = -1; // we have to combine them in a seperate step..
    st.t = NULL;
    inexor::filesystem::getmedianame(st.name, MAXSTRLEN, filename, DIR_TEXTURE);
    path(st.name);
    loaded = false;
}

/// Resolves a missing part of the texturestack so following textures wont be effected ingame.
void rewireslots(int first, int num)
{
    // TODO

}

/// Resets all textures from the slots-stack.
/// @param first: the texturepos from whereon you want to reset
/// @param num: the number of slots you want to reset from thereon. All if 0
/// @example texturereset(0, 40); resets the first 40 textures
void texturereset(int first, int num)
{
    if(!(identflags&IDF_OVERRIDDEN) && !game::allowedittoggle()) return;
    resetslotshader();
    first = clamp(first, 0, slots.length());
    if(!num) num = slots.length() - first;
    num = clamp(num, 0, slots.length() - first);

    loopi(num)
    {
        Slot *s = slots.remove(first);
        for(VSlot *vs = s->variants; vs; vs = vs->next) vs->slot = &dummyslot;
        delete s;
    }

    while(vslots.length())
    {
        VSlot *vs = vslots.last();
        if(vs->slot != &dummyslot || vs->changed) break;
        delete vslots.pop();
    }

    rewireslots(first, num);
}

ICOMMAND(texturereset, "ii", (int *first, int *last), texturereset(*first, *last));

void materialreset()
{
    if(!(identflags&IDF_OVERRIDDEN) && !game::allowedittoggle()) return;
    loopi((MATF_VOLUME | MATF_INDEX) + 1) materialslots[i].reset();
}

COMMAND(materialreset, "");

static int compactedvslots = 0, compactvslotsprogress = 0, clonedvslots = 0;
static bool markingvslots = false;

void clearslots()
{
    resetslotshader();
    slots.deletecontents();
    vslots.deletecontents();
    loopi((MATF_VOLUME | MATF_INDEX) + 1) materialslots[i].reset();
    clonedvslots = 0;
}

static void assignvslot(VSlot &vs);

static inline void assignvslotlayer(VSlot &vs)
{
    if(vs.layer && vslots.inrange(vs.layer))
    {
        VSlot &layer = *vslots[vs.layer];
        if(layer.index < 0) assignvslot(layer);
    }
}

/// Set the vs.index according to compactvslots global counter
static void assignvslot(VSlot &vs)
{
    vs.index = compactedvslots++;
    assignvslotlayer(vs);
}

///compacting mode: set index to compactvslots++ = vs.index, marking mode, just set vs.index
void compactvslot(int &index)
{
    if(vslots.inrange(index))
    {
        VSlot &vs = *vslots[index];
        if(vs.index < 0) assignvslot(vs);
        if(!markingvslots) index = vs.index;
    }
}

void compactvslot(VSlot &vs)
{
    if(vs.index < 0) assignvslot(vs);
}

/// Loops through all n subcubes of c, (reassigning vs.index to compactvslots++ in markmode and otherwise sets cubetexture to the new vslot) when texture was used
void compactvslots(cube *c, int n)
{
    if((compactvslotsprogress++ & 0xFFF) == 0) renderprogress(min(float(compactvslotsprogress) / allocnodes, 1.0f), markingvslots ? "marking slots..." : "compacting slots...");
    loopi(n)
    {
        if(c[i].children) compactvslots(c[i].children);
        else loopj(6) if(vslots.inrange(c[i].texture[j]))
        {
            VSlot &vs = *vslots[c[i].texture[j]];
            if(vs.index < 0) assignvslot(vs);
            if(!markingvslots) c[i].texture[j] = vs.index;
        }
    }
}

// reset all indicies of the vslots
// the first indicies go to the first variants of all slots, then to all first variants layers
int compactvslots()
{
    clonedvslots = 0;
    markingvslots = false;
    compactedvslots = 0;
    compactvslotsprogress = 0;
    loopv(vslots) vslots[i]->index = -1;
    loopv(slots) slots[i]->variants->index = compactedvslots++;
    loopv(slots) assignvslotlayer(*slots[i]->variants);
    loopv(vslots)
    {
        VSlot &vs = *vslots[i];
        if(!vs.changed && vs.index < 0) { markingvslots = true; break; }
    }
    compactvslots(worldroot);
    int total = compactedvslots;
    compacteditvslots();
    loopv(vslots)
    {
        VSlot *vs = vslots[i];
        if(vs->changed) continue;
        while(vs->next)
        {
            if(vs->next->index < 0) vs->next = vs->next->next;
            else vs = vs->next;
        }
    }
    if(markingvslots)
    {
        markingvslots = false;
        compactedvslots = 0;
        compactvslotsprogress = 0;
        int lastdiscard = 0;
        loopv(vslots)
        {
            VSlot &vs = *vslots[i];
            if(vs.changed || (vs.index < 0 && !vs.next)) vs.index = -1;
            else
            {
                while(lastdiscard < i)
                {
                    VSlot &ds = *vslots[lastdiscard++];
                    if(!ds.changed && ds.index < 0) ds.index = compactedvslots++;
                }
                vs.index = compactedvslots++;
            }
        }
        compactvslots(worldroot);
        total = compactedvslots;
        compacteditvslots();
    }
    compactmruvslots();
    loopv(vslots)
    {
        VSlot &vs = *vslots[i];
        if(vs.index >= 0 && vs.layer && vslots.inrange(vs.layer)) vs.layer = vslots[vs.layer]->index;
    }
    loopv(vslots)
    {
        while(vslots[i]->index >= 0 && vslots[i]->index != i)
            swap(vslots[i], vslots[vslots[i]->index]);
    }
    for(int i = compactedvslots; i < vslots.length(); i++) delete vslots[i];
    vslots.setsize(compactedvslots);
    return total;
}

ICOMMAND(compactvslots, "", (),
{
    extern SharedVar<int> nompedit;
    if(nompedit && multiplayer()) return;
    int oldamount = vslots.length();
    compactvslots();
    conoutf("compacted virtual Slots (before: %d, now: %d)", oldamount, vslots.length());
    allchanged();
});

static void clampvslotoffset(VSlot &dst, Slot *slot = NULL)
{
    if(!slot) slot = dst.slot;
    if(slot && slot->sts.inrange(0))
    {
        if(!slot->loaded) slot->load(false, false);
        int xs = slot->sts[0].t->xs, ys = slot->sts[0].t->ys;
        if((dst.rotation & 5) == 1) swap(xs, ys);
        dst.offset.x %= xs; if(dst.offset.x < 0) dst.offset.x += xs;
        dst.offset.y %= ys; if(dst.offset.y < 0) dst.offset.y += ys;
    }
    else dst.offset.max(0);
}

static void propagatevslot(VSlot &dst, const VSlot &src, int diff, bool edit = false)
{
    if(diff & (1 << VSLOT_SHPARAM)) loopv(src.params) dst.params.add(src.params[i]); //todo cleanup ?? doppelt gemoppelt..
    if(diff & (1 << VSLOT_SCALE)) dst.scale = src.scale;
    if(diff & (1 << VSLOT_ROTATION))
    {
        dst.rotation = src.rotation;
        if(edit && !dst.offset.iszero()) clampvslotoffset(dst);
    }
    if(diff & (1 << VSLOT_OFFSET))
    {
        dst.offset = src.offset;
        if(edit) clampvslotoffset(dst);
    }
    if(diff & (1 << VSLOT_SCROLL)) dst.scroll = src.scroll;
    if(diff & (1 << VSLOT_LAYER)) dst.layer = src.layer;
    if(diff & (1 << VSLOT_ALPHA))
    {
        dst.alphafront = src.alphafront;
        dst.alphaback = src.alphaback;
    }
    if(diff & (1 << VSLOT_COLOR)) dst.colorscale = src.colorscale;
}

/// Apply changes to all following neighbors of root.
/// Making it accordingly to roots vslot
/// @param changed includes info about what changed
void propagatevslot(VSlot *root, int changed)
{
    for(VSlot *vs = root->next; vs; vs = vs->next)
    {
        int diff = changed & ~vs->changed;
        if(diff) propagatevslot(*vs, *root, diff);
    }
}

static void mergevslot(VSlot &dst, const VSlot &src, int diff, Slot *slot = NULL)
{
    if(diff & (1 << VSLOT_SHPARAM)) loopv(src.params)
    {
        const SlotShaderParam &sp = src.params[i];
        loopvj(dst.params)
        {
            SlotShaderParam &dp = dst.params[j];
            if(sp.name == dp.name)
            {
                memcpy(dp.val, sp.val, sizeof(dp.val));
                goto nextparam;
            }
        }
        dst.params.add(sp);
    nextparam:;
    }
    if(diff & (1 << VSLOT_SCALE))
    {
        dst.scale = clamp(dst.scale*src.scale, 1 / 8.0f, 8.0f);
    }
    if(diff & (1 << VSLOT_ROTATION))
    {
        dst.rotation = clamp(dst.rotation + src.rotation, 0, 5);
        if(!dst.offset.iszero()) clampvslotoffset(dst, slot);
    }
    if(diff & (1 << VSLOT_OFFSET))
    {
        dst.offset.add(src.offset);
        clampvslotoffset(dst, slot);
    }
    if(diff & (1 << VSLOT_SCROLL)) dst.scroll.add(src.scroll);
    if(diff & (1 << VSLOT_LAYER)) dst.layer = src.layer;
    if(diff & (1 << VSLOT_ALPHA))
    {
        dst.alphafront = src.alphafront;
        dst.alphaback = src.alphaback;
    }
    if(diff & (1 << VSLOT_COLOR)) dst.colorscale.mul(src.colorscale);
}

void mergevslot(VSlot &dst, const VSlot &src, const VSlot &delta)
{
    dst.changed = src.changed | delta.changed;
    propagatevslot(dst, src, (1 << VSLOT_NUM) - 1);
    mergevslot(dst, delta, delta.changed, src.slot);
}

static bool comparevslot(const VSlot &dst, const VSlot &src, int diff)
{
    if(diff & (1 << VSLOT_SHPARAM))
    {
        if(src.params.length() != dst.params.length()) return false;
        loopv(src.params)
        {
            const SlotShaderParam &sp = src.params[i], &dp = dst.params[i];
            if(sp.name != dp.name || memcmp(sp.val, dp.val, sizeof(sp.val))) return false;
        }
    }
    if(diff & (1 << VSLOT_SCALE) && dst.scale != src.scale) return false;
    if(diff & (1 << VSLOT_ROTATION) && dst.rotation != src.rotation) return false;
    if(diff & (1 << VSLOT_OFFSET) && dst.offset != src.offset) return false;
    if(diff & (1 << VSLOT_SCROLL) && dst.scroll != src.scroll) return false;
    if(diff & (1 << VSLOT_LAYER) && dst.layer != src.layer) return false;
    if(diff & (1 << VSLOT_ALPHA) && (dst.alphafront != src.alphafront || dst.alphaback != src.alphaback)) return false;
    if(diff & (1 << VSLOT_COLOR) && dst.colorscale != src.colorscale) return false;
    return true;
}

///Pack a virtual Slot after it has been modified for broadcasting it to the other clients.
void packvslot(vector<uchar> &buf, const VSlot &src)
{
    if(src.changed & (1 << VSLOT_SHPARAM))
    {
        loopv(src.params)
        {
            const SlotShaderParam &p = src.params[i];
            buf.put(VSLOT_SHPARAM);
            sendstring(p.name, buf);
            loopj(4) putfloat(buf, p.val[j]);
        }
    }
    if(src.changed & (1 << VSLOT_SCALE))
    {
        buf.put(VSLOT_SCALE);
        putfloat(buf, src.scale);
    }
    if(src.changed & (1 << VSLOT_ROTATION))
    {
        buf.put(VSLOT_ROTATION);
        putfloat(buf, src.rotation);
    }
    if(src.changed & (1 << VSLOT_OFFSET))
    {
        buf.put(VSLOT_OFFSET);
        putint(buf, src.offset.x);
        putint(buf, src.offset.y);
    }
    if(src.changed & (1 << VSLOT_SCROLL))
    {
        buf.put(VSLOT_SCROLL);
        putfloat(buf, src.scroll.x);
        putfloat(buf, src.scroll.y);
    }
    if(src.changed & (1 << VSLOT_LAYER))
    {
        buf.put(VSLOT_LAYER);
        putuint(buf, vslots.inrange(src.layer) && !vslots[src.layer]->changed ? src.layer : 0);
    }
    if(src.changed & (1 << VSLOT_ALPHA))
    {
        buf.put(VSLOT_ALPHA);
        putfloat(buf, src.alphafront);
        putfloat(buf, src.alphaback);
    }
    if(src.changed & (1 << VSLOT_COLOR))
    {
        buf.put(VSLOT_COLOR);
        putfloat(buf, src.colorscale.r);
        putfloat(buf, src.colorscale.g);
        putfloat(buf, src.colorscale.b);
    }
    buf.put(0xFF);
}

void packvslot(vector<uchar> &buf, int index)
{
    if(vslots.inrange(index)) packvslot(buf, *vslots[index]);
    else buf.put(0xFF);
}

void packvslot(vector<uchar> &buf, const VSlot *vs)
{
    if(vs) packvslot(buf, *vs);
    else buf.put(0xFF);
}

/// Unpack a virtual Slot modified by another client.
bool unpackvslot(ucharbuf &buf, VSlot &dst, bool delta)
{
    while(buf.remaining())
    {
        int changed = buf.get();
        if(changed >= 0x80) break;
        switch(changed)
        {
        case VSLOT_SHPARAM:
        {
            string name;
            getstring(name, buf);
            SlotShaderParam p = { name[0] ? getshaderparamname(name) : NULL, -1,{ 0, 0, 0, 0 } };
            loopi(4) p.val[i] = getfloat(buf);
            if(p.name) dst.params.add(p);
            break;
        }
        case VSLOT_SCALE:
            dst.scale = getfloat(buf);
            if(dst.scale <= 0) dst.scale = 1;
            else if(!delta) dst.scale = clamp(dst.scale, 1 / 8.0f, 8.0f);
            break;
        case VSLOT_ROTATION:
            dst.rotation = getint(buf);
            if(!delta) dst.rotation = clamp(dst.rotation, 0, 5);
            break;
        case VSLOT_OFFSET:
            dst.offset.x = getint(buf);
            dst.offset.y = getint(buf);
            if(!delta) dst.offset.max(0);
            break;
        case VSLOT_SCROLL:
            dst.scroll.x = getfloat(buf);
            dst.scroll.y = getfloat(buf);
            break;
        case VSLOT_LAYER:
        {
            int tex = getuint(buf);
            dst.layer = vslots.inrange(tex) ? tex : 0;
            break;
        }
        case VSLOT_ALPHA:
            dst.alphafront = clamp(getfloat(buf), 0.0f, 1.0f);
            dst.alphaback = clamp(getfloat(buf), 0.0f, 1.0f);
            break;
        case VSLOT_COLOR:
            dst.colorscale.r = clamp(getfloat(buf), 0.0f, 2.0f);
            dst.colorscale.g = clamp(getfloat(buf), 0.0f, 2.0f);
            dst.colorscale.b = clamp(getfloat(buf), 0.0f, 2.0f);
            break;
        default:
            return false;
        }
        dst.changed |= 1 << changed;
    }
    if(buf.overread()) return false;
    return true;
}


inline void Slot::addvariant(VSlot *vs)
{
    if(!variants) variants = vs;
    else
    {
        VSlot *prev = variants;
        while(prev->next) prev = prev->next;
        prev->next = vs;
    }
}

VSlot *Slot::findvariant(const VSlot &src, const VSlot &delta)
{
    for(VSlot *dst = variants; dst; dst = dst->next)
    {
        if((!dst->changed || dst->changed == (src.changed | delta.changed)) &&
            comparevslot(*dst, src, src.changed & ~delta.changed) &&
            comparevslot(*dst, delta, delta.changed))
            return dst;
    }
    return NULL;
}

VSlot::VSlot(Slot *slot, int index) : slot(slot), next(NULL), index(index), changed(0)
{
    reset();
    if(slot) slot->addvariant(this);
}

/// Sets a chain of VSlot variants for the owner slot.
VSlot *Slot::setvariantchain(VSlot *vs)
{
    variants = vs;
    while(vs)
    {
        vs->slot = this;
        vs->linked = false;
        vs = vs->next;
    }
    return vs;
}

/// Wrapper around new VSlot to reuse unused dead vslots.
/// @warning not threadsafe since it accesses slots and vslots globals.
VSlot *emptyvslot(Slot &owner)
{
    int offset = 0;
    loopvrev(slots) if(slots[i]->variants) { offset = slots[i]->variants->index + 1; break; }
    for(int i = offset; i < vslots.length(); i++) if(!vslots[i]->changed) return owner.setvariantchain(vslots[i]);
    return vslots.add(new VSlot(&owner, vslots.length()));
}

static VSlot *clonevslot(const VSlot &src, const VSlot &delta)
{
    VSlot *dst = vslots.add(new VSlot(src.slot, vslots.length()));
    dst->changed = src.changed | delta.changed;
    propagatevslot(*dst, src, ((1 << VSLOT_NUM) - 1) & ~delta.changed);
    propagatevslot(*dst, delta, delta.changed, true);
    return dst;
}

VARP(autocompactvslots, 0, 256, 0x10000);

/// Select a VSlot beeing src but having modifications based on delta (??)
VSlot *editvslot(const VSlot &src, const VSlot &delta)
{
    VSlot *exists = src.slot->findvariant(src, delta);
    if(exists) return exists;
    if(vslots.length() >= 0x10000)
    {
        compactvslots();
        allchanged();
        if(vslots.length() >= 0x10000) return NULL;
    }
    if(autocompactvslots && ++clonedvslots >= autocompactvslots)
    {
        compactvslots();
        allchanged();
    }
    return clonevslot(src, delta);
}

static void fixinsidefaces(cube *c, const ivec &o, int size, int tex)
{
    loopi(8)
    {
        ivec co(i, o, size);
        if(c[i].children) fixinsidefaces(c[i].children, co, size>>1, tex);
        else loopj(6) if(!visibletris(c[i], j, co, size))
            c[i].texture[j] = tex;
    }
}

ICOMMAND(fixinsidefaces, "i", (int *tex),
{
    extern SharedVar<int> nompedit;
    if(noedit(true) || (nompedit && multiplayer())) return;
    fixinsidefaces(worldroot, ivec(0, 0, 0), worldsize >> 1, *tex && vslots.inrange(*tex) ? *tex : DEFAULT_GEOM);
    allchanged();
});

static int findtextype(Slot &s, int type, int last = -1)
{
    for(int i = last + 1; i<s.sts.length(); i++) if((type&(1 << s.sts[i].type)) && s.sts[i].combined<0) return i;
    return -1;
}

static void addglow(ImageData &c, ImageData &g, const vec &glowcolor)
{
    if(g.bpp < 3)
    {
        readwritergbtex(c, g,
            loopk(3) dst[k] = clamp(int(dst[k]) + int(src[0] * glowcolor[k]), 0, 255);
        );
    }
    else
    {
        readwritergbtex(c, g,
            loopk(3) dst[k] = clamp(int(dst[k]) + int(src[k] * glowcolor[k]), 0, 255);
        );
    }
}

static void mergespec(ImageData &c, ImageData &s, bool envmap = false)
{
    if(s.bpp < 3)
    {
        if(envmap)
        {
            readwritergbatex(c, s,
                dst[3] = int(dst[3])*int(src[0]) / 255;
            );
        }
        else
        {
            readwritergbatex(c, s,
                dst[3] = src[0];
            );
        }
    }
    else if(envmap)
    {
        readwritergbatex(c, s,
            dst[3] = int(dst[3])*(int(src[0]) + int(src[1]) + int(src[2])) / (3 * 255);
        );
    }
    else
    {
        readwritergbatex(c, s,
            dst[3] = (int(src[0]) + int(src[1]) + int(src[2])) / 3;
        );
    }
}

static void mergedepth(ImageData &c, ImageData &z)
{
    readwritergbatex(c, z,
        dst[3] = src[0];
    );
}

static void addname(vector<char> &key, Slot &slot, Slot::Tex &t, bool combined = false, const char *prefix = NULL)
{
    if(combined) key.add('&');
    if(prefix) { while(*prefix) key.add(*prefix++); }
    for(const char *s = path(t.name); *s; key.add(*s++));
}

/// Generate a keyname to find a combined texture in the texture registry.
void gencombinedname(vector<char> &name, int &texmask, Slot &s, Slot::Tex &t, int index, bool forceload)
{
    addname(name, s, t);

    if(!forceload) switch(t.type)
    {
        case TEX_DIFFUSE:
        case TEX_NORMAL:
        {
            int i = findtextype(s, t.type == TEX_DIFFUSE ? (1 << TEX_SPEC) : (1 << TEX_DEPTH));
            if(i<0) break;
            texmask |= 1 << s.sts[i].type;
            s.sts[i].combined = index;
            addname(name, s, s.sts[i], true);
            break;
        }
    }
    name.add('\0');
}

/// Combine and load texture data to be ready for sending it to the gpu.
/// Combination is used to merge the diffuse and the specularity map into one texture (spec as alpha)
/// and to merge the normal info and the depth info into another (depth as alpha)
/// @param msg show progress bar.
void Slot::combinetextures(int index, Slot::Tex &t, bool msg, bool forceload)
{
    vector<char> key;
    int texmask = 0; // receive control mask, todo check neccessarity

    gencombinedname(key, texmask, *this, t, index, forceload);

    t.t = gettexture(key.getbuf()); //todo check if working
    if(t.t) return;
    int compress = 0;
    ImageData ts;
    if(!texturedata(ts, NULL, &t, msg, &compress)) { t.t = notexture; return; }
    switch(t.type)
    {
        case TEX_DIFFUSE:
        case TEX_NORMAL:
            if(!ts.compressed) loopv(sts)
            {
                Slot::Tex &a = sts[i];
                if(a.combined != index) continue;
                ImageData as;
                if(!texturedata(as, NULL, &a, msg)) continue;
                if(as.w != ts.w || as.h != ts.h) scaleimage(as, ts.w, ts.h);
                switch(a.type)
                {
                    case TEX_SPEC: mergespec(ts, as); break;
                    case TEX_DEPTH: mergedepth(ts, as); break;
                }
                break; // only one combination
            }
            break;
    }
    t.t = newtexture( t.t, key.getbuf(), ts, 0, true, true, true, compress);
}

MSlot &lookupmaterialslot(int index, bool load)
{
    MSlot &s = materialslots[index];
    if(load && !s.linked)
    {
        if(!s.loaded) s.load(true, true);
        linkvslotshader(s);
        s.linked = true;
    }
    return s;
}

Slot &lookupslot(int index, bool load)
{
    Slot &s = slots.inrange(index) ? *slots[index] : (slots.inrange(DEFAULT_GEOM) ? *slots[DEFAULT_GEOM] : dummyslot);
    return s.loaded || !load ? s : s.load(true, false);
}

VSlot &lookupvslot(int index, bool load)
{
    VSlot &s = vslots.inrange(index) && vslots[index]->slot ? *vslots[index] : (slots.inrange(DEFAULT_GEOM) && slots[DEFAULT_GEOM]->variants ? *slots[DEFAULT_GEOM]->variants : dummyvslot);
    if(load && !s.linked)
    {
        if(!s.slot->loaded) s.slot->load(true, false);
        linkvslotshader(s);
        s.linked = true;
    }
    return s;
}

void linkslotshaders()
{
    loopv(slots) if(slots[i]->loaded) linkslotshader(*slots[i]);
    loopv(vslots) if(vslots[i]->linked) linkvslotshader(*vslots[i]);
    loopi((MATF_VOLUME | MATF_INDEX) + 1) if(materialslots[i].loaded)
    {
        linkslotshader(materialslots[i]);
        linkvslotshader(materialslots[i]);
    }
}

Slot &Slot::load(bool msg, bool forceload)
{
    linkslotshader(*this);
    loopv(sts)
    {
        Slot::Tex &t = sts[i];
        if(t.combined >= 0) continue;
        switch(t.type)
        {
        case TEX_ENVMAP:
            t.t = cubemapload(t.name);
            break;

        default:
            combinetextures(i, t, msg, forceload);
            break;
        }
    }
    loaded = true;
    return *this;
}

/// Generate a preview image of this slot for the texture browser.
Texture *Slot::loadthumbnail()
{
    if(thumbnail) return thumbnail;
    if(!variants)
    {
        thumbnail = notexture;
        return thumbnail;
    }
    VSlot &vslot = *variants;
    linkslotshader(*this, false);
    linkvslotshader(vslot, false);
    vector<char> name;
    if(vslot.colorscale == vec(1, 1, 1)) addname(name, *this, sts[0], false, "<thumbnail>");
    else
    {
        defformatstring(prefix, "<thumbnail:%.2f/%.2f/%.2f>", vslot.colorscale.x, vslot.colorscale.y, vslot.colorscale.z);
        addname(name, *this, sts[0], false, prefix);
    }
    int glow = -1;
    if(texmask&(1 << TEX_GLOW))
    {
        loopvj(sts) if(sts[j].type == TEX_GLOW) { glow = j; break; }
        if(glow >= 0)
        {
            defformatstring(prefix, "<glow:%.2f/%.2f/%.2f>", vslot.glowcolor.x, vslot.glowcolor.y, vslot.glowcolor.z);
            addname(name, *this, sts[glow], true, prefix);
        }
    }
    VSlot *layer = vslot.layer ? &lookupvslot(vslot.layer, false) : NULL;
    if(layer)
    {
        if(layer->colorscale == vec(1, 1, 1)) addname(name, *layer->slot, layer->slot->sts[0], true, "<layer>");
        else
        {
            defformatstring(prefix, "<layer:%.2f/%.2f/%.2f>", vslot.colorscale.x, vslot.colorscale.y, vslot.colorscale.z);
            addname(name, *layer->slot, layer->slot->sts[0], true, prefix);
        }
    }
    name.add('\0');
    Texture *t = gettexture(name.getbuf());
    if(t) thumbnail = t;
    else
    {
        ImageData s, g, l;
        texturedata(s, NULL, &sts[0], false);
        if(glow >= 0) texturedata(g, NULL, &sts[glow], false);
        if(layer) texturedata(l, NULL, &layer->slot->sts[0], false);
        if(!s.data) t = thumbnail = notexture;
        else
        {
            if(vslot.colorscale != vec(1, 1, 1)) texmad(s, vslot.colorscale, vec(0, 0, 0));
            int xs = s.w, ys = s.h;
            if(s.w > 64 || s.h > 64) scaleimage(s, min(s.w, 64), min(s.h, 64));
            if(g.data)
            {
                if(g.w != s.w || g.h != s.h) scaleimage(g, s.w, s.h);
                addglow(s, g, vslot.glowcolor);
            }
            if(l.data)
            {
                if(layer->colorscale != vec(1, 1, 1)) texmad(l, layer->colorscale, vec(0, 0, 0));
                if(l.w != s.w / 2 || l.h != s.h / 2) scaleimage(l, s.w / 2, s.h / 2);
                forcergbimage(s);
                forcergbimage(l);
                uchar *dstrow = &s.data[s.pitch*l.h + s.bpp*l.w], *srcrow = l.data;
                loop(y, l.h)
                {
                    for(uchar *dst = dstrow, *src = srcrow, *end = &srcrow[l.w*l.bpp]; src < end; dst += s.bpp, src += l.bpp)
                        loopk(3) dst[k] = src[k];
                    dstrow += s.pitch;
                    srcrow += l.pitch;
                }
            }
            t = newtexture(NULL, name.getbuf(), s, 0, false, false, true);
            t->xs = xs;
            t->ys = ys;
            thumbnail = t;
        }
    }
    return t;
}

void Slot::loadlayermask()
{
    if(loaded && layermaskname && !layermask)
    {
        layermask = new ImageData;
        texturedata(*layermask, layermaskname);
        if(!layermask->data) DELETEP(layermask);
    }
}

void Slot::setscroll(float scrollS, float scrollT)
{
    variants->scroll = vec2(scrollS, scrollT).div(1000.0f);
    propagatevslot(variants, 1 << VSLOT_SCROLL);
}

void Slot::setoffset(int xoffset, int yoffset)
{
    variants->offset = ivec2(xoffset, yoffset).max(0);
    propagatevslot(variants, 1 << VSLOT_OFFSET);
}

void Slot::setrotate(int rot)
{
    variants->rotation = clamp(rot, 0, 5);
    propagatevslot(variants, 1 << VSLOT_ROTATION);
}

void Slot::setscale(float scale)
{
    variants->scale = scale <= 0 ? 1 : scale;
    propagatevslot(variants, 1 << VSLOT_SCALE);
}

void Slot::setalpha(float front, float back)
{
    variants->alphafront = clamp(front, 0.0f, 1.0f);
    variants->alphaback = clamp(back, 0.0f, 1.0f);
    propagatevslot(variants, 1 << VSLOT_ALPHA);
}

void Slot::setcolor(float r, float g, float b)
{
    variants->colorscale = vec(r, g, b).clamp(0.0f, 1.0f);
    propagatevslot(variants, 1 << VSLOT_COLOR);
}

void loadlayermasks()
{
    loopv(slots) slots[i]->loadlayermask();
}

void cleanupslots()
{
    loopv(slots) slots[i]->cleanup();
}

void cleanupvslots()
{
    loopv(vslots) vslots[i]->cleanup();
}

void cleanupmaterialslots()
{
    loopi((MATF_VOLUME | MATF_INDEX) + 1) materialslots[i].cleanup();
}
