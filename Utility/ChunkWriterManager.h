/*
 * Author   : Xiangyu Zou
 * Date     : 04/23/2021
 * Time     : 15:39
 * Project  : MeGA
 This source code is licensed under the GPLv2
 */

#ifndef MEGA_CHUNKWRITERMANAGER_H
#define MEGA_CHUNKWRITERMANAGER_H

#include "Likely.h"

DEFINE_uint64(WriteBufferLength,
              8388608, "WriteBufferLength");

DEFINE_uint64(ChunkWriterManagerFlushThreshold,
              8, "WriteBufferLength");

extern std::string ClassFilePath;
extern std::string VersionFilePath;

int WorkerExitMagicNumber = -1;

struct WriteBuffer {
    char *buffer;
    uint64_t totalLength;
    uint64_t available;
};


class ChunkWriterManager {
public:
    ChunkWriterManager(uint64_t currentVersion):runningFlag(true), taskAmount(0), mutexLock(), condition(mutexLock) {
        classId = (currentVersion + 1) * currentVersion / 2;

        sprintf(pathBuffer, ClassFilePath.data(), classId);
        writer = new FileOperator(pathBuffer, FileOpenType::Write);
        syncCounter = 0;
//        writeBuffer = {
//                (char *) malloc(FLAGS_WriteBufferLength),
//                FLAGS_WriteBufferLength,
//                FLAGS_WriteBufferLength,
//        }; old
        writeBuffer = {
                (char *) malloc(FLAGS_WriteBufferLength),
                FLAGS_WriteBufferLength,
                0,
        };

        syncWorker = new std::thread(std::bind(&ChunkWriterManager::ChunkWriterManagerCallback, this));
    }

    int writeClass(uint8_t *header, uint64_t headerLen, uint8_t *buffer, uint64_t bufferLen) {
        writer->write((uint8_t *) header, headerLen);
        writer->write((uint8_t *) buffer, bufferLen);
        writeBuffer.available += headerLen + bufferLen;
        if (writeBuffer.available >= writeBuffer.totalLength) {
            classFlush();
        }
        return 0;
    }

    int writeClass_old(uint8_t *header, uint64_t headerLen, uint8_t *buffer, uint64_t bufferLen) {

        if ((headerLen + bufferLen) > writeBuffer.available) {
            classFlush();
        }
        char *writePoint = writeBuffer.buffer + writeBuffer.totalLength - writeBuffer.available;
        memcpy(writePoint, header, headerLen);
        writeBuffer.available -= headerLen;
        writePoint += headerLen;
        memcpy(writePoint, buffer, bufferLen);
        writeBuffer.available -= bufferLen;

        return 0;
    }


    ~ChunkWriterManager() {
        addTask(WorkerExitMagicNumber);
        syncWorker->join();
        classFlush();
        writer->fdatasync();
        delete writer;
        delete writeBuffer.buffer;
    }

private:
    int classFlush() {
        writeBuffer.available = 0;
        if (syncCounter >= FLAGS_ChunkWriterManagerFlushThreshold) {
            addTask(classId);
            syncCounter = 0;
        } else {
            syncCounter++;
        }
        return 0;
    }

    int classFlush_old() {
        uint64_t flushLength = writeBuffer.totalLength - writeBuffer.available;
        writer->write((uint8_t *) writeBuffer.buffer, flushLength);
        writeBuffer.available = writeBuffer.totalLength;
        if (syncCounter >= FLAGS_ChunkWriterManagerFlushThreshold) {
            addTask(classId);
            syncCounter = 0;
        } else {
            syncCounter++;
        }
        return 0;
    }

    int addTask(uint64_t classId) {
        MutexLockGuard mutexLockGuard(mutexLock);
        taskList.push_back(classId);
        taskAmount++;
        condition.notify();
    }

    void ChunkWriterManagerCallback(){
        pthread_setname_np(pthread_self(), "CWriter Thread");
        uint64_t classId;
        while (likely(runningFlag)) {
            {
                MutexLockGuard mutexLockGuard(mutexLock);
                while (!taskAmount) {
                    condition.wait();
                    if (unlikely(!runningFlag)) break;
                }
                if (unlikely(!runningFlag)) continue;
                taskAmount--;
                classId = taskList.front();
                taskList.pop_front();
            }

            if(classId == WorkerExitMagicNumber){
                break;
            }

            writer->fdatasync();
        }
    }

    FileOperator * writer = nullptr;
    WriteBuffer writeBuffer;
    uint64_t syncCounter = 0;
    uint64_t classId;
    char pathBuffer[256];

    std::thread* syncWorker;
    bool runningFlag;
    uint64_t taskAmount;
    std::list<uint64_t> taskList;
    MutexLock mutexLock;
    Condition condition;
};

#endif //MEGA_CHUNKWRITERMANAGER_H
