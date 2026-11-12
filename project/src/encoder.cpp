#include <cassert>
#include "encoder.h"
#include <iostream>
#include <unordered_map>
#include <stdexcept>

extern "C" {
    void gf_vect_dot_prod_avx2(int len, int vec, unsigned char *g_tbls, unsigned char **buffs, unsigned char*dests);
    void gf_2vect_dot_prod_avx2(int len, int vec, unsigned char *g_tbls, unsigned char **buffs, unsigned char**dests);
    void gf_3vect_dot_prod_avx2(int len, int vec, unsigned char *g_tbls, unsigned char **buffs, unsigned char**dests);
    void gf_4vect_dot_prod_avx2(int len, int vec, unsigned char *g_tbls, unsigned char **buffs, unsigned char**dests);
    void gf_5vect_dot_prod_avx2(int len, int vec, unsigned char *g_tbls, unsigned char **buffs, unsigned char**dests);
    void gf_6vect_dot_prod_avx2(int len, int vec, unsigned char *g_tbls, unsigned char **buffs, unsigned char**dests);
    int xor_gen_avx(int vects, int len, void **array);
}

int ECProject::xor_avx(int vects, int len, void **array)
{
    return xor_gen_avx(vects, len, array);
}

unsigned char
ECProject::gf_inv(unsigned char a)
{
    if (a == 0)
        return 0;
    return ECProject::gff_base[255 - ECProject::gflog_base[a]];
}

void ECProject::gf_gen_local_vector(unsigned char *a, int k, int p)
{
    int i;

    for (i = 0; i < k; i++)
    {
        a[i] = ECProject::gf_inv(i ^ (k + p));
    }
}

void
ECProject::gf_gen_rs_matrix1(unsigned char *a, int m, int k)
{
        int i, j;
        unsigned char p, gen = 2;

        memset(a, 0, k * m);
        for (i = 0; i < k; i++)
                a[k * i + i] = 1;

        for (i = k; i < m; i++) {
                p = 1;
                for (j = 0; j < k; j++) {
                        a[k * i + j] = p;
                        p = gf_mul(p, gen);
                }
                gen = gf_mul(gen, 2);
        }
}

void
ECProject::gf_gen_cauchy_matrix1(unsigned char *a, int m, int k)
{
        int i, j;
        unsigned char *p;

        // Identity matrix in high position
        memset(a, 0, k * m);
        for (i = 0; i < k; i++)
                a[k * i + i] = 1;

        // For the rest choose 1/(i + j) | i != j
        p = &a[k * k];
        for (i = k; i < m; i++)
                for (j = 0; j < k; j++)
                        *p++ = gf_inv(i ^ j);
}

unsigned char
ECProject::gf_mul(unsigned char a, unsigned char b)
{
#ifndef GF_LARGE_TABLES
        int i;

        if ((a == 0) || (b == 0))
                return 0;

        return gff_base[(i = gflog_base[a] + gflog_base[b]) > 254 ? i - 255 : i];
#else
        return gf_mul_table_base[b * 256 + a];
#endif
}

int
ECProject::gf_invert_matrix(unsigned char *in_mat, unsigned char *out_mat, const int n)
{
        int i, j, k;
        unsigned char temp;

        // Set out_mat[] to the identity matrix
        for (i = 0; i < n * n; i++) // memset(out_mat, 0, n*n)
                out_mat[i] = 0;

        for (i = 0; i < n; i++)
                out_mat[i * n + i] = 1;

        // Inverse
        for (i = 0; i < n; i++) {
                // Check for 0 in pivot element
                if (in_mat[i * n + i] == 0) {
                        // Find a row with non-zero in current column and swap
                        for (j = i + 1; j < n; j++)
                                if (in_mat[j * n + i])
                                        break;

                        if (j == n) // Couldn't find means it's singular
                                return -1;

                        for (k = 0; k < n; k++) { // Swap rows i,j
                                temp = in_mat[i * n + k];
                                in_mat[i * n + k] = in_mat[j * n + k];
                                in_mat[j * n + k] = temp;

                                temp = out_mat[i * n + k];
                                out_mat[i * n + k] = out_mat[j * n + k];
                                out_mat[j * n + k] = temp;
                        }
                }

                temp = gf_inv(in_mat[i * n + i]); // 1/pivot
                for (j = 0; j < n; j++) {         // Scale row i by 1/pivot
                        in_mat[i * n + j] = gf_mul(in_mat[i * n + j], temp);
                        out_mat[i * n + j] = gf_mul(out_mat[i * n + j], temp);
                }

                for (j = 0; j < n; j++) {
                        if (j == i)
                                continue;

                        temp = in_mat[j * n + i];
                        for (k = 0; k < n; k++) {
                                out_mat[j * n + k] ^= gf_mul(temp, out_mat[i * n + k]);
                                in_mat[j * n + k] ^= gf_mul(temp, in_mat[i * n + k]);
                        }
                }
        }
        return 0;
}


namespace {
constexpr unsigned char HLRC_PARITY_64[8][64] = {
    {3,2,5,4,7,6,9,8,11,10,13,12,15,14,17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30,33,32,35,34,37,36,39,38,41,40,43,42,45,44,47,46,49,48,51,50,53,52,55,54,57,56,59,58,61,60,63,62,65,64},
    {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65},
    {20,29,48,57,36,45,0,9,20,29,48,57,36,45,157,148,248,188,125,57,245,177,106,46,226,166,103,35,239,171,44,104,164,224,33,101,169,237,54,114,190,250,59,127,179,247,24,92,144,212,21,81,157,217,2,70,138,206,15,75,135,195,148,208},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,121,198,206,113,171,20,197,122,160,31,23,168,114,205,4,187,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {42,63,84,65,126,107,168,189,130,151,252,233,214,195,77,88,103,114,25,12,51,38,229,240,207,218,177,164,155,142,154,143,52,196,60,204,199,55,84,164,175,95,167,87,92,172,121,137,253,232,131,150,169,188,127,106,85,64,43,62,1,20,41,60},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,126,113,78,65,86,89,86,89,78,65,126,113,102,105,88,87},
    {0,140,150,180,250,91,112,253,5,11,161,1,184,155,252,173,0,0,28,252,133,69,30,153,212,115,110,41,185,222,206,185,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,220,77,173,111,85,126,136,161,141,11,27,74,128,83,44,0,0,156,162,192,77,66,9,243,11,168,110,232,157,202,23}
};

