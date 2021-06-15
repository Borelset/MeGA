/*
 * Author   : Xiangyu Zou
 * Date     : 04/23/2021
 * Time     : 15:39
 * Project  : MeGA
 This source code is licensed under the GPLv2
 */

#ifndef MEGA_ARRANGEMENTWRITEPIPELINE_H
#define MEGA_ARRANGEMENTWRITEPIPELINE_H

#include <string>
#include "../Utility/StorageTask.h"
#include "../Utility/Lock.h"
#include "../Utility/Likely.h"
#include <thread>
#include <functional>
#include <sys/time.h>
#include "gflags/gflags.h"
#include "../Utility/BufferedFileWriter.h"

DEFINE_uint64(ArrangementFlushBufferLength,
              8388608, "ArrangementFlushBufferLength");

class ArrangementWritePipeline{
public:
    ArrangementWritePipeline(): taskAmount(0), runningFlag(true), mutexLock(), condition(mutexLock){
        worker = new std::thread(std::bind(&ArrangementWritePipeline::arrangementWriteCallback, this));
    }

    int addTask(ArrangementWriteTask* arrangementFilterTask) {
        MutexLockGuard mutexLockGuard(mutexLock);
        taskList.push_back(arrangementFilterTask);
        taskAmount++;
        condition.notify();
    }

    ~ArrangementWritePipeline() {
        runningFlag = false;
        condition.notifyAll();
        worker->join();
    }

private:
    void arrangementWriteCallback(){
        pthread_setname_np(pthread_self(), "AWriting Thread");
        ArrangementWriteTask* arrangementWriteTask;
        char pathBuffer[256];
        uint64_t* length;
        uint64_t currentVersion = 0;
        uint64_t classIter = 0;
        uint64_t classCounter =0;
        uint64_t offsetPtr = 0;
        uint64_t baseClassId = 0;

        while (likely(runningFlag)) {
            {
                MutexLockGuard mutexLockGuard(mutexLock);
                while (!taskAmount) {
                    condition.wait();
                    if (unlikely(!runningFlag)) break;
                }
                if (unlikely(!runningFlag)) continue;
                taskAmount--;
                arrangementWriteTask = taskList.front();
                taskList.pop_front();
            }

            if(arrangementWriteTask->startFlag){
                VolumeFileHeader versionFileHeader = {
                        .offsetCount = arrangementWriteTask->arrangementVersion
                };
                length = (uint64_t*)malloc(sizeof(uint64_t)*versionFileHeader.offsetCount);
                currentVersion = arrangementWriteTask->arrangementVersion;
                classIter = 0;
                classCounter = 0;
                baseClassId = (currentVersion+1)*(currentVersion)/2+1;

                sprintf(pathBuffer, VersionFilePath.data(), arrangementWriteTask->arrangementVersion);
                archivedFileOperator = new FileOperator(pathBuffer, FileOpenType::Write);
                archivedFileOperator->trunc(GlobalMetadataManagerPtr->arrangementGetTruncateSize() + (arrangementWriteTask->arrangementVersion+1)*sizeof(uint64_t));
                archivedFileOperator->seek(0);
                archivedFileOperator->write((uint8_t*)&versionFileHeader, sizeof(uint64_t));
                archivedFileOperator->seek(sizeof(VolumeFileHeader) + sizeof(uint64_t) * versionFileHeader.offsetCount);
                archivedFileWriter = new BufferedFileWriter(archivedFileOperator, FLAGS_ArrangementFlushBufferLength, 4);

                sprintf(pathBuffer, ClassFilePath.data(), baseClassId);
                activeFileOperator = new FileOperator(pathBuffer, FileOpenType::Write);
                activeFileWriter = new BufferedFileWriter(activeFileOperator, FLAGS_ArrangementFlushBufferLength, 4);
            }
            else if(arrangementWriteTask->classEndFlag){
                length[classIter] = classCounter;
                classIter++;
                classCounter = 0;
                offsetPtr = 0;

                sprintf(pathBuffer, ClassFilePath.data(), arrangementWriteTask->beforeClassId);
                remove(pathBuffer);

                delete arrangementWriteTask;
                delete activeFileWriter;
                delete activeFileOperator;

                if(classIter < currentVersion){
                    sprintf(pathBuffer, ClassFilePath.data(), baseClassId+classIter);
                    activeFileOperator = new FileOperator(pathBuffer, FileOpenType::Write);
                    activeFileWriter = new BufferedFileWriter(activeFileOperator, FLAGS_ArrangementFlushBufferLength, 4);
                }
                continue;
            }
            else if(arrangementWriteTask->finalEndFlag){
                printf("ActiveChunks:%lu, ArchivedChunks:%lu\n", activeChunks, archivedChunks);
                delete archivedFileWriter;
                archivedFileWriter = nullptr;

                archivedFileOperator->seek(sizeof(VolumeFileHeader));
                archivedFileOperator->write((uint8_t *) length, sizeof(uint64_t) * currentVersion);

                archivedFileOperator->fdatasync();
                delete archivedFileOperator;
                archivedFileOperator = nullptr;

                free(length);
                currentVersion = -1;

                GlobalMetadataManagerPtr->tableRolling();
                arrangementWriteTask->countdownLatch->countDown();
                delete arrangementWriteTask;
                printf("ArrangementWritePipeline finish\n");
                continue;
            }
            else if(arrangementWriteTask->isArchived){
                archivedFileWriter->write(arrangementWriteTask->writeBuffer, arrangementWriteTask->length);
                classCounter += arrangementWriteTask->length;
                archivedChunks++;
            }else{
                BlockHeader* bhPtr = (BlockHeader*)arrangementWriteTask->writeBuffer;
                activeFileWriter->write(arrangementWriteTask->writeBuffer, arrangementWriteTask->length);
                if (!bhPtr->type) {
                    GlobalMetadataManagerPtr->addSimilarFeature(
                            bhPtr->sFeatures,
                            {bhPtr->fp, (uint32_t) classIter + 1, offsetPtr,
                             arrangementWriteTask->length - sizeof(BlockHeader)});
                }
                offsetPtr += arrangementWriteTask->length;
                activeChunks++;
            }
        }
    }

    bool runningFlag;
    std::thread *worker;
    uint64_t taskAmount;
    std::list<ArrangementWriteTask*> taskList;
    MutexLock mutexLock;
    Condition condition;

    FileOperator* archivedFileOperator = nullptr;
//    FileOperator* activeFileOperator = nullptr;
    BufferedFileWriter* archivedFileWriter = nullptr;
//    BufferedFileWriter* activeFileWriter = nullptr;

    FileOperator* activeFileOperator = nullptr;
    BufferedFileWriter* activeFileWriter = nullptr;

    uint64_t activeChunks = 0, archivedChunks = 0;
};

static ArrangementWritePipeline* GlobalArrangementWritePipelinePtr;

#endif //MEGA_ARRANGEMENTWRITEPIPELINE_H
