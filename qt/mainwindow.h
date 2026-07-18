#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QScreen>
#include <cmath>
#include <QTimer>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QSerialPort>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    //第一页系统总览界面
    QWidget* createSystemStatusWidget();
    void updateSystemViwe(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets);
    //第二页雷达页面
    QWidget* createRadarWidget();
    void updateRadarView(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets);//更新雷达页面
    //第三页摄像头状态页面
    QWidget* createCameraWidget();
    void updateCameraView(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets);//更新摄像头状态页面
    void changepit(QString  newImagePath);
    //第四页视频页面
    QWidget* createVideoListWidget();
    void refreshVideoList();
    void addVideoItem(const QFileInfo& fileInfo);
    void playVideo(const QFileInfo& fileInfo);
    void deleteVideo(const QFileInfo& fileInfo);
    /*串口*/
    void openSerialPort(const QString &portName);
    /*人脸识别结果*/
    bool checkFaceDetection();
    void processFaceDetection();

    /*摄像头控制*/
    //文件写入命令
    uint8_t command = 0xf0;
    //四个摄像头一米内是否有人
    bool all = false;
    bool one = false;
    bool two = false;
    bool three = false;
    bool four = false;

private:
    Ui::MainWindow *ui;

    /*串口通信*/
    QThread *serialThread;
    QSerialPort *serial;
    bool abort = false;
    //bool doorstatus = false;

    /*主页面*/
    int main_hight;
    int main_width;
    //widget 小部件
    QWidget *widget;
    //水平布局
    QHBoxLayout *hBoxLayout;
    // 列表视图
    QListWidget *listWidget;
    //堆栈窗口部件
    QStackedWidget *stackedWidget;

    /*堆栈窗口界面*/
    //第一页
    QWidget *systemwidge;
    QLabel *doorstatuslabel;
    QLabel *radarStatusLabel;
    QLabel *radarresultlabel;
    QLabel *cameraStatusLabel;
    QLabel *cameraresultlabel;
    QLabel *timeDateLabel;
    QTimer *timeUpdateTimer;
    // 第二页雷达页面
    QWidget *radarPage;
    QGraphicsPixmapItem *backgroundItem; //车图片
    QGraphicsScene *scene;//雷达车图片
    QLabel *radar_statusLabel; // 用于显示雷达检测状态
    QVBoxLayout *peopleInfoLayout;; // 用于显示目标的位置状态
    //第三页摄像头状态页面
    QWidget *CameraPage;
    QGraphicsScene *camscene;
    QLabel *cam1statusLabel;
    QLabel *cam2statusLabel;
    QLabel *cam3statusLabel;
    QLabel *cam4statusLabel;
    // 第四页视频列表布局
    QVBoxLayout *videoListLayout;
    QWidget *videoListWidget;



 private slots:
    void updateTime();//更新时间槽函数
    void currentRowChangeSlot(int row);//qlist和qstack的槽
    void updateStatus(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets);//雷达更新位置槽函数
    void handleSerialError(const QString &error);

    void closeSerialPort();
    void process();
signals:
    void stopSerial();   // 停止串口信号
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);
    void statusUpdated(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets);//定义门的状态，true开启，false关闭


};
#endif // MAINWINDOW_H
