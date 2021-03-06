#include "Config.h"

#include "stdio_fl.h"
#include <stdlib.h>
#include <math.h>
#include "memory_fl.h"

#define LOG_TAG "ComputeH_Direct"
#include "Log.h"
#include "ErrorTools.h"

#include "AlvaMatrix.h"

#include "ComputeH_Direct.h"

#undef FILE_NUM
#define FILE_NUM 0xF900

void AllocComputeH_Direct_Data(pComputeH_Direct_Data* data, int maxPointNum) {

#undef FUNC_CODE
#define FUNC_CODE 0x01

    pComputeH_Direct_Data temp = (pComputeH_Direct_Data)calloc(sizeof(ComputeH_Direct_Data), 1);
    temp->maxPointNum = maxPointNum;
    temp->mTMCoor0 = (ComputeH_Direct_Coord*)malloc(sizeof(ComputeH_Direct_Coord) * maxPointNum);
    temp->mTMCoor1 = (ComputeH_Direct_Coord*)malloc(sizeof(ComputeH_Direct_Coord) * maxPointNum);
    temp->mTMxy    = (float*)malloc(sizeof(float) * maxPointNum * 2);
    temp->mTMH     = (float*)malloc(sizeof(float) * maxPointNum * 2 * 8);
    temp->mTMHTra  = (float*)malloc(sizeof(float) * maxPointNum * 8 * 2);
    *data = temp;
}

void FreeComputeH_Direct_Data(pComputeH_Direct_Data* data) {

#undef FUNC_CODE
#define FUNC_CODE 0x02

    if (NULL != data) {
        if (NULL != *data) {
            if (NULL != (*data)->mTMCoor0) {
                free((*data)->mTMCoor0);
            }
            if (NULL != (*data)->mTMCoor1) {
                free((*data)->mTMCoor1);
            }
            if (NULL != (*data)->mTMxy) {
                free((*data)->mTMxy);
            }
            if (NULL != (*data)->mTMH) {
                free((*data)->mTMH);
            }
            if (NULL != (*data)->mTMHTra) {
                free((*data)->mTMHTra);
            }
        }
        *data = NULL;
    }
}

//齐次坐标方程
static void ComputeHHomogeneous(ComputeH_Direct_Coord match0[], ComputeH_Direct_Coord match1[], float coefficient[], int num){

#undef FUNC_CODE
#define FUNC_CODE 0x03

    int i;
    for (i = 0; i < num; i++){
        //x坐标
        coefficient[i * 8 + 0] = match0[i].x;
        coefficient[i * 8 + 1] = match0[i].y;
        coefficient[i * 8 + 2] = 1;
        coefficient[i * 8 + 3] = 0;
        coefficient[i * 8 + 4] = 0;
        coefficient[i * 8 + 5] = 0;
        coefficient[i * 8 + 6] = -match0[i].x * match1[i].x;
        coefficient[i * 8 + 7] = -match0[i].y * match1[i].x;

        //y坐标
        coefficient[(i + num) * 8 + 0] = 0;
        coefficient[(i + num) * 8 + 1] = 0;
        coefficient[(i + num) * 8 + 2] = 0;
        coefficient[(i + num) * 8 + 3] = match0[i].x;
        coefficient[(i + num) * 8 + 4] = match0[i].y;
        coefficient[(i + num) * 8 + 5] = 1;
        coefficient[(i + num) * 8 + 6] = -match0[i].x * match1[i].y;
        coefficient[(i + num) * 8 + 7] = -match0[i].y * match1[i].y;
    }
}

//转置矩阵
static void ComputeHMatrixTranspose(float* H, float* H_tra, int m, int n){

#undef FUNC_CODE
#define FUNC_CODE 0x04

    int i, j;
    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++){
            H_tra[i*m + j] = H[j*n + i];
        }
}

//矩阵相乘  a*b=c
static void ComputeHfMatmul(float* a, float* b, float* c, int m, int n, int l){

#undef FUNC_CODE
#define FUNC_CODE 0x05

    int i, j, k;
    for (i = 0; i < m; i++)
        for (j = 0; j < l; j++){
            c[i*l + j] = 0.0;
            for (k = 0; k < n; k++){
                c[i*l + j] += a[i*n + k] * b[k*l + j];
            }
        }
}