constexpr unsigned char HLRC_PARITY_96[10][96] = {
    {3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30, 33, 32, 35, 34, 37, 36, 39, 38, 41, 40, 43, 42, 45, 44, 47, 46, 49, 48, 51, 50, 53, 52, 55, 54, 57, 56, 59, 58, 61, 60, 63, 62, 65, 64, 67, 66, 69, 68, 71, 70, 73, 72, 75, 74, 77, 76, 79, 78, 81, 80, 83, 82, 85, 84, 87, 86, 89, 88, 91, 90, 93, 92, 95, 94, 97, 96},
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97},
    {136, 207, 21, 82, 157, 218, 74, 13, 194, 133, 95, 24, 215, 144, 9, 78, 6, 203, 146, 95, 21, 216, 167, 106, 32, 237, 180, 121, 51, 254, 85, 152, 210, 31, 70, 139, 193, 12, 115, 190, 244, 57, 96, 173, 231, 42, 25, 212, 158, 83, 10, 199, 141, 64, 63, 242, 184, 117, 44, 225, 171, 102, 224, 45, 103, 170, 243, 62, 116, 185, 198, 11, 65, 140, 213, 24, 82, 159, 172, 97, 43, 230, 191, 114, 56, 245, 138, 71, 13, 192, 153, 84, 30, 211, 120, 181},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 138, 124, 150, 96, 97, 151, 214, 32, 33, 215, 61, 203, 202, 60, 84, 162, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 193, 39, 189, 91, 106, 140, 61, 219, 234, 12, 150, 112, 65, 167, 221, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {84, 126, 168, 130, 252, 214, 77, 103, 25, 51, 229, 207, 177, 155, 154, 176, 206, 228, 50, 24, 102, 76, 215, 253, 131, 169, 127, 85, 43, 1, 41, 3, 125, 87, 129, 171, 213, 255, 100, 78, 48, 26, 204, 230, 152, 178, 179, 153, 36, 7, 252, 223, 188, 159, 41, 10, 105, 74, 177, 146, 241, 210, 134, 165, 6, 44, 250, 208, 174, 132, 31, 53, 75, 97, 183, 157, 227, 201, 200, 226, 156, 182, 96, 74, 52, 30, 133, 175, 209, 251, 45, 7, 121, 83, 123, 81},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 233, 170, 108, 47, 236, 175, 3, 64, 131, 192, 6, 69, 134, 197, 32, 99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 129, 68, 212, 127, 239, 150, 6, 173, 61, 248, 104, 195, 83, 113, 225},
    {0, 151, 39, 67, 179, 75, 13, 237, 82, 46, 254, 113, 53, 38, 189, 169, 0, 0, 158, 245, 80, 46, 147, 55, 34, 147, 66, 152, 91, 148, 151, 220, 0, 0, 46, 213, 83, 141, 216, 54, 46, 229, 11, 59, 200, 221, 105, 114, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 35, 167, 97, 8, 134, 4, 220, 208, 64, 129, 244, 47, 18, 106, 89, 0, 0, 16, 185, 41, 2, 191, 163, 167, 57, 245, 194, 137, 60, 21, 177, 0, 0, 150, 183, 151, 210, 29, 253, 9, 141, 181, 16, 166, 103, 223, 44}
};

constexpr unsigned char HLRC_PARITY_128[12][128] = {
    {3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30, 33, 32, 35, 34, 37, 36, 39, 38, 41, 40, 43, 42, 45, 44, 47, 46, 49, 48, 51, 50, 53, 52, 55, 54, 57, 56, 59, 58, 61, 60, 63, 62, 65, 64, 67, 66, 69, 68, 71, 70, 73, 72, 75, 74, 77, 76, 79, 78, 81, 80, 83, 82, 85, 84, 87, 86, 89, 88, 91, 90, 93, 92, 95, 94, 97, 96, 99, 98, 101, 100, 103, 102, 105, 104, 107, 106, 109, 108, 111, 110, 113, 112, 115, 114, 117, 116, 119, 118, 121, 120, 123, 122, 125, 124, 127, 126, 129, 128},
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129},
    {140, 201, 29, 88, 145, 212, 90, 31, 214, 147, 71, 2, 203, 142, 41, 108, 164, 13, 117, 220, 58, 147, 202, 99, 133, 44, 84, 253, 27, 178, 45, 132, 98, 203, 179, 26, 252, 85, 12, 165, 67, 234, 146, 59, 221, 116, 111, 198, 32, 137, 241, 88, 190, 23, 78, 231, 1, 168, 208, 121, 159, 54, 188, 21, 243, 90, 34, 139, 109, 196, 157, 52, 210, 123, 3, 170, 76, 229, 254, 87, 177, 24, 96, 201, 47, 134, 223, 118, 144, 57, 65, 232, 14, 167, 56, 145, 119, 222, 166, 15, 233, 64, 25, 176, 86, 255, 135, 46, 200, 97, 122, 211, 53, 156, 228, 77, 171, 2, 91, 242, 20, 189, 197, 108, 138, 35, 131, 42},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 160, 125, 70, 155, 231, 58, 239, 50, 78, 147, 168, 117, 9, 212, 171, 118, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 212, 8, 244, 235, 23, 48, 204, 211, 47, 243, 15, 16, 236, 189, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 98, 239, 121, 244, 120, 245, 55, 186, 54, 187, 45, 160, 44, 161, 31, 146, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {81, 247, 162, 4, 243, 85, 89, 255, 8, 174, 251, 93, 170, 12, 178, 20, 227, 69, 16, 182, 65, 231, 235, 77, 186, 28, 73, 239, 24, 190, 121, 223, 40, 142, 219, 125, 138, 44, 32, 134, 113, 215, 130, 36, 211, 117, 203, 109, 154, 60, 105, 207, 56, 158, 146, 52, 195, 101, 48, 150, 97, 199, 242, 84, 250, 34, 2, 218, 169, 113, 151, 79, 60, 228, 196, 28, 111, 183, 93, 133, 17, 183, 226, 68, 179, 21, 25, 191, 72, 238, 187, 29, 234, 76, 139, 45, 218, 124, 41, 143, 120, 222, 210, 116, 131, 37, 112, 214, 33, 135, 57, 159, 104, 206, 155, 61, 202, 108, 96, 198, 49, 151, 194, 100, 147, 53, 249, 95},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 89, 71, 15, 17, 53, 43, 219, 197, 225, 255, 183, 169, 141, 147, 32, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 223, 241, 43, 94, 132, 124, 166, 211, 9, 39, 253, 136, 82, 134, 92, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 80, 225, 179, 67, 17, 66, 16, 224, 178, 3, 81, 161, 243, 119, 37},
    {0, 220, 145, 139, 131, 220, 52, 146, 88, 187, 53, 16, 75, 43, 203, 188, 0, 0, 212, 172, 40, 249, 227, 183, 248, 5, 255, 122, 197, 233, 192, 54, 0, 0, 156, 196, 153, 182, 105, 99, 150, 235, 227, 198, 131, 209, 152, 153, 0, 0, 42, 125, 9, 145, 160, 46, 1, 64, 50, 36, 165, 124, 185, 54, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 245, 125, 165, 221, 10, 129, 207, 84, 21, 195, 175, 110, 13, 63, 87, 0, 0, 169, 214, 152, 79, 87, 30, 185, 88, 179, 45, 116, 66, 211, 177, 0, 0, 137, 123, 96, 180, 71, 142, 109, 130, 127, 98, 120, 67, 113, 225, 0, 0, 155, 121, 138, 33, 208, 54, 130, 45, 159, 210, 191, 187, 223, 1}
};

