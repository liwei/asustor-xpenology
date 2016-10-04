//[GOOD]pka_bn_modexp_289.dat
//modulus
#if 0
unsigned int modexp_size = 128;
unsigned int modexp_m[] = {
0xDCF46EBA,
0x8CC39F01,
0xF5A0BCC3,
0x60436681
};
//op1
unsigned int modexp_a[] = {
0x1AF005B6,
0xDD9855E5,
0x743201BB,
0xCDC086A3
};
//op2
unsigned int modexp_b[] = {
0x00000000,
0x00000000,
0x00000000,
0x00000001
};
unsigned int modexp_ry[] = {
0x1AF005B6,
0xDD9855E5,
0x743201BB,
0xCDC086A3
};
#endif

#if 1
unsigned int modexp_size = 512;
unsigned int modexp_m[] = {
0xCF0688B1,
0x5701BC24,
0xE0F69F93,
0x86A6BA53,
0x571E128A,
0x916EDDD9,
0x60757D7D,
0x0ED8415E,
0xDEC90F36,
0xCACB5AAB,
0xC1F93E47,
0x9FF99BF6,
0x17AD81A8,
0x1B5E827C,
0xD3FFF9E2,
0xD83A40B7
};
//op1
unsigned int modexp_a[] = {
0x5310184B,
0x5EC2B501,
0x1AFC0722,
0x17855FBC,
0xA4AA33BE,
0x3E28CCBD,
0x08F9C2F6,
0x4A88BE9D,
0x98D6E8F6,
0x989EF7B2,
0x9AFED4B1,
0x84336D28,
0xDEA0E71C,
0xC9B3DAD1,
0xAD9CC7F7,
0x258595BE
};
//op2
unsigned int modexp_b[] = {
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000001
};
unsigned int modexp_ry[] = {
0x5310184B,
0x5EC2B501,
0x1AFC0722,
0x17855FBC,
0xA4AA33BE,
0x3E28CCBD,
0x08F9C2F6,
0x4A88BE9D,
0x98D6E8F6,
0x989EF7B2,
0x9AFED4B1,
0x84336D28,
0xDEA0E71C,
0xC9B3DAD1,
0xAD9CC7F7,
0x258595BE
};
#endif

//[GOOD]pka_bn_modmul_135
unsigned int modmult_size = 128;
unsigned int modmult_m[] = {
0xF982A68A,
0x59A8A3BD,
0x9AB4F98D,
0x006E87AD
};
unsigned int modmult_a[] = {
0x752FF588,
0x90D60219,
0x64F2113C,
0xF34C74FA
};

unsigned int modmult_b[] = {

0x00000000,
0x00000000,
0x00000000,
0x00000001
};
unsigned int modmult_ry[] = {
0x752FF588,
0x90D60219,
0x64F2113C,
0xF34C74FA
};

//[GOOD]pka_bn_moddiv_2
unsigned int moddiv_size = 128;
unsigned int moddiv_m[] = {
0x8986190F,
0x3A60471E,
0x178B38EE,
0x18F3F17B
};
unsigned int moddiv_a[] = {
0x5E3B3E5C,
0x4C54001A,
0xCD2B0B2C,
0xBE98C888
};

unsigned int moddiv_b[] = {
0x62CD996F,
0x5ED0D66D,
0x0147E7F3,
0xC6C82FB0
};
unsigned int moddiv_ry[] = {
0x5CD8538C,
0x1719D8F4,
0x481C0EBE,
0x0C145949
};

//mod add
unsigned int modadd_size = 128;
unsigned int modadd_m[] = {
0xF982A68A,
0x59A8A3BD,
0x9AB4F98D,
0x006E87AD
};
unsigned int modadd_a[] = {
0x752FF588,
0x90D60219,
0x64F2113C,
0xF34C74F9
};

unsigned int modadd_b[] = {

0x00000000,
0x00000000,
0x00000000,
0x00000001
};
unsigned int modadd_ry[] = {
0x752FF588,
0x90D60219,
0x64F2113C,
0xF34C74FA
};

//mod sub
unsigned int modsub_size = 128;
unsigned int modsub_m[] = {
0xF982A68A,
0x59A8A3BD,
0x9AB4F98D,
0x006E87AD
};
unsigned int modsub_a[] = {
0x752FF588,
0x90D60219,
0x64F2113C,
0xF34C74FB
};

unsigned int modsub_b[] = {

0x00000000,
0x00000000,
0x00000000,
0x00000001
};
unsigned int modsub_ry[] = {
0x752FF588,
0x90D60219,
0x64F2113C,
0xF34C74FA
};

//mod inv based on [GOOD]pka_bn_moddiv_7
unsigned int modinv_size = 128;
unsigned int modinv_m[] = {
0x8EA9FEF1,
0x91EBB10C,
0xFB4117F5,
0xD5876649
};

unsigned int modinv_b[] = {
0x73EC7E71,
0xB1D8BF1F,
0x47481AC9,
0xB88A0FDC
};
unsigned int modinv_ry[] = {
0x45AE5E14,
0x97ED544C,
0xFFC69D93,
0xDA98F1F8
};
