#include "Config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "AlvaFilter.h"

#include "Filter_ConditionDamp.h"

#undef FILE_NUM
#define FILE_NUM 0xE500

typedef struct _CONDITION_DAMP_FILTER_DATA_ {
    float* lastValue[2];
    float* alpha;
    float* beta;
    float threshold;
}Condition_Damp_Filter_Data;

static
void
initMemory(Filter* filter) {

#undef FUNC_CODE
#define FUNC_CODE 0x01

    Condition_Damp_Filter_Data* ptr = (Condition_Damp_Filter_Data*)calloc(1, sizeof(Condition_Damp_Filter_Data));

    ptr->alpha = (float*)calloc(filter->numGroup, sizeof(float));
    ptr->beta = (float*)calloc(filter->numGroup, sizeof(float));
    ptr->lastValue[0] = (float*)calloc(filter->numGroup, sizeof(float));
    ptr->lastValue[1] = (float*)calloc(filter->numGroup, sizeof(float));

    filter->data = (void*)ptr;
}

static
void
condition_damp(Filter* filter, float* data) {

#undef FUNC_CODE
#define FUNC_CODE 0x02

    Condition_Damp_Filter_Data* ptr = (Condition_Damp_Filter_Data*)filter->data;
    int i = 0;
    float sum = 0.0f;

    if (filter->frameIndex < 2) {
        memcpy(ptr->lastValue[filter->frameIndex], data, filter->numGroup * sizeof(float));
        filter->frameIndex++;
    }
    else {
        for (i = 0; i < filter->numGroup; i++) {
            sum += (float)fabs(data[i] - ptr->lastValue[1][i]);
        }
        if (sum < ptr->threshold) {
            for (i = 0; i < filter->numGroup; i++) {
                ptr->alpha[i] = 0.5;
                ptr->beta[i] = 2 * (float)sqrt(ptr->alpha[i]) - ptr->alpha[i];
            }
        }
        else {
            for (i = 0; i < filter->numGroup; i++) {
                ptr->alpha[i] = 1.0f;
                ptr->beta[i] = 2 * (float)sqrt(ptr->alpha[i]) - ptr->alpha[i];
            }
        }
        for (i = 0; i < filter->numGroup; i++) {
            data[i] = ptr->alpha[i] * data[i]
                    + ptr->lastValue[1][i] * (2.0f - ptr->alpha[i] - ptr->beta[i])
                    + ptr->lastValue[0][i] * (ptr->beta[i] - 1.0f);
        }
        memcpy(ptr->lastValue[0], ptr->lastValue[1], filter->numGroup * sizeof(float));
        memcpy(ptr->lastValue[1], data, filter->numGroup * sizeof(float));
    }
}

static
void
unit(Filter* filter) {

#undef FUNC_CODE
#define FUNC_CODE 0x03

    if (NULL != filter) {
        Condition_Damp_Filter_Data* ptr = (Condition_Damp_Filter_Data*)filter->data;
        if (NULL != ptr) {
            if (NULL != ptr->lastValue[0]) {
                free(ptr->lastValue[0]);
            }
            if (NULL != ptr->lastValue[1]) {
                free(ptr->lastValue[1]);
            }
            if (NULL != ptr->alpha) {
                free(ptr->alpha);
            }
            if (NULL != ptr->beta) {
                free(ptr->beta);
            }
            free(ptr);
        }
        filter->type = Filter_None;
        filter->numGroup = 0;
    }
}

static
void
reset(Filter* filter) {

#undef FUNC_CODE
#define FUNC_CODE 0x04

    filter->frameIndex = 0;
}

void condition_damp_init(Filter* filter, int dataNum, float* userData) {

#undef FUNC_CODE
#define FUNC_CODE 0x05

    filter->type = Filter_ConditionDamp;
    filter->numGroup = dataNum;
    filter->frameIndex = 0;

    filter->filter = condition_damp;
    filter->unit = unit;
    filter->reset = reset;

    initMemory(filter);

    Condition_Damp_Filter_Data* ptr = (Condition_Damp_Filter_Data*)filter->data;
    ptr->threshold = userData[0];
}
