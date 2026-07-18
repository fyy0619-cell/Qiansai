#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //页面标题
    this->setWindowTitle("车辆安全检测系统");
//    this->setStyleSheet("QMainWindow { background-color: #f0f0f0;}"
//                          ) ;  // 标签透明);
    /* 获取屏幕的分辨率，Qt官方建议使用这
     * 种方法获取屏幕分辨率，防上多屏设备导致对应不上
     * 注意，这是获取整个桌面系统的分辨率
     */
    QList <QScreen *> list_screen =  QGuiApplication::screens();
        this->resize(list_screen.at(0)->geometry().width(),
                        list_screen.at(0)->geometry().height());
    main_hight = list_screen.at(0)->geometry().height();
    main_width = list_screen.at(0)->geometry().width();

    main_hight = 480;
    main_width = 800;
    //设置整体窗口大小
    this->setFixedSize(main_width, main_hight);

    //widget 小部件实例化
    widget = new QWidget(this);
    this->setCentralWidget(widget);//设置居中
//    widget->setStyleSheet("background-color: transparent;"
//                                               "QPushButton {"
//                                               "    background-color: palette(button);"
//                                               "    border: 1px solid #8f8f91;"
//                                               "    border-radius: 4px;"
//                                               "    padding: 5px;"
//                                               "}"
//                                               "QPushButton:pressed {"
//                                               "    background-color: #d0d0d0;"
//                                               "}"); // 确保透明
    //垂直布局实例化,堆栈和列表会添加到这个垂直布局上
    hBoxLayout = new QHBoxLayout();
    //堆栈部件实例化
    stackedWidget = new QStackedWidget();

    //列表实例化
    listWidget = new QListWidget();
    /*左侧list列表*/
    //设置 listWidget 的固定高度（确保总高度固定）
    listWidget->setFixedHeight(main_hight);
    listWidget->setFixedWidth(main_width * 0.17);
    //移除列表的内边距、边框和项间距（避免样式影响）
    listWidget->setSpacing(0);  // 移除项间距
    listWidget->setFrameShape(QFrame::NoFrame);  // 移除边框
    listWidget->setStyleSheet(
        "QListWidget {"
        "    background-color: white;"
        "    padding: 0px;"
        "    margin: 0px;"
        "    font-size: 12px;"
        "}"
        "QListWidget::item {"
        "    height: 110px;"  // 保持项高度不变
        "    display: flex;"  // 使用flex布局
        "    flex-direction: column;"  // 垂直方向排列
        "    justify-content: center;"  // 垂直居中
        "    align-items: center;"  // 水平居中
        "}"
        "QListWidget::item:selected {"
        "    background-color: #e0e0e0;"  // 选中项的背景色
        "}"
    );
    // 设置图标大小
    listWidget->setIconSize(QSize(64, 64)); // 根据实际图标大小调整

    // 准备图标资源路径
    QStringList iconPaths = {
        ":/image/主页.jpg",      // 首页图标
        ":/image/雷达监测.jpg",     // 雷达检测图标
        ":/image/摄像头.png",    // 摄像头检测图标
        ":/image/录像观看.png"      // 录像观看图标
    };
    //添加列表项
    QList<QString> strListWidgetList;
    strListWidgetList << "首页" << "雷达检测" << "摄像头检测" << "录像观看";
    // 添加列表项
    for (int i = 0; i < 4; i++) {
        QPixmap pixmap(iconPaths[i]);
        if(pixmap.isNull()) {
            qDebug() << "Failed to load image:" << iconPaths[i];
            continue;
        }
        // 创建列表项时不设置图标,用自定义widget显示
        QListWidgetItem *item = new QListWidgetItem(listWidget);
        item->setSizeHint(QSize(item->sizeHint().width(), 120));

        // 创建包含图标和文本的自定义widget
        QWidget *itemWidget = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(itemWidget);
        layout->setSpacing(5);  // 图标和文本之间的间距
        layout->setContentsMargins(0, 10, 0, 10);  // 上下留一些边距

        // 图标标签
        QLabel *iconLabel = new QLabel();
        QPixmap scaledPixmap = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        iconLabel->setPixmap(scaledPixmap);
        iconLabel->setAlignment(Qt::AlignCenter);

        // 文本标签
        QLabel *textLabel = new QLabel(strListWidgetList[i]);
        textLabel->setAlignment(Qt::AlignCenter);
        QFont font = textLabel->font();
        font.setPointSize(12);  // 字体大小
        textLabel->setFont(font);

        // 添加到布局
        layout->addWidget(iconLabel, 0, Qt::AlignCenter);
        layout->addWidget(textLabel, 0, Qt::AlignCenter);
        layout->addStretch();  // 添加弹性空间保持居中

        itemWidget->setLayout(layout);

        // 将自定义widget设置为列表项
        listWidget->setItemWidget(item, itemWidget);
        item->setData(Qt::UserRole, QVariant(strListWidgetList[i]));
    }

    //禁用滚动条，避免内容超出后出现滚动条占用空间
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /*stack内容*/
    //一页系统状态
    systemwidge = createSystemStatusWidget();
    //二页雷达检测
    radarPage = createRadarWidget();
    //三页摄像头检测
    CameraPage = createCameraWidget();
    //四页录像观看
    videoListWidget = createVideoListWidget();

    //进行stack添加
    stackedWidget->addWidget(systemwidge);
    stackedWidget->addWidget(radarPage);
    stackedWidget->addWidget(CameraPage);
    stackedWidget->addWidget(videoListWidget);

    /* 添加到水平布局 */
    hBoxLayout->addWidget(listWidget);
    hBoxLayout->addWidget(stackedWidget);

    widget->setLayout(hBoxLayout);//将 widget 的布局设置成 hboxLayout
    connect(listWidget, SIGNAL(currentRowChanged(int)),stackedWidget, SLOT(setCurrentIndex(int)));

    //串口信号槽连接
    connect(this, &MainWindow::statusUpdated, this, &MainWindow::updateStatus);
    connect(this, &MainWindow::errorOccurred, this, &MainWindow::handleSerialError);

    //打开串口
    openSerialPort("/dev/ttyAMA0");
    //进行人脸识别结果读取以及发送
    processFaceDetection();
    /*测试更新代码*/
    QVector<QPointF> testtargets;
    testtargets.append(QPointF(-1.72,1.13));
    testtargets.append(QPointF(-0.37, -1.29));
    testtargets.append(QPointF(3.54, -1.31));
    emit statusUpdated(0,1, 3, testtargets);

}