void validate_hlrc_parameters(int k, int r, int z)
{
    if (!((k == 64 && r == 2 && z == 6) ||
          (k == 96 && r == 2 && z == 8) ||
          (k == 128 && r == 2 && z == 10)))
        throw std::invalid_argument("HLRC requires (k=64,r=2,z=6), (k=96,r=2,z=8), or (k=128,r=2,z=10)");
}

const unsigned char *get_hlrc_parity(int k)
{
    switch (k) {
    case 64: return &HLRC_PARITY_64[0][0];
    case 96: return &HLRC_PARITY_96[0][0];
    case 128: return &HLRC_PARITY_128[0][0];
    default: throw std::invalid_argument("unsupported HLRC k");
    }
}
void validate_nlrc_parameters(int k, int r, int z)
{
    if (!((k == 64 && r == 3 && z == 5) ||
          (k == 96 && r == 3 && z == 7) ||
          (k == 128 && r == 3 && z == 9)))
        throw std::invalid_argument("NLRC requires (k=64,r=3,z=5), (k=96,r=3,z=7), or (k=128,r=3,z=9)");
}

std::vector<std::pair<int, int>> nlrc_data_bounds(int k)
{
    if (k == 64) return {{0, 22}, {22, 44}, {44, 64}};
    if (k == 96) return {{0, 24}, {24, 49}, {49, 74}, {74, 96}};
    return {{0, 26}, {26, 52}, {52, 78}, {78, 104}, {104, 128}};
}
} // namespace

void ECProject::gen_nlrc_matrix(unsigned char *encode_matrix, int k, int r, int z)
{
    validate_nlrc_parameters(k, r, z);
    if (encode_matrix == nullptr)
        throw std::invalid_argument("NLRC encode matrix must not be null");
    const int n = k + r + z;
    memset(encode_matrix, 0, static_cast<size_t>(n) * k);
    gf_gen_cauchy_matrix1(encode_matrix, k + r, k);
    const auto bounds = nlrc_data_bounds(k);
    int local_row = k + r;
    for (size_t group = 0; group + 1 < bounds.size(); ++group) {
        const int begin = bounds[group].first;
        const int count = bounds[group].second - begin;
        std::vector<unsigned char> local_matrix(static_cast<size_t>(count + 2) * count);
        gf_gen_cauchy_matrix1(local_matrix.data(), count + 2, count);
        for (int parity = 0; parity < 2; ++parity)
            memcpy(encode_matrix + static_cast<size_t>(local_row++) * k + begin,
                   local_matrix.data() + static_cast<size_t>(count + parity) * count,
                   static_cast<size_t>(count));
    }
    const int tail_begin = bounds.back().first;
    const int tail_data = k - tail_begin;
    const int member_count = tail_data + r;
    std::vector<unsigned char> tail_matrix(static_cast<size_t>(member_count + 1) * member_count);
    gf_gen_cauchy_matrix1(tail_matrix.data(), member_count + 1, member_count);
    const unsigned char *coeff = tail_matrix.data() + static_cast<size_t>(member_count) * member_count;
    unsigned char *row = encode_matrix + static_cast<size_t>(local_row) * k;
    for (int i = 0; i < tail_data; ++i)
        row[tail_begin + i] = coeff[i];
    for (int global = 0; global < r; ++global)
        for (int data = 0; data < k; ++data)
            row[data] ^= gf_mul(coeff[tail_data + global], encode_matrix[(k + global) * k + data]);
}

void ECProject::encode_nlrc(int k, int r, int z, unsigned char **data_ptrs,
                            unsigned char **parity_ptrs, int block_size)
{
    validate_nlrc_parameters(k, r, z);
    if (data_ptrs == nullptr || parity_ptrs == nullptr || block_size <= 0)
        throw std::invalid_argument("NLRC requires non-null buffers and a positive block size");
    std::vector<unsigned char> matrix(static_cast<size_t>(k + r + z) * k);
    std::vector<unsigned char> tables(static_cast<size_t>(k) * (r + z) * 32);
    gen_nlrc_matrix(matrix.data(), k, r, z);
    ec_init_tables(k, r + z, matrix.data() + static_cast<size_t>(k) * k, tables.data());
    ec_encode_data_avx2(block_size, k, r + z, tables.data(), data_ptrs, parity_ptrs);
}

void ECProject::partial_encode_nlrc(int k, int r, int z, int data_block_num,
                                    unsigned char **data_ptrs, unsigned char **parity_ptrs,
                                    int block_size)
{
    validate_nlrc_parameters(k, r, z);
    if (data_block_num <= 0 || data_block_num > k || data_ptrs == nullptr ||
        parity_ptrs == nullptr || block_size <= 0)
        throw std::invalid_argument("NLRC partial encode arguments are invalid");
    std::vector<unsigned char> matrix(static_cast<size_t>(k + r + z) * k);
    std::vector<unsigned char> sub_matrix(static_cast<size_t>(r + z) * data_block_num);
    std::vector<unsigned char> tables(static_cast<size_t>(data_block_num) * (r + z) * 32);
    gen_nlrc_matrix(matrix.data(), k, r, z);
    for (int row = 0; row < r + z; ++row)
        memcpy(sub_matrix.data() + static_cast<size_t>(row) * data_block_num,
               matrix.data() + static_cast<size_t>(k + row) * k,
               static_cast<size_t>(data_block_num));
    ec_init_tables(data_block_num, r + z, sub_matrix.data(), tables.data());
    ec_encode_data_avx2(block_size, data_block_num, r + z, tables.data(), data_ptrs, parity_ptrs);
}

