#include "databasemanager.h"
#include <cstring>
#include <chrono>
#include <QFile>
#include <QDebug>

DatabaseManager::DatabaseManager() {}

DatabaseManager::~DatabaseManager() {
    closeDatabase();
}
void DatabaseManager::loadIndexToMemory() {
    memoryIndex.clear();
    if (!indexFile.is_open()) return;

    indexFile.clear();
    indexFile.seekg(0, std::ios::beg);

    IndexEntry entry;
    while (indexFile.read(reinterpret_cast<char*>(&entry), sizeof(IndexEntry))) {
        if (entry.file_offset == UINT64_MAX)
            memoryIndex.erase(entry.key);
        else
            memoryIndex[entry.key] = entry.file_offset;
    }
    indexFile.clear();
}

void DatabaseManager::rebuildIndexFromData() {
    memoryIndex.clear();
    dataFile.clear();
    dataFile.seekg(0, std::ios::beg);

    Record rec;
    uint64_t offset = 0;
    while (dataFile.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
        if (!rec.is_deleted)
            updateIndex(rec.id, offset);
        offset += sizeof(Record);
    }
    dataFile.clear();
}

bool DatabaseManager::createDatabase(const QString& filepath) {
    closeDatabase();
    QString path = filepath;

    std::ofstream data(path.toStdString(), std::ios::binary | std::ios::trunc);
    std::ofstream idx((path + ".idx").toStdString(), std::ios::binary | std::ios::trunc);

    if (!data.is_open() || !idx.is_open()) return false;

    data.close();
    idx.close();

    return openDatabase(path);
}

bool DatabaseManager::openDatabase(const QString& filepath) {
    closeDatabase();
    QString path = filepath;
    currentDbPath = path;

    bool needRebuild = !QFile::exists(path + ".idx");
    if (needRebuild) {
        std::ofstream idx((path + ".idx").toStdString(), std::ios::binary);
    }

    dataFile.open(path.toStdString(), std::ios::in | std::ios::out | std::ios::binary);
    indexFile.open((path + ".idx").toStdString(), std::ios::in | std::ios::out | std::ios::binary);

    if (dataFile.is_open() && indexFile.is_open()) {
        if (needRebuild)
            rebuildIndexFromData();
        else
            loadIndexToMemory();
        return true;
    }
    closeDatabase();
    return false;
}

void DatabaseManager::closeDatabase() {
    if (dataFile.is_open()) dataFile.close();
    if (indexFile.is_open()) indexFile.close();
    dataFile.clear();
    indexFile.clear();
    memoryIndex.clear();
}

bool DatabaseManager::clearDatabase() {
    if (currentDbPath.isEmpty()) return false;
    return createDatabase(currentDbPath);
}
uint64_t DatabaseManager::findOffsetInIndex(int64_t key) {
    auto it = memoryIndex.find(key);
    if (it != memoryIndex.end()) {
        return it->second;
    }
    return UINT64_MAX;
}
void DatabaseManager::updateIndex(int64_t key, uint64_t offset) {
    if (!indexFile.is_open()) return;

    indexFile.clear();
    indexFile.seekp(0, std::ios::end);
    IndexEntry entry{key, offset};
    indexFile.write(reinterpret_cast<const char*>(&entry), sizeof(IndexEntry));
    indexFile.flush();

    memoryIndex[key] = offset;
}

void DatabaseManager::removeFromIndex(int64_t key) {
    if (!indexFile.is_open()) return;

    indexFile.clear();
    indexFile.seekp(0, std::ios::end);
    IndexEntry entry{key, UINT64_MAX};
    indexFile.write(reinterpret_cast<const char*>(&entry), sizeof(IndexEntry));
    indexFile.flush();

    memoryIndex.erase(key);
}
bool DatabaseManager::insertRecord(const Record& rec) {
    if (!dataFile.is_open()) return false;

    auto start = std::chrono::high_resolution_clock::now();

    if (findOffsetInIndex(rec.id) != UINT64_MAX) {
        return false;
    }

    dataFile.clear();
    dataFile.seekp(0, std::ios::end);
    uint64_t offset = static_cast<uint64_t>(dataFile.tellp());

    dataFile.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
    dataFile.flush();

    updateIndex(rec.id, offset);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    qDebug() << "[Time Analysis - O(1)] insertRecord took:" << duration << "ns";

    return true;
}

//  Поиск по ключ хэш-таблица даёт смещение, читаем одну запись
bool DatabaseManager::findRecordByKey(int64_t id, Record& result) {
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t offset = findOffsetInIndex(id);
    if (offset == UINT64_MAX || !dataFile.is_open()) return false;

    dataFile.clear();
    dataFile.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    dataFile.read(reinterpret_cast<char*>(&result), sizeof(Record));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    qDebug() << "[Time Analysis - O(1)] findRecordByKey took:" << duration << "ns";

    return !result.is_deleted;
}

