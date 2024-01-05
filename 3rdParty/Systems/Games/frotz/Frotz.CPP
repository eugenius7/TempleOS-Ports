
#include "TempleLoader.CPP";

U0 Frotz (U8 * game)
{
    TL_Loader ("frotz.prg", game);
}

U0 Advent ()
{
    Frotz ("advent.z5");
}