void ECProject::gen_hlrc_matrix(unsigned char *encode_matrix, int k, int r, int z)
{
    validate_hlrc_parameters(k, r, z);
    if (encode_matrix == nullptr)
        throw std::invalid_argument("HLRC encode matrix must not be null");
    const int parity_count = r + z;
    memset(encode_matrix, 0, static_cast<size_t>(k + parity_count) * k);
    for (int i = 0; i < k; ++i)
        encode_matrix[i * k + i] = 1;
    memcpy(encode_matrix + k * k, get_hlrc_parity(k), static_cast<size_t>(parity_count) * k);
}

void ECProject::encode_hlrc(int k, int r, int z, unsigned char **data_ptrs,
                            unsigned char **parity_ptrs, int block_size)
{
    validate_hlrc_parameters(k, r, z);
    if (data_ptrs == nullptr || parity_ptrs == nullptr || block_size <= 0)
        throw std::invalid_argument("HLRC requires non-null buffers and a positive block size");
    const int parity_count = r + z;
    std::vector<unsigned char> matrix(static_cast<size_t>(k + parity_count) * k);
    std::vector<unsigned char> tables(static_cast<size_t>(k) * parity_count * 32);
    gen_hlrc_matrix(matrix.data(), k, r, z);
    ec_init_tables(k, parity_count, matrix.data() + k * k, tables.data());
    ec_encode_data_avx2(block_size, k, parity_count, tables.data(), data_ptrs, parity_ptrs);
}

void ECProject::encode_lotuslrc(int k, int r, int z, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z) * k];
    gen_lotuslrc_matrix(encode_matrix, k, r, z);

    unsigned char *g_tbls = new unsigned char[k * (r + z) * 32];
    ec_init_tables(k, r + z, &encode_matrix[k * k], g_tbls);
    ec_encode_data_avx2(block_size, k, r + z, g_tbls, data_ptrs, parity_ptrs);
    delete[] encode_matrix;
    delete[] g_tbls;
}

void ECProject::encode_unilrc(int k, int r, int z, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z) * k];
    gen_unilrc_matrix(encode_matrix, k, r, z);

    unsigned char *g_tbls = new unsigned char[k * (r + z) * 32];
    ec_init_tables(k, r + z, &encode_matrix[k * k], g_tbls);
    ec_encode_data_avx2(block_size, k, r + z, g_tbls, data_ptrs, parity_ptrs);

    delete[] encode_matrix;
    delete[] g_tbls;
}

void ECProject::partial_encode_unilrc(int k, int r, int z, int data_block_num, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z) * k];
    gen_unilrc_matrix(encode_matrix, k, r, z);

    unsigned char *sub_matrix = new unsigned char[(r + z) * data_block_num];
    for (int i = 0; i < r + z; i++) {
        memcpy(sub_matrix + i * data_block_num, 
               encode_matrix + (k + i) * k,      
               data_block_num);                   
    }

    unsigned char *g_tbls = new unsigned char[data_block_num * (r + z) * 32];
    ec_init_tables(data_block_num, r + z, sub_matrix, g_tbls);

    ec_encode_data_avx2(block_size, 
                        data_block_num,  
                        r + z,           
                        g_tbls, 
                        data_ptrs,       
                        parity_ptrs);

    delete[] encode_matrix;
    delete[] sub_matrix;
    delete[] g_tbls;
}

void ECProject::encode_azure_lrc(int k, int r, int z, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z)* k];
    gen_azure_lrc_matrix(encode_matrix, k, r, z);

    unsigned char *g_tbls = new unsigned char[k * (r + z) * 32];
    ec_init_tables(k, r + z, &encode_matrix[k * k], g_tbls);
    ec_encode_data_avx2(block_size, k, r + z, g_tbls, data_ptrs, parity_ptrs);

    delete[] encode_matrix;
    delete[] g_tbls;
}

void ECProject::partial_encode_azure_lrc(int k, int r, int z, int data_block_num, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z)* k];
    gen_azure_lrc_matrix(encode_matrix, k, r, z);

    unsigned char *sub_matrix = new unsigned char[(r + z) * data_block_num];
    for (int i = 0; i < r + z; i++) {
        memcpy(sub_matrix + i * data_block_num, 
               encode_matrix + (k + i) * k,      
               data_block_num);                   
    }

    unsigned char *g_tbls = new unsigned char[data_block_num * (r + z) * 32];
    ec_init_tables(data_block_num, r + z, sub_matrix, g_tbls);

    ec_encode_data_avx2(block_size, 
                        data_block_num,  
                        r + z,           
                        g_tbls, 
                        data_ptrs,       
                        parity_ptrs);

    delete[] encode_matrix;
    delete[] sub_matrix;
    delete[] g_tbls;
}

void ECProject::encode_optimal_lrc(int k, int r, int z, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z) * k];
    gen_optimal_lrc_matrix(encode_matrix, k, r, z);

    unsigned char *g_tbls = new unsigned char[k * (r + z)* 32];
    ec_init_tables(k, r + z, &encode_matrix[k * k], g_tbls);
    ec_encode_data_avx2(block_size, k, r + z, g_tbls, data_ptrs, parity_ptrs);
    delete[] encode_matrix;
    delete[] g_tbls;
}

void ECProject::partial_encode_optimal_lrc(int k, int r, int z, int data_block_num, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z) * k];
    gen_optimal_lrc_matrix(encode_matrix, k, r, z);

    unsigned char *sub_matrix = new unsigned char[(r + z) * data_block_num];
    for (int i = 0; i < r + z; i++) {
        memcpy(sub_matrix + i * data_block_num, 
               encode_matrix + (k + i) * k,      
               data_block_num);                   
    }

    unsigned char *g_tbls = new unsigned char[data_block_num * (r + z) * 32];
    ec_init_tables(data_block_num, r + z, sub_matrix, g_tbls);

    ec_encode_data_avx2(block_size, 
                        data_block_num,  
                        r + z,           
                        g_tbls, 
                        data_ptrs,       
                        parity_ptrs);

    delete[] encode_matrix;
    delete[] sub_matrix;
    delete[] g_tbls;
}

