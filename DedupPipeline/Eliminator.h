//  Copyright (c) Xiangyu Zou, 2020. All rights reserved.
//  This source code is licensed under the GPLv2

#ifndef MFDEDUP_ELIMINATOR_H
#define MFDEDUP_ELIMINATOR_H

DEFINE_uint64(EliminateReadBuffer,
67108864, "Read buffer size for eliminating old version");

extern std::string LogicFilePath;
extern std::string ClassFilePath;
extern std::string VersionFilePath;
extern std::string ClassFileAppendPath;

class Eliminator {
public:
    Eliminator() {

    }

    int run(uint64_t maxVersion) {
        printf("start to eliminate\n");
        uint64_t startClass = (maxVersion - 1) * maxVersion / 2 + 1;
        uint64_t endClass = (maxVersion + 1) * maxVersion / 2;

        printf("processing categories files\n");
        classFileCombinationProcessor(startClass, startClass + 1, maxVersion);
        for (uint64_t i = startClass + 2; i <= endClass; i++) {
            classFileProcessor(i, maxVersion);
        }

        printf("processing volume files\n");
        for (uint64_t i = 2; i <= maxVersion - 1; i++) {
            versionFileProcessor(i);
        }

        printf("processing recipe files\n");
        for (uint64_t i = 2; i <= maxVersion; i++) {
            recipeFilesProcessor(i);
        }
        printf("finish,  the earliest version has been eliminated\n");
    }

private:
    int recipeFilesProcessor(uint64_t recipeId) {
        // rolling back serial number of recipes
        sprintf(oldPath, LogicFilePath.data(), recipeId);
        sprintf(newPath, LogicFilePath.data(), recipeId - 1);
        rename(oldPath, newPath);

        return 0;
    }

    int versionFileProcessor(uint64_t versionId) {
        // append first two archived categories in volumes.
        sprintf(oldPath, VersionFilePath.data(), versionId);
        sprintf(newPath, VersionFilePath.data(), versionId - 1);
        FileOperator fileOperator(oldPath, FileOpenType::ReadWrite);

        VolumeFileHeader versionFileHeader;
        fileOperator.read((uint8_t * ) & versionFileHeader, sizeof(VolumeFileHeader));
        uint64_t *offset = (uint64_t *) malloc(versionFileHeader.offsetCount * sizeof(uint64_t));
        fileOperator.read((uint8_t *) offset, versionFileHeader.offsetCount * sizeof(uint64_t));
        offset[0] += offset[1];
        for (int i = 1; i < versionFileHeader.offsetCount - 1; i++) {
            offset[i] = offset[i + 1];
        }
        offset[versionFileHeader.offsetCount - 1] = -1;
        fileOperator.seek(sizeof(VolumeFileHeader));
        fileOperator.write((uint8_t *) offset, versionFileHeader.offsetCount * sizeof(uint64_t));

        rename(oldPath, newPath);

        return 0;
    }

    int classFileProcessor(uint64_t classId, uint64_t maxVersion) {
        // rolling back serial number of categories
        sprintf(oldPath, ClassFilePath.data(), classId);
        sprintf(newPath, ClassFilePath.data(), classId - maxVersion);
        rename(oldPath, newPath);
        return 0;
    }

    int classFileCombinationProcessor(uint64_t classId1, uint64_t classId2, uint64_t maxVersion) {
        // rolling back serial number of categories
        // append first two active categories.
        sprintf(oldPath, ClassFilePath.data(), classId1);
        sprintf(newPath, ClassFilePath.data(), classId1 - (maxVersion-1));
        rename(oldPath, newPath);

        sprintf(oldPath, ClassFilePath.data(), classId2);
        sprintf(newPath, ClassFileAppendPath.data(), classId1 - (maxVersion-1));
        rename(oldPath, newPath);

        return 0;
    }

    char oldPath[256];
    char newPath[256];
};

#endif //MFDEDUP_ELIMINATOR_H
