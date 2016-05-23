/// @file Settings for texture loading and handling.

#ifndef INEXOR_TEX_SETTINGS_H
#define INEXOR_TEX_SETTINGS_H

#include "inexor/rpc/SharedTree.hpp"

extern SharedVar<int> usetexcompress; // from rendergl

extern SharedVar<int> hwtexsize;
extern SharedVar<int> hwtexsize;
extern SharedVar<int> hwcubetexsize;
extern SharedVar<int> hwmaxaniso;
extern SharedVar<int> maxtexsize;
extern SharedVar<int> reducefilter;
extern SharedVar<int> texreduce;
extern SharedVar<int> trilinear;
extern SharedVar<int> bilinear;
extern SharedVar<int> aniso;


extern SharedVar<int> texcompress;
extern SharedVar<int> texcompressquality;

extern SharedVar<int> usenp2;


#endif // INEXOR_TEX_SETTINGS_H
