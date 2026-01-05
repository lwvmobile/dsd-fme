// BCH decoder implementation from OP25 (Copyright 2010, KA1RBI, GPL license)
#include "dsd.h"

#include <vector>
typedef std::vector<bool> bit_vector;

static const int bchGFexp[64] = {
    1, 2, 4, 8, 16, 32, 3, 6, 12, 24, 48, 35, 5, 10, 20, 40,
    19, 38, 15, 30, 60, 59, 53, 41, 17, 34, 7, 14, 28, 56, 51, 37,
    9, 18, 36, 11, 22, 44, 27, 54, 47, 29, 58, 55, 45, 25, 50, 39,
    13, 26, 52, 43, 21, 42, 23, 46, 31, 62, 63, 61, 57, 49, 33, 0
};

static const int bchGFlog[64] = {
    -1, 0, 1, 6, 2, 12, 7, 26, 3, 32, 13, 35, 8, 48, 27, 18,
    4, 24, 33, 16, 14, 52, 36, 54, 9, 45, 49, 38, 28, 41, 19, 56,
    5, 62, 25, 11, 34, 31, 17, 47, 15, 23, 53, 51, 37, 44, 55, 40,
    10, 61, 46, 30, 50, 22, 39, 43, 29, 60, 42, 21, 20, 59, 57, 58
};

int bchDec(bit_vector& Codeword)
{
    int elp[24][22], S[23];
    int D[23], L[24], uLu[24];
    int locn[11], reg[12];
    int i, j, U, q, count;
    int SynError, CantDecode;

    SynError = 0; CantDecode = 0;

    for (i = 1; i <= 22; i++) {
        S[i] = 0;
        for (j = 0; j <= 62; j++) {
            if (Codeword[j]) {
                S[i] = S[i] ^ bchGFexp[(i * j) % 63];
            }
        }
        if (S[i]) { SynError = 1; }
        S[i] = bchGFlog[S[i]];
    }

    if (SynError) { // if there are errors, try to correct them
        L[0] = 0; uLu[0] = -1; D[0] = 0;    elp[0][0] = 0;
        L[1] = 0; uLu[1] = 0;  D[1] = S[1]; elp[1][0] = 1;
        for (i = 1; i <= 21; i++) {
            elp[0][i] = -1; elp[1][i] = 0;
        }
        U = 0;

        do {
            U = U + 1;
            if (D[U] == -1) {
                L[U + 1] = L[U];
                for (i = 0; i <= L[U]; i++) {
                    elp[U + 1][i] = elp[U][i];
                    elp[U][i] = bchGFlog[elp[U][i]];
                }
            } else {
                q = U - 1;
                while ((D[q] == -1) && (q > 0)) { q = q - 1; }
                if (q > 0) {
                    j = q;
                    do { j = j - 1; if ((D[j] != -1) && (uLu[q] < uLu[j])) { q = j; }
                    } while (j > 0);
                }

                if (L[U] > L[q] + U - q) {
                    L[U + 1] = L[U];
                } else {
                    L[U + 1] = L[q] + U - q;
                }

                for (i = 0; i <= 21; i++) {
                    elp[U + 1][i] = 0;
                }
                for (i = 0; i <= L[q]; i++) {
                    if (elp[q][i] != -1) {
                        elp[U + 1][i + U - q] = bchGFexp[(D[U] + 63 - D[q] + elp[q][i]) % 63];
                    }
                }
                for (i = 0; i <= L[U]; i++) {
                    elp[U + 1][i] = elp[U + 1][i] ^ elp[U][i];
                    elp[U][i] = bchGFlog[elp[U][i]];
                }
            }
            uLu[U + 1] = U - L[U + 1];

            if (U < 22) {
                if (S[U + 1] != -1) { D[U + 1] = bchGFexp[S[U + 1]]; } else { D[U + 1] = 0; }
                for (i = 1; i <= L[U + 1]; i++) {
                    if ((S[U + 1 - i] != -1) && (elp[U + 1][i] != 0)) {
                        D[U + 1] = D[U + 1] ^ bchGFexp[(S[U + 1 - i] + bchGFlog[elp[U + 1][i]]) % 63];
                    }
                }
                D[U + 1] = bchGFlog[D[U + 1]];
            }
        } while ((U < 22) && (L[U + 1] <= 11));

        U = U + 1;
        if (L[U] <= 11) {
            for (i = 0; i <= L[U]; i++) {
                elp[U][i] = bchGFlog[elp[U][i]];
            }

            for (i = 1; i <= L[U]; i++) {
                reg[i] = elp[U][i];
            }
            count = 0;
            for (i = 1; i <= 63; i++) {
                q = 1;
                for (j = 1; j <= L[U]; j++) {
                    if (reg[j] != -1) {
                        reg[j] = (reg[j] + j) % 63;
                        q = q ^ bchGFexp[reg[j]];
                    }
                }
                if (q == 0) {
                    locn[count] = 63 - i;
                    count = count + 1;
                }
            }
            if (count == L[U]) {
                for (i = 0; i <= L[U] - 1; i++) {
                    Codeword[locn[i]] = Codeword[locn[i]] ^ 1;
                }
                CantDecode = count;
            } else {
                CantDecode = -1;
            }
        } else {
            CantDecode = -2;
        }
    }
    return CantDecode;
}