void ECProject::encode_uniform_lrc(int k, int r, int z, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z) * k];
    gen_uniform_lrc_matrix(encode_matrix, k, r, z);

    unsigned char *g_tbls = new unsigned char[k * (r + z)* 32];
    ec_init_tables(k, r + z, &encode_matrix[k * k], g_tbls);
    ec_encode_data_avx2(block_size, k, r + z, g_tbls, data_ptrs, parity_ptrs);
    delete[] encode_matrix;
    delete[] g_tbls;
}

void ECProject::partial_encode_uniform_lrc(int k, int r, int z, int data_block_num, unsigned char **data_ptrs, unsigned char **parity_ptrs, int block_size)
{
    for(int i = 0; i < r + z; i++){
        memset(parity_ptrs[i], 0, block_size);
    }
    int m = k + r;
    unsigned char *encode_matrix = new unsigned char[(m + z) * k];
    gen_uniform_lrc_matrix(encode_matrix, k, r, z);

    unsigned char *sub_matrix = new unsigned char[(r + z) * data_block_num];
    for (int i = 0; i < r + z; i++) {
        memcpy(sub_matrix + i * data_block_num, 
               encode_matrix + (k + i) * k,      
               data_block_num);                   
    }

    unsigned char *g_tbls = new unsigned char[data_block_num * (r + z) * 32];
    ec_init_tables(data_block_num, r + z, sub_matrix, g_tbls);

    ec_encode_data_avx2(block_size, 
                        data_block_num,  
                        r + z,           
                        g_tbls, 
                        data_ptrs,       
                        parity_ptrs);

    delete[] encode_matrix;
    delete[] sub_matrix;
    delete[] g_tbls;
    
}

// ===== Matrix generation helpers implementations =====
void ECProject::gen_unilrc_matrix(unsigned char *encode_matrix, int k, int r, int z)
{
    int m = k + r;
    memset(encode_matrix, 0, (m + z) * k);
    gf_gen_rs_matrix1(encode_matrix, m, k);
    for(int i = 0; i < k; i++){
        int row = i / (k / z);
        encode_matrix[(m + row) * k + i] = 1;
    }
    for(int i = 0; i < z; i++){
        for(int j = 0; j < k; j++){
            for(int l = 0; l < r / z; l++){
                encode_matrix[(m + i) * k + j] ^= encode_matrix[(k + i * r / z + l) * k + j];
            }
        }
    }
}

void ECProject::gen_azure_lrc_matrix(unsigned char *encode_matrix, int k, int r, int z)
{
    int m = k + r;
    memset(encode_matrix, 0, (m + z) * k);
    gf_gen_rs_matrix1(encode_matrix, m, k);
    const auto bounds = get_azure_lrc_local_group_bounds(k, z);
    for (int group = 0; group < z; ++group) {
        for (int i = bounds[group].first; i < bounds[group].second; ++i)
            encode_matrix[(m + group) * k + i] = 1;
    }
}

void ECProject::gen_optimal_lrc_matrix(unsigned char *encode_matrix, int k, int r, int z)
{
    int m = k + r;
    memset(encode_matrix, 0, (m + z) * k);
    gf_gen_cauchy_matrix1(encode_matrix, m, k);
    unsigned char *local_vector = new unsigned char[k];
    gf_gen_local_vector(local_vector, k, r);
    const auto bounds = get_optimal_lrc_local_group_bounds(k, z);
    for (int group = 0; group < z; ++group) {
        for (int i = bounds[group].first; i < bounds[group].second; ++i)
            encode_matrix[(m + group) * k + i] = local_vector[i];
    }
    for(int i = 0; i < z; i++){
        for(int j = 0; j < k; j++){
            for(int l = 0; l < r; l++){
                encode_matrix[(m + i) * k + j] ^= encode_matrix[(k + l) * k + j];
            }
        }
    }
    delete[] local_vector;
}

void ECProject::gen_uniform_lrc_matrix(unsigned char *encode_matrix, int k, int r, int z)
{
    int m = k + r;
    memset(encode_matrix, 0, (m + z) * k);
    gf_gen_cauchy_matrix1(encode_matrix, m, k);
    unsigned char *local_vector = new unsigned char[k];
    gf_gen_local_vector(local_vector, k, r);
    // The k data blocks and the r global-parity blocks (k+r "data-like" blocks) are split into z
    // contiguous local groups; each local group's local parity protects ALL of its members.
    // This partition MUST match get_uniform_lrc_block_id_to_local_group_id() in encoder_layout.cpp
    // (run sizes (k+r)/z, with the last (k+r)%z groups holding one extra), so that globals fall
    // into the last local group(s). A data member d contributes local_vector[d]*data[d]; a global
    // member gp contributes 1*global[gp] (its Cauchy row XORed into the group's local parity row).
    const auto bounds = get_uniform_lrc_data_like_group_bounds(k, r, z);
    for (int group = 0; group < z; ++group) {
        for (int block = bounds[group].first; block < bounds[group].second; ++block) {
            if (block < k) {
                encode_matrix[(m + group) * k + block] = local_vector[block];
            } else {
                for (int data = 0; data < k; ++data)
                    encode_matrix[(m + group) * k + data] ^= encode_matrix[block * k + data];
            }
        }
    }
    delete[] local_vector;
}

