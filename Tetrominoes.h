#ifndef TETRONIMOS_H
#define TETRONIMOS_H
/* 
 * 
 * 
 * define the 4 rotational states of each of the 7 tetronimos as per https://tetris.wiki/SRS
 * 
 * 
 */

const uint8_t *tet_chars = {"IJLOSTZ"};

#define TET_I 0
#define TET_J 1
#define TET_L 2
#define TET_O 3
#define TET_S 4
#define TET_T 5
#define TET_Z 6

const uint8_t tet_box_size[] = {4, 3, 3, 4, 3, 3, 3};

// can't get PROGMEM to work so define this later when we fix it
// #define FROGMEM PROGMEM`
#define FROGMEM

const uint8_t I0[4] FROGMEM = { 
    B00000000, 
    B00001111, 
    B00000000, 
    B00000000, 
};
const uint8_t I1[4] FROGMEM = { 
    B00000010, 
    B00000010, 
    B00000010, 
    B00000010, 
};
const uint8_t I2[4] FROGMEM = { 
    B00000000, 
    B00000000, 
    B00001111, 
    B00000000, 
};
const uint8_t I3[4] FROGMEM = { 
    B00000100, 
    B00000100, 
    B00000100, 
    B00000100, 
};
const uint8_t J0[3] FROGMEM = { 
    B00000100, 
    B00000111, 
    B00000000, 
};
const uint8_t J1[3] FROGMEM = { 
    B00000011, 
    B00000010, 
    B00000010, 
};
const uint8_t J2[3] FROGMEM = { 
    B00000000, 
    B00000111, 
    B00000001, 
};
const uint8_t J3[3] FROGMEM = { 
    B00000010, 
    B00000010, 
    B00000110, 
};
const uint8_t L0[3] FROGMEM = { 
    B00000001, 
    B00000111, 
    B00000000, 
};
const uint8_t L1[3] FROGMEM = { 
    B00000010, 
    B00000010, 
    B00000011, 
};
const uint8_t L2[3] FROGMEM = { 
    B00000000, 
    B00000111, 
    B00000100, 
};
const uint8_t L3[3] FROGMEM = { 
    B00000110, 
    B00000010, 
    B00000010, 
};
const uint8_t O0[4] FROGMEM = { 
    B00000000, 
    B00000110, 
    B00000110, 
    B00000000, 
};
const uint8_t S0[3] FROGMEM = { 
    B00000011, 
    B00000110, 
    B00000000, 
};
const uint8_t S1[3] FROGMEM = { 
    B00000010, 
    B00000011, 
    B00000001, 
};
const uint8_t S2[3] FROGMEM = { 
    B00000000, 
    B00000011, 
    B00000110, 
};
const uint8_t S3[3] FROGMEM = { 
    B00000100, 
    B00000110, 
    B00000010, 
};
const uint8_t T0[3] FROGMEM = { 
    B00000010, 
    B00000111, 
    B00000000, 
};
const uint8_t T1[3] FROGMEM = { 
    B00000010, 
    B00000011, 
    B00000010, 
};
const uint8_t T2[3] FROGMEM = { 
    B00000000, 
    B00000111, 
    B00000010, 
};
const uint8_t T3[3] FROGMEM = { 
    B00000010, 
    B00000110, 
    B00000010, 
};
const uint8_t Z0[3] FROGMEM = { 
    B00000110, 
    B00000011, 
    B00000000, 
};
const uint8_t Z1[3] FROGMEM = { 
    B00000001, 
    B00000011, 
    B00000010, 
};
const uint8_t Z2[3] FROGMEM = { 
    B00000000, 
    B00000110, 
    B00000011, 
};
const uint8_t Z3[3] FROGMEM = { 
    B00000010, 
    B00000110, 
    B00000100, 
};

const uint8_t* const tet_bms[7][4] FROGMEM = {
    {I0, I1, I2, I3}, 
    {J0, J1, J2, J3}, 
    {L0, L1, L2, L3}, 
    {O0, O0, O0, O0},
    {S0, S1, S2, S3}, 
    {T0, T1, T2, T3}, 
    {Z0, Z1, Z2, Z3}, 
};

#endif // TETRONIMOS_H