//高斯消元法解方程组
static int ComputeHgaussj(float A[], float b[], float X[], const int N){

#undef FUNC_CODE
#define FUNC_CODE 0x06

    int i, j, k, index;
    float atemp, btemp, t;

    for (i = 0; i < N - 1; i++){
        index = i;
        for (j = i + 1; j < N; j++){
            if (fabs(A[j*N + i]) > fabs(A[i*N + i]))  index = j;
        }

        if (i != index){
            for (k = i; k < N; k++){
                atemp = A[index*N + k];
                A[index*N + k] = A[i*N + k];
                A[i*N + k] = atemp;
            }
            btemp = b[index];
            b[index] = b[i];
            b[i] = btemp;
        }

        if (fabs(A[i*N + i]) < 1.0e-6) return 1;

        for (j = i + 1; j < N; j++){
            t = A[j*N + i] / A[i*N + i];
            for (k = i; k < N; k++){
                A[j*N + k] = A[j*N + k] - A[i*N + k] * t;
            }

            b[j] = b[j] - b[i] * t;
        }
    }

    for (i = N - 1; i > 0; i--){
        for (j = i - 1; j >= 0; j--){
            b[j] = b[j] - b[i] * A[j*N + i] / A[i*N + i];
        }
    }
    for (i = 0; i < N; i++){
        X[i] = b[i] / A[i*N + i];
    }

    return 0;
}

int setComputeH_Direct_Data(float* feat0, float* feat1, int* loc, int num, pComputeH_Direct_Data pComHData){

#undef FUNC_CODE
#define FUNC_CODE 0x07

    int i = 0;

    if (num > pComHData->maxPointNum)
        return ERROR_CODE(0x001 | ERROR_OWNER);

    for (i = 0; i < num; i++){
        int n = 0;
        if (loc == NULL)  n = i;
        else  n = loc[i];

        pComHData->mTMCoor0[i].x = feat0[n * 2 + 0];
        pComHData->mTMCoor0[i].y = feat0[n * 2 + 1];

        pComHData->mTMCoor1[i].x = feat1[n * 2 + 0];
        pComHData->mTMCoor1[i].y = feat1[n * 2 + 1];

        pComHData->mTMxy[i] = pComHData->mTMCoor1[i].x;
        pComHData->mTMxy[i + num] = pComHData->mTMCoor1[i].y;
    }
    pComHData->pairNum = num;

    return 0;
}

int setComputeH_Direct_Data_disperse(float* feat0, float* feat1, int* matchLoc, int* consensus, int num, pComputeH_Direct_Data pComHData, int maxNum) {

#undef FUNC_CODE
#define FUNC_CODE 0x07

    int i = 0, k;

    int idx, idy;

    if (num > pComHData->maxPointNum)
        return ERROR_CODE(0x001 | ERROR_OWNER);

    for (i = 0; i < num; i++) {
        k = consensus[i];

        idx = matchLoc[k];
        idy = matchLoc[maxNum + k];

        pComHData->mTMCoor0[i].x = feat0[idx];
        pComHData->mTMCoor0[i].y = feat0[maxNum + idx];

        pComHData->mTMCoor1[i].x = feat1[idy];
        pComHData->mTMCoor1[i].y = feat1[maxNum + idy];

        pComHData->mTMxy[i] = pComHData->mTMCoor1[i].x;
        pComHData->mTMxy[i + num] = pComHData->mTMCoor1[i].y;
    }
    pComHData->pairNum = num;

    return 0;
}

void ComputeH_Direct_Full(pComputeH_Direct_Data pComHData, float H_gauss[]){

#undef FUNC_CODE
#define FUNC_CODE 0x08

    ComputeHHomogeneous(pComHData->mTMCoor0, pComHData->mTMCoor1, pComHData->mTMH, pComHData->pairNum);

    ComputeHMatrixTranspose(pComHData->mTMH, pComHData->mTMHTra, pComHData->pairNum * 2, 8);

    ComputeHfMatmul(pComHData->mTMHTra, pComHData->mTMH, pComHData->mTMHMul, 8, pComHData->pairNum * 2, 8);

    ComputeHfMatmul(pComHData->mTMHTra, pComHData->mTMxy, pComHData->mTMHxy, 8, pComHData->pairNum * 2, 1);

    ComputeHgaussj(pComHData->mTMHMul, pComHData->mTMHxy, H_gauss, 8);

    H_gauss[8] = 1;
}

void ComputeH_Direct_3(pComputeH_Direct_Data pComHData, float H_gauss[]) {

#undef FUNC_CODE
#define FUNC_CODE 0x08

    float matrix[36] = {pComHData->mTMCoor0[0].x, pComHData->mTMCoor0[0].y, 1.0f,                     0.0f,                      0.0f, 0.0f,
                                            0.0f,                      0.0f, 0.0f, pComHData->mTMCoor0[0].x, pComHData->mTMCoor0[0].y, 1.0f,
                        pComHData->mTMCoor0[1].x, pComHData->mTMCoor0[1].y, 1.0f,                     0.0f,                      0.0f, 0.0f,
                                            0.0f,                      0.0f, 0.0f, pComHData->mTMCoor0[1].x, pComHData->mTMCoor0[1].y, 1.0f,
                        pComHData->mTMCoor0[2].x, pComHData->mTMCoor0[2].y, 1.0f,                     0.0f,                      0.0f, 0.0f,
                                            0.0f,                      0.0f, 0.0f, pComHData->mTMCoor0[2].x, pComHData->mTMCoor0[2].y, 1.0f,};

    float inv_matrix[36] = {0.0f};

    float v[6] = {pComHData->mTMCoor1[0].x,
                  pComHData->mTMCoor1[0].y,
                  pComHData->mTMCoor1[1].x,
                  pComHData->mTMCoor1[1].y,
                  pComHData->mTMCoor1[2].x,
                  pComHData->mTMCoor1[2].y};

    AlvaMatrix mMatrix, mInv_matrix, vec, res_vec;
    mMatrix.rows     = 6; mMatrix.cols     = 6; mMatrix.matrix     = matrix;
    mInv_matrix.rows = 6; mInv_matrix.cols = 6; mInv_matrix.matrix = inv_matrix;
    vec.rows         = 6; vec.cols         = 1; vec.matrix         = v;
    res_vec.rows     = 6; res_vec.cols     = 1; res_vec.matrix     = H_gauss;

    InvMatrix(&mMatrix, &mInv_matrix);

    MulMatrix(&mInv_matrix, &vec, &res_vec);

    H_gauss[6] = 0.0f; H_gauss[7] = 0.0f; H_gauss[8] = 1.0f;
}

