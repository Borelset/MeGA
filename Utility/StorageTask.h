#ifndef MEGA_STORAGETASK_H
#define MEGA_STORAGETASK_H

#include "Lock.h"
#include <list>
#include <tuple>
#include <cstring>

struct SHA1FP {
    //std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t> fp;
    uint64_t fp1;
    uint32_t fp2, fp3, fp4;

    void print() {
        printf("%lu:%d:%d:%d\n", fp1, fp2, fp3, fp4);
    }
};


enum class LookupResult {
    Unique,
    InternalDedup,
    AdjacentDedup,
    InternalDeltaDedup, // reference to a delta chunk
    Similar,
    Dissimilar,
};


struct SimilarityFeatures {
    uint64_t feature1;
    uint64_t feature2;
    uint64_t feature3;
};

struct BasePos {
    SHA1FP sha1Fp;
    uint32_t CategoryOrder;
    uint64_t cid;
    uint64_t length: 63;
    uint64_t valid: 1;
};

struct DeltaTask{
    uint8_t *buffer;
    uint64_t pos;
    uint64_t length;
    CountdownLatch *countdownLatch = nullptr;
    BasePos basePos;
    SHA1FP fp;
    uint64_t fileID;
    int type;
    bool deltaTag;
};


struct BlockEntry {
    uint8_t *block = nullptr;
    uint64_t length = 0;
    uint64_t lastVisit = 0;
    uint64_t score = 0;
};


struct DedupTask {
    uint8_t *buffer;
    uint64_t pos;
    uint64_t length;
    SHA1FP fp;
    uint64_t fileID;
    CountdownLatch *countdownLatch = nullptr;
    uint64_t index;
    SimilarityFeatures similarityFeatures;
    BasePos basePos;
    bool inCache = 0;
    LookupResult lookupResult;
    bool deltaReject = false;
    BlockEntry availBase;
};

enum class WriteTaskType{
    Unique,
    Delta,
};

struct WriteTask {
    int type;
    uint8_t *buffer;
    uint64_t pos;
    uint64_t length;
    uint64_t fileID;
    SHA1FP sha1Fp;
    CountdownLatch *countdownLatch = nullptr;
    uint64_t index;
    SHA1FP baseFP;
    bool deltaTag;
    uint64_t oriLength;
    SimilarityFeatures similarityFeatures;
};

struct ChunkTask {
    uint8_t *buffer = nullptr;
    uint64_t length;
    uint64_t fileID;
    uint64_t end;
    CountdownLatch *countdownLatch = nullptr;
    uint64_t index;
};

struct StorageTask {
    std::string path;
    uint8_t *buffer = nullptr;
    uint64_t length;
    uint64_t fileID;
    uint64_t end;
    CountdownLatch *countdownLatch = nullptr;

    void destruction() {
        if (buffer) free(buffer);
    }
};

struct RestoreTask {
    uint64_t maxVersion;
    uint64_t targetVersion;
    uint64_t fallBehind;
};

struct RestoreParseTask {
    uint8_t *buffer = nullptr;
    uint64_t length;
    bool endFlag = false;
    uint64_t index = 0;
    uint64_t beginPos = 0;
    uint64_t sizeAfterCompression = 0;

    RestoreParseTask(uint8_t *buf, uint64_t len, uint64_t sac) {
        buffer = buf;
        length = len;
        beginPos = 0;
        sizeAfterCompression = sac;
    }

    RestoreParseTask(bool flag) {
        endFlag = true;
    }

    ~RestoreParseTask() {
        if (buffer) {
            free(buffer);
        }
    }
};

struct RestoreWriteTask {
    uint8_t *buffer = nullptr;
    uint64_t pos;
    uint64_t type: 1;
    uint64_t base: 1;
    uint64_t length: 62;
    uint64_t deltaLength;
    bool endFlag = false;

    RestoreWriteTask(uint8_t *buf, uint64_t p, uint64_t len, uint64_t t, uint64_t isbase, uint64_t dl) {
        buffer = (uint8_t *) malloc(len);
        memcpy(buffer, buf, len);
        length = len;
        base = isbase;
        type = t;
        pos = p;

        if (isbase) {
            deltaLength = dl;
        }
    }

    RestoreWriteTask(bool flag) {
        endFlag = true;
    }

    ~RestoreWriteTask() {
        if (buffer) {
            free(buffer);
        }
    }
};

struct ArrangementWriteTask{
    uint8_t* writeBuffer = nullptr;
    uint64_t length;
    uint64_t beforeClassId;
    uint64_t arrangementVersion = -1;
    bool isArchived = 0;
    bool classEndFlag = false;
    bool finalEndFlag = false;
    bool startFlag = false;
    CountdownLatch* countdownLatch;

    ArrangementWriteTask(uint8_t *buf, uint64_t len, uint64_t pcid, uint64_t version, bool isArch) {
        writeBuffer = (uint8_t *) malloc(len);
        memcpy(writeBuffer, buf, len);
        length = len;
        beforeClassId = pcid;
        arrangementVersion = version;
        isArchived = isArch;
    }

    ArrangementWriteTask(bool flag, uint64_t pcid) {
        classEndFlag = true;
        beforeClassId = pcid;
    }

    ArrangementWriteTask(bool flag){
        finalEndFlag = true;
    }

    ArrangementWriteTask(){

    }

    ~ArrangementWriteTask() {
        if (writeBuffer) {
            free(writeBuffer);
        }
    }
};

//struct ArrangementDeltaTask{
//    uint8_t* refBuffer =
//    uint8_t* readBuffer = nullptr;
//    uint64_t length;
//    uint64_t classId;
//    uint64_t arrangementVersion;
//    bool classEndFlag = false;
//    bool finalEndFlag = false;
//    bool startFlag = false;
//    CountdownLatch* countdownLatch;
//};

struct ArrangementFilterTask{
    uint8_t* readBuffer = nullptr;
    uint64_t length;
    uint64_t classId;
    uint64_t arrangementVersion;
    bool classEndFlag = false;
    bool finalEndFlag = false;
    bool startFlag = false;
    CountdownLatch* countdownLatch;

    ArrangementFilterTask(uint8_t *buf, uint64_t len, uint64_t cid, uint64_t version) {
        readBuffer = buf;
        length = len;
        classId = cid;
        arrangementVersion = version;
    }

    ArrangementFilterTask(bool flag, uint64_t cid) {
        classEndFlag = true;
        classId = cid;
    }

    ArrangementFilterTask(bool flag){
        finalEndFlag = true;
    }

    ArrangementFilterTask(){

    }

    ~ArrangementFilterTask(){
        if(readBuffer) free(readBuffer);
    }
};

struct ArrangementTask {
    uint64_t arrangementVersion;
    CountdownLatch *countdownLatch = nullptr;
};

struct BlockHeader {
    SHA1FP fp;
    uint64_t type : 1;
    uint64_t length : 63;
    uint64_t oriLength;
    union {
        SHA1FP baseFP;
        SimilarityFeatures sFeatures;
    };
};

struct RestoreMapListEntry {
    uint64_t type: 1;
    uint64_t base: 1;
    uint64_t pos: 62;
    uint64_t deltaLength;
};

struct VolumeFileHeader {
    uint64_t offsetCount;
};

#endif //MEGA_STORAGETASK_H