void ECProject::gen_lotuslrc_matrix(unsigned char *encode_matrix, int k, int r, int z)
{
    if (z <= 0 || z % 2 != 0)
        throw std::invalid_argument("LotusLRC requires a positive even z");
    int group_num = z / 2;
    if (group_num < 2)
        throw std::invalid_argument("LotusLRC requires at least two local groups");
    int m = k + r;
    memset(encode_matrix, 0, (m + z) * k);
    gf_gen_cauchy_matrix1(encode_matrix, m, k);

    std::vector<int> data_block_num_per_local_group =
        get_data_block_num_per_local_group_lotuslrc(k, r, z);
    std::vector<int> global_parity_block_num_per_local_group =
        get_global_parity_block_num_per_local_group_lotuslrc(k, r, z);
    assert((int)data_block_num_per_local_group.size() == group_num);
    assert((int)global_parity_block_num_per_local_group.size() == group_num);

    unsigned char *local_vector0 = new unsigned char[k];
    gf_gen_local_vector(local_vector0, k, r);
    unsigned char *local_vector1 = new unsigned char[k];
    gf_gen_local_vector(local_vector1, k, r + 1);

    int data_offset = 0;
    for (int i = 0; i < group_num; i++)
    {
        int group_size = data_block_num_per_local_group[i];
        memcpy(encode_matrix + (m + i * 2) * k + data_offset,
               local_vector0 + data_offset,
               group_size * sizeof(unsigned char));
        memcpy(encode_matrix + (m + i * 2 + 1) * k + data_offset,
               local_vector1 + data_offset,
               group_size * sizeof(unsigned char));
        data_offset += group_size;
    }
    assert(data_offset == k);

    // The two-group k=24,r=1,z=4 layout has exactly four local parities.
    // Keep both local equations inside their own group. The cross-group fusion
    // below needs a third group as the second destination and is therefore
    // used only by layouts with at least three local groups.
    if (group_num >= 3)
    {
        int data0 = data_block_num_per_local_group[0];
        int half0 = (data0 + 1) / 2; // more-first
        int half1 = data0 - half0;
        int off_a = 0;
        int off_b = half0;

        unsigned char *row_l1 = encode_matrix + (m + 0) * k;
        unsigned char *row_l2 = encode_matrix + (m + 1) * k;
        unsigned char *row_l4 = encode_matrix + (m + 3) * k;
        unsigned char *row_l5 = encode_matrix + (m + 4) * k;

        memset(row_l1 + off_b, 0, half1 * sizeof(unsigned char));
        for (int j = 0; j < half1; j++)
            row_l5[off_b + j] ^= local_vector0[off_b + j];

        memset(row_l2 + off_a, 0, half0 * sizeof(unsigned char));
        for (int j = 0; j < half0; j++)
            row_l4[off_a + j] ^= local_vector1[off_a + j];
    }

    delete[] local_vector0;
    delete[] local_vector1;

    // XOR each global parity Cauchy row into the two local-parity rows of the
    // local group that holds it. Globals are placed only in the latter half of
    // local groups (ceil(group_num/2)), possibly multiple globals per group.
    int global_id = 0;
    for (int lg = 0; lg < group_num; lg++)
    {
        for (int g = 0; g < global_parity_block_num_per_local_group[lg]; g++)
        {
            for (int j = 0; j < k; j++)
            {
                encode_matrix[(m + 2 * lg) * k + j] ^=
                    encode_matrix[(k + global_id) * k + j];
                encode_matrix[(m + 2 * lg + 1) * k + j] ^=
                    encode_matrix[(k + global_id) * k + j];
            }
            global_id++;
        }
    }
    assert(global_id == r);
}

void ECProject::decode_unilrc(const int k, const int r, const int z, const int block_num,
                              const std::vector<int> *block_indexes, unsigned char **block_ptrs, unsigned char *res_ptr, int block_size)
{
    memset(res_ptr, 0, block_size);
    unsigned char *vect_ptrs[block_num + 1];
    for(int i = 0; i < block_num; i++){
        vect_ptrs[i] = block_ptrs[i];
    }
    vect_ptrs[block_num] = res_ptr;
    xor_gen_avx(block_num + 1, block_size, (void **)vect_ptrs);
}

namespace ECProject {
namespace {
// Defined below; exact generator-matrix single-block decode shared by Optimal/Lotus/Azure-global.
void decode_block_via_generator(const std::string &code, int k, int r, int z, int block_num,
                                const std::vector<int> *block_indexes, unsigned char **block_ptrs,
                                unsigned char *res_ptr, int block_size, int failed_block_id);
} // namespace
} // namespace ECProject

void ECProject::decode_azure_lrc(const int k, const int r, const int z, const int block_num,
                                 const std::vector<int> *block_indexes, unsigned char **block_ptrs, unsigned char *res_ptr, int block_size,
                                 int failed_block_id)
{
    if(block_num == 0){
        return;
    }
    memset(res_ptr, 0, block_size);
    if (failed_block_id < k || failed_block_id >= k + r){
        unsigned char *vect_ptrs[block_num + 1];
        for(int i = 0; i < block_num; i++){
            vect_ptrs[i] = block_ptrs[i];
        }
        vect_ptrs[block_num] = res_ptr;
        xor_gen_avx(block_num + 1, block_size, (void **)vect_ptrs);
    }
    else
    {
        // Global-parity recovery: the previous RS-inverse path used a generator matrix that did
        // not match gen_azure_lrc_matrix (and mis-mapped sources), producing wrong data. Use the
        // exact generator-matrix solve instead.
        decode_block_via_generator("AzureLRC", k, r, z, block_num, block_indexes, block_ptrs,
                                   res_ptr, block_size, failed_block_id);
    }
}

namespace ECProject {
namespace {

// Exact, code-type-agnostic single-block decode coefficients for the per-group XOR partial scheme.
//
// Single-block recovery is run as: each cross-rack group computes a weighted partial over ONLY the
// source blocks it holds, and the destination XORs the partials. For that decomposition to be
// correct, every source block's coefficient must be grouping-independent. We obtain those exact
// coefficients by solving, over the FULL deterministic recovery source set
// (= flatten(get_recovery_group_and_block_ids(failed))), the system
//   gen[failed] = sum_s  C_s * gen[s]   over GF(2^8)
// via get_local_fill_plan (generator-matrix based; validated for every code type). Each decode_*
// call then applies the C_s of exactly the blocks it was given.
//
// This is correct for data blocks, local-parity blocks, global-parity blocks and folded-global
// members alike -- no out-of-bounds local_vector indexing and no per-code coefficient special
// casing (the previous root cause of wrong recovered data for Optimal/Lotus and Azure globals).
void decode_block_via_generator(const std::string &code, int k, int r, int z, int block_num,
                                const std::vector<int> *block_indexes, unsigned char **block_ptrs,
                                unsigned char *res_ptr, int block_size, int failed_block_id)
{
    memset(res_ptr, 0, block_size);
    if (block_num == 0)
        return;
    std::vector<int> flat;
    try {
        auto plan = get_recovery_group_and_block_ids(code, k, r, z, failed_block_id);
        for (auto &p : plan)
            for (int b : p.second)
                flat.push_back(b);
    } catch (const std::exception &e) {
        std::cerr << "[decode] " << code << " plan error for block " << failed_block_id
                  << ": " << e.what() << std::endl;
        return;
    }
    std::vector<unsigned char> coeffs;
    if (flat.empty() || !get_local_fill_plan(k, r, z, code, {failed_block_id}, flat, coeffs)) {
        std::cerr << "[decode] " << code << " cannot derive exact coefficients for block "
                  << failed_block_id << std::endl;
        return;
    }
    std::unordered_map<int, int> pos;
    for (int i = 0; i < static_cast<int>(flat.size()); i++)
        pos[flat[i]] = i;
    std::vector<unsigned char> decode_vector(static_cast<size_t>(block_num), 0);
    for (int i = 0; i < block_num; i++) {
        auto it = pos.find(block_indexes->at(i));
        if (it != pos.end())
            decode_vector[i] = coeffs[it->second];
    }
    unsigned char *g_tbls = new unsigned char[static_cast<size_t>(block_num) * 32];
    ec_init_tables(block_num, 1, decode_vector.data(), g_tbls);
    unsigned char *res_ptr_ptr[1] = {res_ptr};
    ec_encode_data_avx2(block_size, block_num, 1, g_tbls, block_ptrs, res_ptr_ptr);
    delete[] g_tbls;
}

} // namespace
} // namespace ECProject

