/*
 *  N dimensional matrix operations
 */

#pragma once

#include "math.h"
#include <stdint.h>
#include "vectorN.h"

template <typename T, uint8_t N>
class VectorN;


template <typename T, uint8_t N>
class MatrixN {

    friend class VectorN<T,N>;

public:
    // constructor from zeros
    MatrixN<T,N>(void) {
        memset(v, 0, sizeof(v));        
    }

    // constructor from 4 diagonals
    MatrixN<T,N>(const float d[N]) {
        memset(v, 0, sizeof(v));
        for (uint8_t i = 0; i < N; i++) {
            v[i][i] = d[i];
        }
    }

    // multiply two vectors to give a matrix, in-place
    void mult(const VectorN<T,N> &A, const VectorN<T,N> &B);

    // subtract B from the matrix
    MatrixN<T,N> &operator -=(const MatrixN<T,N> &B);

    // add B to the matrix
    MatrixN<T,N> &operator +=(const MatrixN<T,N> &B);

    // allow a MatrixN to be used as an array of vectors, 0 indexed
    VectorN<T,N> & operator[](uint8_t i);
    const VectorN<T,N> & operator[](uint8_t i) const;

    // calculate the inverse of this matrix
    // results stored in inv
    // returns true if this matrix is invertible, false otherwise and inv is unmodified
    //bool inverse(MatrixN<T,N>& inv) const;// { return false; }

    // invert this matrix if it is invertible.
    // returns true on success
    bool invert();

    // Matrix symmetry routine
    void force_symmetry(void);

private:
    T v[N][N];
};

typedef MatrixN<float,4> Matrix4f;
