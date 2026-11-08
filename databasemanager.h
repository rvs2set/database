#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <cstdint>

#pragma pack(push, 1)
struct Record {
    int64_t id;
    char    name[32];
    double  price;
    int32_t quantity;
    bool    is_deleted;
    char    padding[11];
};

struct IndexEntry {
    int64_t  key;
    uint64_t file_offset;
};
#pragma pack(pop)

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    bool createDatabase(const QString& filepath);
    bool openDatabase(const QString& filepath);
    void closeDatabase();
    bool clearDatabase();
    bool isOpen() const { return dataFile.is_open() && indexFile.is_open(); }


    bool insertRecord(const Record& rec);
    bool findRecordByKey(int64_t id, Record& result);
    bool deleteRecordByKey(int64_t id);
    bool updateRecord(const Record& rec);


    std::vector<Record> findRecordsByName(const QString& name, int limit=-1);
    int deleteRecordsByName(const QString& name, int limit = -1);


    std::vector<Record> getAllRecords();


    bool backupDatabase(const QString& backupPath);
    bool restoreDatabase(const QString& backupPath, const QString& targetPath);

private:
    void loadIndexToMemory();
    void rebuildIndexFromData();
    uint64_t findOffsetInIndex(int64_t key);
    void updateIndex(int64_t key, uint64_t offset);
    void removeFromIndex(int64_t key);

    std::fstream dataFile;
    std::fstream indexFile;
    QString currentDbPath;
    std::unordered_map<int64_t, uint64_t> memoryIndex;
};

#endif // DATABASEMANAGER_H