void ECProject::decode_optimal_lrc(const int k, const int r, const int z, const int block_num,
                                   const std::vector<int> *block_indexes, unsigned char **block_ptrs, unsigned char *res_ptr, int block_size, int failed_block_id)
{
    decode_block_via_generator("OptimalLRC", k, r, z, block_num, block_indexes, block_ptrs,
                               res_ptr, block_size, failed_block_id);
}
void ECProject::decode_nlrc(const int k, const int r, const int z, const int block_num,
                            const std::vector<int> *block_indexes, unsigned char **block_ptrs,
                            unsigned char *res_ptr, int block_size, int failed_block_id)
{
    decode_block_via_generator("NLRC", k, r, z, block_num, block_indexes, block_ptrs,
                               res_ptr, block_size, failed_block_id);
}

void ECProject::decode_hlrc(const int k, const int r, const int z, const int block_num,
                            const std::vector<int> *block_indexes, unsigned char **block_ptrs,
                            unsigned char *res_ptr, int block_size, int failed_block_id)
{
    decode_block_via_generator("HLRC", k, r, z, block_num, block_indexes, block_ptrs,
                               res_ptr, block_size, failed_block_id);
}

void ECProject::decode_uniform_lrc(const int k, const int r, const int z, const int block_num,
                                   const std::vector<int> *block_indexes, unsigned char **block_ptrs, unsigned char *res_ptr, int block_size, int failed_block_id)
{
    if(block_num == 0){
        return;
    }
    memset(res_ptr, 0, block_size);
    unsigned char *local_vector;
    local_vector = new unsigned char[k];
    gf_gen_local_vector(local_vector, k, r);
    unsigned char *decode_vector = new unsigned char[block_num];

    for(int i = 0; i < block_num; i++){
        if(block_indexes->at(i) < k){
            decode_vector[i] = local_vector[block_indexes->at(i)];
        }
        else{
            decode_vector[i] = 1;
        }
    }
    if(failed_block_id < k){
        unsigned char factor = gf_inv(local_vector[failed_block_id]);
        for(int i = 0; i < block_num; i++){
            decode_vector[i] = gf_mul(decode_vector[i], factor);
        }
    }

    unsigned char *g_tbls = new unsigned char[block_num * 32];
    unsigned char **res_ptr_ptr = new unsigned char *[1];
    res_ptr_ptr[0] = res_ptr;
    ec_init_tables(block_num, 1, decode_vector, g_tbls);
    ec_encode_data_avx2(block_size, block_num, 1, g_tbls, block_ptrs, res_ptr_ptr);

    delete[] local_vector;
    delete[] decode_vector;
    delete[] g_tbls;
    delete[] res_ptr_ptr;
}

bool ECProject::decode_lotus_lrc(const int k, const int r, const int z, const int block_num,
                                 const std::vector<int> *block_indexes, unsigned char **block_ptrs, unsigned char *res_ptr, int block_size,
                                 int failed_block_id, const std::vector<int> *full_source_block_ids)
{
    memset(res_ptr, 0, block_size);
    if (block_num == 0 || block_indexes == nullptr ||
        static_cast<int>(block_indexes->size()) != block_num)
        return false;

    if (full_source_block_ids == nullptr || full_source_block_ids->empty())
    {
        decode_block_via_generator("LotusLRC", k, r, z, block_num, block_indexes,
                                   block_ptrs, res_ptr, block_size, failed_block_id);
        return true;
    }

    const std::vector<int> &sources = *full_source_block_ids;
    std::vector<unsigned char> coefficients;
    if (!get_local_fill_plan(k, r, z, "LotusLRC", {failed_block_id}, sources, coefficients))
    {
        std::cerr << "[decode] LotusLRC custom sources cannot recover block "
                  << failed_block_id << std::endl;
        return false;
    }

    std::unordered_map<int, unsigned char> coefficient_by_block;
    for (size_t i = 0; i < sources.size(); ++i)
        coefficient_by_block[sources[i]] = coefficients[i];

    std::vector<unsigned char> local_coefficients(static_cast<size_t>(block_num), 0);
    for (int i = 0; i < block_num; ++i)
    {
        auto it = coefficient_by_block.find(block_indexes->at(i));
        if (it == coefficient_by_block.end())
        {
            std::cerr << "[decode] LotusLRC received block " << block_indexes->at(i)
                      << " outside the custom source set" << std::endl;
            return false;
        }
        local_coefficients[static_cast<size_t>(i)] = it->second;
    }

    std::vector<unsigned char> tables(static_cast<size_t>(block_num) * 32);
    ec_init_tables(block_num, 1, local_coefficients.data(), tables.data());
    unsigned char *output[1] = {res_ptr};
    ec_encode_data_avx2(block_size, block_num, 1, tables.data(), block_ptrs, output);
    return true;
}

void
ECProject::ec_encode_data_avx2(int len, int k, int rows, unsigned char *g_tbls, unsigned char **data,
                    unsigned char **coding)
{

        if (len < 32) {
                ec_encode_data_base(len, k, rows, g_tbls, data, coding);
                return;
        }

        while (rows >= 6) {
                gf_6vect_dot_prod_avx2(len, k, g_tbls, data, coding);
                g_tbls += 6 * k * 32;
                coding += 6;
                rows -= 6;
        }
        switch (rows) {
        case 5:
                gf_5vect_dot_prod_avx2(len, k, g_tbls, data, coding);
                break;
        case 4:
                gf_4vect_dot_prod_avx2(len, k, g_tbls, data, coding);
                break;
        case 3:
                gf_3vect_dot_prod_avx2(len, k, g_tbls, data, coding);
                break;
        case 2:
                gf_2vect_dot_prod_avx2(len, k, g_tbls, data, coding);
                break;
        case 1:
                gf_vect_dot_prod_avx2(len, k, g_tbls, data, *coding);
                break;
        case 0:
                break;
        }
}