MainWindow::~MainWindow()
{
    delete ui;
    emit stopSerial();
    serialThread->quit();
    serialThread->wait();
}

/*槽函数*/
void MainWindow::currentRowChangeSlot(int row)
{
    stackedWidget->setCurrentIndex(row);
}
//串口错误槽函数
void MainWindow::handleSerialError(const QString &error) {
    qDebug() << "Serial Error:" << error;
}
// 更新槽函数
void MainWindow::updateStatus(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets)
{
    // 更新系统状态界面
    updateSystemViwe(doorstatus, peoplestatus, peoplenum, targets);
    // 更新雷达界面
    updateRadarView(doorstatus, peoplestatus, peoplenum, targets);
    //更新摄像头界面
    updateCameraView(doorstatus, peoplestatus, peoplenum, targets);
    //qDebug()<<"change successful";
}
/*第一页系统状态界面代码*/
QWidget* MainWindow::createSystemStatusWidget()
{
    QWidget *systemStatusWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(systemStatusWidget);

    // 1. 时间日期显示
    timeDateLabel = new QLabel();
    timeDateLabel->setAlignment(Qt::AlignCenter);
    timeDateLabel->setStyleSheet("QLabel { font-size: 24px; font-weight: bold; }");
    mainLayout->addWidget(timeDateLabel);

//    // 添加第一条分割线（时间日期和状态区域之间）
//    QFrame* line1 = new QFrame();
//    line1->setFrameShape(QFrame::HLine);
//    line1->setFrameShadow(QFrame::Sunken);
//    line1->setStyleSheet("color: #ccc;");
//    mainLayout->addWidget(line1);


    // 创建水平分割线
    QFrame* line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Plain);  // 改为Plain避免凹陷效果
    line1->setLineWidth(1);  // 设置线宽为1px
    line1->setStyleSheet(
        "QFrame {"
        "   border: none;"
        "   background-color: rgba(200, 210, 220, 0.5);"  // 柔和的淡灰色
        "   margin: 1px 0;"  // 上下边距16px，左右无边距
        "   min-height: 1px;"  // 明确最小高度
        "   max-height: 1px;"  // 确保不会意外变高
        "}"
    );
    mainLayout->addWidget(line1);


    // 2. 状态显示区域
    QWidget *statusWidget = new QWidget();
    QVBoxLayout *statusLayout = new QVBoxLayout(statusWidget);
    statusLayout->setSpacing(7); // 增加状态项间距

    // 创建车门状态主控件
    QWidget *doorStatusWidget = new QWidget();
    doorStatusWidget->setObjectName("doorStatusWidget");

    // 设置布局 - 使用舒适的间距
    QHBoxLayout *doorLayout = new QHBoxLayout(doorStatusWidget);
    doorLayout->setContentsMargins(20, 16, 20, 16);  // 对称的内边距
    doorLayout->setSpacing(7);  // 适中的元素间距

    // 添加车门图标（使用淡蓝色调图标更协调）
    QLabel *doorIcon = new QLabel();
    QPixmap iconPixmap(":/icons/door.png");
    // 给图标添加淡蓝色调
    QPainter p(&iconPixmap);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(iconPixmap.rect(), QColor(70, 130, 180, 150));
    p.end();
    doorIcon->setPixmap(iconPixmap.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    doorLayout->addWidget(doorIcon);

    // 标题标签 - 适配淡蓝背景的样式
    QLabel *doorTitle = new QLabel("车门状态:");
    doorTitle->setObjectName("doorTitleLabel");
    doorTitle->setStyleSheet(
        "QLabel#doorTitleLabel {"
        "   font-size: 18px;"
        "   color: #2c3e50;"  // 深蓝灰色文字
        "   font-weight: 600;"
        "   padding: 4px 0;"
        "}"
    );

    // 状态标签 - 半透明淡蓝背景
    doorstatuslabel = new QLabel("未知");
    doorstatuslabel->setObjectName("doorStatusLabel");
    doorstatuslabel->setStyleSheet(
        "QLabel#doorStatusLabel {"
        "   font-size: 18px;"
        "   font-weight: 600;"
        "   padding: 8px 24px;"
        "   border-radius: 8px;"
        "   background-color: rgba(220, 235, 245, 0.7);"  // 半透明淡蓝
        "   color: #2c3e50;"  // 深蓝灰色文字
        "   border: 1px solid rgba(100, 150, 200, 0.3);"  // 淡蓝色边框
        "   min-width: 100px;"
        "}"
    );
    doorstatuslabel->setAlignment(Qt::AlignCenter);

    // 添加控件到布局
    doorLayout->addWidget(doorTitle);
    doorLayout->addWidget(doorstatuslabel);
    doorLayout->addStretch();

    // 整体样式 - 淡蓝色渐变背景
    doorStatusWidget->setStyleSheet(
        "QWidget#doorStatusWidget {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "               stop:0 #f5f9fc, stop:1 #e0ecf7);"  // 淡蓝渐变
        "   border-radius: 12px;"
        "   border: 1px solid rgba(150, 180, 210, 0.5);"  // 半透明淡蓝边框
        "}"
    );

    // 添加柔和的水波纹阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setBlurRadius(20);
    shadowEffect->setOffset(2, 4);
    shadowEffect->setColor(QColor(100, 150, 200, 30));  // 淡蓝色阴影
    doorStatusWidget->setGraphicsEffect(shadowEffect);

    // 添加到主布局
    statusLayout->addWidget(doorStatusWidget);

    // 添加雷达状态和摄像头状态之间的分割线
    QFrame* line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Plain);
    line2->setLineWidth(1);
    line2->setStyleSheet(
        "QFrame {"
        "   color: rgba(150, 180, 210, 0.3);"  // 半透明淡蓝色
        "   margin: 10px 20px;"  // 上下边距10px，左右边距20px
        "   border: none;"
        "   background-color: rgba(150, 180, 210, 0.3);"  // 与边框同色
        "   height: 1px;"  // 明确高度
        "}"
    );
    statusLayout->addWidget(line2);

    // 创建雷达状态主控件
    QWidget *radarStatusWidget = new QWidget();
    radarStatusWidget->setObjectName("radarStatusWidget");

    // 主垂直布局
    QVBoxLayout *radarLayout = new QVBoxLayout(radarStatusWidget);
    radarLayout->setContentsMargins(0, 0, 0, 0);
    radarLayout->setSpacing(0);

    // 第一行控件容器
    QWidget *radarStatusWidget1 = new QWidget();
    radarStatusWidget1->setObjectName("radarStatusWidget1");
    QHBoxLayout *radarLayout1 = new QHBoxLayout(radarStatusWidget1);
    radarLayout1->setContentsMargins(20, 16, 20, 16);
    radarLayout1->setSpacing(7);

    // 雷达图标
    QLabel *radarIcon = new QLabel();
    QPixmap radarPixmap(":/icons/radar.png");
    {
        QPainter radarIconPainter(&radarPixmap);
        radarIconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        radarIconPainter.fillRect(radarPixmap.rect(), QColor(70, 130, 180, 150));
    }
    radarIcon->setPixmap(radarPixmap.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 雷达标题
    QLabel *radarTitle = new QLabel("雷达状态:");
    radarTitle->setObjectName("radarTitleLabel");
    radarTitle->setStyleSheet(
        "QLabel#radarTitleLabel {"
        "   font-size: 18px;"
        "   color: #2c3e50;"
        "   font-weight: 600;"
        "   padding: 4px 0;"
        "}"
    );

    // 雷达结果标签
    radarresultlabel = new QLabel("未知");
    radarresultlabel->setObjectName("radarResultLabel");
    radarresultlabel->setStyleSheet(
        "QLabel#radarResultLabel {"
        "   font-size: 18px;"
        "   font-weight: 600;"
        "   padding: 8px 24px;"
        "   border-radius: 8px;"
        "   background-color: rgba(220, 235, 245, 0.7);"
        "   color: #2c3e50;"
        "   border: 1px solid rgba(100, 150, 200, 0.3);"
        "   min-width: 100px;"
        "}"
    );
    radarresultlabel->setAlignment(Qt::AlignCenter);

    // 添加第一行控件
    radarLayout1->addWidget(radarIcon);
    radarLayout1->addWidget(radarTitle);
    radarLayout1->addWidget(radarresultlabel);
    radarLayout1->addStretch();

    // 雷达状态标签（第二行）- 修改为左对齐
    radarStatusLabel = new QLabel("初始化测试文字");
    radarStatusLabel->setObjectName("radarStatusLabel");
    radarStatusLabel->setStyleSheet(
        "QLabel#radarStatusLabel {"
        "   font-size: 16px;"
        "   font-weight: 500;"
        "   padding: 12px 20px;"
        "   color: #2c3e50;"
        "   background-color: rgba(210, 230, 240, 0.5);"
        "   border-top: 1px solid rgba(150, 180, 210, 0.3);"
        "   border-radius: 0 0 12px 12px;"
        "   text-align: left;"  // 确保文本左对齐
        "}"
    );
    radarStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);  // 修改为左对齐

    // 添加所有控件到主布局
    radarLayout->addWidget(radarStatusWidget1);
    radarLayout->addWidget(radarStatusLabel);

    // 整体样式
    radarStatusWidget->setStyleSheet(
        "QWidget#radarStatusWidget {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "               stop:0 #f5f9fc, stop:1 #e0ecf7);"
        "   border-radius: 12px;"
        "   border: 1px solid rgba(150, 180, 210, 0.5);"
        "}"
    );

    // 添加阴影效果
    QGraphicsDropShadowEffect *radarShadow = new QGraphicsDropShadowEffect();
    radarShadow->setBlurRadius(20);
    radarShadow->setOffset(2, 4);
    radarShadow->setColor(QColor(100, 150, 200, 30));
    radarStatusWidget->setGraphicsEffect(radarShadow);

    // 添加到主布局
    statusLayout->addWidget(radarStatusWidget);

    // 添加雷达状态和摄像头状态之间的分割线
    QFrame* line3 = new QFrame();
    line3->setFrameShape(QFrame::HLine);
    line3->setFrameShadow(QFrame::Plain);
    line3->setLineWidth(1);
    line3->setStyleSheet(
        "QFrame {"
        "   color: rgba(150, 180, 210, 0.3);"  // 半透明淡蓝色
        "   margin: 10px 20px;"  // 上下边距10px，左右边距20px
        "   border: none;"
        "   background-color: rgba(150, 180, 210, 0.3);"  // 与边框同色
        "   height: 1px;"  // 明确高度
        "}"
    );
    statusLayout->addWidget(line3);

    // 创建摄像头状态主控件
    QWidget *cameraStatusWidget = new QWidget();
    cameraStatusWidget->setObjectName("cameraStatusWidget");

    // 主垂直布局
    QVBoxLayout *cameraLayout = new QVBoxLayout(cameraStatusWidget);
    cameraLayout->setContentsMargins(0, 0, 0, 0);
    cameraLayout->setSpacing(0);

    // 第一行控件容器
    QWidget *cameraStatusWidget1 = new QWidget();
    cameraStatusWidget1->setObjectName("cameraStatusWidget1");
    QHBoxLayout *cameraLayout1 = new QHBoxLayout(cameraStatusWidget1);
    cameraLayout1->setContentsMargins(20, 16, 20, 16);
    cameraLayout1->setSpacing(7);

    // 摄像头图标（使用唯一变量名 cameraIconPainter）
    QLabel *cameraIcon = new QLabel();
    QPixmap cameraPixmap(":/icons/camera.png");
    {
        QPainter cameraIconPainter(&cameraPixmap);  // 使用独立的作用域和变量名
        cameraIconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        cameraIconPainter.fillRect(cameraPixmap.rect(), QColor(70, 130, 180, 150));
    }
    cameraIcon->setPixmap(cameraPixmap.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 摄像头标题
    QLabel *cameraTitle = new QLabel("摄像头状态:");
    cameraTitle->setObjectName("cameraTitleLabel");
    cameraTitle->setStyleSheet(
        "QLabel#cameraTitleLabel {"
        "   font-size: 18px;"
        "   color: #2c3e50;"
        "   font-weight: 600;"
        "   padding: 4px 0;"
        "}"
    );

    // 摄像头结果标签
    cameraresultlabel = new QLabel("未知");
    cameraresultlabel->setObjectName("cameraResultLabel");
    cameraresultlabel->setStyleSheet(
        "QLabel#cameraResultLabel {"
        "   font-size: 18px;"
        "   font-weight: 600;"
        "   padding: 8px 24px;"
        "   border-radius: 8px;"
        "   background-color: rgba(220, 235, 245, 0.7);"
        "   color: #2c3e50;"
        "   border: 1px solid rgba(100, 150, 200, 0.3);"
        "   min-width: 100px;"
        "}"
    );
    cameraresultlabel->setAlignment(Qt::AlignCenter);

    // 添加第一行控件
    cameraLayout1->addWidget(cameraIcon);
    cameraLayout1->addWidget(cameraTitle);
    cameraLayout1->addWidget(cameraresultlabel);
    cameraLayout1->addStretch();

    // 摄像头状态标签（第二行）
    cameraStatusLabel = new QLabel("测试文字");
    cameraStatusLabel->setObjectName("cameraStatusLabel");
    cameraStatusLabel->setStyleSheet(
        "QLabel#cameraStatusLabel {"
        "   font-size: 15px;"
        "   font-weight: 500;"
        "   padding: 12px 20px;"
        "   color: #2c3e50;"
        "   background-color: rgba(210, 230, 240, 0.5);"
        "   border-top: 1px solid rgba(150, 180, 210, 0.3);"
        "   border-radius: 0 0 12px 12px;"
        "}"
    );
    cameraStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 添加所有控件到主布局
    cameraLayout->addWidget(cameraStatusWidget1);
    cameraLayout->addWidget(cameraStatusLabel);

    // 整体样式
    cameraStatusWidget->setStyleSheet(
        "QWidget#cameraStatusWidget {"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "               stop:0 #f5f9fc, stop:1 #e0ecf7);"
        "   border-radius: 12px;"
        "   border: 1px solid rgba(150, 180, 210, 0.5);"
        "}"
    );

    // 添加阴影效果（使用唯一变量名 cameraShadow）
    QGraphicsDropShadowEffect *cameraShadow = new QGraphicsDropShadowEffect();
    cameraShadow->setBlurRadius(20);
    cameraShadow->setOffset(2, 4);
    cameraShadow->setColor(QColor(100, 150, 200, 30));
    cameraStatusWidget->setGraphicsEffect(cameraShadow);

    // 添加到主布局
    statusLayout->addWidget(cameraStatusWidget);

    mainLayout->addWidget(statusWidget, 1);  // 使用剩余空间

    // 设置时间更新定时器
    timeUpdateTimer = new QTimer(this);
    connect(timeUpdateTimer, &QTimer::timeout, this, &MainWindow::updateTime);
    timeUpdateTimer->start(500);  // 每0.5秒更新一次
    updateTime();  // 立即更新时间

    return systemStatusWidget;
}
void MainWindow::updateTime()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    timeDateLabel->setText(currentTime.toString("yyyy-MM-dd hh:mm:ss"));
}
void MainWindow::updateSystemViwe(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets)
{
    int num1_people = 0; //1m内目标数量
    int num3_people = 0; //3m内目标数量
    bool owner_include = false; //车主是否在3m内
    if(doorstatus == true)
    {
        doorstatuslabel->setText("开启");
        radarresultlabel->setText("关闭");
        cameraresultlabel->setText("关闭");
        radarStatusLabel->setText("");
        cameraStatusLabel->setText("");
        doorstatuslabel->setStyleSheet("QLabel { font-size: 20px; font-weight: bold;color: green; }");
        radarresultlabel->setStyleSheet("QLabel { font-size: 20px; font-weight: bold;color: red; }");
        cameraresultlabel->setStyleSheet("QLabel { font-size: 20px; font-weight: bold; color: red;}");
        return;

    }
    else
    {
        doorstatuslabel->setText("关闭");
        radarresultlabel->setText("开启");
        cameraresultlabel->setText("开启");
        doorstatuslabel->setStyleSheet("QLabel { font-size: 20px; font-weight: bold;color: red; }");
        radarresultlabel->setStyleSheet("QLabel { font-size: 20px; font-weight: bold;color: green; }");
        cameraresultlabel->setStyleSheet("QLabel { font-size: 20px; font-weight: bold; color: green;}");
        for(int i =0 ; i < targets.size() ; i++)
        {
            QPointF Target = targets[i];  // 取出所有点坐标
            double x = Target.x();
            double y = Target.y();
            qreal distance = std::sqrt(x*x + y*y);
            if(distance <= 3)
            {
                num3_people++;
            }
            if(distance <= 1)
            {
                num1_people++;
            }
        }
        QPointF firstTarget = targets[0];  // 取出第一个点坐标
        double x = firstTarget.x();
        double y = firstTarget.y();
        qreal distance = std::sqrt(x*x + y*y);

        if(peoplestatus == true && distance < 3)
        {
            owner_include = true;
        }

        //雷达状态
        if(num3_people != 0)
        {
            if(owner_include == true)
            {
                radarStatusLabel->setText(QString("检测%1个目标，其中包含车主，车主位置为(%2, %3)，距离%4米")
                                                .arg(num3_people)
                                                .arg(x, 0, 'f', 2)
                                                .arg(y, 0, 'f', 2)
                                                .arg(distance, 0, 'f', 2));
            }
            else
            {
                radarStatusLabel->setText(QString("检测%1个目标，其中不包含车主").arg(num3_people));
            }

        }
        else
        {
            radarStatusLabel->setText("未检测到行人");
        }

        //摄像头状态
        if(num1_people > 0 && owner_include == false) //如果车主不再3m内，并且检测到1m内有人，开启录音录像功能
        {
            cameraStatusLabel->setText("检测到有陌生人越过警戒线，摄像头录音录像中...");
        }
        else if(num1_people > 0 && owner_include == true)
        {
            cameraStatusLabel->setText("检测到有车主在车身周围，摄像头待机中...");
        }
        else if(num1_people == 0)
        {
            cameraStatusLabel->setText("未检测到有陌生人越过警戒线，摄像头待机中...");
        }

    }
}
/*第二页雷达代码*/
// 更新雷达视图 ,位于更新槽函数中
void MainWindow::updateRadarView(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets)
{
    //清空左侧标点
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem *item : items) {
        if (item->type() != QGraphicsPixmapItem::Type) {  // 保留所有图片项
            scene->removeItem(item);
            delete item;
        }
    }
    //清空右侧目标数据
    QLayoutItem* child;
    while ((child = peopleInfoLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    if (doorstatus == true)
    {
        radar_statusLabel->setText("雷达处于关闭状态");
        radar_statusLabel->setStyleSheet("QLabel { color: red; font-size: 16px; }");
        return;
    }

    // 统计3米内的目标
      int nearCount = 0;
      QVector<QPair<QPointF, bool>> nearTargets; // 存储3米内的目标及其身份

      for (int i = 0; i < targets.size(); i++)
      {
          const QPointF& target = targets[i];
          qreal x = target.x();
          qreal y = target.y();
          qreal distance = std::sqrt(x*x + y*y);

          if (distance < 3.0)
          {
              nearCount++;
              // 假设第一个3米内目标是车主（根据实际逻辑调整）
              bool isOwner = (i == 0 && peoplestatus);
              nearTargets.append(qMakePair(target, isOwner));

              // 在雷达图上标记点
              qreal viewX = main_width * 0.24 + x * 45;
              qreal viewY = main_width * 0.24 - y * 75;

              // 车主用红色，陌生人用蓝色
              QColor pointColor = isOwner ? Qt::red : Qt::blue;
              QGraphicsEllipseItem *point = scene->addEllipse(
                  viewX - 5, viewY - 5, 10, 10,
                  QPen(pointColor), QBrush(pointColor)
              );

              // 添加距离和身份标签
              QGraphicsTextItem *label = scene->addText(
                  QString("%1\n%2米").arg(isOwner ? "车主" : "陌生人").arg(distance, 0, 'f', 1)
              );
              label->setPos(viewX + 10, viewY - 15);
   //           label->setDefaultTextColor(isOwner ? Qt::red : Qt::blue);  // 关键修改
              label->setDefaultTextColor(Qt::black);
              label->setFont(QFont("Arial", 8));
          }
      }

      // 更新右侧状态显示
         if (nearCount == 0)
         {
             radar_statusLabel->setText("3米内未检测到人员");
             radar_statusLabel->setStyleSheet(
                     "QLabel {"
                     "   font-size: 18px;"
                     "   font-weight: 500;"
                     "   color: #2c3e50;"
                     "   padding: 8px 0 8px 5px;"
                     "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                     "stop:0 #f5f9fc, stop:1 #e0ecf7);"  // 淡蓝渐变
                     "   border-radius: 8px;"
                     "   border: 1px solid rgba(180, 210, 230, 0.5);"
                     "   margin-left: 0;"
                     "}"
                 );
         }
         else
         {
             radar_statusLabel->setText(QString("3米内检测到%1人").arg(nearCount));
             radar_statusLabel->setStyleSheet(
                     "QLabel {"
                     "   font-size: 18px;"
                     "   font-weight: 500;"
                     "   color: #2c3e50;"
                     "   padding: 8px 0 8px 5px;"
                     "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                     "  stop:0 #f5f9fc, stop:1 #e0ecf7);"  // 淡蓝渐变
                     "   border-radius: 8px;"
                     "   border: 1px solid rgba(180, 210, 230, 0.5);"
                     "   margin: 0 0 8px 0;"
                     "}"
                 );
             // 为每个3米内目标创建信息标签
             for (int i = 0; i < nearTargets.size(); i++)
             {
                 const auto& target = nearTargets[i];
                 qreal x = target.first.x();
                 qreal y = target.first.y();
                 qreal distance = std::sqrt(x*x + y*y);

   //              QLabel *personLabel = new QLabel(
   //                  QString("第%1人: %2\n坐标: (%3, %4)\n距离: %5米")
   //                      .arg(i+1)
   //                      .arg(target.second ? "车主" : "陌生人")
   //                      .arg(x, 0, 'f', 2)
   //                      .arg(y, 0, 'f', 2)
   //                      .arg(distance, 0, 'f', 2)
   //              );

                 QLabel *personLabel = new QLabel(
                             QString("<div style='line-height:1'>"
                                     "<b>第%1人</b>: <span style='color:%2'>%3</span><br>"
                                     "坐标: (%4, %5)<br>"
                                     "距离: %6米</div>")
                             .arg(i+1)
                             .arg(target.second ? "#e74c3c" : "#3498db")
                             .arg(target.second ? "车主" : "陌生人")
                             .arg(x, 0, 'f', 2)
                             .arg(y, 0, 'f', 2)
                             .arg(distance, 0, 'f', 2)
                         );
                 personLabel->setStyleSheet(
                     QString("QLabel { font-size: 17px;"
                             "font-weight: 500;"
                             "padding: 5px; "
                             "border: 1px solid rgba(180, 210, 230, 0.5);"
                             "background-color: rgba(240, 248, 255, 0.95);"
                             "color: #2c3e50;"
                             "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                             "stop:0 #f5f9fc, stop:1 #e0ecf7);"  // 淡蓝渐变
                             "border-radius: 5px;"
                             "margin: 0 0 0px 0; }")
                 );
                 personLabel->setWordWrap(true);
                 personLabel->setMargin(5);

                 peopleInfoLayout->addWidget(personLabel);
             }
         }
}
QWidget* MainWindow::createRadarWidget()
{
    radarPage = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(radarPage); // 改为水平主布局

    // 1. 右侧控制区域（垂直布局）
    QWidget *controlPanel = new QWidget();
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);


    /* 右侧状态显示区域*/
    //顶部
    radar_statusLabel = new QLabel("等待检测...");
    radar_statusLabel->setWordWrap(true);
    radar_statusLabel->setStyleSheet("QLabel { color: black; font-size: 16px; }");
    radar_statusLabel->setAlignment(Qt::AlignCenter);

    // 中部人员信息区域（使用ScrollArea显示多人信息）
    QScrollArea *scrollArea = new QScrollArea();
    QWidget *peopleInfoWidget = new QWidget();
    peopleInfoLayout = new QVBoxLayout(peopleInfoWidget);
    peopleInfoLayout->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(peopleInfoWidget);
    scrollArea->setWidgetResizable(true);

    // 底部空白区域
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    controlLayout->addWidget(radar_statusLabel);
    controlLayout->addWidget(scrollArea);
    //controlLayout->addWidget(spacer);
    //controlLayout->addStretch(); // 添加伸缩项使状态标签在上方

    // 左侧雷达图形区域
    QGraphicsView *radarView = new QGraphicsView();
    scene = new QGraphicsScene();
    radarView->setScene(scene);
    radarView->setFixedSize(main_width *0.48+15, main_width *0.48+15);  // 固定大小

    // 加载背景图片
    QPixmap bgImage(":/image/雷达车图.png");
    if (!bgImage.isNull()) {
        backgroundItem = scene->addPixmap(bgImage.scaled(main_width *0.48, main_width *0.48, Qt::KeepAspectRatioByExpanding));
        backgroundItem->setZValue(-1);  // 确保背景在最底层
    } else {
        qDebug() << "Failed to load background image!";
    }

    // 将控制面板和雷达视图添加到主布局
    mainLayout->addWidget(radarView);
    mainLayout->addWidget(controlPanel);

    controlPanel->setFixedWidth(200); // 设置控制面板的宽度,根据需要调整宽度

    return radarPage;
}
/*第三页摄像头状态代码*/
QWidget* MainWindow::createCameraWidget()
{
    CameraPage = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(CameraPage); // 改为水平主布局

    // 1. 右侧控制区域（垂直布局）
    QWidget *controlPanel = new QWidget();
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);

    // 右侧状态显示区域
    cam1statusLabel = new QLabel("摄像头1状态：未知");
    cam1statusLabel->setWordWrap(true);
    cam1statusLabel->setStyleSheet("QLabel { color: black; font-size: 16px; }");
    cam2statusLabel = new QLabel("摄像头2状态：未知");
    cam2statusLabel->setWordWrap(true);
    cam2statusLabel->setStyleSheet("QLabel { color: black; font-size: 16px; }");
    cam3statusLabel = new QLabel("摄像头3状态：未知");
    cam3statusLabel->setWordWrap(true);
    cam3statusLabel->setStyleSheet("QLabel { color: black; font-size: 16px; }");
    cam4statusLabel = new QLabel("摄像头4状态：未知");
    cam4statusLabel->setWordWrap(true);
    cam4statusLabel->setStyleSheet("QLabel { color: black; font-size: 16px; }");

    controlLayout->addWidget(cam1statusLabel);
    controlLayout->addWidget(cam2statusLabel);
    controlLayout->addWidget(cam3statusLabel);
    controlLayout->addWidget(cam4statusLabel);


    // 左侧摄像头状态图区域
    QGraphicsView *CamView = new QGraphicsView();
    camscene = new QGraphicsScene();
    CamView->setScene(camscene);
    CamView->setFixedSize(main_width *0.485+15, main_width *0.485+15);  // 固定大小

    // 加载背景图片
    QPixmap bgImage(":/image/0.jpg");
    if (!bgImage.isNull()) {
        backgroundItem = camscene->addPixmap(bgImage.scaled(main_width *0.45, main_width *0.45, Qt::KeepAspectRatioByExpanding));
        backgroundItem->setZValue(-1);  // 确保背景在最底层
    } else {
        qDebug() << "Failed to load background image!";
    }

    // 添加到主布局
    mainLayout->addWidget(CamView);
    mainLayout->addWidget(controlPanel);

    // 设置控制面板的宽度
    controlPanel->setFixedWidth(200); // 根据需要调整宽度

    return CameraPage;
}
void MainWindow::updateCameraView(bool doorstatus, bool peoplestatus, int peoplenum, const QVector<QPointF>& targets)
{
    //摄像头开关指令文件，字节高四位控制关闭，低四位开启
    QFile commandFile("/home/j/opendoor/carmer_receive");

    if(doorstatus == true)
    {
        cam1statusLabel->setText("摄像头1状态：关闭");
        cam1statusLabel->setStyleSheet("color: red;");
        cam2statusLabel->setText("摄像头2状态：关闭");
        cam2statusLabel->setStyleSheet("color: red;");
        cam3statusLabel->setText("摄像头3状态：关闭");
        cam3statusLabel->setStyleSheet("color: red;");
        cam4statusLabel->setText("摄像头4状态：关闭");
        cam4statusLabel->setStyleSheet("color: red;");
        command = 0xf0;
        //更改图片，并发送控制摄像头指令
        changepit(":/image/0.jpg");
        if (commandFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            commandFile.write(reinterpret_cast<const char*>(&command), 1);  // 写入一个字节
            commandFile.close();
        }
        return;
    }

    for(int i = 0; i < targets.size(); i++)
    {
        QPointF Target = targets[i];  // 取出所有点坐标
        double x = Target.x();
        double y = Target.y();
        qreal distance = std::sqrt(x*x + y*y);
        if(distance <= 1.6)
        {

            if(y >= 0 && qAbs(y) > x)
            {
                one = true;
            }
            else if(x < 0 && qAbs(y) < qAbs(x))
            {
                two = true;
            }
            else if(y < 0 && y > qAbs(x))
            {
                three = true;
            }
            else if(x >= 0 && y <= qAbs(x))
            {
                four = true;
            }
        }
        else if(distance >= 2.1)
        {
            if(y >= 0 && qAbs(y) > x)
            {
                one = false;
            }
            else if(x < 0 && qAbs(y) < qAbs(x))
            {
                two = false;
            }
            else if(y < 0 && y > qAbs(x))
            {
                three = false;
            }
            else if(x >= 0 && y <= qAbs(x))
            {
                four = false;
            }
        }

    }


    if( (one || two || three|| four)  && peoplestatus == false)
    {
        if(two)
        {
            cam2statusLabel->setText("摄像头2状态：录制中...");
            cam2statusLabel->setStyleSheet("color: green;");
            command |= 0x02;
            command &= 0xDF;
        }
        else
        {
            cam2statusLabel->setText("摄像头2状态：待机...");
            cam2statusLabel->setStyleSheet("color: red;");
            command |= 0x20;
            command &= 0xFD;
        }
        if(one)
        {
            cam1statusLabel->setText("摄像头1状态：录制中...");
            cam1statusLabel->setStyleSheet("color: green;");
            command |= 0x01;
            command &= 0xEF;
        }
        else
        {
            cam1statusLabel->setText("摄像头1状态：待机...");
            cam1statusLabel->setStyleSheet("color: red;");
            command |= 0x10;
            command &= 0xFE;
        }
        if(three)
        {
            cam3statusLabel->setText("摄像头3状态：录制中...");
            cam3statusLabel->setStyleSheet("color: green;");
            command |= 0x04;
            command &= 0xBF;
        }
        else
        {
            cam3statusLabel->setText("摄像头3状态：待机...");
            cam3statusLabel->setStyleSheet("color: red;");
            command |= 0x40;
            command &= 0xFB;
        }
        if(four)
        {
            cam4statusLabel->setText("摄像头4状态：录制中...");
            cam4statusLabel->setStyleSheet("color: green;");
            command |= 0x08;
            command &= 0x7F;
        }
        else
        {
            cam4statusLabel->setText("摄像头4状态：待机...");
            cam4statusLabel->setStyleSheet("color: red;");
            command |= 0x80;
            command &= 0xF7;
        }
    }
    else
    {
        command = 0xf0;
        cam4statusLabel->setText("摄像头4状态：待机...");
        cam4statusLabel->setStyleSheet("color: gray;");
        cam1statusLabel->setText("摄像头1状态：待机");
        cam1statusLabel->setStyleSheet("color: gray;");
        cam3statusLabel->setText("摄像头3状态：待机");
        cam3statusLabel->setStyleSheet("color: gray;");
        cam2statusLabel->setText("摄像头2状态：待机");
        cam2statusLabel->setStyleSheet("color: gray;");
        changepit(":/image/0.jpg");
    }
    //判断各个摄像头状态
    if(command != 0xf0)
    {
        if((command & 0x0f) == 0x01)
        {
            changepit(":/image/1.jpg");
        }
        else if((command & 0x0f) == 0x02)
        {
            changepit(":/image/2.jpg");
        }
        else if((command & 0x0f) == 0x03)
        {
            changepit(":/image/12.jpg");
        }
        else if((command & 0x0f) == 0x04)
        {
            changepit(":/image/3.jpg");
        }
        else if((command & 0x0f) == 0x05)
        {
            changepit(":/image/13.jpg");
        }
        else if((command & 0x0f) == 0x06)
        {
            changepit(":/image/23.jpg");
        }
        else if((command & 0x0f) == 0x07)
        {
            changepit(":/image/123.jpg");
        }
        else if((command & 0x0f) == 0x08)
        {
            changepit(":/image/4.jpg");
        }
        else if((command & 0x0f) == 0x09)
        {
            changepit(":/image/14.jpg");
        }
        else if((command & 0x0f) == 0x0a)
        {
            changepit(":/image/24.jpg");
        }
        else if((command & 0x0f) == 0x0b)
        {
            changepit(":/image/124.jpg");
        }
        else if((command & 0x0f) == 0x0c)
        {
            changepit(":/image/34.jpg");
        }
        else if((command & 0x0f) == 0x0d)
        {
            changepit(":/image/124.jpg");
        }
        else if((command & 0x0f) == 0x0e)
        {
            changepit(":/image/234.jpg");
        }
        else if((command & 0x0f) == 0x0f)
        {
            changepit(":/image/1234.jpg");
        }
    }

    //检测到1m内目标进行摄像头控制命令写入
    if (commandFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        commandFile.write(reinterpret_cast<const char*>(&command), 1);  // 写入一个字节
        commandFile.close();
    }

}
void MainWindow::changepit(QString  newImagePath)
{
    QPixmap newBgImage(newImagePath);
    if (!newBgImage.isNull()) {
           // 先移除旧的背景（如果存在）
           if (backgroundItem) {
               camscene->removeItem(backgroundItem);
               delete backgroundItem;
           }
           // 添加新背景
           backgroundItem = camscene->addPixmap(newBgImage.scaled(main_width *0.5, main_width *0.5, Qt::KeepAspectRatioByExpanding));
           backgroundItem->setZValue(-1); // 保持背景在最底层
       } else {
           qDebug() << "Failed to load new background image:" << newImagePath;
       }
}
/*第四页视频代码*/
QWidget* MainWindow::createVideoListWidget()
{
    QWidget *container = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(container);

    // 添加标题
    QLabel *titleLabel = new QLabel("录像文件列表");
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // 添加刷新按钮
    QPushButton *refreshButton = new QPushButton("刷新列表");
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshVideoList);
    mainLayout->addWidget(refreshButton);

    // 添加滚动区域
    QScrollArea *scrollArea = new QScrollArea();
    QWidget *scrollContent = new QWidget();
    videoListLayout = new QVBoxLayout(scrollContent);  // 初始化成员变量
    scrollContent->setLayout(videoListLayout);
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    mainLayout->addWidget(scrollArea);

    // 初始加载视频列表
    refreshVideoList();

    return container;
}
void MainWindow::refreshVideoList()
{
    // 清除现有视频列表项
    QLayoutItem* child;
    while ((child = videoListLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // 获取视频目录
    QString videoDirPath = "/home/j/opendoor/video";
    QDir videoDir(videoDirPath);

    // 检查目录是否存在
    if (!videoDir.exists()) {
        QLabel *errorLabel = new QLabel("视频目录不存在: " + videoDirPath);
        videoListLayout->addWidget(errorLabel);
        return;
    }

    // 获取视频文件列表
    QStringList filters;
    filters << "*.mp4" << "*.avi" << "*.mkv" << "*.mov"; // 常见视频格式
    QFileInfoList videoFiles = videoDir.entryInfoList(filters, QDir::Files, QDir::Time);

    if (videoFiles.isEmpty()) {
        QLabel *emptyLabel = new QLabel("没有找到视频文件");
        videoListLayout->addWidget(emptyLabel);
        return;
    }

    // 添加每个视频文件到列表
    foreach (QFileInfo fileInfo, videoFiles) {
        addVideoItem(fileInfo);
    }
}
void MainWindow::addVideoItem(const QFileInfo& fileInfo)
{
    QWidget *itemWidget = new QWidget();
    QHBoxLayout *itemLayout = new QHBoxLayout(itemWidget);

    // 文件名标签
    QLabel *nameLabel = new QLabel(fileInfo.fileName());
    nameLabel->setWordWrap(true);
    itemLayout->addWidget(nameLabel, 1);

    // 文件大小标签
    QString sizeStr;
    qint64 size = fileInfo.size();
    if (size < 1024) {
        sizeStr = QString("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        sizeStr = QString("%1 KB").arg(size / 1024);
    } else {
        sizeStr = QString("%1 MB").arg(size / (1024 * 1024));
    }
    QLabel *sizeLabel = new QLabel(sizeStr);
    itemLayout->addWidget(sizeLabel);

    // 播放按钮
    QPushButton *playButton = new QPushButton("播放");
    connect(playButton, &QPushButton::clicked, this, [this, fileInfo](){
        playVideo(fileInfo);
    });
    itemLayout->addWidget(playButton);

    // 删除按钮
    QPushButton *deleteButton = new QPushButton("删除");
    connect(deleteButton, &QPushButton::clicked, this, [this, fileInfo](){
        deleteVideo(fileInfo);
    });
    itemLayout->addWidget(deleteButton);

    videoListLayout->addWidget(itemWidget);

    // 添加分隔线
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    videoListLayout->addWidget(line);
}
void MainWindow::playVideo(const QFileInfo& fileInfo)
{
    // 使用树莓派默认播放器播放视频
    QString playerCommand = "xdg-open";
    QStringList arguments;
    arguments << fileInfo.absoluteFilePath();

    QProcess *process = new QProcess(this);
    process->start(playerCommand, arguments);

    if (!process->waitForStarted()) {
        QMessageBox::warning(this, "错误", "无法启动视频播放器");
    }
}
void MainWindow::deleteVideo(const QFileInfo& fileInfo)
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认删除",
                                 "确定要删除文件: " + fileInfo.fileName() + "?",
                                 QMessageBox::Yes|QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QFile file(fileInfo.absoluteFilePath());
        if (file.remove()) {
            // 刷新列表，直接调用 refreshVideoList()
            refreshVideoList();
        } else {
            QMessageBox::warning(this, "错误", "删除文件失败");
        }
    }
}
/*串口*/
void MainWindow::process()
{
    static QByteArray buffer;
    buffer.append(serial->readAll());
    while (buffer.contains(';')) {
        int endIndex = buffer.indexOf(';');
        QByteArray packet = buffer.left(endIndex);
        buffer.remove(0, endIndex + 1);
        QString strData = QString::fromUtf8(packet).trimmed();
        QStringList parts = strData.split(',');

        if (parts.size() == 8) {
            //数据符合格式
            bool doorstatus = (parts[0].trimmed().toLower() == "true" || parts[0] == "1");
            bool peoplestatus = (parts[1].trimmed().toLower() == "true" || parts[1] == "1");
            int peoplenum = 3;
            QVector <QPointF> targets;

            // 解析所有目标坐标
            for (int i = 0; i < peoplenum; i++) {
                double x = parts[2 + i * 2].toDouble();
                double y = parts[3 + i * 2].toDouble();
                targets.append(QPointF(x, y));
            }

            qDebug() << "doorstatu,peoplestatus,num,target" << doorstatus << peoplestatus << peoplenum << targets;
            qDebug() << "发送更新页面信号";
            emit statusUpdated(doorstatus, peoplestatus, peoplenum, targets);
        }
    }

}
void MainWindow::openSerialPort(const QString &portName)
{

    qDebug() << "串口开启";

    // 创建线程
    serialThread = new QThread();
    // 创建串口对象
    serial = new QSerialPort();
    serial->setPortName(portName);
    serial->setBaudRate(QSerialPort::Baud115200);
    if (!serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(serial->errorString());
        delete serial;
        return;
    }

    // 将串口对象移动到线程
    serial->moveToThread(serialThread);
    // 连接信号槽
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::process);
    connect(serialThread, &QThread::finished, serial, &QSerialPort::deleteLater);

    abort = false;
    qDebug()<<"开启线程";
    serialThread->start();
    qDebug()<<"线程运行状态"<<serialThread->isRunning();

}
void MainWindow::closeSerialPort()
{
    abort = true;
    if (serial->isOpen()) {
        serial->close();
    }
}
/*读取人脸识别结果*/
bool MainWindow::checkFaceDetection()
{
    QFile responseFile("/home/j/opendoor/qt_receive");
    if (!responseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&responseFile);
    QString response = in.readLine().trimmed();
    responseFile.close();

    // 清空文件内容以便下次检测
    responseFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    responseFile.close();

    return response == "1";
}
void MainWindow::processFaceDetection()
{
    static QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
            if (checkFaceDetection()) {
                if (serial && serial->isOpen()) {
                    QByteArray data;
                    data.append(0x01);
                    serial->write(data);
                    qDebug() << "检测到人脸，已通过串口发送true";
                }
            }
        });

    timer->start(100);  // 每100ms检查一次
}