// Поиск по неключевому полю: O(n) - индекса по названию нет, читаем весь файл
std::vector<Record> DatabaseManager::findRecordsByName(const QString& name, int limit) {
    std::vector<Record> results;
    if (!dataFile.is_open()) return results;

    auto start = std::chrono::high_resolution_clock::now();

    dataFile.clear();
    dataFile.seekg(0, std::ios::beg);

    Record rec;
    std::string searchName = name.toStdString();

    while (dataFile.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
        if (!rec.is_deleted && std::string(rec.name) == searchName) {
            results.push_back(rec);
        }
    }
    dataFile.clear();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    qDebug() << "[Time Analysis - O(n)] findRecordsByName took:" << duration << "ns";

    return results;
}

bool DatabaseManager::deleteRecordByKey(int64_t id) {
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t offset = findOffsetInIndex(id);
    if (offset == UINT64_MAX || !dataFile.is_open()) return false;

    Record rec;
    dataFile.clear();
    dataFile.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    dataFile.read(reinterpret_cast<char*>(&rec), sizeof(Record));

    if (rec.is_deleted) return false;

    rec.is_deleted = true;
    dataFile.clear();
    dataFile.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    dataFile.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
    dataFile.flush();

    removeFromIndex(id);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    qDebug() << "[Time Analysis - O(1)] deleteRecordByKey took:" << duration << "ns";
    return true;
}
int DatabaseManager::deleteRecordsByName(const QString& name, int limit) {
    if (!dataFile.is_open()) return 0;

    auto start = std::chrono::high_resolution_clock::now();

    std::string searchName = name.toStdString();
    int count = 0;

    Record rec;
    uint64_t offset = 0;
    dataFile.clear();
    dataFile.seekg(0, std::ios::beg);

    while (dataFile.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
        if (!rec.is_deleted && std::string(rec.name) == searchName) {
            rec.is_deleted = true;
            dataFile.clear();
            dataFile.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
            dataFile.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
            dataFile.flush();
            removeFromIndex(rec.id);
            count++;
            if (limit > 0 && count >= limit)
                break;
            dataFile.seekg(static_cast<std::streamoff>(offset + sizeof(Record)), std::ios::beg);
        }
        offset += sizeof(Record);
    }
    dataFile.clear();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    qDebug() << "[Time Analysis - O(n)] deleteRecordsByName took:" << duration << "ns";

    return count;
}

// 4. Редактирование по ключу: O(1) - смещение из хэш-таблицы, перезапись записи на месте
bool DatabaseManager::updateRecord(const Record& rec) {
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t offset = findOffsetInIndex(rec.id);
    if (offset == UINT64_MAX || !dataFile.is_open()) return false;

    Record toWrite = rec;
    toWrite.is_deleted = false;

    dataFile.clear();
    dataFile.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    dataFile.write(reinterpret_cast<const char*>(&toWrite), sizeof(Record));
    dataFile.flush();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    qDebug() << "[Time Analysis - O(1)] updateRecord took:" << duration << "ns";

    return true;
}

// чтение всех записей для показа на экране: O(n)
std::vector<Record> DatabaseManager::getAllRecords() {
    std::vector<Record> results;
    if (!dataFile.is_open()) return results;

    dataFile.clear();
    dataFile.seekg(0, std::ios::beg);

    Record rec;
    while (dataFile.read(reinterpret_cast<char*>(&rec), sizeof(Record))) {
        if (!rec.is_deleted) results.push_back(rec);
    }
    dataFile.clear();
    return results;
}

static bool copyReplace(const QString& src, const QString& dst) {
    if (!QFile::exists(src)) return false;
    if (QFile::exists(dst)) QFile::remove(dst);
    return QFile::copy(src, dst);
}

// Backup: O(n) - копирование файлов
bool DatabaseManager::backupDatabase(const QString& backupPath) {
    if (currentDbPath.isEmpty()) return false;
    QString path = currentDbPath;

    closeDatabase();

    bool ok = copyReplace(path, backupPath);
    if (ok) copyReplace(path + ".idx", backupPath + ".idx");

    openDatabase(path);
    return ok;
}

// Restore: O(n) - копирование файлов
bool DatabaseManager::restoreDatabase(const QString& backupPath, const QString& targetPath) {
    closeDatabase();

    if (!copyReplace(backupPath, targetPath)) return false;

    if (!copyReplace(backupPath + ".idx", targetPath + ".idx")) {
        QFile::remove(targetPath + ".idx");   // индекс будет перестроен при открытии
    }

    return openDatabase(targetPath);
}
