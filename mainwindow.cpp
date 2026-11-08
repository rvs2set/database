#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <cstring>
static const int SEARCH_LIMIT = -1;
static const int DELETE_LIMIT = -1;

static void fillName(Record& rec, const QString& name)
{
    std::memset(rec.name, 0, sizeof(rec.name));
    std::strncpy(rec.name, name.toUtf8().constData(), sizeof(rec.name) - 1);
}
static void appendRecordRow(QStandardItemModel* model, const Record& rec)
{
    QList<QStandardItem*> items;
    items << new QStandardItem(QString::number(rec.id))
          << new QStandardItem(QString::fromUtf8(rec.name))
          << new QStandardItem(QString::number(rec.price))
          << new QStandardItem(QString::number(rec.quantity));
    model->appendRow(items);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    tableModel = new QStandardItemModel(this);
    tableModel->setColumnCount(4);
    tableModel->setHeaderData(0, Qt::Horizontal, "ID");
    tableModel->setHeaderData(1, Qt::Horizontal, "Название");
    tableModel->setHeaderData(2, Qt::Horizontal, "Цена");
    tableModel->setHeaderData(3, Qt::Horizontal, "Количество");

    ui->tableView->setModel(tableModel);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::loadTableData()
{
    tableModel->removeRows(0, tableModel->rowCount());
    if (!db.isOpen()) return;

    for (const Record& rec : db.getAllRecords()) {
        appendRecordRow(tableModel, rec);
    }
}
void MainWindow::on_btnCreate_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Создать БД", "", "Binary DB (*.bin)");
    if (fileName.isEmpty()) return;

    if (db.createDatabase(fileName)) {
        currentDbFile = fileName;
        loadTableData();
        QMessageBox::information(this, "Успех", "База данных успешно создана!");
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл БД.");
    }
}
void MainWindow::on_btnOpen_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Открыть БД", "", "Binary DB (*.bin)");
    if (fileName.isEmpty()) return;

    if (db.openDatabase(fileName)) {
        currentDbFile = fileName;
        loadTableData();
        QMessageBox::information(this, "Успех", "База данных успешно открыта!");
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть базу данных.");
    }
}
void MainWindow::on_btnClearDb_clicked()
{
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Ошибка", "Сначала откройте или создайте базу данных!");
        return;
    }

    if (QMessageBox::question(this, "Подтверждение", "Очистить всю базу данных?") == QMessageBox::Yes) {
        if (db.clearDatabase()) {
            loadTableData();
            QMessageBox::information(this, "Успех", "База данных очищена.");
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось очистить базу данных.");
        }
    }
}

void MainWindow::on_btnAdd_clicked()
{
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Ошибка", "Сначала откройте или создайте базу данных");
        return;
    }

    bool okId = false;
    qint64 id = ui->txtId->text().toLongLong(&okId);
    QString nameStr = ui->txtName->text();

    if (!okId) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный числовой ID");
        return;
    }
    if (nameStr.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите название!");
        return;
    }

    Record newRec{};
    newRec.id = id;
    newRec.price = ui->txtPrice->text().replace(',', '.').toDouble();
    newRec.quantity = ui->txtQuantity->text().toInt();
    newRec.is_deleted = false;
    fillName(newRec, nameStr);

    if (!db.insertRecord(newRec)) {
        QMessageBox::warning(this, "Ошибка", "Запись с таким ID уже существует!");
        return;
    }

    loadTableData();
    QMessageBox::information(this, "Успех", "Запись добавлена!");

    ui->txtId->clear();
    ui->txtName->clear();
    ui->txtPrice->clear();
    ui->txtQuantity->clear();
}

