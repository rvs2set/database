#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnCreate_clicked();
    void on_btnOpen_clicked();
    void on_btnClearDb_clicked();
    void on_btnAdd_clicked();
    void on_btnSearch_clicked();
    void on_btnDelete_clicked();
    void on_btnEdit_clicked();
    void on_btnBackup_clicked();
    void on_btnRestore_clicked();
    void on_btnExportExcel_clicked();

private:
    Ui::MainWindow *ui;
    QStandardItemModel *tableModel;
    DatabaseManager db;
    QString currentDbFile;

    void loadTableData();
};

#endif // MAINWINDOW_H
