{
   /* smwide_* : custom exports for the Super Metroid Android fork's
    * second-screen companion display - see libretro.cpp's own comment on
    * smwide_write_wram for why this isn't just another retro_* symbol. */
   global: retro_*; smwide_*;
   local: *;
};