void MainWindow::on_btnSearch_clicked()
{
    if (!db.isOpen()) return;

    QString searchText = ui->txtSearch->text().trimmed();
    if (searchText.isEmpty()) {
        loadTableData();
        return;
    }

    tableModel->removeRows(0, tableModel->rowCount());

    bool isNumeric = false;
    qint64 searchId = searchText.toLongLong(&isNumeric);

    if (isNumeric) {
        Record rec;
        if (db.findRecordByKey(searchId, rec)) {
            appendRecordRow(tableModel, rec);
            return;
        }
    }
    for (const Record& rec : db.findRecordsByName(searchText, SEARCH_LIMIT)) {
        appendRecordRow(tableModel, rec);
    }
}
void MainWindow::on_btnDelete_clicked()
{
    if (!db.isOpen()) return;

    QString targetText = ui->txtSearch->text().trimmed();
    if (targetText.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите ID или Название для удаления в поле поиска/удаления!");
        return;
    }

    bool isNumeric = false;
    qint64 targetId = targetText.toLongLong(&isNumeric);

    bool found = false;
    if (isNumeric && db.deleteRecordByKey(targetId)) {
        found = true;
    } else if (db.deleteRecordsByName(targetText,DELETE_LIMIT) > 0) {
        found = true;
    }

    if (found) {
        loadTableData();
        QMessageBox::information(this, "Успех", "Запись успешно удалена!");
    } else {
        QMessageBox::warning(this, "Ошибка", "Записи для удаления не найдены.");
    }
}
void MainWindow::on_btnEdit_clicked()
{
    if (!db.isOpen()) return;

    bool okId = false;
    qint64 editId = ui->txtId->text().toLongLong(&okId);
    if (!okId) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный числовой ID!");
        return;
    }

    Record rec{};
    rec.id = editId;
    rec.price = ui->txtPrice->text().replace(',', '.').toDouble();
    rec.quantity = ui->txtQuantity->text().toInt();
    rec.is_deleted = false;
    fillName(rec, ui->txtName->text());

    if (db.updateRecord(rec)) {
        loadTableData();
        QMessageBox::information(this, "Успех", "Запись успешно обновлена!");
    } else {
        QMessageBox::warning(this, "Ошибка", "Запись с таким ID не найдена для редактирования.");
    }
}

// 8. Создание backup-файла БД
void MainWindow::on_btnBackup_clicked()
{
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Ошибка", "Нет открытой базы данных!");
        return;
    }

    QString backupName = QFileDialog::getSaveFileName(this, "Сохранить бэкап", "", "Backup (*.bak)");
    if (backupName.isEmpty()) return;

    if (db.backupDatabase(backupName)) {
        loadTableData();
        QMessageBox::information(this, "Успех", "Резервная копия успешно создана!");
    } else {
        loadTableData();
        QMessageBox::critical(this, "Ошибка", "Не удалось создать бэкап.");
    }
}

// 9. Восстановление БД из backup-файла
void MainWindow::on_btnRestore_clicked()
{
    QString backupName = QFileDialog::getOpenFileName(this, "Выберите бэкап", "", "Backup (*.bak)");
    if (backupName.isEmpty()) return;

    QString targetName = QFileDialog::getSaveFileName(this, "Куда восстановить БД?", "", "Binary DB (*.bin)");
    if (targetName.isEmpty()) return;

    if (db.restoreDatabase(backupName, targetName)) {
        currentDbFile = targetName;
        loadTableData();
        QMessageBox::information(this, "Успех", "База данных успешно восстановлена!");
    } else {
        loadTableData();
        QMessageBox::critical(this, "Ошибка", "Не удалось восстановить базу данных.");
    }
}

// 10. Экспорт
void MainWindow::on_btnExportExcel_clicked()
{
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Внимание", "Сначала откройте или создайте базу данных!");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, "Экспорт в Excel", "", "CSV файлы (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream.setGenerateByteOrderMark(true);   // BOM, чтобы Excel правильно прочитал кириллицу

        stream << "ID;Название;Цена;Количество\n";

        for (const Record& rec : db.getAllRecords()) {
            // для русской локали Excel десятичный разделитель - запятая
            QString price = QString::number(rec.price, 'f', 2).replace('.', ',');
            stream << rec.id << ";"
                   << QString::fromUtf8(rec.name) << ";"
                   << price << ";"
                   << rec.quantity << "\n";
        }
        file.close();
        QMessageBox::information(this, "Успех", "Данные успешно экспортированы! Файл можно открыть в Excel.");
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл для экспорта.");
    }
}