void ComputeH_Direct_2(pComputeH_Direct_Data pComHData, float H_gauss[]) {

#undef FUNC_CODE
#define FUNC_CODE 0x08

    float matrix[16] = {pComHData->mTMCoor0[0].x, -pComHData->mTMCoor0[0].y, 1.0f, 0.0f,
                        pComHData->mTMCoor0[0].y,  pComHData->mTMCoor0[0].x, 0.0f, 1.0f,
                        pComHData->mTMCoor0[1].x, -pComHData->mTMCoor0[1].y, 1.0f, 0.0f,
                        pComHData->mTMCoor0[1].y,  pComHData->mTMCoor0[1].x, 0.0f, 1.0f};

    float inv_matrix[16] = {0.0f};

    float v[4] = {pComHData->mTMCoor1[0].x,
                  pComHData->mTMCoor1[0].y,
                  pComHData->mTMCoor1[1].x,
                  pComHData->mTMCoor1[1].y};

    float res[4] = {0.0f};

    asM4Inv(matrix, inv_matrix);

    asM4xV4(inv_matrix, v, res);

    H_gauss[0] = res[0]; H_gauss[1] = -res[1]; H_gauss[2] = res[2];
    H_gauss[3] = res[1]; H_gauss[4] =  res[0]; H_gauss[5] = res[3];
    H_gauss[6] = 0.0f  ; H_gauss[7] = 0.0f   ; H_gauss[8] = 1.0f  ;
}

void ComputeH_Direct_1(pComputeH_Direct_Data pComHData, float H_gauss[]) {

#undef FUNC_CODE
#define FUNC_CODE 0x08

    H_gauss[0] = 1.0f; H_gauss[1] = 0.0f;
    H_gauss[3] = 0.0f; H_gauss[4] = 1.0f;
    H_gauss[6] = 0.0f; H_gauss[7] = 0.0f; H_gauss[8] = 1.0f;

    H_gauss[2] = pComHData->mTMCoor1[0].x - pComHData->mTMCoor0[0].x;
    H_gauss[5] = pComHData->mTMCoor1[0].y - pComHData->mTMCoor0[0].y;
}

int ComputeH_Direct(pComputeH_Direct_Data pComHData, float H_gauss[]) {

#undef FUNC_CODE
#define FUNC_CODE 0x08

    if(pComHData->pairNum >= 4) {
        ComputeH_Direct_Full(pComHData, H_gauss);
        return 0;
    }

    if(pComHData->pairNum == 3) {
        ComputeH_Direct_3(pComHData, H_gauss);
        return 0;
    }

    if(pComHData->pairNum == 2) {
        ComputeH_Direct_2(pComHData, H_gauss);
        return 0;
    }

    if(pComHData->pairNum == 1) {
        ComputeH_Direct_1(pComHData, H_gauss);
        return 0;
    }

    OWN_ERROR_RETURN(0x001, "There is no enough point pairs!");
}

int Project2DPointsWithH(float H[],
                         float* srcPoints, float* dstPoints, int num,
                         float cx, float cy) {

#undef FUNC_CODE
#define FUNC_CODE 0x0A

    int i = 0, memI = 0;
    float mod = 0.0f;
    float u1, v1, u, v;

    for(int i = 0; i < num; i++, memI += 2) {
        u1 = srcPoints[memI + 0] - cx;
        v1 = srcPoints[memI + 1] - cy;

        mod = H[6] * u1 + H[7] * v1 + H[8];
        if(fabs(mod) < 0.000001f) {
            return ERROR_CODE(0x8000 | i);
        }
        else {
            mod = 1.0f / mod;
        }

        u = (H[0] * u1 + H[1] * v1 + H[2]) * mod;
        v = (H[3] * u1 + H[4] * v1 + H[5]) * mod;

        dstPoints[memI + 0] = u + cx;
        dstPoints[memI + 1] = v + cy;
    }
    return 0;
}