#pragma once

#include <inttypes.h>

enum class Tile : uint8_t
{
    air,
    stone,
    grass,
    dirt,
    sand,
    glass,
    wood_plank,
    leaf,
    snow
};

enum class TileFacing
{
    xp,
    xn,
    yp,
    yn,
    zp,
    zn
};

enum class Axis
{
    x,
    y,
    z
};

typedef uint8_t TextureID;

const TextureID tile_texture_table[] = {0,1,2,3,4,5,6,7,8};