/*
 * Author   : Xiangyu Zou
 * Date     : 04/23/2021
 * Time     : 15:39
 * Project  : MeGA
 This source code is licensed under the GPLv2
 */

#ifndef MEGA_BASECACHE_H
#define MEGA_BASECACHE_H

#include <unordered_map>
#include <map>

#define PreloadSize 2*1024*1024

extern std::string ClassFileAppendPath;

struct BlockEntry {
    uint8_t *block;
    uint64_t length;
    uint64_t lastVisit;
};

uint64_t TotalSizeThreshold = 400 * 1024 * 1024;

class BaseCache{
public:
    BaseCache(): totalSize(0), index(0), cacheMap(65536), write(0), read(0) {
        preloadBuffer = (uint8_t*)malloc(PreloadSize);
    }

    void setCurrentVersion(uint64_t verison){
        currentVersion = verison;
    }

    ~BaseCache(){
        statistics();
        free(preloadBuffer);
        for(const auto& blockEntry: cacheMap){
            free(blockEntry.second.block);
        }
    }

    void statistics(){
        printf("block cache:\n");
        printf("total size:%lu, items:%lu\n", totalSize, items);
        printf("cache write:%lu, cache read:%lu\n", write, read);
        printf("hit rate: %f(%lu/%lu)\n", float(success)/access, success, access);
        printf("cache miss %lu times, total loading time %lu us, average %f us\n", access-success, loadingTime, (float)loadingTime/(access-success));
    }

    void loadBaseChunks(const BasePos& basePos){
        gettimeofday(&t0, NULL);
        char pathBuffer[256];
        uint64_t targetCategory;
        if (basePos.CategoryOrder) {
            targetCategory = (currentVersion - 2) * (currentVersion - 1) / 2 + basePos.CategoryOrder;
            sprintf(pathBuffer, ClassFilePath.data(), targetCategory);
        } else {
            targetCategory = (currentVersion - 2) * (currentVersion - 1) / 2 + 1;
            sprintf(pathBuffer, ClassFileAppendPath.data(), targetCategory);
        }

        uint64_t readSize = 0;
        {
            FileOperator basefile(pathBuffer, FileOpenType::Read);
            basefile.seek(basePos.offset);
            readSize = basefile.read(preloadBuffer, PreloadSize);
            basefile.releaseBufferedData();
            assert(basePos.length <= readSize);
        }

        BlockHeader *headPtr;

        uint64_t preLoadPos = 0;
        uint64_t leftLength = readSize;

        while (leftLength > sizeof(BlockHeader) &&
               leftLength >= (2048 + sizeof(BlockHeader))) {// todo: min chunksize configured to 2048
            headPtr = (BlockHeader *) (preloadBuffer + preLoadPos);
            if (headPtr->length + sizeof(BlockHeader) > leftLength) {
                break;
            } else if (!headPtr->type) {
                addRecord(headPtr->fp, preloadBuffer + preLoadPos + sizeof(BlockHeader),
                          headPtr->length);
            }

            preLoadPos += headPtr->length + sizeof(BlockHeader);
            if (preLoadPos >= readSize) break;
            leftLength = readSize - preLoadPos;
        }
        gettimeofday(&t1, NULL);
        loadingTime += (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
    }

    void addRecord(const SHA1FP &sha1Fp, uint8_t *buffer, uint64_t length) {
        {
            //MutexLockGuard cacheLockGuard(cacheLock);
            auto iter = cacheMap.find(sha1Fp);
            if (iter == cacheMap.end()) {
                uint8_t *cacheBuffer = (uint8_t *) malloc(length);
                memcpy(cacheBuffer, buffer, length);
                cacheMap[sha1Fp] = {
                        cacheBuffer, length, index,
                };
                items++;
                write += length;
                {
                    //MutexLockGuard lruLockGuard(lruLock);
                    lruList[index] = sha1Fp;
                    index++;
                    totalSize += length;
                    while (totalSize > TotalSizeThreshold) {
                        auto iterLru = lruList.begin();
                        assert(iterLru != lruList.end());
                        auto iterCache = cacheMap.find(iterLru->second);
                        assert(iterCache != cacheMap.end());
                        totalSize -= iterCache->second.length;
                        free(iterCache->second.block);
                        cacheMap.erase(iterCache);
                        lruList.erase(iterLru);
                        items--;
                    }
                }
            } else {
                // it should not happen
                freshLastVisit(iter);
            }
        }
    }

    int getRecord(const SHA1FP &sha1Fp, BlockEntry *cacheBlock) {
        {
            //MutexLockGuard cacheLockGuard(cacheLock);
            access++;
            auto iterCache = cacheMap.find(sha1Fp);
            if (iterCache == cacheMap.end()) {
                return 0;
            } else {
                success++;
                *cacheBlock = iterCache->second;
                read += cacheBlock->length;
                {
                    freshLastVisit(iterCache);
                }
                return 1;
            }
        }
    }

private:
    void freshLastVisit(
            std::unordered_map<SHA1FP, BlockEntry, TupleHasher, TupleEqualer>::iterator iter) {
        //MutexLockGuard lruLockGuard(lruLock);
        auto iterl = lruList.find(iter->second.lastVisit);
        lruList[index] = iterl->second;
        lruList.erase(iterl);
        iter->second.lastVisit = index;
        index++;
    }

    struct timeval t0, t1;
    uint64_t index;
    uint64_t totalSize;
    std::unordered_map<SHA1FP, BlockEntry, TupleHasher, TupleEqualer> cacheMap;
    std::map<uint64_t, SHA1FP> lruList;
    //MutexLock cacheLock;
    //MutexLock lruLock;
    uint64_t write, read;
    uint64_t access = 0, success = 0;
    uint64_t loadingTime = 0;
    uint64_t items = 0;
    uint64_t currentVersion = 0;

    uint8_t* preloadBuffer;
};

#endif //MEGA_BASECACHE_H