void
ECProject::ec_encode_data_base(int len, int srcs, int dests, unsigned char *v, unsigned char **src,
                    unsigned char **dest)
{
        int i, j, l;
        unsigned char s;

        for (l = 0; l < dests; l++) {
                for (i = 0; i < len; i++) {
                        s = 0;
                        for (j = 0; j < srcs; j++)
                                s ^= gf_mul(src[j][i], v[j * 32 + l * srcs * 32 + 1]);

                        dest[l][i] = s;
                }
        }
}

void
ECProject::encode_data(int len, int k, int rows, unsigned char *matrix, unsigned char **data,
                    unsigned char **coding)
{
        unsigned char *g_tbls = new unsigned char[k * rows * 32];

        ec_init_tables(k, rows, matrix, g_tbls);
        ec_encode_data_avx2(len, k, rows, g_tbls, data, coding);
        delete[] g_tbls;
}

void
ECProject::ec_init_tables(int k, int rows, unsigned char *a, unsigned char *g_tbls)
{
        int i, j;

        for (i = 0; i < rows; i++) {
                for (j = 0; j < k; j++) {
                        gf_vect_mul_init(*a++, g_tbls);
                        g_tbls += 32;
                }
        }
}

void
ECProject::gf_vect_mul_init(unsigned char c, unsigned char *tbl)
{
        unsigned char c2 = (c << 1) ^ ((c & 0x80) ? 0x1d : 0);   // Mult by GF{2}
        unsigned char c4 = (c2 << 1) ^ ((c2 & 0x80) ? 0x1d : 0); // Mult by GF{2}
        unsigned char c8 = (c4 << 1) ^ ((c4 & 0x80) ? 0x1d : 0); // Mult by GF{2}

#if (__WORDSIZE == 64 || _WIN64 || __x86_64__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        unsigned long long v1, v2, v4, v8, *t;
        unsigned long long v10, v20, v40, v80;
        unsigned char c17, c18, c20, c24;

        t = (unsigned long long *) tbl;

        v1 = c * 0x0100010001000100ull;
        v2 = c2 * 0x0101000001010000ull;
        v4 = c4 * 0x0101010100000000ull;
        v8 = c8 * 0x0101010101010101ull;

        v4 = v1 ^ v2 ^ v4;
        t[0] = v4;
        t[1] = v8 ^ v4;

        c17 = (c8 << 1) ^ ((c8 & 0x80) ? 0x1d : 0);   // Mult by GF{2}
        c18 = (c17 << 1) ^ ((c17 & 0x80) ? 0x1d : 0); // Mult by GF{2}
        c20 = (c18 << 1) ^ ((c18 & 0x80) ? 0x1d : 0); // Mult by GF{2}
        c24 = (c20 << 1) ^ ((c20 & 0x80) ? 0x1d : 0); // Mult by GF{2}

        v10 = c17 * 0x0100010001000100ull;
        v20 = c18 * 0x0101000001010000ull;
        v40 = c20 * 0x0101010100000000ull;
        v80 = c24 * 0x0101010101010101ull;

        v40 = v10 ^ v20 ^ v40;
        t[2] = v40;
        t[3] = v80 ^ v40;

#else // 32-bit or other
        unsigned char c3, c5, c6, c7, c9, c10, c11, c12, c13, c14, c15;
        unsigned char c17, c18, c19, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30, c31;

        c3 = c2 ^ c;
        c5 = c4 ^ c;
        c6 = c4 ^ c2;
        c7 = c4 ^ c3;

        c9 = c8 ^ c;
        c10 = c8 ^ c2;
        c11 = c8 ^ c3;
        c12 = c8 ^ c4;
        c13 = c8 ^ c5;
        c14 = c8 ^ c6;
        c15 = c8 ^ c7;

        tbl[0] = 0;
        tbl[1] = c;
        tbl[2] = c2;
        tbl[3] = c3;
        tbl[4] = c4;
        tbl[5] = c5;
        tbl[6] = c6;
        tbl[7] = c7;
        tbl[8] = c8;
        tbl[9] = c9;
        tbl[10] = c10;
        tbl[11] = c11;
        tbl[12] = c12;
        tbl[13] = c13;
        tbl[14] = c14;
        tbl[15] = c15;

        c17 = (c8 << 1) ^ ((c8 & 0x80) ? 0x1d : 0);   // Mult by GF{2}
        c18 = (c17 << 1) ^ ((c17 & 0x80) ? 0x1d : 0); // Mult by GF{2}
        c19 = c18 ^ c17;
        c20 = (c18 << 1) ^ ((c18 & 0x80) ? 0x1d : 0); // Mult by GF{2}
        c21 = c20 ^ c17;
        c22 = c20 ^ c18;
        c23 = c20 ^ c19;
        c24 = (c20 << 1) ^ ((c20 & 0x80) ? 0x1d : 0); // Mult by GF{2}
        c25 = c24 ^ c17;
        c26 = c24 ^ c18;
        c27 = c24 ^ c19;
        c28 = c24 ^ c20;
        c29 = c24 ^ c21;
        c30 = c24 ^ c22;
        c31 = c24 ^ c23;

        tbl[16] = 0;
        tbl[17] = c17;
        tbl[18] = c18;
        tbl[19] = c19;
        tbl[20] = c20;
        tbl[21] = c21;
        tbl[22] = c22;
        tbl[23] = c23;
        tbl[24] = c24;
        tbl[25] = c25;
        tbl[26] = c26;
        tbl[27] = c27;
        tbl[28] = c28;
        tbl[29] = c29;
        tbl[30] = c30;
        tbl[31] = c31;

#endif //__WORDSIZE == 64 || _WIN64 || __x86_64__
}

void
ECProject::gf_mul_vect_matrix(unsigned char* vect, unsigned char* matrix, unsigned char *dest, int k){
    for(int i = 0; i < k; i++){
        dest[i] = 0;
        for(int j = 0; j < k; j++){
            dest[i] ^= gf_mul(vect[j], matrix[j * k + i]);
        }
    }
}