/**
 * Convenience class to calculate the parity of the DUID values. Keeps a table with the expected outcomes
 * for fast lookup.
 */
class ParityTable
{
private:
    unsigned char table[16];

    unsigned char get_index(unsigned char x, unsigned char y)
    {
        return (x << 2) + y;
    }

public:
    ParityTable()
    {
        for (unsigned int i = 0; i < sizeof(table); i++) {
            table[i] = 0;
        }
        table[get_index(1,1)] = 1;
        table[get_index(2,2)] = 1;
    }

    unsigned char get_value(unsigned char x, unsigned char y)
    {
        return table[get_index(x, y)];
    }
} parity_table;

int check_NID(char* bch_code, int* new_nac, char* new_duid, unsigned char parity)
{
    int result;

    // Fill up with the given input
    bit_vector codeword(63);
    for (unsigned int i = 0; i < 63; i++) {
        codeword[i] = bch_code[62-i];
    }

    // Decode it
    int errors = bchDec(codeword);
    bool ok = (errors >= 0);

    if (!ok) {
        // Decode failed
        // fprintf (stderr, "NID ERR: %d; ", errors);
        result = 0;
    } else {
        //reload the codeword back into correct bit order
        for (unsigned int i = 0; i < 63; i++) {
            bch_code[i] = codeword[62-i];
        }
        for (unsigned int i = 0; i < 63; i++) {
            codeword[i] = bch_code[i];
        }
        // Take the NAC from the decoded output. It's a 12 bit number starting from position 0.
        int nac = 0;
        for (int i = 0; i < 12; i++) {
            nac <<= 1;
            nac |= (int)codeword[i];
        }
        *new_nac = nac;

        // Take the fixed DUID from the encoded output. 4 bit value starting at position 12.
        unsigned char new_duid_0 = (((int)codeword[12]) << 1) + ((int)codeword[13]);
        unsigned char new_duid_1 = (((int)codeword[14]) << 1) + ((int)codeword[15]);
        new_duid[0] = new_duid_0 + '0';
        new_duid[1] = new_duid_1 + '0';
        new_duid[2] = 0;    // Null terminate

        // Check the parity
        unsigned char expected_parity = parity_table.get_value(new_duid_0, new_duid_1);

        if (expected_parity != parity) {
            // Parity mismatch – optional handling
            // fprintf(stderr, "Error in parity detected?\n");
        }

        result = 1;
    }

    return result;
}